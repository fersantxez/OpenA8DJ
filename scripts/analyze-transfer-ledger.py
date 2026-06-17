#!/usr/bin/env python3
"""Analyze OpenA8DJ transfer-ledger TSV evidence.

Offline-only: reads files produced by `opena8dj-control transfer-ledger` or
`scripts/run-soundcheck --stream-stats-snapshots`. It does not open audio
devices, query CoreAudio, touch USB, install drivers, or mutate system state.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


EVENTS = {
    "capture_queue",
    "capture_complete",
    "playback_queue",
    "playback_complete",
}

STOP_ABORT_STATUS = "0xe00002eb"
STOP_ABORT_SEQUENCE_WINDOW = 32


def to_int(value: str, default: int = 0) -> int:
    text = value.strip()
    if not text:
        return default
    try:
        return int(text, 16) if text.lower().startswith("0x") else int(text)
    except ValueError:
        return default


def read_ledger(path: Path) -> tuple[dict[str, str], list[dict[str, str]]]:
    metadata: dict[str, str] = {}
    rows: list[dict[str, str]] = []
    header: list[str] | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("#"):
            continue
        if line == "OpenA8DJ transfer ledger":
            continue
        if "=" in line and "\t" not in line:
            key, value = line.split("=", 1)
            metadata[key.strip()] = value.strip()
            continue
        columns = raw_line.split("\t")
        if columns and columns[0] == "sequence":
            header = columns
            continue
        if header is None:
            continue
        row = {name: columns[index] if index < len(columns) else ""
               for index, name in enumerate(header)}
        rows.append(row)
    return metadata, rows


def event_counts(rows: list[dict[str, str]]) -> dict[str, int]:
    counts = {event: 0 for event in sorted(EVENTS)}
    counts["unknown"] = 0
    for row in rows:
        event = row.get("event", "")
        if event in counts:
            counts[event] += 1
        else:
            counts["unknown"] += 1
    return counts


def sequence_gaps(rows: list[dict[str, str]]) -> list[dict[str, int]]:
    gaps = []
    previous = None
    for row in rows:
        sequence = to_int(row.get("sequence", "0"))
        if previous is not None and sequence != previous + 1:
            gaps.append({"previous": previous, "current": sequence, "gap": sequence - previous - 1})
        previous = sequence
    return gaps


def host_delta_stats(rows: list[dict[str, str]], event: str) -> dict[str, float | int]:
    values = [to_int(row.get("hostTime", "0")) for row in rows if row.get("event") == event]
    deltas = [b - a for a, b in zip(values, values[1:]) if b >= a]
    if not deltas:
        return {"samples": 0, "min": 0, "max": 0, "avg": 0.0}
    return {
        "samples": len(deltas),
        "min": min(deltas),
        "max": max(deltas),
        "avg": sum(deltas) / len(deltas),
    }


def first_frame_regressions(rows: list[dict[str, str]]) -> int:
    previous = None
    regressions = 0
    for row in rows:
        if row.get("event") != "playback_queue":
            continue
        frame = to_int(row.get("firstFrameNumber", "0"))
        if frame == 0:
            continue
        if previous is not None and frame <= previous:
            regressions += 1
        previous = frame
    return regressions


def split_stop_abort_status_rows(
    rows: list[dict[str, str]],
    non_success_status_rows: list[dict[str, str]],
) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    max_sequence = max((to_int(row.get("sequence", "0")) for row in rows), default=0)
    stop_abort_rows = []
    real_status_rows = []
    for row in non_success_status_rows:
        sequence = to_int(str(row.get("sequence", "0")))
        status = str(row.get("status", "")).lower()
        event = str(row.get("event", ""))
        final_window = max_sequence > 0 and sequence >= max_sequence - STOP_ABORT_SEQUENCE_WINDOW
        if status == STOP_ABORT_STATUS and event.endswith("_complete") and final_window:
            stop_abort_rows.append(row)
        else:
            real_status_rows.append(row)
    return real_status_rows, stop_abort_rows


def summarize(path: Path) -> dict:
    metadata, rows = read_ledger(path)
    counts = event_counts(rows)
    gaps = sequence_gaps(rows)
    metadata_latest = to_int(metadata.get("latestSequence", "0"))
    metadata_overwritten = to_int(metadata.get("overwritten", "0"))
    metadata_start = to_int(metadata.get("startSequence", "0"))
    metadata_count = to_int(metadata.get("count", "0"))
    expected_count = (
        metadata_latest - metadata_start + 1
        if metadata_latest >= metadata_start and metadata_start > 0 else 0
    )
    non_success_status_rows = [
        {
            "sequence": to_int(row.get("sequence", "0")),
            "event": row.get("event", ""),
            "status": row.get("status", ""),
        }
        for row in rows
        if row.get("event", "").endswith("_complete") and
        row.get("status", "0x00000000") not in ("0x00000000", "0", "")
    ]
    real_non_success_status_rows, stop_abort_status_rows = split_stop_abort_status_rows(
        rows,
        non_success_status_rows,
    )
    playback_failed_transaction_rows = [
        {
            "sequence": to_int(row.get("sequence", "0")),
            "event": row.get("event", ""),
            "failedTransactions": to_int(row.get("failedTransactions", "0")),
            "shortTransactions": to_int(row.get("shortTransactions", "0")),
        }
        for row in rows
        if row.get("event", "") == "playback_complete" and
        (to_int(row.get("failedTransactions", "0")) > 0 or
         to_int(row.get("shortTransactions", "0")) > 0)
    ]
    capture_failed_transaction_rows = [
        {
            "sequence": to_int(row.get("sequence", "0")),
            "failedTransactions": to_int(row.get("failedTransactions", "0")),
            "shortTransactions": to_int(row.get("shortTransactions", "0")),
        }
        for row in rows
        if row.get("event", "") == "capture_complete" and
        to_int(row.get("failedTransactions", "0")) > 0
    ]
    short_transaction_rows = [
        {
            "sequence": to_int(row.get("sequence", "0")),
            "event": row.get("event", ""),
            "shortTransactions": to_int(row.get("shortTransactions", "0")),
        }
        for row in rows
        if
        to_int(row.get("shortTransactions", "0")) > 0
    ]
    output_active_underrun_frames = max(
        (to_int(row.get("outputActiveUnderrunFrames", "0")) for row in rows),
        default=0,
    )
    output_elastic_drop_frames = max(
        (to_int(row.get("outputElasticDropFrames", "0")) for row in rows),
        default=0,
    )
    output_elastic_replay_frames = max(
        (to_int(row.get("outputElasticReplayFrames", "0")) for row in rows),
        default=0,
    )
    max_in_flight = max((to_int(row.get("inFlight", "0")) for row in rows), default=0)
    queue_count = counts.get("playback_queue", 0)
    complete_count = counts.get("playback_complete", 0)
    balance_delta = queue_count - complete_count
    failures = []
    if not rows:
        failures.append("no_ledger_rows")
    if counts.get("unknown", 0) > 0:
        failures.append("unknown_events")
    if len(gaps) > 0:
        failures.append("sequence_gaps")
    if metadata_overwritten > 0:
        failures.append("ledger_overwritten")
    if metadata_count and metadata_count != len(rows):
        failures.append("declared_count_mismatch")
    if expected_count and expected_count != len(rows):
        failures.append("coverage_count_mismatch")
    if real_non_success_status_rows:
        failures.append("non_success_status")
    if playback_failed_transaction_rows:
        failures.append("playback_failed_or_short_transactions")
    if short_transaction_rows:
        failures.append("short_transactions")
    if first_frame_regressions(rows) > 0:
        failures.append("playback_first_frame_regressions")
    if metadata_overwritten == 0 and metadata_count and (
        not math.isfinite(balance_delta) or abs(balance_delta) > 1
    ):
        failures.append("playback_queue_complete_imbalance")
    warnings = []
    if stop_abort_status_rows:
        warnings.append("final_stop_abort_status_observed")
    if output_active_underrun_frames > 0:
        warnings.append("output_active_underrun_frames_observed")
    if capture_failed_transaction_rows:
        warnings.append("capture_failed_transactions_observed")
    return {
        "schema": "opena8djcpp.transfer-ledger-analysis.v1",
        "path": str(path),
        "metadata": metadata,
        "coverage": {
            "latest_sequence": metadata_latest,
            "overwritten": metadata_overwritten,
            "start_sequence": metadata_start,
            "declared_count": metadata_count,
            "expected_count_from_metadata": expected_count,
            "row_count": len(rows),
            "continuous": len(gaps) == 0 and (not expected_count or expected_count == len(rows)),
            "full_ring_window": metadata_overwritten == 0,
        },
        "row_count": len(rows),
        "event_counts": counts,
        "sequence_gaps": gaps[:16],
        "sequence_gap_count": len(gaps),
        "non_success_status_count": len(real_non_success_status_rows),
        "non_success_status_rows": real_non_success_status_rows[:16],
        "final_stop_abort_status_count": len(stop_abort_status_rows),
        "final_stop_abort_status_rows": stop_abort_status_rows[:16],
        "playback_failed_transaction_row_count": len(playback_failed_transaction_rows),
        "playback_failed_transaction_rows": playback_failed_transaction_rows[:16],
        "capture_failed_transaction_row_count": len(capture_failed_transaction_rows),
        "capture_failed_transaction_rows": capture_failed_transaction_rows[:16],
        "short_transaction_row_count": len(short_transaction_rows),
        "short_transaction_rows": short_transaction_rows[:16],
        "playback_queue_complete_delta": balance_delta,
        "playback_first_frame_regressions": first_frame_regressions(rows),
        "max_in_flight": max_in_flight,
        "output_active_underrun_frames": output_active_underrun_frames,
        "output_elastic_drop_frames": output_elastic_drop_frames,
        "output_elastic_replay_frames": output_elastic_replay_frames,
        "host_delta": {
            event: host_delta_stats(rows, event) for event in sorted(EVENTS)
        },
        "warnings": warnings,
        "failures": failures,
        "result": "PASS" if not failures else "FAIL",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ledger_tsv", type=Path)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()
    result = summarize(args.ledger_tsv)
    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
