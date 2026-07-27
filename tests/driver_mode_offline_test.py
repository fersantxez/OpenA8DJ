#!/usr/bin/env python3
"""Offline driver-mode IPC, public API, locking, and compatibility tests."""

import argparse
import json
import os
import re
import socket
import struct
import subprocess
import tempfile
import threading
import time
from pathlib import Path


MAGIC = 0x4A443841
VERSION = 1
CONTROL_GET = 4
CONTROL_SET = 5
CONTROL_STATE = 6
STREAM_STATS_GET = 10
STREAM_STATS = 11
DRIVER_MODE_GET = 12
DRIVER_MODE_SET = 13
DRIVER_MODE_STATE = 14
VINTAGE_COMPATIBLE_GET = 19
HEADER = struct.Struct("=IBBH")
SET_PAYLOAD = struct.Struct("=HHI8s")
STATE_PAYLOAD = struct.Struct("=HHIIBBBBQQQQQQIIII8s")
SCHEMA = "org.opena8dj.public-api.response.v1"


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def invoke(binary, *args, timeout=5):
    result = subprocess.run(
        [str(binary), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )
    document = json.loads(result.stdout)
    return result, document


def validate_envelope(document, operation, ok=True):
    check(document["schema"] == SCHEMA, "wrong public schema")
    check(document["apiVersion"] == "1.0", "wrong public API version")
    check(document["operation"] == operation, "wrong operation")
    check(document["ok"] is ok, "wrong success flag")


def balanced_state(**overrides):
    state = {
        "schema": 1,
        "reserved0": 0,
        "requested": 1,
        "effective": 1,
        "pending": 0,
        "streaming": 0,
        "last_result": 0,
        "rejection": 0,
        "generation": 0,
        "accepted": 0,
        "rejected": 0,
        "applied": 0,
        "failures": 0,
        "pending_transitions": 0,
        "start": 8192,
        "restart": 4096,
        "target": 8192,
        "qos": 0,
        "reserved": b"\0" * 8,
    }
    state.update(overrides)
    return state


def pack_state(state):
    return STATE_PAYLOAD.pack(
        state["schema"],
        state["reserved0"],
        state["requested"],
        state["effective"],
        state["pending"],
        state["streaming"],
        state["last_result"],
        state["rejection"],
        state["generation"],
        state["accepted"],
        state["rejected"],
        state["applied"],
        state["failures"],
        state["pending_transitions"],
        state["start"],
        state["restart"],
        state["target"],
        state["qos"],
        state["reserved"],
    )


def apply_mode(state, mode):
    state = dict(state)
    state["accepted"] += 1
    state["rejection"] = 0
    if state["streaming"]:
        if mode == state["effective"]:
            if state["pending"]:
                state["requested"] = state["effective"]
                state["pending"] = 0
                state["last_result"] = 3
            else:
                state["last_result"] = 0
        else:
            if not state["pending"] or state["requested"] != mode:
                state["requested"] = mode
                state["pending"] = 1
                state["pending_transitions"] += 1
            state["last_result"] = 2
        return state
    if mode == state["effective"]:
        state["requested"] = mode
        state["pending"] = 0
        state["last_result"] = 0
        return state
    state["requested"] = mode
    state["effective"] = mode
    state["pending"] = 0
    state["last_result"] = 1
    state["applied"] += 1
    state["generation"] += 1
    if mode == 2:
        state.update(start=4096, restart=4096, target=4096, qos=1)
    else:
        state.update(start=8192, restart=4096, target=8192, qos=0)
    return state


class ModeIPC:
    def __init__(
        self,
        path,
        state=None,
        stats=b"",
        accepts=1,
        reply_transform=None,
        readback_transform=None,
        state_bytes_transform=None,
        delay_after_set=0,
    ):
        self.path = str(path)
        self.state = balanced_state() if state is None else dict(state)
        self.stats = stats
        self.accepts = accepts
        self.reply_transform = reply_transform
        self.readback_transform = readback_transform
        self.state_bytes_transform = state_bytes_transform
        self.delay_after_set = delay_after_set
        self.requests = []
        self.error = None
        self.state_lock = threading.Lock()
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(self.path)
        os.chmod(self.path, 0o666)
        self.listener.listen(accepts)
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.handlers = []

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, traceback):
        self.listener.close()
        self.thread.join(timeout=2)
        for handler in self.handlers:
            handler.join(timeout=2)
        if self.error is not None and exc_type is None:
            raise self.error

    @staticmethod
    def read_full(connection, length):
        data = bytearray()
        while len(data) < length:
            chunk = connection.recv(length - len(data))
            if not chunk:
                return None
            data.extend(chunk)
        return bytes(data)

    @staticmethod
    def send(connection, message_type, payload):
        connection.sendall(HEADER.pack(MAGIC, VERSION, message_type, len(payload)) + payload)

    def _handle(self, connection, connection_id):
        set_seen = False
        try:
            with connection:
                self.send(connection, CONTROL_STATE, bytes(range(13)))
                while True:
                    raw_header = self.read_full(connection, HEADER.size)
                    if raw_header is None:
                        break
                    magic, version, message_type, length = HEADER.unpack(raw_header)
                    payload = self.read_full(connection, length)
                    if payload is None:
                        break
                    self.requests.append((connection_id, magic, version, message_type, payload))
                    if message_type == DRIVER_MODE_SET:
                        _, _, mode, _ = SET_PAYLOAD.unpack(payload)
                        with self.state_lock:
                            self.state = apply_mode(self.state, mode)
                            reply = dict(self.state)
                        if self.reply_transform is not None:
                            reply = self.reply_transform(reply)
                        state_bytes = pack_state(reply)
                        if self.state_bytes_transform is not None:
                            state_bytes = self.state_bytes_transform(state_bytes)
                        self.send(connection, DRIVER_MODE_STATE, state_bytes)
                        set_seen = True
                        if self.delay_after_set:
                            time.sleep(self.delay_after_set)
                    elif message_type == DRIVER_MODE_GET:
                        with self.state_lock:
                            reply = dict(self.state)
                        if set_seen and self.readback_transform is not None:
                            reply = self.readback_transform(reply)
                        state_bytes = pack_state(reply)
                        if self.state_bytes_transform is not None:
                            state_bytes = self.state_bytes_transform(state_bytes)
                        self.send(connection, DRIVER_MODE_STATE, state_bytes)
                    elif message_type == STREAM_STATS_GET:
                        self.send(connection, STREAM_STATS, self.stats)
                    elif message_type == CONTROL_GET:
                        self.send(connection, CONTROL_STATE, bytes(range(13)))
                    elif message_type == CONTROL_SET:
                        self.send(connection, CONTROL_STATE, payload)
        except (BrokenPipeError, ConnectionResetError, OSError, struct.error) as error:
            if self.listener.fileno() != -1:
                self.error = error

    def _serve(self):
        try:
            for connection_id in range(self.accepts):
                connection, _ = self.listener.accept()
                handler = threading.Thread(
                    target=self._handle, args=(connection, connection_id), daemon=True
                )
                self.handlers.append(handler)
                handler.start()
        except OSError as error:
            if self.listener.fileno() != -1:
                self.error = error


def compile_control(source, output, socket_path, lock_path):
    subprocess.run(
        [
            "xcrun",
            "clang",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            "-O2",
            f'-DOPENA8DJ_PUBLIC_API_SOCKET_PATH="{socket_path}"',
            f'-DOPENA8DJ_PUBLIC_API_LOCK_PATH="{lock_path}"',
            "-DOPENA8DJ_PUBLIC_API_TEST_TRUST_CURRENT_UID=1",
            "-framework",
            "CoreAudio",
            "-framework",
            "CoreFoundation",
            "-o",
            str(output),
            str(source),
        ],
        check=True,
        timeout=30,
    )


def make_stream_payload(source, mode_values=None):
    text = source.read_text()
    match = re.search(
        r"typedef struct OpenA8DJStreamStatsPayload \{(.*?)\}"
        r" __attribute__\(\(packed\)\) OpenA8DJStreamStatsPayload;",
        text,
        re.S,
    )
    check(match is not None, "stream stats payload missing")
    formats = {"uint8_t": "B", "uint32_t": "I", "uint64_t": "Q", "double": "d"}
    fields = re.findall(
        r"^\s*(uint8_t|uint32_t|uint64_t|double)\s+([A-Za-z0-9_]+);",
        match.group(1),
        re.M,
    )
    payload = bytearray(sum(struct.calcsize("=" + formats[t]) for t, _ in fields))
    offset = 0
    offsets = {}
    for type_name, name in fields:
        offsets[name] = (offset, formats[type_name])
        offset += struct.calcsize("=" + formats[type_name])
    values = {"streaming": 1, "sampleRate": 48000.0}
    if mode_values is not None:
        values.update(mode_values)
    for name, value in values.items():
        field_offset, field_format = offsets[name]
        struct.pack_into("=" + field_format, payload, field_offset, value)
    legacy_length = offsets["deviceDataAlignment"][0] + struct.calcsize(
        "=" + offsets["deviceDataAlignment"][1]
    )
    return bytes(payload), legacy_length


def check_mode_document(document, requested, effective, pending, result):
    data = document["data"]
    check(data["schemaVersion"] == 1, "missing mode schema")
    check(data["requestedMode"] == requested, "wrong requested mode")
    check(data["effectiveMode"] == effective, "wrong effective mode")
    check(data["pending"] is pending, "wrong pending state")
    check(data["lastResult"] == result, "wrong result")
    check(set(data["counters"]) == {
        "acceptedRequests",
        "rejectedRequests",
        "appliedTransitions",
        "applyFailures",
        "pendingTransitions",
    }, "wrong counters")


def expect_error(binary, exit_code, operation, code, *args):
    result, document = invoke(binary, *args)
    check(result.returncode == exit_code, f"{code}: wrong exit code")
    validate_envelope(document, operation, False)
    check(document["error"]["code"] == code, f"{code}: wrong public error")


def run_tests(repo, shipping_binary):
    source = repo / "src/tools/opena8dj-control.c"
    state_test = repo / "build/driver-mode-state-test"
    subprocess.run(
        [
            "xcrun",
            "clang",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            "-I",
            str(repo / "src/hal"),
            "-o",
            str(state_test),
            str(repo / "tests/driver_mode_state_test.c"),
        ],
        check=True,
        timeout=30,
    )
    state_result = subprocess.run(
        [str(state_test)], text=True, capture_output=True, check=False, timeout=5
    )
    check(state_result.returncode == 0, state_result.stderr)
    check("PASS" in state_result.stdout, "state harness did not pass")

    result, document = invoke(shipping_binary, "api", "driver-modes")
    check(result.returncode == 0, "offline mode list failed")
    validate_envelope(document, "driver_modes.list")
    check([entry["id"] for entry in document["data"]["driverModes"]] == [
        "balanced", "performance", "timecode-optimized",
        "vintage-compatible"
    ], "mode allowlist/order changed")
    check(document["data"]["driverModes"][2]["requiresArm"] is True,
          "timecode mode is not explicitly armed")
    expect_error(
        shipping_binary,
        2,
        "driver_mode.set",
        "driver_mode_arm_required",
        "api",
        "driver-mode",
        "set",
        "timecode-optimized",
    )
    expect_error(
        shipping_binary,
        2,
        "driver_mode.set",
        "driver_mode_not_allowed",
        "api",
        "driver-mode",
        "set",
        "timecode-vinyl",
    )

    with tempfile.TemporaryDirectory(prefix="a8-mode-", dir="/tmp") as temporary:
        base = Path(temporary)
        socket_path = base / "control.sock"
        lock_path = base / "mutation.lock"
        harness = base / "control"
        compile_control(source, harness, socket_path, lock_path)

        with ModeIPC(socket_path) as server:
            result, document = invoke(harness, "api", "driver-mode")
            check(result.returncode == 0, "default get failed")
            validate_envelope(document, "driver_mode.get")
            check_mode_document(document, "balanced", "balanced", False, "unchanged")
            check(document["data"]["effectivePolicy"] == {
                "outputStartLatencyFrames": 8192,
                "outputRestartLatencyFrames": 4096,
                "outputTargetLatencyFrames": 8192,
                "workerQoS": "default",
                "inputLeadGuardEnabled": False,
                "inputLeadCeilingFrames": 32768,
                "timecodeEvidenceRequired": False,
            }, "balanced policy differs from shipping defaults")
            check([request[3] for request in server.requests] == [
                DRIVER_MODE_GET, VINTAGE_COMPATIBLE_GET
            ],
                  "mode get performed extra IPC")
        socket_path.unlink(missing_ok=True)

        with ModeIPC(socket_path, balanced_state(reserved=b"\x7f" * 8)):
            result, document = invoke(harness, "api", "driver-mode")
            check(result.returncode == 0, "future reserved response bytes were not ignored")
            check_mode_document(document, "balanced", "balanced", False, "unchanged")
        socket_path.unlink(missing_ok=True)

        with ModeIPC(socket_path) as server:
            result, document = invoke(
                harness, "api", "driver-mode", "set", "performance"
            )
            check(result.returncode == 0, "idle performance set failed")
            validate_envelope(document, "driver_mode.set")
            check_mode_document(document, "performance", "performance", False, "applied")
            check(document["data"]["effectivePolicy"]["outputTargetLatencyFrames"] == 4096,
                  "performance target is not 4096")
            check(document["data"]["effectivePolicy"]["workerQoS"] == "user-interactive",
                  "performance worker QoS missing")
            check([request[3] for request in server.requests] == [
                DRIVER_MODE_SET, DRIVER_MODE_GET,
                VINTAGE_COMPATIBLE_GET
            ], "set did not do same-connection read-back")
        socket_path.unlink(missing_ok=True)

        pending = balanced_state(streaming=1)
        with ModeIPC(socket_path, pending) as server:
            result, document = invoke(
                harness, "api", "driver-mode", "set", "performance"
            )
            check(result.returncode == 0, "streaming pending set failed")
            check_mode_document(document, "performance", "balanced", True, "pending")
            check(document["data"]["effectivePolicy"]["outputStartLatencyFrames"] == 8192,
                  "pending request rewrote effective policy")
        socket_path.unlink(missing_ok=True)

        cancel = balanced_state(
            requested=2,
            effective=1,
            pending=1,
            streaming=1,
            last_result=2,
            generation=1,
            accepted=1,
            pending_transitions=1,
        )
        with ModeIPC(socket_path, cancel):
            result, document = invoke(
                harness, "api", "driver-mode", "set", "balanced"
            )
            check(result.returncode == 0, "pending cancel failed")
            check_mode_document(document, "balanced", "balanced", False, "cancelled")
        socket_path.unlink(missing_ok=True)

        def apply_failed(_state):
            return balanced_state(
                last_result=5, accepted=1, failures=1
            )

        with ModeIPC(socket_path, reply_transform=apply_failed,
                     readback_transform=apply_failed):
            expect_error(
                harness,
                5,
                "driver_mode.set",
                "driver_mode_apply_failed",
                "api",
                "driver-mode",
                "set",
                "performance",
            )
        socket_path.unlink(missing_ok=True)

        mismatch_cases = [
            ("schema", lambda state: dict(state, schema=2)),
            ("enum", lambda state: dict(state, effective=99)),
            (
                "contradictory",
                lambda state: dict(state, requested=2, effective=1, pending=0),
            ),
        ]
        for _name, transform in mismatch_cases:
            with ModeIPC(socket_path, state=balanced_state(), reply_transform=transform):
                expect_error(
                    harness,
                    4,
                    "driver_mode.set",
                    "backend_protocol_error",
                    "api",
                    "driver-mode",
                    "set",
                    "performance",
                )
            socket_path.unlink(missing_ok=True)

        def disagree(state):
            return balanced_state(accepted=state["accepted"])

        with ModeIPC(socket_path, readback_transform=disagree):
            expect_error(
                harness,
                4,
                "driver_mode.set",
                "backend_protocol_error",
                "api",
                "driver-mode",
                "set",
                "performance",
            )
        socket_path.unlink(missing_ok=True)

        with ModeIPC(socket_path, state_bytes_transform=lambda payload: payload[:-1]):
            expect_error(
                harness,
                4,
                "driver_mode.set",
                "backend_protocol_error",
                "api",
                "driver-mode",
                "set",
                "performance",
            )
        socket_path.unlink(missing_ok=True)

        mode_stats = {
            "driverModeSchemaVersion": 1,
            "driverModeRequested": 2,
            "driverModeEffective": 1,
            "driverModePending": 1,
            "driverModeLastResult": 2,
            "driverModeRejectionReason": 0,
            "driverModeGeneration": 7,
            "driverModeAcceptedRequests": 9,
            "driverModeRejectedRequests": 3,
            "driverModeAppliedTransitions": 2,
            "driverModeApplyFailures": 1,
            "driverModePendingTransitions": 4,
            "driverModeOutputStartLatencyFrames": 8192,
            "driverModeOutputRestartLatencyFrames": 4096,
            "driverModeOutputTargetLatencyFrames": 8192,
            "driverModeWorkerQoS": 0,
        }
        stats, legacy_length = make_stream_payload(source, mode_stats)
        with ModeIPC(socket_path, stats=stats):
            result, document = invoke(harness, "api", "stats")
            check(result.returncode == 0, "stats mode tail failed")
            validate_envelope(document, "stats.get")
            check_mode_document(
                {"data": document["data"]["driverMode"]},
                "performance",
                "balanced",
                True,
                "pending",
            )
            check(document["data"]["driverMode"]["counters"]["rejectedRequests"] == 3,
                  "mode rejection counter missing from stats")
        socket_path.unlink(missing_ok=True)

        with ModeIPC(socket_path, stats=stats[:legacy_length]):
            result, document = invoke(harness, "api", "stats")
            check(result.returncode == 0, "legacy stats payload rejected")
            check(document["data"]["driverMode"] is None,
                  "legacy stats fabricated balanced mode")
        socket_path.unlink(missing_ok=True)

        with ModeIPC(socket_path, accepts=2, delay_after_set=0.2) as server:
            processes = [
                subprocess.Popen(
                    [str(harness), "api", "driver-mode", "set", mode],
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                for mode in ("performance", "balanced")
            ]
            outputs = [process.communicate(timeout=5) for process in processes]
            check(all(process.returncode == 0 for process in processes),
                  f"concurrent writers failed: {outputs}")
            by_connection = {}
            for connection, _magic, _version, message_type, _payload in server.requests:
                by_connection.setdefault(connection, []).append(message_type)
            check(all(messages == [
                DRIVER_MODE_SET, DRIVER_MODE_GET,
                VINTAGE_COMPATIBLE_GET
            ]
                      for messages in by_connection.values()),
                  f"mutation transactions interleaved or tore: {by_connection}")
            check(server.state["requested"] == server.state["effective"] and
                  not server.state["pending"], "concurrent final state is torn")
        socket_path.unlink(missing_ok=True)

        check(lock_path.exists(), "shared mutation lock was not reused")
        check((lock_path.stat().st_mode & 0o777) == 0o600, "mutation lock mode changed")

    hal = (repo / "src/hal/OpenA8DJUSB.m").read_text()
    check("dispatch_block_create_with_qos_class" in hal and
          "QOS_CLASS_USER_INTERACTIVE" in hal,
          "performance QoS is not on the dispatched worker block")
    check(hal.count("gTimecodeState.counters.activations++") == 1 and
          hal.count("TimecodeMarkActivatedLocked();") == 3,
          "timecode activation can be counted more than once per transition")
    check("_streamDriverModePolicy.outputStartLatencyFrames" in hal and
          "_streamDriverModePolicy.outputRestartLatencyFrames" in hal and
          "_streamDriverModePolicy.outputTargetLatencyFrames" in hal,
          "hot output path does not use the per-stream policy snapshot")
    check("kIPCTypeDriverModeGet = 12" in hal and
          "kIPCTypeDriverModeSet = 13" in hal and
          "kIPCTypeDriverModeState = 14" in hal,
          "private IPC IDs were not appended")
    timecode_get_handler = hal[
        hal.index("case kIPCTypeTimecodeOptimizedGet:"):
        hal.index("case kIPCTypeTimecodeOptimizedArm:")
    ]
    timecode_arm_method = hal[
        hal.index("- (void)armTimecodeForClient:"):
        hal.index("- (void)disarmTimecodeForClient:")
    ]
    timecode_disarm_handler = hal[
        hal.index("case kIPCTypeTimecodeOptimizedDisarm:"):
        hal.index("default:", hal.index(
            "case kIPCTypeTimecodeOptimizedDisarm:"
        ))
    ]
    check("sendTimecodeRejectionToClient" in timecode_get_handler and
          "kOpenA8DJTimecodeRejectionBadLength" in
          timecode_get_handler and
          "sendTimecodeRejectionToClient" in
          timecode_disarm_handler and
          "kOpenA8DJTimecodeRejectionBadLength" in
          timecode_disarm_handler,
          "malformed timecode get/disarm can hang without a reply")
    check("OpenA8DJTimecodeValidateArmPayloadDetailed" in
          timecode_arm_method and
          "sendTimecodeRejectionToClient" in timecode_arm_method,
          "malformed private arm is not rejected observably")
    check("OpenA8DJDriverModeValidateSetPayload" in hal,
          "HAL does not validate driver-mode set payloads")
    check("realloc(" not in hal and
          "RingInit(&_inputRing, kRingFrames, kChannels)" in hal and
          "OutputTimelineInit(&_outputTimeline, kRingFrames, kChannels)" in hal,
          "driver mode introduced dynamic ring capacity")
    first_timecode_feed = hal.index(
        "[self addTimecodePhysicalFrame:_pendingPhysicalInput];"
    )
    first_input_write = hal.index(
        "RingWriteWithDropped(&_inputRing, routedInput, 1,",
        first_timecode_feed,
    )
    read_input_handler = hal[
        hal.rindex("- (uint32_t)readInput:"):
        hal.rindex("- (void)setInputDecodeEnabled:")
    ]
    evaluate_handler = hal[
        hal.index("- (void)evaluateTimecodeWindow:"):
        hal.index("- (void)addTimecodePhysicalFrame:")
    ]
    add_timecode_handler = hal[
        hal.rindex("- (void)addTimecodePhysicalFrame:"):
        hal.rindex("- (void)addInputStatsBatch:")
    ]
    arm_timecode_handler = hal[
        hal.index("- (void)armTimecodeOnWriterQueueWithProfile:"):
        hal.index("- (void)publishTimecodeWindow:")
    ]
    fail_open_handler = hal[
        hal.index("static void TimecodeFailOpenLocked"):
        hal.index("static void TimecodeMarkActivatedLocked")
    ]
    set_mode_handler = hal[
        hal.index(
            "static OpenA8DJDriverModeStatePayload "
            "DriverModeSetRequested"
        ):
        hal.index("static dispatch_queue_attr_t OpenA8DJUSBQueueAttributes")
    ]
    check(first_timecode_feed < first_input_write and
          "RingTrimToLatest(&_inputRing" not in hal and
          "TimecodeFailOpenLocked" in read_input_handler and
          "RingRead(" in read_input_handler and
          "_inputRing, outInterleaved" in read_input_handler,
          "C/D/lead decisions can trim or drop optimized input samples")
    check("readControls" not in evaluate_handler and
          "sendCommand" not in evaluate_handler and
          "loadControlPayload" in evaluate_handler and
          evaluate_handler.count(
              "pthread_mutex_lock(&gDriverModeMutex)") == 1,
          "timecode classifier hot path performs blocking control I/O")
    for forbidden in (
        "pthread_mutex", "calloc", "malloc", "USBTrace",
        "Trace(", "readControls", "sendCommand"
    ):
        check(forbidden not in add_timecode_handler,
              f"per-frame timecode classifier contains {forbidden}")
    check("publishTimecodeWindow" in add_timecode_handler and
          "if (complete)" in add_timecode_handler,
          "classifier does not publish only at complete windows")
    classification_gate = "atomic_load(&gTimecodeClassificationArmed)"
    check("ATOMIC_VAR_INIT(false)" in hal[
              hal.index("gTimecodeClassificationArmed") - 80:
              hal.index("gTimecodeClassificationArmed") + 100
          ] and
          classification_gate in add_timecode_handler and
          add_timecode_handler.index(classification_gate) <
              add_timecode_handler.index(
                  "OpenA8DJTimecodeClassifierFeedFrame") and
          "return;" in add_timecode_handler[
              add_timecode_handler.index(classification_gate):
              add_timecode_handler.index(
                  "OpenA8DJTimecodeClassifierFeedFrame")
          ],
          "disarmed timecode classification does not exit before feeding")
    check("bool accepted = OpenA8DJTimecodeArm(" in
              arm_timecode_handler and
          "accepted && gTimecodeState.armed" in
              arm_timecode_handler and
          "atomic_store(&gTimecodeClassificationArmed, true)" in
              arm_timecode_handler,
          "accepted arm does not enable timecode classification")
    check("atomic_store(&gTimecodeClassificationArmed, false)" in
              fail_open_handler and
          "atomic_store(&gTimecodeClassificationArmed, false)" in
              set_mode_handler and
          hal.count(
              "atomic_store(&gTimecodeClassificationArmed, false)") == 3,
          "a timecode disarm route leaves classification enabled")
    check("atomic_load(&gTimecodeClassificationArmed) ?" in hal and
          "mach_absolute_time() : 0" in hal,
          "engine reopen loses armed classifier timeout state")
    check("dispatch_queue_set_specific" in hal and
          "dispatch_get_specific" in hal and
          "dispatch_sync(_queue, armBlock)" in hal,
          "arm/reset is not serialized with the classifier writer")
    mode_header = (repo / "src/hal/OpenA8DJDriverMode.h").read_text()
    check("malloc(" not in mode_header and "calloc(" not in mode_header and
          "realloc(" not in mode_header,
          "driver-mode policy/state unexpectedly allocates")
    check(hal.count("pthread_mutex_lock(&gDriverModeMutex)") >= 5 and
          "_streamDriverModePolicy = DriverModeBeginStream();" in hal and
          "DriverModeEndStream();" in hal,
          "one mode mutex does not cover IPC state and stream boundaries")
    check("OpenA8DJDriverModePolicyIsSafe(policy, kRingFrames)" in hal and
          "outputStartLatencyFrames >= outputRingCapacityFrames" in mode_header and
          "outputRestartLatencyFrames > policy->outputStartLatencyFrames" in
          mode_header and
          "outputTargetLatencyFrames < policy->outputRestartLatencyFrames" in
          mode_header and
          "outputTargetLatencyFrames > policy->outputStartLatencyFrames" in
          mode_header,
          "production preflight does not enforce ring and watermark invariants")
    check('_queue = dispatch_queue_create("org.opena8dj.driver.usb",' in hal and
          "OpenA8DJUSBQueueAttributes());" in hal and
          "dispatch_block_create_with_qos_class" in hal,
          "worker QoS is not scoped independently from queue creation")
    control_set_handler = hal[
        hal.index("case kIPCTypeControlSet:"):hal.index("case kIPCTypeInputStatsGet:")
    ]
    mode_set_handler = hal[
        hal.index("case kIPCTypeDriverModeSet:"):hal.index("default:", hal.index(
            "case kIPCTypeDriverModeSet:"
        ))
    ]
    timecode_arm_handler = hal[
        hal.index("case kIPCTypeTimecodeOptimizedArm:"):
        hal.index("case kIPCTypeTimecodeOptimizedDisarm:")
    ]
    check("storeControlPayload" not in mode_set_handler and
          "storeControlPayload" not in timecode_arm_handler and
          "writeControls" not in timecode_arm_handler and
          "TimecodeFailOpenLocked" in control_set_handler,
          "hardware profile/control and driver mode are not independent axes")
    control_source = source.read_text()
    check("OPENA8DJ_ERROR_DRIVER_MODE_BUSY" in control_source and
          '"driver_mode_busy"' in control_source,
          "reserved stable driver_mode_busy public error is missing")
    print(state_result.stdout.strip())
    print("driver mode offline contract tests: PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--shipping-binary", type=Path, required=True)
    args = parser.parse_args()
    run_tests(args.repo.resolve(), args.shipping_binary.resolve())


if __name__ == "__main__":
    main()
