#!/usr/bin/env python3
"""Offline Vintage Compatible HAL/IPC/public API contract tests."""

import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading

from public_api_contract_test import stream_payload


MAGIC = 0x4A443841
HEADER = struct.Struct("=IBBH")
CONTROL_STATE = 6
STREAM_GET, STREAM_STATE = 10, 11
DRIVER_GET, DRIVER_SET, DRIVER_STATE = 12, 13, 14
VINTAGE_GET, VINTAGE_STATE = 19, 20


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def compile_binary(repo, output, *sources, extra=()):
    subprocess.run([
        "xcrun", "clang", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-I", str(repo / "src/hal"), *extra,
        "-o", str(output), *(str(repo / source) for source in sources),
    ], check=True)


def compile_control(repo, output, sock, lock):
    compile_binary(
        repo, output, "src/tools/opena8dj-control.c",
        extra=(
            f'-DOPENA8DJ_PUBLIC_API_SOCKET_PATH="{sock}"',
            f'-DOPENA8DJ_PUBLIC_API_LOCK_PATH="{lock}"',
            "-DOPENA8DJ_PUBLIC_API_TEST_TRUST_CURRENT_UID=1",
            "-framework", "CoreAudio", "-framework", "CoreFoundation",
        ),
    )


def fixture(binary, name):
    return subprocess.check_output([str(binary), name])


class VintageServer:
    def __init__(self, path, drivers, vintages, initial="balanced",
                 malformed=None, stats=None):
        self.path = path
        self.drivers = drivers
        self.vintages = vintages
        self.kind = initial
        self.malformed = malformed
        self.stats = stats
        self.requests = []
        if path.exists():
            path.unlink()
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(str(path))
        os.chmod(path, 0o666)
        self.listener.listen(1)
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    @staticmethod
    def read_full(conn, length):
        data = b""
        while len(data) < length:
            chunk = conn.recv(length - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    @staticmethod
    def send(conn, kind, payload):
        conn.sendall(HEADER.pack(MAGIC, 1, kind, len(payload)) + payload)

    def run(self):
        conn, _ = self.listener.accept()
        with conn:
            self.send(conn, CONTROL_STATE, bytes(13))
            while True:
                raw = self.read_full(conn, HEADER.size)
                if raw is None:
                    return
                magic, version, kind, length = HEADER.unpack(raw)
                payload = self.read_full(conn, length)
                if payload is None:
                    return
                self.requests.append((magic, version, kind, payload))
                if kind == DRIVER_GET:
                    self.send(conn, DRIVER_STATE, self.drivers[self.kind])
                elif kind == DRIVER_SET:
                    mode = struct.unpack_from("=I", payload, 4)[0]
                    if self.kind == "conflict":
                        self.send(conn, DRIVER_STATE,
                                  self.drivers["conflict"])
                    elif mode == 4:
                        self.kind = "effective"
                        self.send(conn, DRIVER_STATE,
                                  self.drivers["effective"])
                    elif mode == 1:
                        self.kind = "balanced"
                        self.send(conn, DRIVER_STATE,
                                  self.drivers["balanced"])
                elif kind == VINTAGE_GET:
                    vintage_kind = self.malformed or self.kind
                    reply = self.vintages[vintage_kind]
                    if self.malformed == "short":
                        reply = self.vintages["balanced"][:-1]
                    self.send(conn, VINTAGE_STATE, reply)
                elif kind == STREAM_GET and self.stats is not None:
                    self.send(conn, STREAM_STATE, self.stats)

    def close(self):
        self.listener.close()
        self.thread.join(timeout=1)
        if self.path.exists():
            self.path.unlink()


def invoke(binary, *args):
    result = subprocess.run(
        [str(binary), *args],
        text=True, capture_output=True, check=False, timeout=6,
    )
    return result, json.loads(result.stdout)


def check_source_contract(repo):
    driver = (repo / "src/hal/OpenA8DJDriverMode.h").read_text()
    vintage = (repo / "src/hal/OpenA8DJVintageCompatible.h").read_text()
    hal = (repo / "src/hal/OpenA8DJHAL.c").read_text()
    usb = (repo / "src/hal/OpenA8DJUSB.m").read_text()
    cli = (repo / "src/tools/opena8dj-control.c").read_text()

    check("kOpenA8DJDriverModeVintageCompatible = 4" in driver,
          "Vintage mode ID must append after Timecode")
    check("kIPCTypeVintageCompatibleGet = 19" in usb and
          "kIPCTypeVintageCompatibleState = 20" in usb,
          "Vintage IPC IDs must append after Timecode")
    check("Vintage Compatible (Experimental — Unverified)" in cli,
          "exact Vintage label missing")
    check("88200.0" in hal and "96000.0" in hal,
          "shipping rate surface lost 88.2/96")
    supported = hal[hal.index("static bool IsSupportedRate"):
                    hal.index("static UInt32 RecommendedBufferFramesForRate")]
    check("192000" not in supported, "192 kHz must not be enabled")
    check("OpenA8DJUSBNormalizeCoreAudioBufferFrames" in hal,
          "HAL buffer normalization is not mode-aware")
    perform_start = hal.rindex(
        "static OSStatus STDMETHODCALLTYPE "
        "OpenA8DJ_PerformDeviceConfigurationChange")
    perform_end = hal.rindex(
        "static OSStatus STDMETHODCALLTYPE "
        "OpenA8DJ_AbortDeviceConfigurationChange")
    perform = hal[perform_start:perform_end]
    check("normalizedSize = NormalizeBufferFrames(newSize)" in perform,
          "pending buffer commit is not revalidated")
    check("RequestDeviceConfigurationChange" in hal and
          "gPendingBufferFrames" in hal and "gPendingSampleRate" in hal,
          "Core Audio configuration transaction missing")
    check("gBufferFrames" not in usb,
          "private USB/IPC code must not mutate HAL gBufferFrames")
    check("OPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM" in usb and
          "OPENA8DJ_PLAYBACK_CAPTURE_PACED" in usb and
          "OPENA8DJ_OUTPUT_START_BYTE" in usb,
          "compiled USB invariants missing from descriptor")
    check("OPENA8DJ_VINTAGE_KNOWN_REASON_MASK" in vintage and
          "OpenA8DJVintageValidateStatePayload" in vintage,
          "strict reason/schema validation missing")


def make_stats(repo, timecode, vintage, driver_kind):
    payload, _, _, _, offsets = stream_payload(
        repo / "src/tools/opena8dj-control.c")
    data = bytearray(payload)
    fields = {
        "driverModeSchemaVersion": 1,
        "driverModeRequested": 4 if driver_kind == "effective" else 1,
        "driverModeEffective": 4 if driver_kind == "effective" else 1,
        "driverModePending": 0,
        "driverModeLastResult": 1 if driver_kind == "effective" else 0,
        "driverModeRejectionReason": 0,
        "driverModeGeneration": 1 if driver_kind == "effective" else 0,
        "driverModeAcceptedRequests": 1 if driver_kind == "effective" else 0,
        "driverModeRejectedRequests": 0,
        "driverModeAppliedTransitions": 1 if driver_kind == "effective" else 0,
        "driverModeApplyFailures": 0,
        "driverModePendingTransitions": 0,
        "driverModeOutputStartLatencyFrames": 8192,
        "driverModeOutputRestartLatencyFrames": 4096,
        "driverModeOutputTargetLatencyFrames": 8192,
        "driverModeWorkerQoS": 0,
    }
    for name, value in fields.items():
        offset, fmt = offsets[name]
        struct.pack_into("=" + fmt, data, offset, value)
    return bytes(data) + timecode + vintage


def run(repo):
    check_source_contract(repo)
    with tempfile.TemporaryDirectory(prefix="a8-vintage-", dir="/tmp") as tmp:
        root = Path(tmp)
        control = root / "control"
        fixture_bin = root / "fixture"
        timecode_fixture = root / "timecode-fixture"
        sock = root / "control.sock"
        lock = root / "mutation.lock"
        compile_control(repo, control, sock, lock)
        compile_binary(repo, fixture_bin, "tests/vintage_state_fixture.c")
        compile_binary(repo, timecode_fixture,
                       "tests/timecode_state_fixture.c", extra=("-lm",))
        drivers = {
            kind: fixture(fixture_bin, f"driver-{kind}")
            for kind in ("balanced", "effective", "pending", "conflict")
        }
        vintages = {
            kind: fixture(fixture_bin, f"vintage-{kind}")
            for kind in (
                "balanced", "effective", "pending", "conflict",
                "effective-streaming",
                "unknown-reason", "capability-mismatch",
                "enum-mismatch", "generation-mismatch",
                "requested-mismatch",
            )
        }
        vintages["short"] = vintages["balanced"][:-1]

        result, doc = invoke(control, "api", "driver-modes")
        check(result.returncode == 0, "catalog failed")
        modes = doc["data"]["driverModes"]
        check([mode["id"] for mode in modes] == [
            "balanced", "performance", "timecode-optimized",
            "vintage-compatible",
        ], "catalog order mismatch")
        check(modes[-1]["name"] ==
              "Vintage Compatible (Experimental — Unverified)",
              "catalog label mismatch")
        check(modes[-1]["experimental"] is True and
              modes[-1]["conformanceVerified"] is False,
              "catalog claim boundary mismatch")

        result, doc = invoke(control, "api", "version")
        check(result.returncode == 0, "version failed")
        capabilities = doc["data"]["capabilities"]
        check("driver-mode.vintage-compatible.read" in capabilities and
              "driver-mode.vintage-compatible.write" in capabilities,
              "version capabilities missing")

        server = VintageServer(sock, drivers, vintages)
        result, doc = invoke(control, "api", "driver-mode")
        server.close()
        check(result.returncode == 0, "balanced Vintage get failed")
        vintage = doc["data"]["vintageCompatible"]
        check(vintage["status"] == "unverified", "default status")
        check("not_requested" in vintage["reasons"], "default reason")
        check(doc["data"]["requestedMode"] == "balanced" and
              doc["data"]["effectiveMode"] == "balanced" and
              doc["data"]["pending"] is False, "session default")

        server = VintageServer(sock, drivers, vintages)
        result, doc = invoke(
            control, "api", "driver-mode", "set",
            "vintage-compatible")
        server.close()
        check(result.returncode == 0, "Vintage set failed")
        check(doc["data"]["requestedMode"] == "vintage-compatible" and
              doc["data"]["effectiveMode"] == "vintage-compatible",
              "Vintage set state mismatch")
        vintage = doc["data"]["vintageCompatible"]
        check(vintage["status"] == "partial", "shipping status exceeds/loses partial")
        check(vintage["claim"] == "unverified", "shipping claim mismatch")
        check(vintage["capabilities"]["rate88200"] is True and
              vintage["capabilities"]["rate192000"] is False,
              "rate capability mismatch")
        check(vintage["preflight"]["bufferNormalization"] == "fixed" and
              vintage["preflight"]["normalizedBufferFrames"] == 512,
              "fixed 512 state missing")

        server = VintageServer(sock, drivers, vintages, initial="conflict")
        result, doc = invoke(
            control, "api", "driver-mode", "set",
            "vintage-compatible")
        server.close()
        check(result.returncode == 3, "conflict exit mismatch")
        check(doc["error"]["code"] == "driver_mode_conflict",
              "conflict code mismatch")

        for malformed in (
                "unknown-reason", "capability-mismatch",
                "enum-mismatch", "generation-mismatch",
                "requested-mismatch", "short"):
            server = VintageServer(
                sock, drivers, vintages,
                initial="effective", malformed=malformed)
            result, doc = invoke(control, "api", "driver-mode")
            server.close()
            check(result.returncode == 4,
                  f"{malformed} protocol exit mismatch")
            check(doc["error"]["code"] == "backend_protocol_error",
                  f"{malformed} protocol code mismatch")

        timecode = fixture(timecode_fixture, "disarmed")
        full_stats = make_stats(
            repo, timecode, vintages["effective-streaming"], "effective")
        server = VintageServer(
            sock, drivers, vintages, initial="effective",
            stats=full_stats)
        result, doc = invoke(control, "api", "stats")
        server.close()
        check(result.returncode == 0,
              f"Vintage stats failed: {result.stdout} {result.stderr}")
        check(doc["data"]["vintageCompatible"]["status"] == "partial",
              "stats Vintage tail missing")

        legacy_stats = stream_payload(
            repo / "src/tools/opena8dj-control.c")[0]
        server = VintageServer(
            sock, drivers, vintages, stats=legacy_stats)
        result, doc = invoke(control, "api", "stats")
        server.close()
        check(result.returncode == 0, "legacy stats failed")
        check(doc["data"]["vintageCompatible"] is None,
              "legacy stats must yield null")

        packing = subprocess.run([
            "python3", str(repo / "scripts/validate-mode2-output-packing.py"),
            "--start-byte", "4", "--frames", "64",
        ], text=True, capture_output=True, check=False)
        check(packing.returncode == 0, "mode-2 start-byte 4 packing failed")

    print("vintage compatible offline API/HAL contract: PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path("."))
    args = parser.parse_args()
    run(args.repo.resolve())


if __name__ == "__main__":
    main()
