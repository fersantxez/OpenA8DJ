#!/usr/bin/env python3
"""Classify failure modes in existing physical soundcheck captures.

This is an offline diagnostic only. It reads already-recorded WAV/evidence
files and does not open audio devices, query CoreAudio, touch USB, install
drivers, or mutate system state.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
from pathlib import Path

import numpy as np
from scipy import signal, stats


ROOT = Path(__file__).resolve().parents[1]
ANALYZER_PATH = ROOT / "scripts" / "analyze-soundcheck-capture.py"


def load_analyzer():
    spec = importlib.util.spec_from_file_location("soundcheck_analyzer", ANALYZER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def pair_to_array(pair):
    if not pair:
        return np.zeros((0, 2), dtype=np.float64)
    return np.asarray(pair, dtype=np.float64)


def rms(values):
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values))))


def db(value):
    if value <= 0.0:
        return -240.0
    return float(20.0 * math.log10(value))


def load_run(analyzer, run_dir):
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
        "reference_path": reference_path,
        "capture_path": capture_path,
        "reference": pair_to_array(ref_pair),
        "capture": pair_to_array(cap_pair),
    }


def aligned_arrays(run, analysis_seconds):
    metrics = run["metrics"]
    rate = run["rate"]
    ref_start = int(metrics["reference_start"])
    cap_start = int(metrics["capture_start"])
    frames = int(metrics["compared_frames"])
    if analysis_seconds > 0:
        frames = min(frames, int(round(analysis_seconds * rate)))
    ref = run["reference"]
    cap = run["capture"]
    frames = min(frames, len(ref) - ref_start, len(cap) - cap_start)
    if frames <= 0:
        raise SystemExit(f"{run['run_dir']}: no aligned frames")
    return ref[ref_start:ref_start + frames], cap[cap_start:cap_start + frames]


def solve_matrix(ref, cap):
    matrix, *_ = np.linalg.lstsq(ref, cap, rcond=None)
    predicted = ref @ matrix
    residual = cap - predicted
    return matrix, predicted, residual


def scalar_fit(ref, cap):
    ref_flat = ref.reshape(-1)
    cap_flat = cap.reshape(-1)
    denom = float(np.dot(ref_flat, ref_flat))
    gain = float(np.dot(ref_flat, cap_flat) / denom) if denom > 1e-18 else 0.0
    predicted = gain * ref
    residual = cap - predicted
    return gain, predicted, residual


def polynomial_fit(ref, cap, degree):
    x = ref.reshape(-1)
    y = cap.reshape(-1)
    powers = [x]
    if degree >= 2:
        powers.append(x * x)
    if degree >= 3:
        powers.append(x * x * x)
    design = np.vstack(powers).T
    coeffs, *_ = np.linalg.lstsq(design, y, rcond=None)
    predicted = (design @ coeffs).reshape(cap.shape)
    residual = cap - predicted
    return coeffs.tolist(), predicted, residual


def best_channel_model(ref, cap):
    candidates = []
    for swap in (False, True):
        routed = ref[:, ::-1] if swap else ref
        for left_sign in (-1.0, 1.0):
            for right_sign in (-1.0, 1.0):
                candidate_ref = routed * np.array([left_sign, right_sign])
                gain, predicted, residual = scalar_fit(candidate_ref, cap)
                residual_rms = rms(residual)
                candidates.append({
                    "swap_lr": swap,
                    "left_polarity": int(left_sign),
                    "right_polarity": int(right_sign),
                    "gain": gain,
                    "snr_db": db(rms(predicted) / residual_rms) if residual_rms > 0 else 999.0,
                    "residual_rms": residual_rms,
                })
    return max(candidates, key=lambda item: item["snr_db"])


def lag_for_window(ref_mono, cap_mono, start, frames, max_lag):
    ref = ref_mono[start:start + frames]
    if ref.size < frames:
        return None
    search_start = max(0, start - max_lag)
    search_end = min(cap_mono.size, start + frames + max_lag)
    cap = cap_mono[search_start:search_end]
    corr = signal.correlate(cap, ref, mode="valid", method="fft")
    if corr.size == 0:
        return None
    best = int(np.argmax(np.abs(corr)))
    lag = search_start + best - start
    ref_energy = float(np.dot(ref, ref))
    cap_slice = cap_mono[start + lag:start + lag + frames]
    cap_energy = float(np.dot(cap_slice, cap_slice)) if cap_slice.size == frames else 0.0
    score = 0.0
    if ref_energy > 0.0 and cap_energy > 0.0:
        score = float(corr[best] / math.sqrt(ref_energy * cap_energy))
    return lag, score


def drift_metrics(ref, cap, rate, window_seconds, hop_seconds, max_lag):
    ref_mono = np.mean(ref, axis=1)
    cap_mono = np.mean(cap, axis=1)
    window = max(256, int(round(window_seconds * rate)))
    hop = max(1, int(round(hop_seconds * rate)))
    points = []
    for start in range(0, max(0, len(ref_mono) - window + 1), hop):
        item = lag_for_window(ref_mono, cap_mono, start, window, max_lag)
        if item is None:
            continue
        lag, score = item
        points.append((start, lag, score))
    if len(points) < 2:
        return {"windows": len(points)}
    starts = np.asarray([point[0] for point in points], dtype=np.float64)
    lags = np.asarray([point[1] for point in points], dtype=np.float64)
    scores = np.asarray([abs(point[2]) for point in points], dtype=np.float64)
    slope, intercept = np.polyfit(starts, lags, 1)
    predicted = starts * slope + intercept
    residual = lags - predicted
    jumps = np.abs(np.diff(lags))
    return {
        "windows": len(points),
        "lag_min_frames": int(np.min(lags)),
        "lag_max_frames": int(np.max(lags)),
        "lag_span_frames": int(np.max(lags) - np.min(lags)),
        "lag_slope_frames_per_frame": float(slope),
        "drift_ppm": float(slope * 1_000_000.0),
        "lag_fit_residual_rms_frames": rms(residual),
        "lag_jump_max_frames": float(np.max(jumps)) if jumps.size else 0.0,
        "lag_jump_p95_frames": float(np.percentile(jumps, 95)) if jumps.size else 0.0,
        "score_min": float(np.min(scores)),
        "score_median": float(np.median(scores)),
    }


def window_gain_metrics(ref, cap, rate, window_seconds, hop_seconds):
    window = max(256, int(round(window_seconds * rate)))
    hop = max(1, int(round(hop_seconds * rate)))
    gains = []
    snrs = []
    residuals = []
    signal_levels = []
    for start in range(0, max(0, len(ref) - window + 1), hop):
        ref_w = ref[start:start + window]
        cap_w = cap[start:start + window]
        gain, predicted, residual = scalar_fit(ref_w, cap_w)
        pred_rms = rms(predicted)
        res_rms = rms(residual)
        gains.append(gain)
        snrs.append(db(pred_rms / res_rms) if res_rms > 0 else 999.0)
        residuals.append(res_rms)
        signal_levels.append(rms(ref_w))
    if not gains:
        return {"windows": 0}
    gain_array = np.asarray(gains)
    residual_array = np.asarray(residuals)
    signal_array = np.asarray(signal_levels)
    corr = 0.0
    if np.std(signal_array) > 0.0 and np.std(residual_array) > 0.0:
        corr = float(np.corrcoef(signal_array, residual_array)[0, 1])
    spearman = 0.0
    if len(signal_array) > 2:
        spearman = float(stats.spearmanr(signal_array, residual_array).correlation)
    return {
        "windows": len(gains),
        "gain_mean": float(np.mean(gain_array)),
        "gain_std": float(np.std(gain_array)),
        "gain_relative_std": float(np.std(gain_array) / abs(np.mean(gain_array))) if abs(np.mean(gain_array)) > 1e-12 else 0.0,
        "snr_min_db": float(np.min(snrs)),
        "snr_median_db": float(np.median(snrs)),
        "residual_vs_signal_r": corr,
        "residual_vs_signal_spearman": spearman,
    }


def waveform_metrics(ref, cap):
    def stats_for(name, values):
        flat = values.reshape(-1)
        peak = float(np.max(np.abs(flat))) if flat.size else 0.0
        value_rms = rms(flat)
        return {
            f"{name}_peak": peak,
            f"{name}_rms": value_rms,
            f"{name}_crest_db": db(peak / value_rms) if value_rms > 0 else 0.0,
            f"{name}_dc": float(np.mean(flat)) if flat.size else 0.0,
            f"{name}_near_clip_frames": int(np.sum(np.any(np.abs(values) >= 0.98, axis=1))) if values.ndim == 2 else 0,
        }
    result = {}
    result.update(stats_for("reference", ref))
    result.update(stats_for("capture", cap))
    return result


def classify(result):
    reasons = []
    drift = result["drift"]
    gain = result["window_gain"]
    if abs(drift.get("drift_ppm", 0.0)) > 100.0 or drift.get("lag_span_frames", 0) > 256:
        reasons.append("timebase_or_alignment_instability")
    if drift.get("lag_jump_p95_frames", 0.0) > 8.0 and drift.get("score_median", 1.0) < 0.95:
        reasons.append("window_alignment_is_unstable_for_music")
    if result["matrix_fit"]["snr_improvement_db"] < 3.0:
        reasons.append("static_lr_mix_or_polarity_not_sufficient")
    if result["polynomial_fit"]["snr_improvement_db"] < 3.0:
        reasons.append("simple_memoryless_nonlinearity_not_sufficient")
    if abs(gain.get("residual_vs_signal_spearman", 0.0)) > 0.65:
        reasons.append("residual_tracks_program_level")
    if result["waveform"]["capture_near_clip_frames"] > 0:
        reasons.append("capture_near_clip_present")
    if not reasons:
        reasons.append("no_single_offline_model_explains_failure")
    return reasons


def analyze_run(analyzer, run_dir, args):
    run = load_run(analyzer, run_dir)
    ref, cap = aligned_arrays(run, args.analysis_seconds)
    scalar_gain, scalar_pred, scalar_residual = scalar_fit(ref, cap)
    matrix, matrix_pred, matrix_residual = solve_matrix(ref, cap)
    poly_coeffs, poly_pred, poly_residual = polynomial_fit(ref, cap, 3)
    scalar_residual_rms = rms(scalar_residual)
    matrix_residual_rms = rms(matrix_residual)
    poly_residual_rms = rms(poly_residual)
    scalar_snr = db(rms(scalar_pred) / scalar_residual_rms) if scalar_residual_rms > 0 else 999.0
    matrix_snr = db(rms(matrix_pred) / matrix_residual_rms) if matrix_residual_rms > 0 else 999.0
    poly_snr = db(rms(poly_pred) / poly_residual_rms) if poly_residual_rms > 0 else 999.0
    result = {
        "run_dir": str(run_dir),
        "reference": str(run["reference_path"]),
        "capture": str(run["capture_path"]),
        "rate": run["rate"],
        "analysis_seconds": len(ref) / run["rate"],
        "source_metrics": {
            "quality_alignment_score": run["metrics"].get("quality_alignment_score"),
            "snr_db": run["metrics"].get("snr_db"),
            "lag_jumps_gt_2_frames": run["metrics"].get("lag_jumps_gt_2_frames"),
            "mid_band_residual_ratio": run["metrics"].get("mid_band_residual_ratio"),
            "high_band_residual_ratio": run["metrics"].get("high_band_residual_ratio"),
        },
        "best_polarity_route": best_channel_model(ref, cap),
        "scalar_fit": {
            "gain": scalar_gain,
            "snr_db": scalar_snr,
            "residual_rms": scalar_residual_rms,
        },
        "matrix_fit": {
            "matrix_ref_to_capture": matrix.tolist(),
            "snr_db": matrix_snr,
            "snr_improvement_db": matrix_snr - scalar_snr,
            "residual_rms": matrix_residual_rms,
        },
        "polynomial_fit": {
            "degree": 3,
            "coefficients": poly_coeffs,
            "snr_db": poly_snr,
            "snr_improvement_db": poly_snr - scalar_snr,
            "residual_rms": poly_residual_rms,
        },
        "drift": drift_metrics(ref,
                               cap,
                               run["rate"],
                               args.drift_window_seconds,
                               args.drift_hop_seconds,
                               args.drift_max_lag),
        "window_gain": window_gain_metrics(ref,
                                           cap,
                                           run["rate"],
                                           args.gain_window_seconds,
                                           args.gain_hop_seconds),
        "waveform": waveform_metrics(ref, cap),
    }
    result["classification"] = classify(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("soundcheck_dirs", nargs="+", type=Path)
    parser.add_argument("--analysis-seconds", type=float, default=12.0)
    parser.add_argument("--drift-window-seconds", type=float, default=0.25)
    parser.add_argument("--drift-hop-seconds", type=float, default=0.125)
    parser.add_argument("--drift-max-lag", type=int, default=2048)
    parser.add_argument("--gain-window-seconds", type=float, default=0.25)
    parser.add_argument("--gain-hop-seconds", type=float, default=0.125)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    analyzer = load_analyzer()
    rows = [analyze_run(analyzer, path, args) for path in args.soundcheck_dirs]
    summary = {
        "schema": "opena8djcpp.soundcheck-failure-modes.v1",
        "result": "PASS_DIAGNOSTIC" if rows else "FAIL",
        "safety": "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch",
        "threshold_notes": {
            "drift_ppm_over_100": "suggests timebase/alignment instability, not proof by itself",
            "matrix_or_polynomial_snr_improvement_under_3db": "rejects that simple model as sufficient explanation",
            "residual_vs_signal_spearman_abs_over_0_65": "suggests level-dependent residual",
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
