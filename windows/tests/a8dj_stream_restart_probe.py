#!/usr/bin/env python3
"""Exercise rapid exclusive-WASAPI stop/start transitions with durable checkpoints."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path

import sounddevice as sd

from a8dj_duplex_soak import find_device, write_json


COUNTERS = (
    "underruns", "overruns", "packet-errors", "late-completions",
    "iso-out-empty", "iso-out-late", "iso-out-bad-start", "iso-out-other-err",
    "iso-output-panic", "iso-cap-late", "iso-cap-bad-start", "iso-cap-other-err",
    "rate-settle-fails",
)


def checkpoint(path: Path, payload: dict) -> None:
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(payload, separators=(",", ":")) + "\n")
        handle.flush()
        os.fsync(handle.fileno())


def diagnostics(ctl: str) -> tuple[str, dict[str, int], int, int]:
    completed = subprocess.run(
        [ctl, "diagnostics"], check=False, capture_output=True, text=True, timeout=8.0
    )
    if completed.returncode != 0:
        raise RuntimeError(f"diagnostics failed: {completed.returncode}")
    text = completed.stdout + completed.stderr
    values: dict[str, int] = {}
    for name in COUNTERS:
        match = re.search(rf"(?m)^\s*{re.escape(name)}:\s*(\d+)", text)
        if match is None:
            raise RuntimeError(f"missing diagnostics counter: {name}")
        values[name] = int(match.group(1))
    query = re.search(r"(?m)^\s*iso-frame-query:\s*runs=(\d+)\s+failures=(\d+)", text)
    if query is None:
        raise RuntimeError("missing iso-frame-query diagnostics")
    return text, values, int(query.group(1)), int(query.group(2))


def run_stream(direction: str, device: int, rate: int, blocksize: int, seconds: float) -> dict:
    state = {"callbacks": 0, "frames": 0, "statuses": [], "last": time.monotonic()}

    def input_callback(indata, frames, timing, status):
        del indata, timing
        if status:
            state["statuses"].append(str(status))
        state["callbacks"] += 1
        state["frames"] += frames
        state["last"] = time.monotonic()

    def output_callback(outdata, frames, timing, status):
        del timing
        outdata.fill(0.0)
        if status:
            state["statuses"].append(str(status))
        state["callbacks"] += 1
        state["frames"] += frames
        state["last"] = time.monotonic()

    stream_type = sd.OutputStream if direction == "render" else sd.InputStream
    callback = output_callback if direction == "render" else input_callback
    started = time.monotonic()
    watchdog = False
    with stream_type(
        device=device, channels=8, samplerate=rate, blocksize=blocksize,
        dtype="float32", latency="high", extra_settings=sd.WasapiSettings(exclusive=True),
        callback=callback,
    ):
        deadline = started + seconds
        while time.monotonic() < deadline:
            if time.monotonic() - state["last"] > 1.0:
                watchdog = True
                break
            time.sleep(0.010)
    elapsed = time.monotonic() - started
    return {
        "direction": direction, "callbacks": state["callbacks"], "frames": state["frames"],
        "elapsed_seconds": elapsed, "status_events": state["statuses"],
        "watchdog_expired": watchdog,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ctl", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--stream-milliseconds", type=int, default=200)
    parser.add_argument("--repeats", type=int, default=1)
    args = parser.parse_args()
    if args.rate not in (44100, 48000) or args.blocksize <= 0 or args.repeats <= 0:
        parser.error("invalid restart arguments")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    checkpoint_path = out_dir / "restart-checkpoints.jsonl"
    render = find_device("output", "Audio 8 DJ (8ch Out) (Audio 8 DJ)", "Windows WASAPI", 8)
    capture = find_device("input", "Audio 8 DJ (8ch In) (Audio 8 DJ)", "Windows WASAPI", 8)
    devices = {"render": render, "capture": capture}
    gaps = (0, 2, 4, 6, 10, 100, 1100)
    transitions = (("render", "render"), ("capture", "capture"),
                   ("render", "capture"), ("capture", "render"))
    initial_text, initial_counters, initial_queries, initial_query_failures = diagnostics(args.ctl)
    (out_dir / "restart-diagnostics-initial.txt").write_text(initial_text, encoding="utf-8")
    cases = []
    sequence = 0
    failure = None
    try:
        for repeat in range(args.repeats):
            for first, second in transitions:
                for gap_ms in gaps:
                    sequence += 1
                    case = {"sequence": sequence, "repeat": repeat, "first": first,
                            "second": second, "gap_ms": gap_ms}
                    checkpoint(checkpoint_path, {**case, "stage": "case-start", "time": time.time()})
                    first_result = run_stream(first, devices[first], args.rate, args.blocksize,
                                              args.stream_milliseconds / 1000.0)
                    first_closed_ns = time.perf_counter_ns()
                    time.sleep(gap_ms / 1000.0)
                    second_open_ns = time.perf_counter_ns()
                    second_result = run_stream(second, devices[second], args.rate, args.blocksize,
                                               args.stream_milliseconds / 1000.0)
                    text, counters, queries, query_failures = diagnostics(args.ctl)
                    counter_deltas = {name: counters[name] - initial_counters[name] for name in COUNTERS}
                    expected_queries = sequence * 2
                    passed = (
                        first_result["callbacks"] > 0 and second_result["callbacks"] > 0
                        and not first_result["status_events"] and not second_result["status_events"]
                        and not first_result["watchdog_expired"] and not second_result["watchdog_expired"]
                        and all(value == 0 for value in counter_deltas.values())
                        and queries - initial_queries == expected_queries
                        and query_failures == initial_query_failures
                    )
                    case.update({"first_result": first_result, "second_result": second_result,
                                 "counter_deltas": counter_deltas,
                                 "query_delta": queries - initial_queries,
                                 "actual_gap_ms": (second_open_ns - first_closed_ns) / 1_000_000.0,
                                 "passed": passed})
                    cases.append(case)
                    checkpoint(checkpoint_path, {**case, "stage": "case-complete", "time": time.time()})
                    if not passed:
                        (out_dir / f"restart-diagnostics-failure-{sequence:03d}.txt").write_text(text, encoding="utf-8")
                        raise RuntimeError(f"restart case {sequence} failed: {first}->{second}, gap={gap_ms}ms")
    except Exception as exc:  # retain the precise durable checkpoint on all failures
        failure = str(exc)

    report = {
        "rate": args.rate, "blocksize": args.blocksize, "stream_milliseconds": args.stream_milliseconds,
        "repeats": args.repeats, "case_count": len(cases), "expected_case_count": args.repeats * 28,
        "expected_frame_queries": args.repeats * 56, "cases": cases, "failure": failure,
        "passed": failure is None and len(cases) == args.repeats * 28 and all(case["passed"] for case in cases),
    }
    write_json(out_dir / "cadence-summary.json", report)
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
