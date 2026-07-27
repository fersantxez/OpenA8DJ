#!/usr/bin/env python3
"""Offline contract tests for opena8dj-control public API v1."""

import argparse
import json
import os
import re
import socket
import stat
import struct
import subprocess
import tempfile
import threading
from pathlib import Path


MAGIC = 0x4A443841
VERSION = 1
CONTROL_GET = 4
CONTROL_SET = 5
CONTROL_STATE = 6
INPUT_STATS_GET = 8
STREAM_STATS_GET = 10
STREAM_STATS = 11
HEADER = struct.Struct("=IBBH")
SCHEMA = "org.opena8dj.public-api.response.v1"
CANONICAL_PROFILES = [
    "playback-4out",
    "traktor-dvs-vinyl",
    "traktor-dvs-cd-line",
    "vinyl-recording",
    "dj-set-recording",
    "effects-loop",
    "microphone",
    "midi-only",
    "ground-diagnostics",
    "engineering-diagnostics",
]


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def parse_one_json(stdout):
    check(stdout.endswith("\n"), "JSON output is not newline terminated")
    decoder = json.JSONDecoder()
    value, end = decoder.raw_decode(stdout)
    check(not stdout[end:].strip(), "stdout contains more than one JSON document")
    return value


def invoke(binary, *args):
    result = subprocess.run(
        [str(binary), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=4,
        check=False,
    )
    return result, parse_one_json(result.stdout)


def validate_envelope(document, operation, ok):
    check(document["schema"] == SCHEMA, "wrong schema")
    check(document["apiVersion"] == "1.0", "wrong API version")
    check(document["ok"] is ok, "wrong ok value")
    check(document["operation"] == operation, "wrong operation")
    check(isinstance(document["data" if ok else "error"], dict), "wrong envelope payload type")


def stream_payload(source_path):
    source = source_path.read_text()
    match = re.search(
        r"typedef struct OpenA8DJStreamStatsPayload \{(.*?)\}"
        r" __attribute__\(\(packed\)\) OpenA8DJStreamStatsPayload;",
        source,
        re.S,
    )
    check(match is not None, "could not find private stream payload")
    formats = {"uint8_t": "B", "uint32_t": "I", "uint64_t": "Q", "double": "d"}
    fields = []
    for type_name, field_name in re.findall(
        r"^\s*(uint8_t|uint32_t|uint64_t|double)\s+([A-Za-z0-9_]+);",
        match.group(1),
        re.M,
    ):
        fields.append((field_name, formats[type_name]))
    offsets = {}
    offset = 0
    for name, fmt in fields:
        offsets[name] = (offset, fmt)
        offset += struct.calcsize("=" + fmt)
    payload = bytearray(offset)
    values = {
        "streaming": 1,
        "clockAnchorValid": 1,
        "outputRingFrames": 256,
        "outputTargetLatencyFrames": 64,
        "sampleRate": 48000.0,
        "clockUSBFrameResyncs": 4,
        "clockAcceptedAnchors": 7,
        "clockRejectedAnchors": 2,
        "clockAnchorResets": 3,
        "captureTransfers": 11,
        "captureTransactions": 12,
        "captureBytes": 13,
        "captureTransactionFailures": 14,
        "captureShortTransfers": 15,
        "captureQueueFailures": 16,
        "playbackTransfers": 21,
        "playbackTransactions": 22,
        "playbackBytes": 23,
        "playbackTransactionFailures": 24,
        "playbackShortTransfers": 25,
        "playbackQueueFailures": 26,
        "outputFramesWritten": 31,
        "outputFramesRead": 32,
        "outputUnderruns": 33,
        "outputActiveUnderruns": 34,
        "outputRingOverruns": 35,
        "outputTimelineResets": 36,
        "outputLateWriteFrames": 37,
        "outputLateWriteBatches": 38,
        "inputCheckErrors": 41,
        "outputPanicFlags": 42,
    }
    for name, value in values.items():
        field_offset, fmt = offsets[name]
        struct.pack_into("=" + fmt, payload, field_offset, value)
    sample_rate_offset, sample_rate_format = offsets["sampleRate"]
    base_length = sample_rate_offset + struct.calcsize("=" + sample_rate_format)
    return bytes(payload), values, base_length


class MockIPC:
    def __init__(
        self,
        path,
        state,
        stats=b"",
        mismatch=False,
        malformed=False,
        mode=0o666,
        replace_path_on_accept=False,
    ):
        self.path = str(path)
        self.state = bytearray(state)
        self.stats = stats
        self.mismatch = mismatch
        self.malformed = malformed
        self.mode = mode
        self.replace_path_on_accept = replace_path_on_accept
        self.requests = []
        self.error = None
        self.replacement = None
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(self.path)
        os.chmod(self.path, mode)
        self.original_inode = os.lstat(self.path).st_ino
        self.replacement_inode = None
        self.listener.listen(1)
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, traceback):
        self.listener.close()
        if self.replacement is not None:
            self.replacement.close()
        self.thread.join(timeout=1)
        if self.error is not None and exc_type is None:
            raise self.error

    @staticmethod
    def _read_full(connection, count):
        chunks = bytearray()
        while len(chunks) < count:
            chunk = connection.recv(count - len(chunks))
            if not chunk:
                return None
            chunks.extend(chunk)
        return bytes(chunks)

    @staticmethod
    def _send(connection, message_type, payload, magic=MAGIC):
        connection.sendall(HEADER.pack(magic, VERSION, message_type, len(payload)) + payload)

    def _serve(self):
        try:
            connection, _ = self.listener.accept()
            with connection:
                if self.replace_path_on_accept:
                    os.unlink(self.path)
                    self.replacement = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    self.replacement.bind(self.path)
                    os.chmod(self.path, self.mode)
                    self.replacement_inode = os.lstat(self.path).st_ino
                    self.replacement.listen(1)
                while True:
                    raw_header = self._read_full(connection, HEADER.size)
                    if raw_header is None:
                        break
                    magic, version, message_type, length = HEADER.unpack(raw_header)
                    payload = self._read_full(connection, length)
                    if payload is None:
                        break
                    self.requests.append((magic, version, message_type, payload))
                    if self.malformed:
                        self._send(connection, CONTROL_STATE, b"", magic=0)
                        continue
                    if message_type == CONTROL_GET:
                        self._send(connection, CONTROL_STATE, bytes(self.state))
                    elif message_type == CONTROL_SET and not self.mismatch:
                        self.state[:] = payload
                    elif message_type == STREAM_STATS_GET:
                        self._send(connection, STREAM_STATS, self.stats)
        except (BrokenPipeError, ConnectionResetError, OSError) as error:
            if self.listener.fileno() != -1:
                self.error = error


def compile_harness(source, output, socket_path, lock_path):
    command = [
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
        "-DOPENA8DJ_PUBLIC_API_TEST_POST_CONNECT_DELAY_USEC=150000",
        "-DOPENA8DJ_PUBLIC_API_TEST_ESCAPE=1",
        "-framework",
        "CoreAudio",
        "-framework",
        "CoreFoundation",
        "-o",
        str(output),
        str(source),
    ]
    subprocess.run(command, check=True, timeout=30)


def compile_and_run_peer_policy(repo, output):
    command = [
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
        str(output),
        str(repo / "tests/public_api_peer_policy_test.c"),
    ]
    subprocess.run(command, check=True, timeout=30)
    result = subprocess.run(
        [str(output)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=4,
        check=False,
    )
    check(result.returncode == 0, f"peer policy executable failed: {result.stderr}")
    check("PASS" in result.stdout, "peer policy executable did not report success")
    print(result.stdout.strip())


def assert_error(binary, expected_exit, expected_operation, expected_code, *args):
    result, document = invoke(binary, *args)
    check(result.returncode == expected_exit, f"{expected_code}: wrong exit {result.returncode}")
    validate_envelope(document, expected_operation, False)
    check(document["error"]["code"] == expected_code, f"wrong error code: {document}")
    check(isinstance(document["error"]["message"], str), "error message is not a string")
    check(isinstance(document["error"]["retryable"], bool), "retryable is not a boolean")


def run_tests(repo, shipping_binary):
    source = repo / "src/tools/opena8dj-control.c"
    hal_source = repo / "src/hal/OpenA8DJUSB.m"
    auth_header = repo / "src/hal/OpenA8DJIPCAuth.h"

    result, document = invoke(shipping_binary, "api", "version")
    check(result.returncode == 0, "shipping version failed")
    validate_envelope(document, "version.get", True)
    check(document["data"]["capabilities"] == [
        "stats.read", "profiles.list", "profile.read", "profile.write"
    ], "wrong capabilities")

    result, document = invoke(shipping_binary, "api", "profiles")
    check(result.returncode == 0, "shipping profiles failed")
    validate_envelope(document, "profiles.list", True)
    profiles = document["data"]["profiles"]
    check([item["id"] for item in profiles] == CANONICAL_PROFILES, "wrong profile catalog")
    for item in profiles:
        check(set(item) == {"id", "title", "surface", "summary"}, "wrong catalog member set")
        check(all(isinstance(value, str) for value in item.values()), "catalog value is not text")

    with tempfile.TemporaryDirectory(prefix="a8api-", dir="/tmp") as temporary:
        temporary_path = Path(temporary)
        socket_path = temporary_path / "control.sock"
        lock_path = temporary_path / "mutation.lock"
        harness = temporary_path / "opena8dj-control-test"
        peer_policy_harness = temporary_path / "peer-policy-test"
        compile_harness(source, harness, socket_path, lock_path)
        compile_and_run_peer_policy(repo, peer_policy_harness)

        escaped = 'quote" slash\\ newline\n tab\t control\x01'
        escaped_result, escaped_document = invoke(
            harness, "--public-api-test-escape", escaped
        )
        check(escaped_result.returncode == 0 and escaped_document == escaped, "JSON escaping failed")

        assert_error(harness, 2, "unknown", "invalid_request", "api", "unknown")
        assert_error(harness, 2, "version.get", "invalid_request", "api", "version", "extra")
        for rejected in ["playback", "dvs-vinyl", "../playback-4out", "x" * 65]:
            assert_error(
                harness, 2, "profile.set", "profile_not_allowed",
                "api", "profile", "set", rejected
            )
        assert_error(harness, 3, "profile.get", "backend_unavailable", "api", "profile")

        initial_state = bytes([0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 2, 3, 1])
        with MockIPC(socket_path, initial_state, mode=0o777):
            assert_error(
                harness, 4, "profile.get", "backend_permission_denied", "api", "profile"
            )
        socket_path.unlink(missing_ok=True)

        socket_path.write_text("not a socket")
        os.chmod(socket_path, 0o600)
        assert_error(
            harness, 4, "profile.get", "backend_permission_denied", "api", "profile"
        )
        socket_path.unlink()
        socket_path.symlink_to(temporary_path / "missing.sock")
        assert_error(
            harness, 4, "profile.get", "backend_permission_denied", "api", "profile"
        )
        socket_path.unlink()

        with MockIPC(socket_path, initial_state, replace_path_on_accept=True) as server:
            assert_error(
                harness, 4, "profile.get", "backend_permission_denied", "api", "profile"
            )
            check(server.replacement_inode is not None, "mock did not replace the socket path")
            check(server.replacement_inode != server.original_inode,
                  "mock socket replacement reused the original inode")
        print("public API socket inode replacement rejection: PASS")
        socket_path.unlink(missing_ok=True)

        with MockIPC(socket_path, initial_state, malformed=True):
            assert_error(
                harness, 4, "profile.get", "backend_protocol_error", "api", "profile"
            )
        socket_path.unlink(missing_ok=True)

        with MockIPC(socket_path, initial_state) as server:
            result, document = invoke(harness, "api", "profile")
            check(result.returncode == 0, "profile query failed")
            validate_envelope(document, "profile.get", True)
            state = document["data"]
            check(set(state) == {
                "activeProfile", "inputMode", "inputModeValue", "inputDecode",
                "softwareLock", "groundLiftVinyl", "groundLiftCDLine",
                "groundLiftPhono", "inputSources", "inputTransforms",
            }, "wrong profile state member set")
            check(state["activeProfile"] == "traktor-dvs-vinyl", "wrong inferred profile")
            check(state["inputMode"] == "timecode-vinyl", "wrong input mode")
            check(isinstance(state["inputModeValue"], int), "input mode value is not integer")
            for boolean_name in [
                "inputDecode", "softwareLock", "groundLiftVinyl",
                "groundLiftCDLine", "groundLiftPhono",
            ]:
                check(isinstance(state[boolean_name], bool), f"{boolean_name} is not boolean")
            check(state["inputSources"] == {"A": "A", "B": "B", "C": "C", "D": "D"},
                  "wrong input sources")
            check(set(state["inputTransforms"]) == {"A", "B", "C", "D"},
                  "wrong transform members")
            check(all(request[2] != INPUT_STATS_GET for request in server.requests),
                  "public profile read used destructive input statistics")
        socket_path.unlink(missing_ok=True)

        stats_payload, expected_stats, stats_base_length = stream_payload(source)
        with MockIPC(socket_path, initial_state, stats=stats_payload) as server:
            result, document = invoke(harness, "api", "stats")
            check(result.returncode == 0, "stats query failed")
            validate_envelope(document, "stats.get", True)
            data = document["data"]
            check(set(data) == {"stream", "clock", "capture", "playback", "output", "health"},
                  "wrong stats group set")
            check(set(data["stream"]) == {
                "streaming", "sampleRate", "outputRingFrames", "outputTargetLatencyFrames"
            }, "wrong stream field set")
            check(set(data["clock"]) == {
                "anchorValid", "acceptedAnchors", "rejectedAnchors",
                "anchorResets", "usbFrameResyncs"
            }, "wrong clock field set")
            check(set(data["capture"]) == {
                "transfers", "transactions", "bytes", "transactionFailures",
                "shortTransfers", "queueFailures"
            }, "wrong capture field set")
            check(set(data["playback"]) == set(data["capture"]), "wrong playback field set")
            check(set(data["output"]) == {
                "framesWritten", "framesRead", "underruns", "activeUnderruns",
                "ringOverruns", "timelineResets", "lateWriteFrames", "lateWriteBatches"
            }, "wrong output field set")
            check(set(data["health"]) == {"inputCheckErrors", "outputPanicFlags"},
                  "wrong health field set")
            check(isinstance(data["stream"]["streaming"], bool), "streaming is not boolean")
            check(isinstance(data["stream"]["sampleRate"], (int, float)), "rate is not number")
            check(isinstance(data["clock"]["anchorValid"], bool), "anchorValid is not boolean")
            for group_name, group in data.items():
                for field_name, value in group.items():
                    if field_name not in {"streaming", "sampleRate", "anchorValid"}:
                        check(isinstance(value, int) and not isinstance(value, bool),
                              f"{group_name}.{field_name} is not integer")
            check(data["stream"] == {
                "streaming": True,
                "sampleRate": 48000,
                "outputRingFrames": 256,
                "outputTargetLatencyFrames": 64,
            }, "wrong stream stats")
            check(data["clock"]["acceptedAnchors"] == expected_stats["clockAcceptedAnchors"],
                  "wrong clock stats")
            check(data["capture"]["queueFailures"] == expected_stats["captureQueueFailures"],
                  "wrong capture stats")
            check(data["playback"]["bytes"] == expected_stats["playbackBytes"],
                  "wrong playback stats")
            check(data["output"]["lateWriteBatches"] == expected_stats["outputLateWriteBatches"],
                  "wrong output stats")
            check(data["health"]["outputPanicFlags"] == expected_stats["outputPanicFlags"],
                  "wrong health stats")
            check([request[2] for request in server.requests] == [STREAM_STATS_GET],
                  "stats request was not a single non-destructive stream snapshot")
        socket_path.unlink(missing_ok=True)

        with MockIPC(socket_path, initial_state, stats=b""):
            assert_error(
                harness, 4, "stats.get", "backend_protocol_error", "api", "stats"
            )
        socket_path.unlink(missing_ok=True)

        with MockIPC(
            socket_path, initial_state, stats=stats_payload[:stats_base_length - 1]
        ):
            assert_error(
                harness, 4, "stats.get", "backend_protocol_error", "api", "stats"
            )
        socket_path.unlink(missing_ok=True)

        with MockIPC(socket_path, initial_state, stats=stats_payload[:stats_base_length]):
            result, document = invoke(harness, "api", "stats")
            check(result.returncode == 0, "append-compatible base stats failed")
            check(document["data"]["stream"]["sampleRate"] == 48000,
                  "base sample rate was not preserved")
            check(document["data"]["capture"]["bytes"] == 0,
                  "missing trailing counter was not zero")
        socket_path.unlink(missing_ok=True)

        for profile in CANONICAL_PROFILES:
            with MockIPC(socket_path, initial_state) as server:
                result, document = invoke(harness, "api", "profile", "set", profile)
                check(result.returncode == 0, f"canonical profile rejected: {profile}")
                validate_envelope(document, "profile.set", True)
                check(document["data"]["requestedProfile"] == profile, "wrong requested profile")
                check(document["data"]["applied"] is True, "profile was not marked applied")
                check([request[2] for request in server.requests] == [
                    CONTROL_GET, CONTROL_SET, CONTROL_GET
                ], "set did not use read/modify/write/read-back")
            socket_path.unlink(missing_ok=True)

        check(lock_path.exists(), "mutation lock was not created")
        check(stat.S_IMODE(lock_path.stat().st_mode) == 0o600, "mutation lock is not 0600")
        check(lock_path.stat().st_uid == os.geteuid(), "mutation lock has wrong owner")
        os.chmod(lock_path, 0o644)
        assert_error(
            harness, 5, "profile.set", "profile_apply_failed",
            "api", "profile", "set", "playback-4out"
        )
        os.chmod(lock_path, 0o600)

        with MockIPC(socket_path, initial_state, mismatch=True):
            assert_error(
                harness, 5, "profile.set", "profile_apply_failed",
                "api", "profile", "set", "playback-4out"
            )

    hal_text = hal_source.read_text()
    check(re.search(r"chmod\(kIPCSocketPath,\s*0666\)", hal_text) is not None,
          "HAL socket policy is not cross-UID connectable")
    check("getpeereid(fd, &peerUID, &peerGID)" in hal_text,
          "HAL does not obtain accepted peer credentials")
    check('stat("/dev/console", &consoleState)' in hal_text and
          "OpenA8DJIPCPeerUIDIsAuthorized(peerUID, geteuid(), consoleUID)" in hal_text,
          "HAL peer policy is missing current console UID")
    auth_text = auth_header.read_text()
    check("peerUID == 0" in auth_text and "peerUID == hostUID" in auth_text and
          "peerUID == consoleUID" in auth_text,
          "factored peer policy is missing an allowed UID class")
    accept_policy = hal_text[hal_text.index("int client = accept"):]
    authorize_index = accept_policy.index("IPCPeerIsAuthorized(client)")
    add_index = accept_policy.index("[strongSelf addIPCClient:client]")
    send_index = accept_policy.index("[strongSelf sendControlStateToClient:client]")
    dispatch_index = accept_policy.index("dispatch_async", send_index)
    check(authorize_index < add_index < send_index < dispatch_index,
          "HAL peer authorization does not precede registration and dispatch")
    check("getpeereid(fd, &peerUID, &peerGID)" in source.read_text(),
          "public client does not authenticate server credentials")
    check('getpwnam("_coreaudiod")' in source.read_text(),
          "public client does not resolve the Core Audio host account")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--shipping-binary", type=Path, required=True)
    args = parser.parse_args()
    run_tests(args.repo.resolve(), args.shipping_binary.resolve())
    print("public API v1 offline contract tests: PASS")


if __name__ == "__main__":
    main()
