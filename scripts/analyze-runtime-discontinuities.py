#!/usr/bin/env python3
"""Correlate existing soundcheck audio failures with runtime telemetry.

Offline diagnostic only: reads saved WAV/JSON/TSV evidence and does not open
audio devices, query CoreAudio, touch USB, install drivers, or mutate system
state.
"""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
import math
from pathlib import Path

import numpy as np
from scipy import signal


ROOT = Path(__file__).resolve().parents[1]
ANALYZER_PATH = ROOT / "scripts" / "analyze-soundcheck-capture.py"


def load_analyzer():
    spec = importlib.util.spec_from_file_location("soundcheck_analyzer", ANALYZER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def rms(values: np.ndarray) -> float:
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values))))


def db(value: float) -> float:
    if value <= 0.0:
        return -240.0
    return float(20.0 * math.log10(value))


def pair_to_array(pair) -> np.ndarray:
    if not pair:
        return np.zeros((0, 2), dtype=np.float64)
    return np.asarray(pair, dtype=np.float64)


def read_numeric_tsv(path: Path) -> list[dict[str, float]]:
    if not path.exists():
        return []
    rows: list[dict[str, float]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            parsed: dict[str, float] = {}
            for key, value in row.items():
                if value is None or value == "":
                    continue
                try:
                    parsed[key] = float(value)
                except ValueError:
                    pass
            if parsed:
                rows.append(parsed)
    return rows


def load_run(analyzer, run_dir: Path):
    metrics_path = run_dir / "metrics.json"
    reference_path = run_dir / "fixture" / "reference.wav"
    capture_path = run_dir / "captured.wav"
    if not metrics_path.exists():
        raise SystemExit(f"{run_dir}: missing metrics.json")
    if not reference_path.exists() or not capture_path.exists():
        raise SystemExit(f"{run_dir}: missing reference/captured WAV")

    metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    ref_rate, ref_pair = analyzer.read_wav_pair(str(reference_path))
    cap_rate, cap_pair = analyzer.read_wav_pair(str(capture_path))
    if cap_rate != ref_rate:
        cap_pair = analyzer.resample_pair_linear(cap_pair, cap_rate, ref_rate)
        cap_rate = ref_rate
    return {
        "run_dir": run_dir,
        "metrics": metrics,
        "rate": ref_rate,
        "reference": pair_to_array(ref_pair),
        "capture": pair_to_array(cap_pair),
        "cpu": read_numeric_tsv(run_dir / "cpu-profile.tsv"),
        "stream": read_numeric_tsv(run_dir / "stream-stats-during.tsv"),
    }


def aligned_arrays(run, analysis_seconds: float):
    metrics = run["metrics"]
    rate = int(run["rate"])
    ref_start = int(metrics["reference_start"])
    cap_start = int(metrics["capture_start"])
    frames = int(metrics["compared_frames"])
    if analysis_seconds > 0:
        frames = min(frames, int(round(analysis_seconds * rate)))
    frames = min(frames,
                 len(run["reference"]) - ref_start,
                 len(run["capture"]) - cap_start)
    if frames <= 0:
        raise SystemExit(f"{run['run_dir']}: no aligned frames")
    ref = run["reference"][ref_start:ref_start + frames]
    cap = run["capture"][cap_start:cap_start + frames]
    return ref, cap, ref_start, cap_start


def scalar_residual(ref: np.ndarray, cap: np.ndarray):
    x = ref.reshape(-1)
    y = cap.reshape(-1)
    denom = float(np.dot(x, x))
    gain = float(np.dot(x, y) / denom) if denom > 1e-18 else 0.0
    pred = gain * ref
    residual = cap - pred
    pred_rms = rms(pred)
    res_rms = rms(residual)
    return gain, pred_rms, res_rms, db(pred_rms / res_rms) if res_rms > 0 else 999.0


def best_lag(ref: np.ndarray, cap: np.ndarray, max_lag: int):
    ref = ref - np.mean(ref)
    cap = cap - np.mean(cap)
    if ref.size == 0 or cap.size == 0:
        return 0, 0.0
    search = np.pad(cap, (max_lag, max_lag), mode="constant")
    corr = signal.correlate(search, ref, mode="valid", method="fft")
    if corr.size == 0:
        return 0, 0.0
    idx = int(np.argmax(np.abs(corr)))
    lag = idx - max_lag
    got = search[idx:idx + ref.size]
    denom = math.sqrt(float(np.dot(ref, ref)) * float(np.dot(got, got)))
    score = float(corr[idx] / denom) if denom > 0 else 0.0
    return lag, score


def audio_windows(ref: np.ndarray,
                  cap: np.ndarray,
                  rate: int,
                  cap_start: int,
                  window_seconds: float,
                  hop_seconds: float,
                  max_lag: int):
    window = max(256, int(round(window_seconds * rate)))
    hop = max(1, int(round(hop_seconds * rate)))
    ref_mono = np.mean(ref, axis=1)
    cap_mono = np.mean(cap, axis=1)
    rows = []
    for start in range(0, max(0, len(ref) - window + 1), hop):
        stop = start + window
        lag, score = best_lag(ref_mono[start:stop], cap_mono[start:stop], max_lag)
        gain, pred_rms, res_rms, snr = scalar_residual(ref[start:stop], cap[start:stop])
        rows.append({
            "audio_seconds": (cap_start + start + window / 2.0) / rate,
            "relative_seconds": (start + window / 2.0) / rate,
            "lag_frames": float(lag),
            "abs_lag_jump_frames": 0.0,
            "lag_score": abs(score),
            "scalar_gain": gain,
            "predicted_rms": pred_rms,
            "residual_rms": res_rms,
            "scalar_snr_db": snr,
        })
    for prev, curr in zip(rows, rows[1:]):
        curr["abs_lag_jump_frames"] = abs(curr["lag_frames"] - prev["lag_frames"])
    return rows


def nearest_rows(rows: list[dict[str, float]], seconds: np.ndarray, offset: float):
    if not rows:
        return {}
    times = np.asarray([row.get("elapsed_seconds", math.nan) for row in rows], dtype=np.float64)
    valid = np.isfinite(times)
    if not np.any(valid):
        return {}
    times = times[valid]
    filtered = [row for row, keep in zip(rows, valid) if keep]
    result = {}
    for index, second in enumerate(seconds):
        target = second + offset
        nearest = int(np.argmin(np.abs(times - target)))
        result[index] = filtered[nearest]
    return result


def series_from_nearest(nearest: dict[int, dict[str, float]],
                        count: int,
                        column: str,
                        delta: bool):
    values = np.full(count, np.nan, dtype=np.float64)
    previous = None
    for index in range(count):
        row = nearest.get(index)
        if not row or column not in row:
            continue
        value = float(row[column])
        if delta:
            values[index] = 0.0 if previous is None else value - previous
            previous = value
        else:
            values[index] = value
    return values


def corrcoef(a: np.ndarray, b: np.ndarray) -> float | None:
    valid = np.isfinite(a) & np.isfinite(b)
    if int(np.sum(valid)) < 4:
        return None
    aa = a[valid]
    bb = b[valid]
    if float(np.std(aa)) <= 1e-12 or float(np.std(bb)) <= 1e-12:
        return None
    return float(np.corrcoef(aa, bb)[0, 1])


def best_offset_correlation(windows: list[dict[str, float]],
                            telemetry: list[dict[str, float]],
                            metric: str,
                            column: str,
                            offsets: list[float],
                            delta: bool):
    if not windows or not telemetry:
        return None
    seconds = np.asarray([row["audio_seconds"] for row in windows], dtype=np.float64)
    target = np.asarray([row[metric] for row in windows], dtype=np.float64)
    best = None
    for offset in offsets:
        nearest = nearest_rows(telemetry, seconds, offset)
        values = series_from_nearest(nearest, len(windows), column, delta)
        corr = corrcoef(target, values)
        if corr is None:
            continue
        candidate = {
            "metric": metric,
            "column": column,
            "delta": delta,
            "offset_seconds": offset,
            "correlation": corr,
            "abs_correlation": abs(corr),
        }
        if best is None or candidate["abs_correlation"] > best["abs_correlation"]:
            best = candidate
    return best


def percentile(values, pct: float) -> float:
    arr = np.asarray(values, dtype=np.float64)
    arr = arr[np.isfinite(arr)]
    if arr.size == 0:
        return 0.0
    return float(np.percentile(arr, pct))


def summarize_run(run, args):
    ref, cap, _ref_start, cap_start = aligned_arrays(run, args.analysis_seconds)
    windows = audio_windows(ref,
                            cap,
                            int(run["rate"]),
                            cap_start,
                            args.window_seconds,
                            args.hop_seconds,
                            args.max_lag)
    offsets = [round(v, 3) for v in np.arange(args.offset_min, args.offset_max + 0.0001, args.offset_step)]
    probes = []
    cpu_columns = [
        "opena8dj_driver",
        "coreaudiod",
        "audio_services",
        "total_audio_ui",
        "system_load1",
    ]
    stream_columns = [
        "outputUnderruns",
        "outputActiveUnderruns",
        "outputElasticReplays",
        "outputElasticDrops",
        "outputTimelineResets",
        "outputLateWriteFrames",
        "playbackTransferErrors",
        "playbackCompletionDeltaOutliers",
        "captureCompletionDeltaOutliers",
        "captureToPlaybackQueueDeltaOutliers",
        "playbackTransfersCompleted",
        "captureTransfersCompleted",
    ]
    for metric in ("residual_rms", "abs_lag_jump_frames", "scalar_snr_db"):
        for column in cpu_columns:
            item = best_offset_correlation(windows, run["cpu"], metric, column, offsets, False)
            if item:
                item["source"] = "cpu"
                probes.append(item)
        for column in stream_columns:
            item = best_offset_correlation(windows, run["stream"], metric, column, offsets, True)
            if item:
                item["source"] = "stream_delta"
                probes.append(item)

    lag_jumps = [row["abs_lag_jump_frames"] for row in windows[1:]]
    residual = [row["residual_rms"] for row in windows]
    snr = [row["scalar_snr_db"] for row in windows]
    probes = sorted(probes, key=lambda row: row["abs_correlation"], reverse=True)
    strong = [row for row in probes if row["abs_correlation"] >= args.strong_corr]
    return {
        "run_dir": str(run["run_dir"]),
        "rate": run["rate"],
        "window_seconds": args.window_seconds,
        "hop_seconds": args.hop_seconds,
        "windows": len(windows),
        "source_metrics": {
            "quality_alignment_score": run["metrics"].get("quality_alignment_score"),
            "lag_jumps_gt_2_frames": run["metrics"].get("lag_jumps_gt_2_frames"),
            "mid_band_residual_ratio": run["metrics"].get("mid_band_residual_ratio"),
            "high_band_residual_ratio": run["metrics"].get("high_band_residual_ratio"),
            "quiet_mid_band_noise_dbfs": run["metrics"].get("quiet_mid_band_noise_dbfs"),
            "capture_clipped_frames": run["metrics"].get("capture_clipped_frames"),
        },
        "audio_window_summary": {
            "lag_jump_max_frames": percentile(lag_jumps, 100),
            "lag_jump_p95_frames": percentile(lag_jumps, 95),
            "residual_rms_median": percentile(residual, 50),
            "residual_rms_p95": percentile(residual, 95),
            "scalar_snr_db_median": percentile(snr, 50),
            "scalar_snr_db_min": percentile(snr, 0),
        },
        "telemetry_rows": {
            "cpu": len(run["cpu"]),
            "stream": len(run["stream"]),
        },
        "strong_correlations": strong[: args.max_reported_correlations],
        "top_correlations": probes[: args.max_reported_correlations],
        "interpretation": interpret(strong, windows, run),
    }


def interpret(strong, windows, run):
    reasons = []
    if not strong:
        reasons.append("no_strong_cpu_or_stream_correlation_found")
    else:
        stream_hits = [row for row in strong if row["source"] == "stream_delta"]
        cpu_hits = [row for row in strong if row["source"] == "cpu"]
        if stream_hits:
            reasons.append("runtime_stream_counter_correlation_present")
        if cpu_hits:
            reasons.append("cpu_correlation_present")
    if run["metrics"].get("capture_clipped_frames", 0) == 0:
        reasons.append("no_capture_clipping")
    if len(windows) >= 4:
        jumps = np.asarray([row["abs_lag_jump_frames"] for row in windows[1:]], dtype=np.float64)
        if np.percentile(jumps, 95) > 2.0:
            reasons.append("window_lag_jumps_present")
    return reasons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("soundcheck_dirs", nargs="+", type=Path)
    parser.add_argument("--analysis-seconds", type=float, default=12.0)
    parser.add_argument("--window-seconds", type=float, default=0.25)
    parser.add_argument("--hop-seconds", type=float, default=0.125)
    parser.add_argument("--max-lag", type=int, default=256)
    parser.add_argument("--offset-min", type=float, default=-5.0)
    parser.add_argument("--offset-max", type=float, default=5.0)
    parser.add_argument("--offset-step", type=float, default=0.25)
    parser.add_argument("--strong-corr", type=float, default=0.70)
    parser.add_argument("--max-reported-correlations", type=int, default=12)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    analyzer = load_analyzer()
    rows = [summarize_run(load_run(analyzer, path), args) for path in args.soundcheck_dirs]
    summary = {
        "schema": "opena8djcpp.runtime-discontinuity-analysis.v1",
        "result": "PASS_DIAGNOSTIC" if rows else "FAIL",
        "safety": "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch",
        "threshold_notes": {
            "strong_corr": args.strong_corr,
            "offset_search_seconds": [args.offset_min, args.offset_max],
            "correlation_is_hypothesis_not_proof": True,
        },
        "rows": rows,
    }
    text = json.dumps(summary, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    return 0 if rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
