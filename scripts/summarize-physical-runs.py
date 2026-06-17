#!/usr/bin/env python3
"""Summarize physical OpenA8DJ run artifacts without touching hardware."""

from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path
from typing import Any


DIRECT_DIAG_RE = re.compile(r"direct_diag\s+label=(?P<label>\S+)\s+(?P<body>.*)$")


def load_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def parse_key_values(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for part in text.split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        out[key] = value
    return out


def parse_summary(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except FileNotFoundError:
        return out
    for line in lines:
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        out[key.strip()] = value.strip()
    return out


def parse_latest_direct_diag(path: Path) -> dict[str, str]:
    latest: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except FileNotFoundError:
        return latest
    for line in lines:
        match = DIRECT_DIAG_RE.search(line)
        if not match:
            continue
        values = parse_key_values(match.group("body"))
        values["direct_diag_label"] = match.group("label")
        latest = values
    return latest


def num(value: Any) -> Any:
    if value is None:
        return ""
    if isinstance(value, (int, float, str)):
        return value
    return ""


def row_for_run(run_dir: Path, root: Path) -> dict[str, Any]:
    summary = parse_summary(run_dir / "summary.txt")
    metrics = load_json(run_dir / "metrics.json")
    latency = load_json(run_dir / "physical-latency.json")
    marker = load_json(run_dir / "marker-peak-summary.json")
    diag = parse_latest_direct_diag(run_dir / "play.log")

    row: dict[str, Any] = {
        "run_dir": str(run_dir.relative_to(root)),
        "kind": run_dir.parent.name,
        "verdict": summary.get("DIRECT_USB_SOUNDCHECK")
        or summary.get("SOUNDCHECK")
        or metrics.get("verdict", "")
        or metrics.get("result", ""),
        "pair": summary.get("pair", ""),
        "lead_frames": summary.get("lead_frames", ""),
        "usb_diagnostics": summary.get("usb_diagnostics", ""),
        "quality_alignment_score": num(
            metrics.get("quality_alignment_score", summary.get("quality_alignment_score", ""))
        ),
        "snr_db_min": num(summary.get("snr_db_min", metrics.get("snr_db_min", ""))),
        "mid_band_residual_ratio": num(metrics.get("mid_band_residual_ratio", "")),
        "high_band_residual_ratio": num(metrics.get("high_band_residual_ratio", "")),
        "lag_jumps_gt_2_frames": num(
            metrics.get("lag_jumps_gt_2_frames", summary.get("lag_jumps_gt_2_frames", ""))
        ),
        "capture_clipped_frames": num(
            metrics.get("capture_clipped_frames", summary.get("capture_clipped_frames", ""))
        ),
        "first_energy_seconds": num(latency.get("first_energy_seconds", "")),
        "best_correlation": num(latency.get("best_correlation", "")),
        "aligned_snr_db": num(latency.get("aligned_snr_db", "")),
        "linear_fit_snr_db": num(latency.get("linear_fit_snr_db", "")),
        "marker_offset_mean_seconds": num(marker.get("offset_mean_seconds", "")),
        "marker_offset_std_seconds": num(marker.get("offset_std_seconds", "")),
        "marker_readiness_result": marker.get("readiness_result", marker.get("result", "")),
        "control": diag.get("control", ""),
        "audio_reset_rate": diag.get("audio_reset_rate", ""),
        "audio_reset_depth": diag.get("audio_reset_depth", ""),
        "audio_reset_bpp": diag.get("audio_reset_bpp", ""),
        "audio_reset_ok": diag.get("audio_reset_ok", ""),
        "audio_stream_rate": diag.get("audio_stream_rate", ""),
        "audio_stream_depth": diag.get("audio_stream_depth", ""),
        "audio_stream_bpp": diag.get("audio_stream_bpp", ""),
        "audio_stream_ok": diag.get("audio_stream_ok", ""),
        "output_byte": diag.get("output_byte", ""),
        "select_alt0_before_alt1": diag.get("select_alt0_before_alt1", ""),
        "startup_silence": diag.get("startup_silence", ""),
        "queue_failures": diag.get("queue_failures", ""),
        "qfail_last": diag.get("qfail_last", ""),
        "capture_transfers": diag.get("capture_transfers", ""),
        "playback_transfers": diag.get("playback_transfers", ""),
        "frames_written": diag.get("frames_written", ""),
        "frames_read": diag.get("frames_read", ""),
        "timeline_resets": diag.get("timeline_resets", ""),
        "underruns": diag.get("underruns", ""),
        "active_underruns": diag.get("active_underruns", ""),
    }
    return row


def discover_runs(root: Path) -> list[Path]:
    bases = [
        root / "local-analysis" / "direct-usb-latency-marker",
        root / "local-analysis" / "direct-usb-soundcheck",
        root / "local-analysis" / "soundcheck",
    ]
    runs: list[Path] = []
    for base in bases:
        if not base.exists():
            continue
        for child in sorted(base.iterdir()):
            if not child.is_dir():
                continue
            if any((child / name).exists() for name in ("summary.txt", "metrics.json", "play.log")):
                runs.append(child)
    return runs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--jsonl-out", type=Path, required=True)
    parser.add_argument("--csv-out", type=Path, default=None)
    args = parser.parse_args()

    root = args.root.resolve()
    rows = [row_for_run(run, root) for run in discover_runs(root)]
    args.jsonl_out.parent.mkdir(parents=True, exist_ok=True)
    with args.jsonl_out.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    if args.csv_out is not None:
        args.csv_out.parent.mkdir(parents=True, exist_ok=True)
        fieldnames = list(rows[0].keys()) if rows else ["run_dir"]
        with args.csv_out.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)

    print(json.dumps({"result": "PASS", "rows": len(rows), "jsonl": str(args.jsonl_out)}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
