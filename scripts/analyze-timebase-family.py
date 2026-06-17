#!/usr/bin/env python3
"""Aggregate offline soundcheck window traces into timebase diagnostics.

This reads existing JSON evidence only. It does not touch audio devices,
CoreAudio, USB, drivers, defaults, or hardware.
"""

import argparse
import json
import math
import statistics
from pathlib import Path


def median(values):
    return statistics.median(values) if values else 0.0


def percentile(values, pct):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = int(round((len(ordered) - 1) * pct))
    return ordered[index]


def linear_slope(xs, ys):
    if len(xs) < 2 or len(xs) != len(ys):
        return 0.0
    x_mean = statistics.fmean(xs)
    y_mean = statistics.fmean(ys)
    denom = sum((x - x_mean) * (x - x_mean) for x in xs)
    if denom <= 1e-12:
        return 0.0
    return sum((x - x_mean) * (y - y_mean) for x, y in zip(xs, ys)) / denom


def row_for_trace(path):
    trace = json.loads(path.read_text(encoding="utf-8"))
    windows = trace.get("rows", [])
    lags = [float(row.get("local_lag_frames", 0.0)) for row in windows]
    starts = [float(row.get("start_seconds", 0.0)) for row in windows]
    raw_mid = [float(row.get("raw_mid_band_residual_ratio", 0.0)) for row in windows]
    corrected_mid = [
        float(row.get("lag_corrected_mid_band_residual_ratio", 0.0))
        for row in windows
    ]
    raw_corr = [float(row.get("raw_correlation", 0.0)) for row in windows]
    corrected_corr = [
        float(row.get("lag_corrected_correlation", 0.0))
        for row in windows
    ]
    lag_deltas = [right - left for left, right in zip(lags, lags[1:])]
    lag_jumps = [delta for delta in lag_deltas if abs(delta) > 2.0]
    raw_mid_median = median(raw_mid)
    corrected_mid_median = median(corrected_mid)
    correction_improvement = (
        1.0 - (corrected_mid_median / raw_mid_median)
        if raw_mid_median > 1e-9
        else 0.0
    )
    drift_frames_per_second = linear_slope(starts, lags)
    drift_ppm = (
        drift_frames_per_second / float(trace.get("rate", 48000)) * 1_000_000.0
    )
    abs_lags = [abs(value) for value in lags]
    classification = []
    if len(lag_jumps) > 0:
        classification.append("lag_jumps")
    if abs(drift_ppm) > 100.0:
        classification.append("linear_drift")
    if correction_improvement > 0.01:
        classification.append("lag_correction_helps")
    if corrected_mid_median > 1.35:
        classification.append("residual_after_lag_correction")
    if not classification:
        classification.append("stable_or_unclassified")

    return {
        "trace": str(path),
        "run_dir": trace.get("run_dir", ""),
        "windows": len(windows),
        "lag_min": min(lags, default=0.0),
        "lag_max": max(lags, default=0.0),
        "lag_abs_p95": percentile(abs_lags, 0.95),
        "lag_jump_count_gt_2_frames": len(lag_jumps),
        "lag_jump_abs_p95": percentile([abs(value) for value in lag_jumps], 0.95),
        "drift_frames_per_second": drift_frames_per_second,
        "drift_ppm": drift_ppm,
        "raw_mid_band_residual_ratio_median": raw_mid_median,
        "lag_corrected_mid_band_residual_ratio_median": corrected_mid_median,
        "lag_correction_mid_ratio_improvement": correction_improvement,
        "raw_correlation_median": median(raw_corr),
        "lag_corrected_correlation_median": median(corrected_corr),
        "classification": classification,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("traces", nargs="+", type=Path)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    rows = [row_for_trace(path) for path in args.traces]
    jump_rows = [row for row in rows if row["lag_jump_count_gt_2_frames"] > 0]
    residual_rows = [
        row
        for row in rows
        if row["lag_corrected_mid_band_residual_ratio_median"] > 1.35
    ]
    drift_rows = [row for row in rows if abs(row["drift_ppm"]) > 100.0]
    stability_passed = bool(rows) and not jump_rows and not residual_rows and not drift_rows
    result = {
        "schema": "opena8djcpp.timebase-family.v1",
        "analysis_result": "PASS" if rows else "FAIL",
        "stability_result": "PASS" if stability_passed else "FAIL",
        "result": "PASS" if stability_passed else "FAIL",
        "thresholds": {
            "lag_jump_frames": 2.0,
            "linear_drift_ppm": 100.0,
            "lag_corrected_mid_band_residual_ratio": 1.35,
        },
        "trace_count": len(rows),
        "runs_with_lag_jumps": len(jump_rows),
        "runs_with_residual_after_lag_correction": len(residual_rows),
        "runs_with_linear_drift": len(drift_rows),
        "max_lag_jump_count_gt_2_frames": max(
            (row["lag_jump_count_gt_2_frames"] for row in rows),
            default=0,
        ),
        "max_lag_abs_p95": max((row["lag_abs_p95"] for row in rows), default=0.0),
        "max_abs_drift_ppm": max((abs(row["drift_ppm"]) for row in rows), default=0.0),
        "median_lag_correction_mid_ratio_improvement": median(
            [row["lag_correction_mid_ratio_improvement"] for row in rows]
        ),
        "rows": rows,
    }

    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    return 0 if rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
