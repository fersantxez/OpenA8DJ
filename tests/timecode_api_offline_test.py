#!/usr/bin/env python3
"""Offline fixed-width Timecode Optimized IPC/public API tests."""

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
TC_GET, TC_ARM, TC_DISARM, TC_STATE = 15, 16, 17, 18
STREAM_GET, STREAM_STATE = 10, 11
DRIVER_GET, DRIVER_STATE = 12, 14
DRIVER_STATE_SIZE = 88


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def compile_fixture(repo, output):
    subprocess.run([
        "xcrun", "clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
        "-Werror", "-I", str(repo / "src/hal"), "-o", str(output),
        str(repo / "tests/timecode_state_fixture.c"),
    ], check=True)


def compile_control(repo, output, sock, lock):
    subprocess.run([
        "xcrun", "clang", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        f'-DOPENA8DJ_PUBLIC_API_SOCKET_PATH="{sock}"',
        f'-DOPENA8DJ_PUBLIC_API_LOCK_PATH="{lock}"',
        "-DOPENA8DJ_PUBLIC_API_TEST_TRUST_CURRENT_UID=1",
        "-framework", "CoreAudio", "-framework", "CoreFoundation",
        "-o", str(output), str(repo / "src/tools/opena8dj-control.c"),
    ], check=True)


class Server:
    def __init__(self, path, states, arm_kind="qualifying", mismatch=False,
                 stats=None):
        self.path = path
        self.states = states
        self.kind = "disarmed"
        self.arm_kind = arm_kind
        self.mismatch = mismatch
        self.stats = stats
        self.requests = []
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(str(path))
        os.chmod(path, 0o666)
        self.listener.listen(2)
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
                header = self.read_full(conn, HEADER.size)
                if header is None:
                    break
                magic, version, kind, length = HEADER.unpack(header)
                payload = self.read_full(conn, length)
                if payload is None:
                    break
                self.requests.append((magic, version, kind, payload))
                if kind == TC_ARM:
                    self.kind = self.arm_kind
                    self.send(conn, TC_STATE, self.states[self.kind])
                elif kind == TC_DISARM:
                    self.kind = "disarmed"
                    self.send(conn, TC_STATE, self.states[self.kind])
                elif kind == TC_GET:
                    reply = self.states[self.kind]
                    if self.mismatch:
                        altered = bytearray(reply)
                        altered[-1] ^= 1
                        reply = bytes(altered)
                    self.send(conn, TC_STATE, reply)
                elif kind == STREAM_GET and self.stats is not None:
                    self.send(conn, STREAM_STATE, self.stats)
                elif kind == DRIVER_GET:
                    tc = self.states[self.kind]
                    self.send(conn, DRIVER_STATE,
                              tc[4:4 + DRIVER_STATE_SIZE] + tc)

    def close(self):
        self.listener.close()
        self.thread.join(timeout=1)


def invoke(binary, *args):
    result = subprocess.run(
        [str(binary), *args], text=True, capture_output=True, check=False,
        timeout=5,
    )
    return result, json.loads(result.stdout)


def run(repo):
    with tempfile.TemporaryDirectory(prefix="a8-tc-api-", dir="/tmp") as tmp:
        base = Path(tmp)
        fixture = base / "fixture"
        control = base / "control"
        sock = base / "control.sock"
        lock = base / "mutation.lock"
        compile_fixture(repo, fixture)
        compile_control(repo, control, sock, lock)
        states = {
            name: subprocess.check_output([str(fixture), name])
            for name in ("disarmed", "qualifying", "waiting", "invalid-enum")
        }
        legacy_stats = stream_payload(
            repo / "src/tools/opena8dj-control.c")[0]

        result, doc = invoke(
            control, "api", "driver-mode", "arm",
            "timecode-optimized", "--input-pairs", "A,C",
        )
        check(result.returncode == 2, "invalid pair CLI exit")
        check(doc["error"]["code"] == "timecode_pair_allowlist_invalid",
              "invalid pair error")

        server = Server(sock, states)
        result, doc = invoke(
            control, "api", "driver-mode", "arm",
            "timecode-optimized", "--input-pairs", "A,B",
        )
        server.close()
        check(result.returncode == 0, result.stderr)
        tc = doc["data"]["timecodeOptimized"]
        check(tc["armed"] and tc["armState"] == "qualifying", "arm state")
        check(tc["allowedInputPairs"] == ["A", "B"], "allowlist output")
        check(tc["evidenceKind"] == "observed_activity" and
              tc["intentObserved"] is False, "truthful evidence")
        check([item[2] for item in server.requests] == [TC_ARM, TC_GET],
              "arm did not use one set/read-back connection")
        arm_payload = server.requests[0][3]
        check(len(arm_payload) == 16 and
              struct.unpack_from("=HHIB", arm_payload) == (1, 0, 3, 3) and
              arm_payload[9:] == bytes(7), "arm payload is not canonical")
        sock.unlink(missing_ok=True)

        server = Server(sock, states)
        server.kind = "qualifying"
        result, doc = invoke(control, "api", "driver-mode")
        server.close()
        check(result.returncode == 0 and
              doc["data"]["timecodeOptimized"]["armed"],
              "new driver-mode tail missing")
        sock.unlink(missing_ok=True)

        server = Server(
            sock, states,
            stats=legacy_stats + states["qualifying"])
        result, doc = invoke(control, "api", "stats")
        server.close()
        check(result.returncode == 0 and
              doc["data"]["timecodeOptimized"]["armed"] and
              doc["data"]["timecodeOptimized"]["evidenceKind"] ==
                  "observed_activity",
              "new stats timecode tail missing")
        sock.unlink(missing_ok=True)

        server = Server(sock, states)
        server.kind = "qualifying"
        result, doc = invoke(
            control, "api", "driver-mode", "disarm",
            "timecode-optimized",
        )
        server.close()
        check(result.returncode == 0 and
              not doc["data"]["timecodeOptimized"]["armed"] and
              doc["data"]["timecodeOptimized"]["armState"] == "disarmed",
              "disarm state")
        check([item[2] for item in server.requests] ==
              [TC_DISARM, TC_GET], "disarm read-back transaction")
        sock.unlink(missing_ok=True)

        server = Server(sock, states, arm_kind="waiting")
        result, doc = invoke(
            control, "api", "driver-mode", "arm",
            "timecode-optimized", "--input-pairs", "A,B",
        )
        server.close()
        check(result.returncode == 0 and
              doc["data"]["timecodeOptimized"]["armed"] and
              doc["data"]["timecodeOptimized"]["armState"] ==
                  "waiting_profile" and
              not doc["data"]["timecodeOptimized"]["profileVerified"],
              "unavailable profile did not wait truthfully")
        sock.unlink(missing_ok=True)

        server = Server(sock, states, mismatch=True)
        result, doc = invoke(
            control, "api", "driver-mode", "arm",
            "timecode-optimized", "--input-pairs", "A,B",
        )
        server.close()
        check(result.returncode == 4 and
              doc["error"]["code"] == "backend_protocol_error",
              "arm/read-back mismatch accepted")
        sock.unlink(missing_ok=True)

        server = Server(sock, states, arm_kind="invalid-enum")
        result, doc = invoke(
            control, "api", "driver-mode", "arm",
            "timecode-optimized", "--input-pairs", "A,B",
        )
        server.close()
        check(result.returncode == 4 and
              doc["error"]["code"] == "backend_protocol_error",
              "API/HAL enum mismatch accepted")
    print("timecode optimized API/IPC offline: PASS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    run(parser.parse_args().repo.resolve())
