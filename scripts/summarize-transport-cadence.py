#!/usr/bin/env python3
"""Summarize physical transport cadence evidence without touching hardware.

The soundcheck quality metrics alone are not enough to compare candidates:
several historical OpenA8DJ runs differ primarily by the USB isochronous
cadence they were built with. This script extracts the observable transport
family from existing run artifacts and joins it with quality and CPU metrics.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from pathlib import Path
from typing import Any


KEY_VALUE_RE = re.compile(r"^([A-Za-z0-9_.-]+)=(.*)$")
EXPECTED_TRANSFER_RE = re.compile(r"cadence-outliers:\s+expected-transfer=([0-9]+)")
PLAYBACK_QUEUE_RE = re.compile(r"playback-queue:\s+.*?/ target ([0-9]+)")
CAPTURE_LINE_RE = re.compile(
    r"capture:\s+transfers=([0-9]+)\s+tx=([0-9]+)\s+bytes=([0-9]+)\s+"
    r"failed=([0-9]+)\s+short=([0-9]+)\s+filtered=([0-9]+)\s+qfail=([0-9]+)"
)
PLAYBACK_LINE_RE = re.compile(
    r"playback:\s+(?:submitted=([0-9]+)\s+)?(?:transfers=)?([0-9]+)?\s*"
    r"completed=([0-9]+)\s+tx=([0-9]+)\s+bytes=([0-9]+)\s+"
    r"failed=([0-9]+)\s+short=([0-9]+)\s+qfail=([0-9]+)"
)


def load_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def read_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except FileNotFoundError:
        return values
    for line in lines:
        match = KEY_VALUE_RE.match(line.strip())
        if match:
            values[match.group(1)] = match.group(2).strip()
    return values


def parse_stream_text(path: Path) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return {}

    out: dict[str, Any] = {}
    if match := EXPECTED_TRANSFER_RE.search(text):
        ticks = int(match.group(1))
        out["cadence_expected_transfer_ticks"] = ticks
        # Existing traces in this repo use a 24 MHz mach timebase, so a
        # full-speed USB frame is 3000 ticks. Keep this as an inference, not a
        # compile-time truth, because old artifacts do not store HAL_CFLAGS.
        out["inferred_iso_frames"] = ticks / 3000.0
    if match := PLAYBACK_QUEUE_RE.search(text):
        out["playback_queue_target"] = int(match.group(1))
    if match := CAPTURE_LINE_RE.search(text):
        out.update(
            {
                "stream_capture_transfers": int(match.group(1)),
                "stream_capture_transactions": int(match.group(2)),
                "stream_capture_bytes": int(match.group(3)),
                "stream_capture_failed": int(match.group(4)),
                "stream_capture_short": int(match.group(5)),
                "stream_capture_filtered": int(match.group(6)),
                "stream_capture_queue_failures": int(match.group(7)),
            }
        )
    if match := PLAYBACK_LINE_RE.search(text):
        submitted = match.group(1)
        transfer_or_completed = match.group(2)
        completed = match.group(3)
        out.update(
            {
                "stream_playback_submitted": int(submitted) if submitted is not None else None,
                "stream_playback_transfers": int(transfer_or_completed)
                if transfer_or_completed is not None
                else int(completed),
                "stream_playback_completed": int(completed),
                "stream_playback_transactions": int(match.group(4)),
                "stream_playback_bytes": int(match.group(5)),
                "stream_playback_failed": int(match.group(6)),
                "stream_playback_short": int(match.group(7)),
                "stream_playback_queue_failures": int(match.group(8)),
            }
        )
    out.update(read_key_values(path))
    return out


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, int(fraction * (len(ordered) - 1)))
    return ordered[index]


def cpu_summary(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {}
    rows = list(csv.DictReader(path.open("r", encoding="utf-8", errors="replace"), delimiter="\t"))
    out: dict[str, Any] = {"cpu_samples": len(rows)}
    for column in ("opena8dj_driver", "coreaudiod", "total_audio_ui"):
        values: list[float] = []
        for row in rows:
            try:
                values.append(float(row.get(column, "") or 0.0))
            except ValueError:
                pass
        out[f"{column}_p50"] = percentile(values, 0.50)
        out[f"{column}_p95"] = percentile(values, 0.95)
        out[f"{column}_max"] = max(values) if values else None
    return out


def as_float(value: Any) -> float | None:
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def run_row(run_dir: Path, root: Path) -> dict[str, Any]:
    metrics = load_json(run_dir / "metrics.json")
    stream_after = parse_stream_text(run_dir / "stream-stats-after.txt")
    row: dict[str, Any] = {
        "run_dir": str(run_dir.relative_to(root)),
        "verdict": metrics.get("verdict", ""),
        "quality_alignment_score": as_float(metrics.get("quality_alignment_score")),
        "snr_db_min": min(
            value
            for value in (
                as_float(metrics.get("left_snr_db")),
                as_float(metrics.get("right_snr_db")),
            )
            if value is not None
        )
        if as_float(metrics.get("left_snr_db")) is not None
        and as_float(metrics.get("right_snr_db")) is not None
        else None,
        "mid_band_residual_ratio": as_float(metrics.get("mid_band_residual_ratio")),
        "high_band_residual_ratio": as_float(metrics.get("high_band_residual_ratio")),
        "quiet_mid_band_noise_dbfs": as_float(metrics.get("quiet_mid_band_noise_dbfs")),
        "lag_jumps_gt_2_frames": as_float(metrics.get("lag_jumps_gt_2_frames")),
        "capture_clipped_frames": as_float(metrics.get("capture_clipped_frames")),
    }
    row.update(stream_after)
    row.update(cpu_summary(run_dir / "cpu-profile.tsv"))
    return row


def discover_runs(root: Path) -> list[Path]:
    base = root / "local-analysis" / "soundcheck"
    if not base.is_dir():
        return []
    return sorted(
        path
        for path in base.iterdir()
        if path.is_dir() and (path / "metrics.json").is_file()
    )


def family_key(row: dict[str, Any]) -> str:
    iso = row.get("inferred_iso_frames")
    queue = row.get("playback_queue_target")
    if isinstance(iso, (int, float)) and isinstance(queue, int):
        return f"iso{iso:g}_q{queue}"
    if isinstance(iso, (int, float)):
        return f"iso{iso:g}_q?"
    return "unknown"


def summarize_families(rows: list[dict[str, Any]]) -> dict[str, Any]:
    families: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        families.setdefault(family_key(row), []).append(row)
    out: dict[str, Any] = {}
    for key, group in sorted(families.items()):
        qualities = [row["quality_alignment_score"] for row in group if row.get("quality_alignment_score") is not None]
        driver = [row["opena8dj_driver_p95"] for row in group if row.get("opena8dj_driver_p95") is not None]
        lags = [row["lag_jumps_gt_2_frames"] for row in group if row.get("lag_jumps_gt_2_frames") is not None]
        out[key] = {
            "runs": len(group),
            "best_quality_alignment_score": max(qualities) if qualities else None,
            "median_quality_alignment_score": percentile(qualities, 0.50),
            "min_lag_jumps_gt_2_frames": min(lags) if lags else None,
            "median_driver_cpu_p95": percentile(driver, 0.50),
            "min_driver_cpu_p95": min(driver) if driver else None,
            "best_run": max(
                group,
                key=lambda item: item.get("quality_alignment_score")
                if item.get("quality_alignment_score") is not None
                else -1.0,
            ).get("run_dir"),
        }
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--json-out", type=Path, required=True)
    parser.add_argument("--csv-out", type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    rows = [run_row(run, root) for run in discover_runs(root)]
    payload = {
        "schema": "opena8djcpp.transport-cadence-summary.v1",
        "result": "PASS",
        "run_count": len(rows),
        "families": summarize_families(rows),
        "runs": rows,
    }
    args.json_out.parent.mkdir(parents=True, exist_ok=True)
    args.json_out.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if args.csv_out is not None:
        args.csv_out.parent.mkdir(parents=True, exist_ok=True)
        fieldnames: list[str] = []
        for row in rows:
            for key in row:
                if key not in fieldnames:
                    fieldnames.append(key)
        with args.csv_out.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)

    print(json.dumps({"result": "PASS", "runs": len(rows), "json": str(args.json_out)}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
