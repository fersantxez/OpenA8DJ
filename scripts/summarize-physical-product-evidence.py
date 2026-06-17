#!/usr/bin/env python3
"""Summarize physical product evidence without touching audio hardware.

The promotion decision needs joint evidence, not a cherry-picked metric. This
script reads existing run directories and reports sound quality, CPU, inferred
transport family, and optional same-session C++ vs mainline comparisons.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from pathlib import Path
from typing import Any


EXPECTED_TRANSFER_RE = re.compile(r"cadence-outliers:\s+expected-transfer=([0-9]+)")
PLAYBACK_QUEUE_RE = re.compile(r"playback-queue:\s+.*?/ target ([0-9]+)")


THRESHOLDS = {
    "quality_alignment_score_min": 0.98,
    "snr_db_min": 35.0,
    "mid_band_residual_ratio_max": 1.36,
    "high_band_residual_ratio_max": 1.35,
    "quiet_mid_band_noise_dbfs_max": -58.0,
    "lag_jumps_gt_2_frames_max": 0,
    "click_outliers_max": 0,
    "capture_clipped_frames_max": 0,
    "driver_cpu_p95_max": 6.5,
    "coreaudiod_p95_max": 1.7,
    "fixture_degraded_quality_max": 0.50,
    "fixture_degraded_snr_max": 0.0,
}


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def as_float(value: Any) -> float | None:
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, int(fraction * (len(ordered) - 1)))
    return ordered[index]


def cpu_summary(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"cpu_profile_present": False}
    rows = list(csv.DictReader(path.open("r", encoding="utf-8", errors="replace"), delimiter="\t"))
    out: dict[str, Any] = {"cpu_profile_present": True, "cpu_samples": len(rows)}
    for column in ("opena8dj_driver", "coreaudiod", "total_audio_ui"):
        values: list[float] = []
        for row in rows:
            value = as_float(row.get(column))
            if value is not None:
                values.append(value)
        out[f"{column}_p50"] = percentile(values, 0.50)
        out[f"{column}_p95"] = percentile(values, 0.95)
        out[f"{column}_max"] = max(values) if values else None
    return out


def stream_family(path: Path) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return {"transport_family": "unknown"}
    iso_frames: float | None = None
    queue_target: int | None = None
    if match := EXPECTED_TRANSFER_RE.search(text):
        iso_frames = int(match.group(1)) / 3000.0
    if match := PLAYBACK_QUEUE_RE.search(text):
        queue_target = int(match.group(1))
    if iso_frames is not None and queue_target is not None:
        return {
            "transport_family": f"ISO{iso_frames:g}/q{queue_target}",
            "inferred_iso_frames": iso_frames,
            "playback_queue_target": queue_target,
        }
    if iso_frames is not None:
        return {
            "transport_family": f"ISO{iso_frames:g}/q?",
            "inferred_iso_frames": iso_frames,
            "playback_queue_target": None,
        }
    return {"transport_family": "unknown"}


def min_pair(metrics: dict[str, Any], left_key: str, right_key: str) -> float | None:
    values = [
        item
        for item in (as_float(metrics.get(left_key)), as_float(metrics.get(right_key)))
        if item is not None
    ]
    return min(values) if values else None


def max_pair(metrics: dict[str, Any], left_key: str, right_key: str) -> float | None:
    values = [
        item
        for item in (as_float(metrics.get(left_key)), as_float(metrics.get(right_key)))
        if item is not None
    ]
    return max(values) if values else None


def bool_gate(value: float | None, op: str, threshold: float) -> bool:
    if value is None:
        return False
    if op == ">=":
        return value >= threshold
    if op == "<=":
        return value <= threshold
    raise ValueError(op)


def run_row(run_dir: Path) -> dict[str, Any]:
    metrics = load_json(run_dir / "metrics.json")
    cpu = cpu_summary(run_dir / "cpu-profile.tsv")
    row: dict[str, Any] = {
        "run_dir": str(run_dir),
        "metrics_present": bool(metrics),
        "quality_alignment_score": as_float(metrics.get("quality_alignment_score")),
        "alignment_score": as_float(metrics.get("alignment_score")),
        "snr_db_min": min_pair(metrics, "left_snr_db", "right_snr_db"),
        "mid_band_residual_ratio": as_float(metrics.get("mid_band_residual_ratio")),
        "high_band_residual_ratio": as_float(metrics.get("high_band_residual_ratio")),
        "quiet_mid_band_noise_dbfs": as_float(metrics.get("quiet_mid_band_noise_dbfs")),
        "lag_jumps_gt_2_frames": as_float(metrics.get("lag_jumps_gt_2_frames")),
        "click_outliers": max_pair(metrics, "left_click_outliers", "right_click_outliers"),
        "window_click_outliers_max": as_float(metrics.get("window_click_outliers_max")),
        "capture_clipped_frames": as_float(metrics.get("capture_clipped_frames")),
    }
    if row["click_outliers"] is None:
        row["click_outliers"] = row["window_click_outliers_max"]
    row.update(cpu)
    row.update(stream_family(run_dir / "stream-stats-after.txt"))
    row["quality_gate_pass"] = all(
        (
            bool_gate(row["quality_alignment_score"], ">=", THRESHOLDS["quality_alignment_score_min"]),
            bool_gate(row["snr_db_min"], ">=", THRESHOLDS["snr_db_min"]),
            bool_gate(row["mid_band_residual_ratio"], "<=", THRESHOLDS["mid_band_residual_ratio_max"]),
            bool_gate(row["high_band_residual_ratio"], "<=", THRESHOLDS["high_band_residual_ratio_max"]),
            bool_gate(row["quiet_mid_band_noise_dbfs"], "<=", THRESHOLDS["quiet_mid_band_noise_dbfs_max"]),
            bool_gate(row["lag_jumps_gt_2_frames"], "<=", THRESHOLDS["lag_jumps_gt_2_frames_max"]),
            bool_gate(row["click_outliers"], "<=", THRESHOLDS["click_outliers_max"]),
            bool_gate(row["capture_clipped_frames"], "<=", THRESHOLDS["capture_clipped_frames_max"]),
        )
    )
    row["cpu_gate_pass"] = all(
        (
            bool_gate(row.get("opena8dj_driver_p95"), "<=", THRESHOLDS["driver_cpu_p95_max"]),
            bool_gate(row.get("coreaudiod_p95"), "<=", THRESHOLDS["coreaudiod_p95_max"]),
        )
    )
    row["fixture_degraded_candidate"] = all(
        (
            bool_gate(row["quality_alignment_score"], "<=", THRESHOLDS["fixture_degraded_quality_max"]),
            bool_gate(row["snr_db_min"], "<=", THRESHOLDS["fixture_degraded_snr_max"]),
        )
    )
    return row


def load_failure_modes(path: Path | None) -> dict[str, list[str]]:
    if path is None:
        return {}
    payload = load_json(path)
    out: dict[str, list[str]] = {}
    for row in payload.get("rows", []):
        run_dir = row.get("run_dir")
        if isinstance(run_dir, str):
            out[run_dir] = [str(item) for item in row.get("classification", [])]
    return out


def comparison(cpp: dict[str, Any], mainline: dict[str, Any]) -> dict[str, Any]:
    def delta(key: str) -> float | None:
        left = as_float(cpp.get(key))
        right = as_float(mainline.get(key))
        return left - right if left is not None and right is not None else None

    return {
        "cpp_quality_beats_mainline": bool_gate(delta("quality_alignment_score"), ">=", 0.0),
        "cpp_snr_beats_mainline": bool_gate(delta("snr_db_min"), ">=", 0.0),
        "cpp_mid_residual_beats_mainline": bool_gate(delta("mid_band_residual_ratio"), "<=", 0.0),
        "cpp_high_residual_beats_mainline": bool_gate(delta("high_band_residual_ratio"), "<=", 0.0),
        "cpp_lag_jumps_beats_mainline": bool_gate(delta("lag_jumps_gt_2_frames"), "<=", 0.0),
        "cpp_driver_cpu_beats_mainline": bool_gate(delta("opena8dj_driver_p95"), "<=", 0.0),
        "cpp_coreaudiod_cpu_beats_mainline": bool_gate(delta("coreaudiod_p95"), "<=", 0.0),
        "quality_alignment_score_cpp_minus_mainline": delta("quality_alignment_score"),
        "snr_db_min_cpp_minus_mainline": delta("snr_db_min"),
        "mid_band_residual_ratio_cpp_minus_mainline": delta("mid_band_residual_ratio"),
        "high_band_residual_ratio_cpp_minus_mainline": delta("high_band_residual_ratio"),
        "lag_jumps_gt_2_frames_cpp_minus_mainline": delta("lag_jumps_gt_2_frames"),
        "driver_cpu_p95_cpp_minus_mainline": delta("opena8dj_driver_p95"),
        "coreaudiod_p95_cpp_minus_mainline": delta("coreaudiod_p95"),
    }


def same_session_pair(cpp_rows: list[dict[str, Any]],
                      mainline_rows: list[dict[str, Any]]) -> tuple[dict[str, Any] | None,
                                                                    dict[str, Any] | None]:
    candidates: list[tuple[dict[str, Any], dict[str, Any]]] = []
    for cpp in cpp_rows:
        cpp_parent = Path(str(cpp["run_dir"])).parent
        for mainline in mainline_rows:
            if cpp_parent == Path(str(mainline["run_dir"])).parent:
                candidates.append((cpp, mainline))
    if not candidates:
        return None, None
    return max(
        candidates,
        key=lambda pair: pair[0].get("quality_alignment_score")
        if pair[0].get("quality_alignment_score") is not None
        else -1.0,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp-run", type=Path, action="append", default=[])
    parser.add_argument("--mainline-run", type=Path, action="append", default=[])
    parser.add_argument("--failure-modes-json", type=Path)
    parser.add_argument("--json-out", type=Path, required=True)
    args = parser.parse_args()

    failure_modes = load_failure_modes(args.failure_modes_json)
    cpp_rows = [run_row(path) for path in args.cpp_run]
    mainline_rows = [run_row(path) for path in args.mainline_run]
    for row in cpp_rows + mainline_rows:
        row["failure_modes"] = failure_modes.get(row["run_dir"], [])

    best_cpp = max(
        cpp_rows,
        key=lambda row: row.get("quality_alignment_score")
        if row.get("quality_alignment_score") is not None
        else -1.0,
        default=None,
    )
    best_mainline = max(
        mainline_rows,
        key=lambda row: row.get("quality_alignment_score")
        if row.get("quality_alignment_score") is not None
        else -1.0,
        default=None,
    )
    same_session_cpp, same_session_mainline = same_session_pair(cpp_rows, mainline_rows)
    same_session = comparison(same_session_cpp, same_session_mainline) \
        if same_session_cpp and same_session_mainline else None
    fixture_degraded = False
    if same_session_cpp and same_session_mainline:
        fixture_degraded = bool(same_session_cpp["fixture_degraded_candidate"] and
                                same_session_mainline["fixture_degraded_candidate"])

    promotion_blockers: list[str] = []
    if not best_cpp:
        promotion_blockers.append("missing_cpp_physical_evidence")
    elif not best_cpp["quality_gate_pass"]:
        promotion_blockers.append("cpp_physical_quality_gate_failed")
    if best_cpp and not best_cpp["cpu_gate_pass"]:
        promotion_blockers.append("cpp_runtime_cpu_gate_failed")
    if same_session is not None:
        if not same_session["cpp_quality_beats_mainline"]:
            promotion_blockers.append("cpp_quality_does_not_beat_mainline_same_session")
        if not same_session["cpp_driver_cpu_beats_mainline"]:
            promotion_blockers.append("cpp_driver_cpu_does_not_beat_mainline_same_session")
        if not same_session["cpp_coreaudiod_cpu_beats_mainline"]:
            promotion_blockers.append("cpp_coreaudiod_cpu_does_not_beat_mainline_same_session")
    if fixture_degraded:
        promotion_blockers.append("same_session_fixture_degraded_for_both_candidates")

    payload = {
        "schema": "opena8djcpp.physical-product-evidence-summary.v1",
        "result": "FAIL" if promotion_blockers else "PASS",
        "branch_promotion_allowed": False if promotion_blockers else True,
        "safety": "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch",
        "thresholds": THRESHOLDS,
        "fixture_degraded": fixture_degraded,
        "best_cpp_run": best_cpp,
        "best_mainline_run": best_mainline,
        "same_session_cpp_run": same_session_cpp,
        "same_session_mainline_run": same_session_mainline,
        "same_session_comparison": same_session,
        "promotion_blockers": promotion_blockers,
        "cpp_runs": cpp_rows,
        "mainline_runs": mainline_rows,
    }
    text = json.dumps(payload, indent=2, sort_keys=True)
    args.json_out.parent.mkdir(parents=True, exist_ok=True)
    args.json_out.write_text(text + "\n", encoding="utf-8")
    print(text)
    return 0 if payload["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
