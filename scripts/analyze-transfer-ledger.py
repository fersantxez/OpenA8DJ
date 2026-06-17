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
        if "=" in line and "\t" not in line and header is None:
            key, value = line.split("=", 1)
            metadata[key.strip()] = value.strip()
            continue
        columns = raw_line.split("\t")
        if header is None:
            header = columns
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


def summarize(path: Path) -> dict:
    metadata, rows = read_ledger(path)
    counts = event_counts(rows)
    gaps = sequence_gaps(rows)
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
    failed_transaction_rows = [
        {
            "sequence": to_int(row.get("sequence", "0")),
            "event": row.get("event", ""),
            "failedTransactions": to_int(row.get("failedTransactions", "0")),
            "shortTransactions": to_int(row.get("shortTransactions", "0")),
        }
        for row in rows
        if to_int(row.get("failedTransactions", "0")) > 0 or
        to_int(row.get("shortTransactions", "0")) > 0
    ]
    output_active_underrun_frames = sum(
        to_int(row.get("outputActiveUnderrunFrames", "0")) for row in rows
    )
    output_elastic_drop_frames = sum(
        to_int(row.get("outputElasticDropFrames", "0")) for row in rows
    )
    output_elastic_replay_frames = sum(
        to_int(row.get("outputElasticReplayFrames", "0")) for row in rows
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
    if non_success_status_rows:
        failures.append("non_success_status")
    if failed_transaction_rows:
        failures.append("failed_or_short_transactions")
    if output_active_underrun_frames > 0:
        failures.append("active_underrun_frames")
    if first_frame_regressions(rows) > 0:
        failures.append("playback_first_frame_regressions")
    if not math.isfinite(balance_delta) or abs(balance_delta) > 1:
        failures.append("playback_queue_complete_imbalance")
    return {
        "schema": "opena8djcpp.transfer-ledger-analysis.v1",
        "path": str(path),
        "metadata": metadata,
        "row_count": len(rows),
        "event_counts": counts,
        "sequence_gaps": gaps[:16],
        "sequence_gap_count": len(gaps),
        "non_success_status_count": len(non_success_status_rows),
        "non_success_status_rows": non_success_status_rows[:16],
        "failed_transaction_row_count": len(failed_transaction_rows),
        "failed_transaction_rows": failed_transaction_rows[:16],
        "playback_queue_complete_delta": balance_delta,
        "playback_first_frame_regressions": first_frame_regressions(rows),
        "max_in_flight": max_in_flight,
        "output_active_underrun_frames": output_active_underrun_frames,
        "output_elastic_drop_frames": output_elastic_drop_frames,
        "output_elastic_replay_frames": output_elastic_replay_frames,
        "host_delta": {
            event: host_delta_stats(rows, event) for event in sorted(EVENTS)
        },
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
