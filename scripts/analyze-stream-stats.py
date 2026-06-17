#!/usr/bin/env python3
"""Summarize OpenA8DJ stream-stats snapshots from physical soundcheck runs.

This is an offline evidence tool. It reads existing TSV snapshots produced by
`scripts/run-soundcheck --stream-stats-snapshots` and never touches hardware,
CoreAudio, USB, HAL install state, or external worktrees.
"""

import argparse
import csv
import json
import math
from pathlib import Path


COUNTERS = [
    "captureTransfersCompleted",
    "captureTransactionErrors",
    "captureStatusFailures",
    "captureZeroCompleteTransactions",
    "captureExpectedTransactions",
    "captureOtherByteCountTransactions",
    "captureShortTransfers",
    "filteredCaptureTransactions",
    "playbackTransfersSubmitted",
    "playbackTransfersCompleted",
    "playbackTransferErrors",
    "playbackScheduleErrors",
    "playbackReschedules",
    "outputFramesWritten",
    "outputFramesRead",
    "outputUnderruns",
    "outputActiveUnderruns",
    "outputElasticDrops",
    "outputElasticReplays",
    "outputTimelineResets",
    "outputLateWriteFrames",
    "outputLateWriteBatches",
    "outputPanicFlags",
    "clockAcceptedAnchors",
    "clockRejectedAnchors",
    "clockAnchorResets",
    "captureCompletionDeltaOutliers",
    "playbackCompletionDeltaOutliers",
    "captureToPlaybackQueueDeltaOutliers",
    "captureUSBTimestampOutOfOrder",
    "captureUSBTimestampRepeated",
    "captureUSBTimestampZero",
    "playbackZeroCompleteTransactions",
    "playbackQueueAttempts",
]


GAUGES = [
    "outputRingFrames",
    "playbackInFlightAtQueueMax",
    "playbackInFlightAtCompletionMax",
]


def parse_float(value):
    if value is None or value == "":
        return math.nan
    try:
        return float(value)
    except ValueError:
        return math.nan


def finite(values):
    return [value for value in values if math.isfinite(value)]


def percentile(values, fraction):
    values = sorted(finite(values))
    if not values:
        return math.nan
    index = min(len(values) - 1, int(fraction * (len(values) - 1)))
    return values[index]


def monotonic_regressions(rows, key):
    regressions = 0
    previous = None
    for row in rows:
        value = parse_float(row.get(key))
        if not math.isfinite(value):
            continue
        if previous is not None and value < previous:
            regressions += 1
        previous = value
    return regressions


def delta(rows, key):
    values = [parse_float(row.get(key)) for row in rows]
    values = finite(values)
    if len(values) < 2:
        return math.nan
    return values[-1] - values[0]


def load_rows(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def analyze(path):
    rows = load_rows(path)
    ok_rows = [row for row in rows if row.get("status") == "OK"]
    error_rows = [row for row in rows if row.get("status") != "OK"]
    elapsed = [parse_float(row.get("elapsed_seconds")) for row in ok_rows]
    elapsed = finite(elapsed)
    duration = elapsed[-1] - elapsed[0] if len(elapsed) >= 2 else math.nan

    counters = {}
    for key in COUNTERS:
        key_delta = delta(ok_rows, key)
        counters[key] = {
            "first": parse_float(ok_rows[0].get(key)) if ok_rows else math.nan,
            "last": parse_float(ok_rows[-1].get(key)) if ok_rows else math.nan,
            "delta": key_delta,
            "per_second": key_delta / duration if math.isfinite(key_delta) and duration > 0 else math.nan,
            "monotonic_regressions": monotonic_regressions(ok_rows, key),
        }

    gauges = {}
    for key in GAUGES:
        values = [parse_float(row.get(key)) for row in ok_rows]
        values = finite(values)
        gauges[key] = {
            "min": min(values) if values else math.nan,
            "max": max(values) if values else math.nan,
            "p50": percentile(values, 0.50),
            "p95": percentile(values, 0.95),
        }

    written_last = counters["outputFramesWritten"]["last"]
    read_delta = counters["outputFramesRead"]["delta"]
    written_observable = math.isfinite(written_last) and written_last > 0
    if written_observable:
        write_read_delta = counters["outputFramesWritten"]["last"] - counters["outputFramesRead"]["last"]
    else:
        write_read_delta = math.nan

    capture_tx_delta = counters["captureTransfersCompleted"]["delta"]
    capture_errors_delta = counters["captureTransactionErrors"]["delta"]
    capture_error_per_transfer = (
        capture_errors_delta / capture_tx_delta
        if math.isfinite(capture_errors_delta) and capture_tx_delta > 0 else math.nan
    )

    playback_completed_delta = counters["playbackTransfersCompleted"]["delta"]
    transfer_balance_delta = (
        playback_completed_delta - capture_tx_delta
        if math.isfinite(playback_completed_delta) and math.isfinite(capture_tx_delta) else math.nan
    )

    flags = []
    if error_rows:
        flags.append("stream_stats_timeouts")
    if not written_observable:
        flags.append("output_write_stats_unobservable")
    if counters["outputActiveUnderruns"]["delta"] > 0:
        flags.append("active_underruns")
    if counters["outputTimelineResets"]["delta"] > 0:
        flags.append("timeline_resets")
    if counters["outputLateWriteFrames"]["delta"] > 0:
        flags.append("late_writes")
    if counters["outputPanicFlags"]["delta"] > 0:
        flags.append("panic_flags")
    if counters["playbackTransferErrors"]["delta"] > 0:
        flags.append("playback_transfer_errors")
    if counters["playbackScheduleErrors"]["delta"] > 0:
        flags.append("playback_schedule_errors")

    return {
        "schema": "opena8djcpp.stream-stats-summary.v1",
        "path": str(path),
        "run_dir": str(path.parent),
        "result": "PASS" if not flags else "DIAGNOSTIC_FLAGS",
        "flags": flags,
        "sample_count": len(rows),
        "ok_sample_count": len(ok_rows),
        "error_sample_count": len(error_rows),
        "ok_duration_seconds": duration,
        "output_write_stats_observable": written_observable,
        "output_write_minus_read_frames_last": write_read_delta,
        "output_read_frames_per_second": counters["outputFramesRead"]["per_second"],
        "capture_transfers_per_second": counters["captureTransfersCompleted"]["per_second"],
        "playback_transfers_completed_per_second": counters["playbackTransfersCompleted"]["per_second"],
        "playback_minus_capture_transfer_delta": transfer_balance_delta,
        "capture_transaction_errors_per_capture_transfer": capture_error_per_transfer,
        "capture_status_failures_per_capture_transfer": (
            counters["captureStatusFailures"]["delta"] / capture_tx_delta
            if math.isfinite(counters["captureStatusFailures"]["delta"]) and capture_tx_delta > 0 else math.nan
        ),
        "capture_zero_complete_per_capture_transfer": (
            counters["captureZeroCompleteTransactions"]["delta"] / capture_tx_delta
            if math.isfinite(counters["captureZeroCompleteTransactions"]["delta"]) and capture_tx_delta > 0 else math.nan
        ),
        "capture_other_size_per_capture_transfer": (
            counters["captureOtherByteCountTransactions"]["delta"] / capture_tx_delta
            if math.isfinite(counters["captureOtherByteCountTransactions"]["delta"]) and capture_tx_delta > 0 else math.nan
        ),
        "capture_short_per_capture_transfer": (
            counters["captureShortTransfers"]["delta"] / capture_tx_delta
            if math.isfinite(counters["captureShortTransfers"]["delta"]) and capture_tx_delta > 0 else math.nan
        ),
        "filtered_capture_per_capture_transfer": (
            counters["filteredCaptureTransactions"]["delta"] / capture_tx_delta
            if math.isfinite(counters["filteredCaptureTransactions"]["delta"]) and capture_tx_delta > 0 else math.nan
        ),
        "counters": counters,
        "gauges": gauges,
    }


def json_safe(value):
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    if isinstance(value, dict):
        return {key: json_safe(item) for key, item in value.items()}
    if isinstance(value, list):
        return [json_safe(item) for item in value]
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", help="stream-stats-during.tsv files or soundcheck directories")
    parser.add_argument("--json-out", help="write JSON result to this path")
    args = parser.parse_args()

    targets = []
    for raw in args.paths:
        path = Path(raw)
        if path.is_dir():
            path = path / "stream-stats-during.tsv"
        targets.append(path)

    summaries = [analyze(path) for path in targets]
    result = summaries[0] if len(summaries) == 1 else {
        "schema": "opena8djcpp.stream-stats-summary-set.v1",
        "result": "PASS" if all(item["result"] == "PASS" for item in summaries) else "DIAGNOSTIC_FLAGS",
        "runs": summaries,
    }
    text = json.dumps(json_safe(result), indent=2, sort_keys=True, allow_nan=False)
    print(text)
    if args.json_out:
        Path(args.json_out).write_text(text + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
