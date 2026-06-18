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
    "captureSubmitAttempts",
    "captureTransfersSubmitted",
    "captureTransfersCompleted",
    "captureTransfersSampled",
    "captureTransactionErrors",
    "captureStatusFailures",
    "captureZeroCompleteTransactions",
    "captureExpectedTransactions",
    "captureOtherByteCountTransactions",
    "captureShortTransfers",
    "filteredCaptureTransactions",
    "playbackSubmitAttempts",
    "playbackTransfersSubmitted",
    "playbackTransfersCompleted",
    "playbackTransfersSampled",
    "playbackTransferErrors",
    "captureTransferPoolFallbackAllocations",
    "playbackTransferPoolFallbackAllocations",
    "transferLedgerEntriesWritten",
    "transferLedgerEntriesOverwritten",
    "transferLedgerCaptureQueueEntries",
    "transferLedgerCaptureCompleteEntries",
    "transferLedgerPlaybackQueueEntries",
    "transferLedgerPlaybackCompleteEntries",
    "transferLedgerPlaybackImplicitFirstFrameNumbers",
    "transferLedgerOutputReadFrames",
    "transferLedgerOutputStartupSilenceFrames",
    "transferLedgerOutputActiveUnderrunFrames",
    "transferLedgerOutputElasticDropFrames",
    "transferLedgerOutputElasticReplayFrames",
    "playbackPayloadGuardChecks",
    "playbackPayloadGuardMismatches",
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
    "hotPathCaptureHandlerTicksSum",
    "hotPathCaptureHandlerTicksSamples",
    "hotPathCaptureDecodeTicksSum",
    "hotPathCaptureDecodeTicksSamples",
    "hotPathCaptureRequeueTicksSum",
    "hotPathCaptureRequeueTicksSamples",
    "hotPathPlaybackQueueTicksSum",
    "hotPathPlaybackQueueTicksSamples",
    "hotPathPlaybackFillTicksSum",
    "hotPathPlaybackFillTicksSamples",
    "hotPathPlaybackEnqueueTicksSum",
    "hotPathPlaybackEnqueueTicksSamples",
    "hotPathPlaybackCompletionTicksSum",
    "hotPathPlaybackCompletionTicksSamples",
]


GAUGES = [
    "logicalIsoFramesPerTransfer",
    "captureIsoFramesPerTransfer",
    "playbackBaseIsoFramesPerTransfer",
    "playbackIsoFramesPerTransfer",
    "playbackCoalesceTransfers",
    "captureQueueDepth",
    "playbackQueueTarget",
    "outputRingFrames",
    "transferLedgerCapacity",
    "transferLedgerPlaybackFirstFrameMin",
    "transferLedgerPlaybackFirstFrameMax",
    "playbackInFlightAtQueueMax",
    "playbackInFlightAtCompletionMax",
    "hotPathCaptureHandlerTicksMin",
    "hotPathCaptureHandlerTicksMax",
    "hotPathCaptureDecodeTicksMin",
    "hotPathCaptureDecodeTicksMax",
    "hotPathCaptureRequeueTicksMin",
    "hotPathCaptureRequeueTicksMax",
    "hotPathPlaybackQueueTicksMin",
    "hotPathPlaybackQueueTicksMax",
    "hotPathPlaybackFillTicksMin",
    "hotPathPlaybackFillTicksMax",
    "hotPathPlaybackEnqueueTicksMin",
    "hotPathPlaybackEnqueueTicksMax",
    "hotPathPlaybackCompletionTicksMin",
    "hotPathPlaybackCompletionTicksMax",
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


def average_timing(counters, prefix):
    sum_delta = counters[f"{prefix}TicksSum"]["delta"]
    samples_delta = counters[f"{prefix}TicksSamples"]["delta"]
    if math.isfinite(sum_delta) and samples_delta > 0:
        return sum_delta / samples_delta
    return math.nan


def stable_gauge_value(rows, key):
    values = finite(parse_float(row.get(key)) for row in rows)
    if not values:
        return math.nan
    first = values[0]
    for value in values[1:]:
        if value != first:
            return math.nan
    return first


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

    capture_submit_attempt_delta = counters["captureSubmitAttempts"]["delta"]
    capture_submit_delta = counters["captureTransfersSubmitted"]["delta"]
    capture_submit_failure_delta = (
        capture_submit_attempt_delta - capture_submit_delta
        if math.isfinite(capture_submit_attempt_delta) and math.isfinite(capture_submit_delta)
        else math.nan
    )
    capture_tx_delta = counters["captureTransfersCompleted"]["delta"]
    capture_sampled_delta = counters["captureTransfersSampled"]["delta"]
    capture_ratio_delta = capture_sampled_delta if capture_sampled_delta > 0 else capture_tx_delta
    capture_ratio_denominator = "captureTransfersSampled" if capture_sampled_delta > 0 else "captureTransfersCompleted"
    capture_errors_delta = counters["captureTransactionErrors"]["delta"]
    capture_expected_delta = counters["captureExpectedTransactions"]["delta"]
    capture_zero_delta = counters["captureZeroCompleteTransactions"]["delta"]
    capture_error_per_transfer = (
        capture_errors_delta / capture_ratio_delta
        if math.isfinite(capture_errors_delta) and capture_ratio_delta > 0 else math.nan
    )
    capture_error_per_raw_transfer = (
        capture_errors_delta / capture_tx_delta
        if math.isfinite(capture_errors_delta) and capture_tx_delta > 0 else math.nan
    )

    playback_submit_attempt_delta = counters["playbackSubmitAttempts"]["delta"]
    playback_submit_delta = counters["playbackTransfersSubmitted"]["delta"]
    playback_submit_failure_delta = (
        playback_submit_attempt_delta - playback_submit_delta
        if math.isfinite(playback_submit_attempt_delta) and math.isfinite(playback_submit_delta)
        else math.nan
    )
    playback_completed_delta = counters["playbackTransfersCompleted"]["delta"]
    transfer_balance_delta = (
        playback_completed_delta - capture_tx_delta
        if math.isfinite(playback_completed_delta) and math.isfinite(capture_tx_delta) else math.nan
    )
    logical_iso = stable_gauge_value(ok_rows, "logicalIsoFramesPerTransfer")
    capture_iso = stable_gauge_value(ok_rows, "captureIsoFramesPerTransfer")
    playback_base_iso = stable_gauge_value(ok_rows, "playbackBaseIsoFramesPerTransfer")
    playback_iso = stable_gauge_value(ok_rows, "playbackIsoFramesPerTransfer")
    playback_coalesce = stable_gauge_value(ok_rows, "playbackCoalesceTransfers")
    capture_queue_depth = stable_gauge_value(ok_rows, "captureQueueDepth")
    playback_queue_target = stable_gauge_value(ok_rows, "playbackQueueTarget")
    capture_submit_reduction_ratio = (
        capture_iso / logical_iso
        if math.isfinite(capture_iso) and math.isfinite(logical_iso) and logical_iso > 0 else math.nan
    )
    playback_submit_reduction_ratio = (
        playback_iso / playback_base_iso
        if math.isfinite(playback_iso) and math.isfinite(playback_base_iso) and playback_base_iso > 0 else math.nan
    )
    capture_submits_per_output_frame = (
        capture_submit_delta / read_delta
        if math.isfinite(capture_submit_delta) and math.isfinite(read_delta) and read_delta > 0 else math.nan
    )
    playback_submits_per_output_frame = (
        playback_completed_delta / read_delta
        if math.isfinite(playback_completed_delta) and math.isfinite(read_delta) and read_delta > 0 else math.nan
    )
    expected_capture_transfers_per_output_frame = (
        1.0 / capture_iso
        if math.isfinite(capture_iso) and capture_iso > 0 else math.nan
    )
    expected_playback_transfers_per_output_frame = (
        1.0 / playback_iso
        if math.isfinite(playback_iso) and playback_iso > 0 else math.nan
    )
    capture_submit_rate_ratio = (
        capture_submits_per_output_frame / expected_capture_transfers_per_output_frame
        if (math.isfinite(capture_submits_per_output_frame) and
            math.isfinite(expected_capture_transfers_per_output_frame) and
            expected_capture_transfers_per_output_frame > 0) else math.nan
    )
    playback_submit_rate_ratio = (
        playback_submits_per_output_frame / expected_playback_transfers_per_output_frame
        if (math.isfinite(playback_submits_per_output_frame) and
            math.isfinite(expected_playback_transfers_per_output_frame) and
            expected_playback_transfers_per_output_frame > 0) else math.nan
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
    if counters["captureTransferPoolFallbackAllocations"]["delta"] > 0:
        flags.append("capture_transfer_pool_fallback_allocations")
    if counters["playbackTransferPoolFallbackAllocations"]["delta"] > 0:
        flags.append("playback_transfer_pool_fallback_allocations")
    if counters["transferLedgerEntriesOverwritten"]["delta"] > 0:
        flags.append("transfer_ledger_overwritten")
    if counters["transferLedgerCaptureQueueEntries"]["delta"] > counters["transferLedgerCaptureCompleteEntries"]["delta"] + 1:
        flags.append("transfer_ledger_capture_completion_gap")
    if counters["transferLedgerPlaybackQueueEntries"]["delta"] > counters["transferLedgerPlaybackCompleteEntries"]["delta"] + 1:
        flags.append("transfer_ledger_playback_completion_gap")
    if counters["transferLedgerOutputActiveUnderrunFrames"]["delta"] > 0:
        flags.append("transfer_ledger_active_underruns")
    if counters["playbackPayloadGuardMismatches"]["delta"] > 0:
        flags.append("playback_payload_guard_mismatches")
    if counters["playbackScheduleErrors"]["delta"] > 0:
        flags.append("playback_schedule_errors")
    if math.isfinite(capture_submit_failure_delta) and capture_submit_failure_delta > 0:
        flags.append("capture_submit_failures")
    if math.isfinite(playback_submit_failure_delta) and playback_submit_failure_delta > 0:
        flags.append("playback_submit_failures")
    if math.isfinite(capture_submit_failure_delta) and capture_submit_failure_delta < 0:
        flags.append("capture_submit_attempts_less_than_submitted")
    if math.isfinite(playback_submit_failure_delta) and playback_submit_failure_delta < 0:
        flags.append("playback_submit_attempts_less_than_submitted")
    if ok_rows and not math.isfinite(logical_iso):
        flags.append("runtime_geometry_missing_or_unstable")

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
        "capture_submit_attempts_per_second": counters["captureSubmitAttempts"]["per_second"],
        "capture_transfers_per_second": counters["captureTransfersCompleted"]["per_second"],
        "capture_transfers_submitted_per_second": counters["captureTransfersSubmitted"]["per_second"],
        "capture_submit_failures": capture_submit_failure_delta,
        "capture_transfers_sampled_per_second": counters["captureTransfersSampled"]["per_second"],
        "playback_submit_attempts_per_second": counters["playbackSubmitAttempts"]["per_second"],
        "playback_transfers_submitted_per_second": counters["playbackTransfersSubmitted"]["per_second"],
        "playback_submit_failures": playback_submit_failure_delta,
        "playback_transfers_completed_per_second": counters["playbackTransfersCompleted"]["per_second"],
        "playback_transfers_sampled_per_second": counters["playbackTransfersSampled"]["per_second"],
        "playback_minus_capture_transfer_delta": transfer_balance_delta,
        "runtime_geometry": {
            "logical_iso_frames_per_transfer": logical_iso,
            "capture_iso_frames_per_transfer": capture_iso,
            "playback_base_iso_frames_per_transfer": playback_base_iso,
            "playback_iso_frames_per_transfer": playback_iso,
            "playback_coalesce_transfers": playback_coalesce,
            "capture_queue_depth": capture_queue_depth,
            "playback_queue_target": playback_queue_target,
            "capture_submit_reduction_ratio_vs_logical": capture_submit_reduction_ratio,
            "playback_submit_reduction_ratio_vs_base": playback_submit_reduction_ratio,
            "capture_submits_per_output_frame": capture_submits_per_output_frame,
            "playback_submits_per_output_frame": playback_submits_per_output_frame,
            "expected_capture_transfers_per_output_frame": expected_capture_transfers_per_output_frame,
            "expected_playback_transfers_per_output_frame": expected_playback_transfers_per_output_frame,
            "capture_submit_rate_ratio_to_expected": capture_submit_rate_ratio,
            "playback_submit_rate_ratio_to_expected": playback_submit_rate_ratio,
        },
        "capture_transfer_pool_fallback_allocations_per_second":
            counters["captureTransferPoolFallbackAllocations"]["per_second"],
        "playback_transfer_pool_fallback_allocations_per_second":
            counters["playbackTransferPoolFallbackAllocations"]["per_second"],
        "transfer_ledger_entries_per_second":
            counters["transferLedgerEntriesWritten"]["per_second"],
        "transfer_ledger_capture_queue_minus_complete_delta":
            counters["transferLedgerCaptureQueueEntries"]["delta"] -
            counters["transferLedgerCaptureCompleteEntries"]["delta"],
        "transfer_ledger_playback_queue_minus_complete_delta":
            counters["transferLedgerPlaybackQueueEntries"]["delta"] -
            counters["transferLedgerPlaybackCompleteEntries"]["delta"],
        "transfer_ledger_output_read_frames_per_second":
            counters["transferLedgerOutputReadFrames"]["per_second"],
        "playback_payload_guard_checks_per_second":
            counters["playbackPayloadGuardChecks"]["per_second"],
        "playback_payload_guard_mismatches_per_second":
            counters["playbackPayloadGuardMismatches"]["per_second"],
        "hot_path_average_ticks": {
            "capture_handler": average_timing(counters, "hotPathCaptureHandler"),
            "capture_decode": average_timing(counters, "hotPathCaptureDecode"),
            "capture_requeue": average_timing(counters, "hotPathCaptureRequeue"),
            "playback_queue": average_timing(counters, "hotPathPlaybackQueue"),
            "playback_fill": average_timing(counters, "hotPathPlaybackFill"),
            "playback_enqueue": average_timing(counters, "hotPathPlaybackEnqueue"),
            "playback_completion": average_timing(counters, "hotPathPlaybackCompletion"),
        },
        "capture_transaction_ratio_denominator": capture_ratio_denominator,
        "capture_expected_transactions_per_capture_transfer": (
            capture_expected_delta / capture_ratio_delta
            if math.isfinite(capture_expected_delta) and capture_ratio_delta > 0 else math.nan
        ),
        "capture_classified_transactions_per_capture_transfer": (
            (capture_expected_delta + capture_zero_delta + counters["captureOtherByteCountTransactions"]["delta"]) /
            capture_ratio_delta
            if (math.isfinite(capture_expected_delta) and
                math.isfinite(capture_zero_delta) and
                math.isfinite(counters["captureOtherByteCountTransactions"]["delta"]) and
                capture_ratio_delta > 0) else math.nan
        ),
        "capture_transaction_errors_per_capture_transfer": capture_error_per_transfer,
        "capture_transaction_errors_per_raw_capture_transfer": capture_error_per_raw_transfer,
        "capture_transaction_errors_per_sampled_capture_transfer": (
            capture_errors_delta / capture_sampled_delta
            if math.isfinite(capture_errors_delta) and capture_sampled_delta > 0 else math.nan
        ),
        "capture_status_failures_per_capture_transfer": (
            counters["captureStatusFailures"]["delta"] / capture_ratio_delta
            if math.isfinite(counters["captureStatusFailures"]["delta"]) and capture_ratio_delta > 0 else math.nan
        ),
        "capture_zero_complete_per_capture_transfer": (
            counters["captureZeroCompleteTransactions"]["delta"] / capture_ratio_delta
            if math.isfinite(counters["captureZeroCompleteTransactions"]["delta"]) and capture_ratio_delta > 0 else math.nan
        ),
        "capture_zero_complete_per_raw_capture_transfer": (
            counters["captureZeroCompleteTransactions"]["delta"] / capture_tx_delta
            if math.isfinite(counters["captureZeroCompleteTransactions"]["delta"]) and capture_tx_delta > 0 else math.nan
        ),
        "capture_zero_complete_per_sampled_capture_transfer": (
            counters["captureZeroCompleteTransactions"]["delta"] / capture_sampled_delta
            if math.isfinite(counters["captureZeroCompleteTransactions"]["delta"]) and capture_sampled_delta > 0 else math.nan
        ),
        "capture_other_size_per_capture_transfer": (
            counters["captureOtherByteCountTransactions"]["delta"] / capture_ratio_delta
            if math.isfinite(counters["captureOtherByteCountTransactions"]["delta"]) and capture_ratio_delta > 0 else math.nan
        ),
        "capture_short_per_capture_transfer": (
            counters["captureShortTransfers"]["delta"] / capture_ratio_delta
            if math.isfinite(counters["captureShortTransfers"]["delta"]) and capture_ratio_delta > 0 else math.nan
        ),
        "filtered_capture_per_capture_transfer": (
            counters["filteredCaptureTransactions"]["delta"] / capture_ratio_delta
            if math.isfinite(counters["filteredCaptureTransactions"]["delta"]) and capture_ratio_delta > 0 else math.nan
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
