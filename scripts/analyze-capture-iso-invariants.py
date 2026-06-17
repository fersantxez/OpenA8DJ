#!/usr/bin/env python3
"""Check capture ISO transaction invariants from existing stream-stats evidence.

This tool is offline-only. It parses stream stats already written by soundcheck
runs and never touches hardware, CoreAudio, USB, HAL install state, or external
worktrees.
"""

import argparse
import csv
import json
import math
import re
from pathlib import Path


KEY_VALUE_RE = re.compile(r"^([A-Za-z0-9_]+)=(.*)$")
CAPTURE_RE = re.compile(
    r"capture:\s+transfers=(?P<captureTransfers>\d+)\s+"
    r"tx=(?P<captureTransactions>\d+)\s+"
    r"bytes=(?P<captureBytes>\d+)\s+"
    r"failed=(?P<captureTransactionFailures>\d+)\s+"
    r"short=(?P<captureShortTransfers>\d+)\s+"
    r"filtered=(?P<filteredCaptureTransactions>\d+)\s+"
    r"qfail=(?P<captureQueueFailures>\d+)"
)
DETAIL_RE = re.compile(
    r"capture-detail:\s+status-failed=(?P<captureStatusFailures>\d+)\s+"
    r"zero-complete=(?P<captureZeroCompleteTransactions>\d+)\s+"
    r"expected=(?P<captureExpectedTransactions>\d+)\s+"
    r"other-size=(?P<captureOtherByteCountTransactions>\d+)"
)


FIELDS = [
    "captureTransfers",
    "playbackTransfers",
    "captureTransactions",
    "captureBytes",
    "captureTransactionFailures",
    "captureShortTransfers",
    "filteredCaptureTransactions",
    "captureStatusFailures",
    "captureZeroCompleteTransactions",
    "captureExpectedTransactions",
    "captureOtherByteCountTransactions",
]


def parse_number(value):
    if value is None or value == "":
        return math.nan
    try:
        return float(value)
    except ValueError:
        return math.nan


def finite(value):
    return math.isfinite(value)


def parse_key_value_text(path):
    values = {}
    if not path.is_file():
        return values
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = KEY_VALUE_RE.match(line.strip())
        if match:
            values[match.group(1)] = match.group(2)
            continue
        for regex in (CAPTURE_RE, DETAIL_RE):
            detail = regex.search(line)
            if detail:
                values.update(detail.groupdict())
    return values


def parse_tsv_last_ok(path):
    if not path.is_file():
        return {}
    rows = []
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            if row.get("status") == "OK":
                rows.append(row)
    return rows[-1] if rows else {}


def normalize_values(raw):
    values = {}
    aliases = {
        "captureTransfersCompleted": "captureTransfers",
        "playbackTransfersCompleted": "playbackTransfers",
        "captureTransactionErrors": "captureTransactionFailures",
    }
    for key, value in raw.items():
        canonical = aliases.get(key, key)
        if canonical in FIELDS:
            values[canonical] = parse_number(value)
    return values


def infer_iso_frames(run_dir):
    text = " ".join(part.lower() for part in run_dir.parts)
    match = re.search(r"iso(\d+)", text)
    if match:
        value = int(match.group(1))
        if value > 0:
            return value
    return 5


def analyze_run(run_dir, iso_frames):
    after_values = normalize_values(parse_key_value_text(run_dir / "stream-stats-after.txt"))
    during_values = normalize_values(parse_tsv_last_ok(run_dir / "stream-stats-during.tsv"))
    values = after_values or during_values
    source = "stream-stats-after.txt" if after_values else "stream-stats-during.tsv"

    transfers = values.get("captureTransfers", math.nan)
    playback_transfers = values.get("playbackTransfers", math.nan)
    expected = values.get("captureExpectedTransactions", math.nan)
    zero = values.get("captureZeroCompleteTransactions", math.nan)
    status = values.get("captureStatusFailures", math.nan)
    other = values.get("captureOtherByteCountTransactions", math.nan)
    short = values.get("captureShortTransfers", math.nan)
    filtered = values.get("filteredCaptureTransactions", math.nan)
    failed = values.get("captureTransactionFailures", math.nan)
    transactions = values.get("captureTransactions", math.nan)
    bytes_total = values.get("captureBytes", math.nan)

    detail_values = [expected, zero, status, other, short, filtered]
    detail_complete = all(finite(value) for value in detail_values)
    classified = expected + zero + status + other if detail_complete else math.nan
    iso_frames_source = "argument_or_path"
    if finite(classified) and finite(transfers) and transfers > 0:
        observed_slots = classified / transfers
        rounded_slots = int(round(observed_slots))
        if rounded_slots > 0 and abs(observed_slots - rounded_slots) <= 0.5:
            if iso_frames <= 0 or abs(observed_slots - iso_frames) > 0.5:
                iso_frames = rounded_slots
                iso_frames_source = "observed_classified_slots"
    total_expected = transfers * iso_frames if finite(transfers) else math.nan
    byte_per_expected = bytes_total / expected if finite(bytes_total) and finite(expected) and expected > 0 else math.nan
    zero_ratio = zero / transfers if finite(zero) and finite(transfers) and transfers > 0 else math.nan
    useful_ratio = expected / transfers if finite(expected) and finite(transfers) and transfers > 0 else math.nan
    aggregate_error_ratio = failed / transfers if finite(failed) and finite(transfers) and transfers > 0 else math.nan

    failures = []
    warnings = []
    unknowns = []
    if not values:
        failures.append("missing_stream_stats")
    if values:
        if not detail_complete:
            unknowns.append("missing_capture_detail_fields")
    if finite(total_expected) and finite(classified):
        classified_delta = total_expected - classified
        if classified_delta != 0:
            stop_transfer_gap = 1
            if finite(playback_transfers) and finite(transfers):
                stop_transfer_gap = max(1, int(abs(transfers - playback_transfers)))
            if 0 < classified_delta <= iso_frames * stop_transfer_gap:
                warnings.append("classified_transactions_missing_in_stop_transfer_gap")
            else:
                failures.append("classified_transactions_do_not_match_iso_frames")
    if finite(failed) and finite(zero) and finite(status) and failed != zero + status:
        failures.append("failed_not_equal_zero_plus_status")
    if finite(transactions) and finite(expected) and transactions != expected:
        failures.append("capture_transactions_not_equal_expected")
    if finite(filtered) and finite(other) and filtered != other:
        failures.append("filtered_not_equal_other_size")
    if finite(short) and short != 0:
        failures.append("short_transfers_present")
    if finite(other) and other != 0:
        failures.append("other_size_transfers_present")
    if finite(status) and status != 0:
        failures.append("status_failures_present")
    if finite(byte_per_expected) and abs(byte_per_expected - 352.0) > 0.001:
        failures.append("unexpected_bytes_per_expected_transaction")

    return {
        "run_dir": str(run_dir),
        "source": source,
        "iso_frames_per_transfer": iso_frames,
        "iso_frames_source": iso_frames_source,
        "result": "FAIL" if failures else ("UNKNOWN" if unknowns else "PASS"),
        "failures": failures,
        "warnings": warnings,
        "unknowns": unknowns,
        "capture_transfers": transfers,
        "playback_transfers": playback_transfers,
        "capture_expected_transactions": expected,
        "capture_zero_complete_transactions": zero,
        "capture_status_failures": status,
        "capture_other_size_transactions": other,
        "capture_short_transfers": short,
        "filtered_capture_transactions": filtered,
        "capture_transaction_failures": failed,
        "classified_transactions": classified if values else math.nan,
        "total_iso_slots": total_expected,
        "zero_complete_per_transfer": zero_ratio,
        "useful_transactions_per_transfer": useful_ratio,
        "aggregate_error_per_transfer": aggregate_error_ratio,
        "bytes_per_expected_transaction": byte_per_expected,
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
    parser.add_argument("paths", nargs="+", help="soundcheck directories")
    parser.add_argument("--iso-frames", type=int, help="override ISO frames per transfer")
    parser.add_argument("--json-out")
    args = parser.parse_args()

    runs = []
    for raw in args.paths:
        path = Path(raw)
        iso_frames = args.iso_frames if args.iso_frames is not None else infer_iso_frames(path)
        runs.append(analyze_run(path, iso_frames))

    result = {
        "schema": "opena8djcpp.capture-iso-invariants.v1",
        "result": (
            "FAIL" if any(run["result"] == "FAIL" for run in runs)
            else ("UNKNOWN" if any(run["result"] == "UNKNOWN" for run in runs) else "PASS")
        ),
        "run_count": len(runs),
        "runs": runs,
    }
    text = json.dumps(json_safe(result), indent=2, sort_keys=True, allow_nan=False)
    print(text)
    if args.json_out:
        Path(args.json_out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json_out).write_text(text + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
