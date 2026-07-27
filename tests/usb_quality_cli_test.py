#!/usr/bin/env python3
"""Deterministic mock-socket tests for opena8dj-control usb-quality."""

import argparse
import json
import os
import re
import socket
import struct
import subprocess
import tempfile
import threading
from pathlib import Path


MAGIC = 0x4A443841
VERSION = 1
STREAM_STATS_GET = 10
STREAM_STATS = 11
HEADER = struct.Struct("=IBBH")


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def payload_layout(source):
    match = re.search(
        r"typedef struct OpenA8DJStreamStatsPayload \{(.*?)\}"
        r" __attribute__\(\(packed\)\) OpenA8DJStreamStatsPayload;",
        source.read_text(),
        re.S,
    )
    check(match is not None, "stream payload missing")
    formats = {"uint8_t": "B", "uint32_t": "I", "uint64_t": "Q", "double": "d"}
    fields = re.findall(
        r"^\s*(uint8_t|uint32_t|uint64_t|double)\s+([A-Za-z0-9_]+);",
        match.group(1),
        re.M,
    )
    offsets = {}
    size = 0
    for type_name, name in fields:
        offsets[name] = (size, formats[type_name])
        size += struct.calcsize("=" + formats[type_name])
    return offsets, size


def make_payload(layout, size, **values):
    values.setdefault("streaming", 1)
    values.setdefault("sampleRate", 48000.0)
    values.setdefault("qualityInstrumentationEnabled", 1)
    payload = bytearray(size)
    for name, value in values.items():
        offset, field_format = layout[name]
        struct.pack_into("=" + field_format, payload, offset, value)
    return bytes(payload)


class SequenceIPC:
    def __init__(self, path, payloads):
        self.path = str(path)
        self.payloads = payloads
        self.requests = []
        self.error = None
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(self.path)
        os.chmod(self.path, 0o666)
        self.listener.listen(len(payloads))
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, traceback):
        self.listener.close()
        self.thread.join(timeout=1)
        if self.error is not None and exc_type is None:
            raise self.error

    @staticmethod
    def read_full(connection, count):
        data = bytearray()
        while len(data) < count:
            chunk = connection.recv(count - len(data))
            if not chunk:
                return None
            data.extend(chunk)
        return bytes(data)

    def _serve(self):
        try:
            for payload in self.payloads:
                connection, _ = self.listener.accept()
                with connection:
                    raw_header = self.read_full(connection, HEADER.size)
                    if raw_header is None:
                        continue
                    magic, version, message_type, length = HEADER.unpack(raw_header)
                    request_payload = self.read_full(connection, length)
                    self.requests.append((magic, version, message_type, request_payload))
                    connection.sendall(
                        HEADER.pack(MAGIC, VERSION, STREAM_STATS, len(payload)) + payload
                    )
        except (BrokenPipeError, ConnectionResetError, OSError) as error:
            if self.listener.fileno() != -1:
                self.error = error


def compile_harness(source, output, socket_path):
    subprocess.run(
        [
            "xcrun", "clang", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
            f'-DOPENA8DJ_PUBLIC_API_SOCKET_PATH="{socket_path}"',
            "-DOPENA8DJ_PUBLIC_API_TEST_TRUST_CURRENT_UID=1",
            "-framework", "CoreAudio", "-framework", "CoreFoundation",
            "-o", str(output), str(source),
        ],
        check=True,
        timeout=30,
    )


def invoke_sequence(binary, socket_path, payloads, *arguments, timeout=5):
    with SequenceIPC(socket_path, payloads) as server:
        result = subprocess.run(
            [str(binary), "usb-quality", *arguments],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    socket_path.unlink(missing_ok=True)
    return result, server


def delta_payload(layout, size, **overrides):
    values = {
        "captureTransfers": 1,
        "playbackTransfers": 1,
        "captureCompletionJitterSamples": 20,
        "captureCompletionJitterLe50": 20,
        "playbackCompletionJitterSamples": 20,
        "playbackCompletionJitterLe50": 20,
    }
    values.update(overrides)
    return make_payload(layout, size, **values)


def run_tests(repo):
    source = repo / "src/tools/opena8dj-control.c"
    layout, payload_size = payload_layout(source)
    baseline = make_payload(layout, payload_size)
    with tempfile.TemporaryDirectory(prefix="a8quality-", dir="/tmp") as temporary:
        temporary_path = Path(temporary)
        socket_path = temporary_path / "control.sock"
        harness = temporary_path / "opena8dj-control-test"
        compile_harness(source, harness, socket_path)

        invalid = subprocess.run(
            [str(harness), "usb-quality", "--interval-ms", "99"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
        check(invalid.returncode == 2, "invalid interval did not return 2")
        check(not socket_path.exists(), "invalid arguments attempted a connection")

        stable = delta_payload(layout, payload_size)
        result, server = invoke_sequence(
            harness, socket_path, [baseline, stable],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        check(result.returncode == 0 and result.stderr == "", "stable JSON run failed")
        documents = [json.loads(line) for line in result.stdout.splitlines()]
        check(len(documents) == 2, "NDJSON did not emit one document per observation")
        check(documents[0]["stability"]["classification"] == "warming-up",
              "first observation was not warming-up")
        check(documents[1]["stability"]["classification"] == "stable",
              "clean window was not stable")
        check(documents[1]["jitter"]["capture"]["p95"] ==
              {"upperBoundUs": 50, "overflow": False},
              "percentile was not an honest bin bound")
        check(all(request[2] == STREAM_STATS_GET for request in server.requests) and
              len(server.requests) == 2,
              "meter did not use one non-destructive snapshot per connection")

        result, _ = invoke_sequence(
            harness, socket_path, [baseline, baseline],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        inactive_doc = json.loads(result.stdout.splitlines()[1])
        check(inactive_doc["stability"]["classification"] == "insufficient-data" and
              "no_active_directions" in inactive_doc["stability"]["reasons"],
              "window without active directions was presented as stable")

        stable_boundary = delta_payload(
            layout, payload_size,
            captureCompletionJitterLe50=18,
            captureCompletionJitterLe250=1,
            captureCompletionJitterLe500=1,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [baseline, stable_boundary],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        boundary_doc = json.loads(result.stdout.splitlines()[1])
        check(boundary_doc["stability"]["classification"] == "stable" and
              boundary_doc["jitter"]["capture"]["p95"]["upperBoundUs"] == 250 and
              boundary_doc["jitter"]["capture"]["p99"]["upperBoundUs"] == 500,
              "inclusive stable percentile boundaries were misclassified")

        degraded = delta_payload(
            layout, payload_size,
            captureCompletionJitterLe50=18,
            captureCompletionJitterLe500=2,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [baseline, degraded],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        degraded_doc = json.loads(result.stdout.splitlines()[1])
        check(degraded_doc["stability"]["classification"] == "degraded",
              "degraded jitter boundary was misclassified")
        check("capture.p95_gt_250us" in degraded_doc["stability"]["reasons"],
              "degraded reason missing")

        inconsistent = delta_payload(
            layout, payload_size,
            captureCompletionJitterLe50=19,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [baseline, inconsistent],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        inconsistent_doc = json.loads(result.stdout.splitlines()[1])
        check(inconsistent_doc["stability"]["classification"] == "insufficient-data" and
              "instrumentation_inconsistent" in
              inconsistent_doc["stability"]["reasons"],
              "inconsistent histogram was treated as valid evidence")

        iso_fields = {
            "capture": [
                "captureQueueFailures",
                "captureISOCompletionStatusFailures",
                "captureISOTransactionStatusFailures",
                "captureISOZeroLengthTransactions",
                "captureISOShortTransactions",
            ],
            "playback": [
                "playbackQueueFailures",
                "playbackISOCompletionStatusFailures",
                "playbackISOTransactionStatusFailures",
                "playbackISOZeroLengthTransactions",
                "playbackISOShortTransactions",
            ],
        }
        other_direction = {"capture": "playback", "playback": "capture"}
        for direction, fields in iso_fields.items():
            for field in fields:
                individual_error = delta_payload(
                    layout, payload_size, **{field: 1}
                )
                result, _ = invoke_sequence(
                    harness, socket_path, [baseline, individual_error],
                    "--json", "--interval-ms", "100", "--count", "2",
                )
                error_doc = json.loads(result.stdout.splitlines()[1])
                check(error_doc["stability"]["classification"] == "unstable",
                      f"{field} did not cause unstable")
                check(error_doc["isoErrors"][direction]["totalEvents"] == 1,
                      f"{field} was not mapped to {direction}")
                check(error_doc["isoErrors"][other_direction[direction]]["totalEvents"] == 0,
                      f"{field} leaked to the opposite direction")
                check(f"{direction}.iso_errors" in
                      error_doc["stability"]["reasons"],
                      f"{field} reason has wrong direction")

        unstable = delta_payload(
            layout, payload_size,
            captureCompletionJitterLe50=19,
            captureCompletionJitterGt1000=1,
            captureISOCompletionStatusFailures=1,
            captureISOTransactionStatusFailures=1,
            captureISOZeroLengthTransactions=1,
            captureISOShortTransactions=1,
            captureQueueFailures=1,
            playbackISOCompletionStatusFailures=1,
            playbackISOTransactionStatusFailures=1,
            playbackISOZeroLengthTransactions=1,
            playbackISOShortTransactions=1,
            playbackQueueFailures=1,
            outputActiveUnderruns=1,
            outputRingOverruns=1,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [baseline, unstable],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        unstable_doc = json.loads(result.stdout.splitlines()[1])
        check(unstable_doc["stability"]["classification"] == "unstable",
              "errors did not take precedence over sample count")
        check(unstable_doc["jitter"]["capture"]["p99"] == {
            "upperBoundUs": None,
            "lowerBoundExclusiveUs": 1000,
            "overflow": True,
        }, "overflow percentile was fabricated")
        check(unstable_doc["isoErrors"]["capture"]["totalEvents"] == 5 and
              unstable_doc["isoErrors"]["playback"]["totalEvents"] == 5,
              "ISO classes/directions were not all included")
        check(unstable_doc["xruns"]["totalHardXruns"] == 2,
              "hard xrun total is wrong")

        startup_only = delta_payload(
            layout, payload_size,
            playbackTransfers=0,
            playbackCompletionJitterSamples=0,
            playbackCompletionJitterLe50=0,
            outputUnderruns=7,
            outputLateWriteBatches=2,
            outputLateWriteFrames=64,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [baseline, startup_only],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        startup_doc = json.loads(result.stdout.splitlines()[1])
        check(startup_doc["stability"]["classification"] == "stable",
              "startup underrun/late writes were treated as hard xruns")
        check(startup_doc["jitter"]["playback"]["active"] is False,
              "inactive playback direction became active")
        check(startup_doc["xruns"]["outputUnderruns"] == 7 and
              startup_doc["xruns"]["outputLateWriteFrames"] == 64,
              "leading/startup diagnostics were hidden")

        insufficient = delta_payload(
            layout, payload_size,
            captureCompletionJitterSamples=19,
            captureCompletionJitterLe50=19,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [baseline, insufficient],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        check(json.loads(result.stdout.splitlines()[1])["stability"]["classification"] ==
              "insufficient-data", "short active window was not insufficient")

        reset_baseline = delta_payload(layout, payload_size)
        reset_current = delta_payload(
            layout, payload_size,
            captureTransfers=0,
            playbackTransfers=0,
            captureCompletionJitterSamples=0,
            captureCompletionJitterLe50=0,
            playbackCompletionJitterSamples=0,
            playbackCompletionJitterLe50=0,
        )
        result, _ = invoke_sequence(
            harness, socket_path, [reset_baseline, reset_current],
            "--json", "--interval-ms", "100", "--count", "2",
        )
        reset_doc = json.loads(result.stdout.splitlines()[1])
        check(reset_doc["stability"]["classification"] == "warming-up" and
              "counter_reset" in reset_doc["stability"]["reasons"],
              "counter reset was treated as unsigned delta")

        old_tail_offset, old_tail_format = layout["outputLateWriteBatches"]
        legacy_length = old_tail_offset + struct.calcsize("=" + old_tail_format)
        result, _ = invoke_sequence(
            harness, socket_path, [baseline[:legacy_length]],
            "--json", "--count", "1",
        )
        legacy = json.loads(result.stdout)
        check(legacy["instrumentationAvailable"] is False and
              legacy["stability"]["classification"] == "insufficient-data" and
              "instrumentation_unavailable" in legacy["stability"]["reasons"],
              "legacy HAL was presented as healthy")

        marker_disabled = make_payload(
            layout, payload_size, qualityInstrumentationEnabled=0
        )
        result, _ = invoke_sequence(
            harness, socket_path, [marker_disabled], "--json", "--count", "1",
        )
        disabled = json.loads(result.stdout)
        check(disabled["instrumentationAvailable"] is False and
              disabled["stability"]["classification"] == "insufficient-data" and
              "instrumentation_unavailable" in disabled["stability"]["reasons"],
              "disabled instrumentation marker was presented as healthy")

        not_streaming = make_payload(layout, payload_size, streaming=0)
        result, _ = invoke_sequence(
            harness, socket_path, [not_streaming], "--json", "--count", "1",
        )
        check(json.loads(result.stdout)["stability"]["classification"] == "not-streaming",
              "stopped stream classification is wrong")

        result, _ = invoke_sequence(
            harness, socket_path, [baseline, unstable],
            "--interval-ms", "100", "--count", "2",
        )
        check(result.returncode == 0 and "unstable" in result.stdout and
              ">1000us" in result.stdout and "reasons:" in result.stdout and
              "thresholds:" in result.stdout,
              "human output omitted overflow/reasons/thresholds")

        result, _ = invoke_sequence(
            harness, socket_path, [b""], "--json", "--count", "1",
        )
        malformed = json.loads(result.stdout)
        check(result.returncode == 4 and result.stderr == "" and
              malformed["error"]["code"] == "backend_protocol_error",
              "malformed backend was not bounded/clean")

        unavailable = subprocess.run(
            [str(harness), "usb-quality", "--json", "--count", "1"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=2, check=False,
        )
        error = json.loads(unavailable.stdout)
        check(unavailable.returncode == 3 and unavailable.stderr == "" and
              error["error"]["code"] == "backend_unavailable",
              "JSON backend failure was not bounded/clean")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    args = parser.parse_args()
    run_tests(args.repo.resolve())
    print("USB quality CLI offline tests: PASS")


if __name__ == "__main__":
    main()
