#!/usr/bin/env python3
"""Offline WAV precision analyzer for OpenA8DJ physical evidence.

This tool only reads existing WAV files and writes JSON. It does not touch
CoreAudio, USB, default devices, drivers, services, or hardware.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import tempfile
from typing import Any

import numpy as np
import scipy.signal
import soundfile as sf


EPS = 1.0e-20


def db20(value: float) -> float:
    return 20.0 * math.log10(max(abs(value), EPS))


def rms(values: np.ndarray) -> float:
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values, dtype=np.float64))))


def parse_channels(text: str) -> list[int]:
    channels = [int(part.strip()) - 1 for part in text.split(",") if part.strip()]
    if len(channels) != 2 or min(channels) < 0:
        raise ValueError("capture/reference channel selectors must name two 1-based channels")
    return channels


def read_stereo(path: pathlib.Path, channels: list[int]) -> tuple[int, np.ndarray]:
    data, sample_rate = sf.read(str(path), always_2d=True, dtype="float64")
    if max(channels) >= data.shape[1]:
        raise ValueError(f"{path} has {data.shape[1]} channels, cannot select {channels}")
    return int(sample_rate), np.ascontiguousarray(data[:, channels])


def maybe_trim(data: np.ndarray, sample_rate: int, seconds: float | None) -> np.ndarray:
    if seconds is None or seconds <= 0:
        return data
    frames = min(data.shape[0], int(round(seconds * sample_rate)))
    return data[:frames]


def alignment(reference: np.ndarray, capture: np.ndarray, sample_rate: int, max_lag_seconds: float) -> dict[str, Any]:
    ref_mono = np.mean(reference, axis=1)
    cap_mono = np.mean(capture, axis=1)
    ref_mono = ref_mono - np.mean(ref_mono)
    cap_mono = cap_mono - np.mean(cap_mono)
    corr = scipy.signal.correlate(cap_mono, ref_mono, mode="full", method="fft")
    lags = scipy.signal.correlation_lags(cap_mono.size, ref_mono.size, mode="full")
    max_lag = int(round(max_lag_seconds * sample_rate))
    mask = np.abs(lags) <= max_lag
    if not np.any(mask):
        raise ValueError("max lag window is empty")
    masked_corr = corr[mask]
    masked_lags = lags[mask]
    index = int(np.argmax(np.abs(masked_corr)))
    lag = int(masked_lags[index])
    denom = math.sqrt(float(np.dot(ref_mono, ref_mono)) * float(np.dot(cap_mono, cap_mono))) + EPS
    score = float(masked_corr[index] / denom)
    if lag >= 0:
        reference_start = 0
        capture_start = lag
    else:
        reference_start = -lag
        capture_start = 0
    frames = min(reference.shape[0] - reference_start, capture.shape[0] - capture_start)
    if frames <= 0:
        raise ValueError("aligned files have no overlap")
    return {
        "lag_frames": lag,
        "lag_ms": 1000.0 * lag / sample_rate,
        "score": score,
        "reference_start": int(reference_start),
        "capture_start": int(capture_start),
        "compared_frames": int(frames),
    }


def band_mean(freqs: np.ndarray, values: np.ndarray, low: float, high: float) -> float | None:
    mask = (freqs >= low) & (freqs < high) & np.isfinite(values)
    if not np.any(mask):
        return None
    return float(np.mean(values[mask]))


def band_weighted_mean(
    freqs: np.ndarray, values: np.ndarray, weights: np.ndarray, low: float, high: float
) -> float | None:
    mask = (freqs >= low) & (freqs < high) & np.isfinite(values) & np.isfinite(weights)
    if not np.any(mask):
        return None
    band_values = values[mask]
    band_weights = np.maximum(weights[mask], 0.0)
    active = band_weights >= np.percentile(band_weights, 50.0)
    if np.count_nonzero(active) >= 4:
        band_values = band_values[active]
        band_weights = band_weights[active]
    total = float(np.sum(band_weights))
    if total <= EPS:
        return float(np.mean(band_values))
    return float(np.sum(band_values * band_weights) / total)


def band_percentile_span(freqs: np.ndarray, values_db: np.ndarray, low: float, high: float) -> float | None:
    mask = (freqs >= low) & (freqs < high) & np.isfinite(values_db)
    if np.count_nonzero(mask) < 4:
        return None
    band = values_db[mask]
    return float(np.percentile(band, 95.0) - np.percentile(band, 5.0))


def nperseg(length: int) -> int:
    return min(8192, max(256, length // 4))


def channel_metrics(reference: np.ndarray, capture: np.ndarray, sample_rate: int) -> dict[str, Any]:
    gain = float(np.dot(capture, reference) / (np.dot(reference, reference) + EPS))
    fitted = gain * reference
    residual = capture - fitted
    signal_rms = rms(fitted)
    residual_rms = rms(residual)
    f_coh, coh = scipy.signal.coherence(reference, capture, fs=sample_rate, nperseg=nperseg(reference.size))
    f_welch, pxx = scipy.signal.welch(reference, fs=sample_rate, nperseg=nperseg(reference.size))
    _, pxy = scipy.signal.csd(reference, capture, fs=sample_rate, nperseg=nperseg(reference.size))
    transfer = pxy / (pxx + EPS)
    transfer_db = 20.0 * np.log10(np.maximum(np.abs(transfer), EPS))
    _, residual_psd = scipy.signal.welch(residual, fs=sample_rate, nperseg=nperseg(residual.size))
    _, signal_psd = scipy.signal.welch(fitted, fs=sample_rate, nperseg=nperseg(fitted.size))
    residual_ratio = residual_psd / (signal_psd + EPS)
    nyquist_high = min(18000.0, sample_rate / 2.0)
    return {
        "gain_db": db20(gain),
        "signal_rms_dbfs": db20(signal_rms),
        "residual_rms_dbfs": db20(residual_rms),
        "snr_db": 20.0 * math.log10((signal_rms + EPS) / (residual_rms + EPS)),
        "peak_dbfs": db20(float(np.max(np.abs(capture))) if capture.size else 0.0),
        "clipped_frames": int(np.count_nonzero(np.abs(capture) >= 0.999)),
        "dc_offset_dbfs": db20(float(np.mean(capture)) if capture.size else 0.0),
        "coherence_low_mean": band_mean(f_coh, coh, 20.0, 200.0),
        "coherence_mid_mean": band_mean(f_coh, coh, 200.0, 5000.0),
        "coherence_high_mean": band_mean(f_coh, coh, 5000.0, nyquist_high),
        "coherence_low_active_mean": band_weighted_mean(f_coh, coh, pxx, 20.0, 200.0),
        "coherence_mid_active_mean": band_weighted_mean(f_coh, coh, pxx, 200.0, 5000.0),
        "coherence_high_active_mean": band_weighted_mean(f_coh, coh, pxx, 5000.0, nyquist_high),
        "transfer_ripple_low_db": band_percentile_span(f_welch, transfer_db, 20.0, 200.0),
        "transfer_ripple_mid_db": band_percentile_span(f_welch, transfer_db, 200.0, 5000.0),
        "transfer_ripple_high_db": band_percentile_span(f_welch, transfer_db, 5000.0, nyquist_high),
        "residual_ratio_mid_mean": band_mean(f_welch, residual_ratio, 200.0, 5000.0),
        "residual_ratio_high_mean": band_mean(f_welch, residual_ratio, 5000.0, nyquist_high),
    }


def stereo_matrix(reference: np.ndarray, capture: np.ndarray) -> dict[str, Any]:
    coeff, *_ = np.linalg.lstsq(reference, capture, rcond=None)
    condition_number = float(np.linalg.cond(reference.T @ reference))
    diag_level = max(abs(float(coeff[0, 0])), abs(float(coeff[1, 1])), EPS)
    off_level = max(abs(float(coeff[0, 1])), abs(float(coeff[1, 0])))
    return {
        "matrix": [[float(coeff[row, col]) for col in range(2)] for row in range(2)],
        "worst_offdiag_db_relative": 20.0 * math.log10(max(off_level, EPS) / diag_level),
        "condition_number": condition_number,
        "leakage_evaluable": condition_number <= 20.0,
    }


def window_lags(
    reference: np.ndarray,
    capture: np.ndarray,
    sample_rate: int,
    global_lag: int,
    window_seconds: float,
    search_frames: int,
) -> dict[str, Any]:
    window = int(round(window_seconds * sample_rate))
    if window <= 0 or reference.shape[0] < window * 2:
        return {"windows": 0}
    ref_mono = np.mean(reference, axis=1)
    cap_mono = np.mean(capture, axis=1)
    lags: list[int] = []
    for start in range(0, reference.shape[0] - window + 1, window):
        ref = ref_mono[start : start + window]
        cap_center = start + global_lag
        cap_start = max(0, cap_center - search_frames)
        cap_end = min(capture.shape[0], cap_center + window + search_frames)
        cap = cap_mono[cap_start:cap_end]
        if cap.size < window // 2:
            continue
        corr = scipy.signal.correlate(cap - np.mean(cap), ref - np.mean(ref), mode="full", method="fft")
        corr_lags = scipy.signal.correlation_lags(cap.size, ref.size, mode="full")
        local = int(corr_lags[int(np.argmax(np.abs(corr)))])
        lags.append(cap_start + local - start)
    if not lags:
        return {"windows": 0}
    arr = np.asarray(lags, dtype=np.float64)
    centered = arr - global_lag
    return {
        "windows": int(arr.size),
        "min_frames": float(np.min(centered)),
        "max_frames": float(np.max(centered)),
        "p95_abs_frames": float(np.percentile(np.abs(centered), 95.0)),
        "range_frames": float(np.max(centered) - np.min(centered)),
        "lag_jumps_gt_2_frames": int(np.count_nonzero(np.abs(np.diff(arr)) > 2.0)),
    }


def analyze(args: argparse.Namespace) -> dict[str, Any]:
    reference_channels = parse_channels(args.reference_channels)
    capture_channels = parse_channels(args.capture_channels)
    ref_rate, reference = read_stereo(args.reference, reference_channels)
    cap_rate, capture = read_stereo(args.capture, capture_channels)
    if ref_rate != cap_rate:
        raise ValueError(f"sample-rate mismatch: reference={ref_rate}, capture={cap_rate}")
    reference = maybe_trim(reference, ref_rate, args.seconds)
    capture = maybe_trim(capture, cap_rate, args.seconds + args.max_lag_seconds if args.seconds else None)
    align = alignment(reference, capture, ref_rate, args.max_lag_seconds)
    ref_start = align["reference_start"]
    cap_start = align["capture_start"]
    frames = align["compared_frames"]
    reference_aligned = reference[ref_start : ref_start + frames]
    capture_aligned = capture[cap_start : cap_start + frames]
    left = channel_metrics(reference_aligned[:, 0], capture_aligned[:, 0], ref_rate)
    right = channel_metrics(reference_aligned[:, 1], capture_aligned[:, 1], ref_rate)
    delay = window_lags(
        reference,
        capture,
        ref_rate,
        int(align["lag_frames"]),
        args.delay_window_seconds,
        int(round(args.delay_search_ms * ref_rate / 1000.0)),
    )
    matrix = stereo_matrix(reference_aligned, capture_aligned)
    blockers = []
    if left["clipped_frames"] or right["clipped_frames"]:
        blockers.append("capture_clipping_present")
    if abs(float(align["score"])) < args.min_alignment_score:
        blockers.append("alignment_score_below_threshold")
    if min(left["snr_db"], right["snr_db"]) < args.min_snr_db:
        blockers.append("snr_below_threshold")
    if min(left["coherence_mid_active_mean"] or 0.0, right["coherence_mid_active_mean"] or 0.0) < args.min_mid_coherence:
        blockers.append("mid_band_coherence_below_threshold")
    if delay.get("p95_abs_frames") is None or delay.get("p95_abs_frames", 999.0) > args.max_delay_p95_frames:
        blockers.append("delay_p95_above_threshold")
    if not matrix["leakage_evaluable"]:
        blockers.append("stereo_leakage_not_evaluable_reference_not_decorrelated")
    elif matrix["worst_offdiag_db_relative"] > args.max_leakage_db:
        blockers.append("stereo_leakage_above_threshold")
    return {
        "schema": "opena8djcpp.audiophile-wav-analysis.v1",
        "safety": "offline_wav_read_only_no_audio_coreaudio_usb_driver_or_hardware_touch",
        "label": args.label,
        "reference": str(args.reference),
        "capture": str(args.capture),
        "sample_rate": ref_rate,
        "analysis_seconds_requested": args.seconds,
        "alignment": align,
        "left": left,
        "right": right,
        "stereo_matrix": matrix,
        "delay_windows": delay,
        "thresholds": {
            "min_alignment_score": args.min_alignment_score,
            "min_snr_db": args.min_snr_db,
            "min_mid_coherence": args.min_mid_coherence,
            "max_delay_p95_frames": args.max_delay_p95_frames,
            "max_leakage_db": args.max_leakage_db,
        },
        "result": "PASS" if not blockers else "FAIL",
        "blockers": blockers,
        "product_claim_allowed": False,
    }


def write_json(payload: dict[str, Any], path: pathlib.Path | None) -> None:
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if path:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    print(text, end="")


def self_test(args: argparse.Namespace, degraded: bool = False) -> dict[str, Any]:
    sample_rate = 48_000
    seconds = 3.0
    rng = np.random.default_rng(8)
    sos = scipy.signal.butter(6, [30.0, 18_000.0], btype="bandpass", fs=sample_rate, output="sos")
    left = scipy.signal.sosfiltfilt(sos, rng.standard_normal(int(sample_rate * seconds)))
    right = scipy.signal.sosfiltfilt(sos, rng.standard_normal(int(sample_rate * seconds)))
    reference = np.column_stack([left, right])
    reference *= 0.20 / np.max(np.abs(reference))
    delay = 37
    if degraded:
        capture = 0.10 * rng.standard_normal(reference.shape)
    else:
        capture = np.pad(reference * 0.92, ((delay, 0), (0, 0)), mode="constant")[: reference.shape[0]]
        capture[:, 0] += 0.00003 * reference[:, 1]
        capture[:, 1] += 0.00002 * reference[:, 0]
        capture += 1.0e-5 * rng.standard_normal(capture.shape)
    with tempfile.TemporaryDirectory() as tmp:
        ref = pathlib.Path(tmp) / "reference.wav"
        cap = pathlib.Path(tmp) / "capture.wav"
        sf.write(ref, reference, sample_rate)
        sf.write(cap, capture, sample_rate)
        args.reference = ref
        args.capture = cap
        args.seconds = seconds
        args.label = "self-test-degraded" if degraded else "self-test"
        return analyze(args)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=pathlib.Path)
    parser.add_argument("--capture", type=pathlib.Path)
    parser.add_argument("--reference-channels", default="1,2")
    parser.add_argument("--capture-channels", default="1,2")
    parser.add_argument("--seconds", type=float)
    parser.add_argument("--max-lag-seconds", type=float, default=1.0)
    parser.add_argument("--delay-window-seconds", type=float, default=1.0)
    parser.add_argument("--delay-search-ms", type=float, default=8.0)
    parser.add_argument("--min-alignment-score", type=float, default=0.98)
    parser.add_argument("--min-snr-db", type=float, default=45.0)
    parser.add_argument("--min-mid-coherence", type=float, default=0.90)
    parser.add_argument("--max-delay-p95-frames", type=float, default=2.0)
    parser.add_argument("--max-leakage-db", type=float, default=-70.0)
    parser.add_argument("--label", default="")
    parser.add_argument("--json-out", type=pathlib.Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--self-test-degraded", action="store_true")
    args = parser.parse_args()
    if args.self_test or args.self_test_degraded:
        payload = self_test(args, degraded=args.self_test_degraded)
    else:
        if args.reference is None or args.capture is None:
            parser.error("--reference and --capture are required unless --self-test is used")
        payload = analyze(args)
    write_json(payload, args.json_out)
    return 0 if payload["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
