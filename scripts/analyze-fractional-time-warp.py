#!/usr/bin/env python3
"""Test whether fractional delay/time-warp explains physical captures.

Offline diagnostic only. The script reads existing soundcheck WAV/JSON evidence
and does not open audio devices, query CoreAudio, touch USB, install drivers,
change defaults, or mutate system state.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
import wave
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


def read_wav_pair(path: Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if width == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 3:
        bytes_ = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        vals = (bytes_[:, 0].astype(np.int32) |
                (bytes_[:, 1].astype(np.int32) << 8) |
                (bytes_[:, 2].astype(np.int32) << 16))
        vals = np.where(vals & 0x800000, vals | ~0xffffff, vals)
        data = vals.astype(np.float64) / 8388608.0
    elif width == 4:
        data = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise SystemExit(f"{path}: unsupported WAV width {width * 8}")
    if channels <= 0:
        raise SystemExit(f"{path}: invalid channel count {channels}")
    data = data.reshape(-1, channels)
    if channels == 1:
        return rate, np.column_stack([data[:, 0], data[:, 0]])
    return rate, data[:, :2].copy()


def find_reference(run_dir: Path) -> Path:
    reference = run_dir / "fixture" / "reference.wav"
    if reference.exists():
        return reference
    raise SystemExit(f"{run_dir}: missing fixture/reference.wav")


def rms(values: np.ndarray) -> float:
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values))))


def db(value: float) -> float:
    if value <= 0.0:
        return -240.0
    return float(20.0 * math.log10(value))


def fit_scalar(ref: np.ndarray, cap: np.ndarray) -> dict:
    x = ref.reshape(-1)
    y = cap.reshape(-1)
    denom = float(np.dot(x, x))
    gain = float(np.dot(x, y) / denom) if denom > 1e-18 else 0.0
    pred = gain * ref
    residual = cap - pred
    signal_rms = rms(pred)
    residual_rms = rms(residual)
    return {
        "gain": gain,
        "snr_db": db(signal_rms / residual_rms) if residual_rms > 0.0 else 999.0,
        "signal_rms": signal_rms,
        "residual_rms": residual_rms,
    }


def fit_matrix(ref: np.ndarray, cap: np.ndarray) -> dict:
    matrix, *_ = np.linalg.lstsq(ref, cap, rcond=None)
    pred = ref @ matrix
    residual = cap - pred
    signal_rms = rms(pred)
    residual_rms = rms(residual)
    return {
        "matrix": matrix.tolist(),
        "snr_db": db(signal_rms / residual_rms) if residual_rms > 0.0 else 999.0,
        "signal_rms": signal_rms,
        "residual_rms": residual_rms,
    }


def shifted(signal_in: np.ndarray, delay: float) -> np.ndarray:
    indices = np.arange(signal_in.shape[0], dtype=np.float64) + delay
    source = np.arange(signal_in.shape[0], dtype=np.float64)
    if signal_in.ndim == 1:
        return np.interp(indices, source, signal_in, left=0.0, right=0.0)
    channels = [
        np.interp(indices, source, signal_in[:, ch], left=0.0, right=0.0)
        for ch in range(signal_in.shape[1])
    ]
    return np.column_stack(channels)


def fractional_peak(corr: np.ndarray, index: int) -> float:
    if index <= 0 or index >= corr.size - 1:
        return float(index)
    left = abs(float(corr[index - 1]))
    center = abs(float(corr[index]))
    right = abs(float(corr[index + 1]))
    denom = left - 2.0 * center + right
    if abs(denom) <= 1e-18:
        return float(index)
    offset = 0.5 * (left - right) / denom
    return float(index) + max(-0.5, min(0.5, offset))


def best_window_delay(ref_mono: np.ndarray,
                      cap_mono: np.ndarray,
                      max_lag: int) -> tuple[float, float]:
    ref_zero = ref_mono - np.mean(ref_mono)
    cap_zero = cap_mono - np.mean(cap_mono)
    if ref_zero.size < 4 or cap_zero.size != ref_zero.size:
        return 0.0, 0.0
    padded = np.pad(ref_zero, (max_lag + 2, max_lag + 2), mode="constant")
    corr = signal.correlate(padded, cap_zero, mode="valid", method="fft")
    if corr.size == 0:
        return 0.0, 0.0
    index = int(np.argmax(np.abs(corr)))
    refined = fractional_peak(corr, index)
    lag = refined - float(max_lag + 2)
    shifted_ref = shifted(ref_zero, lag)
    denom = math.sqrt(float(np.dot(shifted_ref, shifted_ref)) * float(np.dot(cap_zero, cap_zero)))
    score = float(np.dot(shifted_ref, cap_zero) / denom) if denom > 0.0 else 0.0
    return float(lag), abs(score)


def band_ratio(residual: np.ndarray,
               signal_ref: np.ndarray,
               rate: int,
               low: float,
               high: float) -> float:
    nperseg = min(8192, residual.shape[0])
    if nperseg < 128:
        return 0.0
    ratios = []
    channel_count = 1 if residual.ndim == 1 else residual.shape[1]
    for channel in range(channel_count):
        residual_channel = residual if residual.ndim == 1 else residual[:, channel]
        signal_channel = signal_ref if signal_ref.ndim == 1 else signal_ref[:, channel]
        freqs, residual_psd = signal.welch(residual_channel, fs=rate, nperseg=nperseg)
        _, signal_psd = signal.welch(signal_channel, fs=rate, nperseg=nperseg)
        mask = (freqs >= low) & (freqs < high)
        if not np.any(mask):
            continue
        residual_rms = float(np.sqrt(np.mean(residual_psd[mask])))
        signal_rms = float(np.sqrt(np.mean(signal_psd[mask])))
        ratios.append(residual_rms / signal_rms if signal_rms > 1e-18 else 0.0)
    return max(ratios) if ratios else 0.0


def apply_time_warp(ref: np.ndarray,
                    centers: np.ndarray,
                    delays: np.ndarray) -> np.ndarray:
    sample_indices = np.arange(ref.shape[0], dtype=np.float64)
    if centers.size == 0:
        return ref.copy()
    delay_curve = np.interp(sample_indices,
                            centers,
                            delays,
                            left=float(delays[0]),
                            right=float(delays[-1]))
    source = np.arange(ref.shape[0], dtype=np.float64)
    channels = [
        np.interp(sample_indices + delay_curve,
                  source,
                  ref[:, ch],
                  left=0.0,
                  right=0.0)
        for ch in range(ref.shape[1])
    ]
    return np.column_stack(channels)


def analyze_run(analyzer, run_dir: Path, args) -> dict:
    metrics_path = run_dir / "metrics.json"
    capture_path = run_dir / "captured.wav"
    if not metrics_path.exists() or not capture_path.exists():
        raise SystemExit(f"{run_dir}: missing metrics/captured.wav")
    metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    ref_rate, ref_pair = read_wav_pair(find_reference(run_dir))
    cap_rate, cap_pair = read_wav_pair(capture_path)
    if cap_rate != ref_rate:
        cap_pair = np.asarray(analyzer.resample_pair_linear(cap_pair.tolist(), cap_rate, ref_rate),
                              dtype=np.float64)
        cap_rate = ref_rate
    ref_start = int(metrics["reference_start"])
    cap_start = int(metrics["capture_start"])
    usable = int(metrics["compared_frames"])
    if args.analysis_seconds > 0:
        usable = min(usable, int(round(args.analysis_seconds * ref_rate)))
    usable = min(usable, len(ref_pair) - ref_start, len(cap_pair) - cap_start)
    if usable <= ref_rate:
        raise SystemExit(f"{run_dir}: not enough aligned audio")
    ref = ref_pair[ref_start:ref_start + usable]
    cap = cap_pair[cap_start:cap_start + usable]
    ref_mono = np.mean(ref, axis=1)
    cap_mono = np.mean(cap, axis=1)
    window = max(512, int(round(args.window_seconds * ref_rate)))
    hop = max(1, int(round(args.hop_seconds * ref_rate)))
    centers = []
    delays = []
    scores = []
    window_snr_before = []
    window_snr_after = []
    for start in range(0, max(0, usable - window + 1), hop):
        stop = start + window
        delay, score = best_window_delay(ref_mono[start:stop], cap_mono[start:stop], args.max_lag)
        ref_window = ref[start:stop]
        cap_window = cap[start:stop]
        warped_window = shifted(ref_window, delay)
        before = fit_scalar(ref_window, cap_window)
        after = fit_scalar(warped_window, cap_window)
        centers.append(start + window / 2.0)
        delays.append(delay)
        scores.append(score)
        window_snr_before.append(before["snr_db"])
        window_snr_after.append(after["snr_db"])
    centers_np = np.asarray(centers, dtype=np.float64)
    delays_np = np.asarray(delays, dtype=np.float64)
    if args.median_filter_windows > 1 and delays_np.size >= args.median_filter_windows:
        kernel = args.median_filter_windows
        if kernel % 2 == 0:
            kernel += 1
        delays_np = signal.medfilt(delays_np, kernel_size=kernel)
    warped = apply_time_warp(ref, centers_np, delays_np)
    scalar_before = fit_scalar(ref, cap)
    scalar_after = fit_scalar(warped, cap)
    matrix_before = fit_matrix(ref, cap)
    matrix_after = fit_matrix(warped, cap)
    pred_before = scalar_before["gain"] * ref
    pred_after = scalar_after["gain"] * warped
    residual_before = cap - pred_before
    residual_after = cap - pred_after
    delay_jumps = np.abs(np.diff(delays_np)) if delays_np.size > 1 else np.asarray([])
    scalar_delta = scalar_after["snr_db"] - scalar_before["snr_db"]
    matrix_delta = matrix_after["snr_db"] - matrix_before["snr_db"]
    classification = "fractional_time_warp_rejected"
    if max(scalar_delta, matrix_delta) >= args.strong_improvement_db:
        classification = "fractional_time_warp_explains_large_residual"
    elif max(scalar_delta, matrix_delta) >= args.partial_improvement_db:
        classification = "fractional_time_warp_partial_factor"
    return {
        "run_dir": str(run_dir),
        "rate": ref_rate,
        "analysis_seconds": usable / ref_rate,
        "window_seconds": args.window_seconds,
        "hop_seconds": args.hop_seconds,
        "max_lag_frames": args.max_lag,
        "windows": len(centers),
        "source_metrics": {
            "quality_alignment_score": metrics.get("quality_alignment_score"),
            "lag_jumps_gt_2_frames": metrics.get("lag_jumps_gt_2_frames"),
            "mid_band_residual_ratio": metrics.get("mid_band_residual_ratio"),
            "high_band_residual_ratio": metrics.get("high_band_residual_ratio"),
        },
        "delay": {
            "min_frames": float(np.min(delays_np)) if delays_np.size else 0.0,
            "max_frames": float(np.max(delays_np)) if delays_np.size else 0.0,
            "median_frames": float(np.median(delays_np)) if delays_np.size else 0.0,
            "p95_abs_frames": float(np.percentile(np.abs(delays_np), 95)) if delays_np.size else 0.0,
            "jump_p95_frames": float(np.percentile(delay_jumps, 95)) if delay_jumps.size else 0.0,
            "score_median": float(np.median(scores)) if scores else 0.0,
            "score_min": float(np.min(scores)) if scores else 0.0,
        },
        "window_snr": {
            "before_median_db": float(np.median(window_snr_before)) if window_snr_before else 0.0,
            "after_median_db": float(np.median(window_snr_after)) if window_snr_after else 0.0,
            "median_delta_db": (
                float(np.median(window_snr_after) - np.median(window_snr_before))
                if window_snr_before and window_snr_after else 0.0
            ),
        },
        "global_scalar": {
            "before_snr_db": scalar_before["snr_db"],
            "after_snr_db": scalar_after["snr_db"],
            "improvement_db": scalar_delta,
            "before_residual_rms": scalar_before["residual_rms"],
            "after_residual_rms": scalar_after["residual_rms"],
        },
        "global_matrix": {
            "before_snr_db": matrix_before["snr_db"],
            "after_snr_db": matrix_after["snr_db"],
            "improvement_db": matrix_delta,
            "before_residual_rms": matrix_before["residual_rms"],
            "after_residual_rms": matrix_after["residual_rms"],
        },
        "band_residual_ratios": {
            "before_mid": band_ratio(residual_before, pred_before, ref_rate, 1000.0, 5000.0),
            "after_mid": band_ratio(residual_after, pred_after, ref_rate, 1000.0, 5000.0),
            "before_high": band_ratio(residual_before, pred_before, ref_rate, 5000.0, 12000.0),
            "after_high": band_ratio(residual_after, pred_after, ref_rate, 5000.0, 12000.0),
        },
        "classification": classification,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("soundcheck_dirs", nargs="+", type=Path)
    parser.add_argument("--analysis-seconds", type=float, default=12.0)
    parser.add_argument("--window-seconds", type=float, default=0.25)
    parser.add_argument("--hop-seconds", type=float, default=0.125)
    parser.add_argument("--max-lag", type=int, default=64)
    parser.add_argument("--median-filter-windows", type=int, default=5)
    parser.add_argument("--partial-improvement-db", type=float, default=3.0)
    parser.add_argument("--strong-improvement-db", type=float, default=6.0)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    analyzer = load_analyzer()
    rows = [analyze_run(analyzer, path, args) for path in args.soundcheck_dirs]
    classifications = [row["classification"] for row in rows]
    result = {
        "schema": "opena8djcpp.fractional-time-warp.v1",
        "result": "PASS_DIAGNOSTIC" if rows else "FAIL",
        "safety": "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch",
        "thresholds": {
            "partial_improvement_db": args.partial_improvement_db,
            "strong_improvement_db": args.strong_improvement_db,
        },
        "summary": {
            "runs": len(rows),
            "large_residual_explained": classifications.count("fractional_time_warp_explains_large_residual"),
            "partial_factor": classifications.count("fractional_time_warp_partial_factor"),
            "rejected": classifications.count("fractional_time_warp_rejected"),
        },
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
