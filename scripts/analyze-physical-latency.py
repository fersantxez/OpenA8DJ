#!/usr/bin/env python3
"""Measure physical capture latency and aligned tone/correlation quality."""

from __future__ import annotations

import argparse
import json
import math
import wave
from pathlib import Path

import numpy as np


def read_wav_pair(path: Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as wav:
        rate = wav.getframerate()
        channels = wav.getnchannels()
        frames = wav.getnframes()
        data = wav.readframes(frames)
    if channels < 1:
        raise SystemExit(f"{path}: no channels")
    samples = np.frombuffer(data, dtype="<i2").astype(np.float64).reshape(-1, channels) / 32768.0
    if channels == 1:
        samples = np.column_stack([samples[:, 0], samples[:, 0]])
    else:
        samples = samples[:, :2]
    return rate, samples


def rms(values: np.ndarray) -> float:
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values))))


def first_energy_time(samples: np.ndarray, rate: int, threshold: float, window_ms: float) -> tuple[float | None, float]:
    mono = np.mean(samples, axis=1)
    window = max(1, int(rate * window_ms / 1000.0))
    best = 0.0
    for start in range(0, max(1, len(mono) - window + 1), window):
        value = rms(mono[start:start + window])
        best = max(best, value)
        if value >= threshold:
            return start / rate, value
    return None, best


def best_aligned_window(reference: np.ndarray, capture: np.ndarray, rate: int, step_ms: float) -> dict:
    ref = np.mean(reference, axis=1)
    ref = ref - np.mean(ref)
    ref_norm = float(np.sqrt(np.sum(ref * ref)))
    if ref_norm == 0.0 or len(capture) < len(reference):
        return {"capture_start_seconds": None, "correlation": 0.0}
    cap_mono = np.mean(capture, axis=1)
    step = max(1, int(rate * step_ms / 1000.0))
    best = {"capture_start_seconds": 0.0, "correlation": 0.0}
    for start in range(0, len(cap_mono) - len(ref) + 1, step):
        segment = cap_mono[start:start + len(ref)]
        segment = segment - np.mean(segment)
        denom = float(np.sqrt(np.sum(segment * segment)) * ref_norm)
        if denom <= 1e-18:
            continue
        corr = float(np.dot(segment, ref) / denom)
        if abs(corr) > abs(best["correlation"]):
            best = {"capture_start_seconds": start / rate, "correlation": corr}
    return best


def band_tone_amplitudes(samples: np.ndarray, rate: int, start: int, frames: int, freqs: list[float]) -> dict[str, float]:
    segment = samples[start:start + frames]
    if len(segment) == 0:
        return {str(int(freq)): 0.0 for freq in freqs}
    mono = np.mean(segment, axis=1)
    window = np.hanning(len(mono))
    spectrum = np.abs(np.fft.rfft(mono * window))
    result = {}
    for freq in freqs:
        index = int(round(freq * len(mono) / rate))
        index = max(0, min(index, len(spectrum) - 1))
        result[str(int(freq))] = float(spectrum[index] / max(1.0, np.sum(window) / 2.0))
    return result


def gate(name: str, passed: bool, value, threshold, reason: str = "") -> dict:
    return {
        "name": name,
        "result": "PASS" if passed else "FAIL",
        "value": value,
        "threshold": threshold,
        "reason": reason,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--energy-threshold", type=float, default=0.005)
    parser.add_argument("--energy-window-ms", type=float, default=50.0)
    parser.add_argument("--correlation-step-ms", type=float, default=25.0)
    parser.add_argument("--max-first-energy-seconds", type=float, default=1.5)
    parser.add_argument("--min-abs-correlation", type=float, default=0.98)
    parser.add_argument("--min-aligned-snr-db", type=float, default=35.0)
    parser.add_argument("--min-linear-fit-snr-db", type=float, default=35.0)
    parser.add_argument("--max-linear-residual-over-capture-rms", type=float, default=0.10)
    args = parser.parse_args()

    ref_rate, reference = read_wav_pair(args.reference)
    cap_rate, capture = read_wav_pair(args.capture)
    if ref_rate != cap_rate:
        raise SystemExit(f"sample-rate mismatch: reference={ref_rate} capture={cap_rate}")

    first_time, first_rms = first_energy_time(
        capture,
        cap_rate,
        args.energy_threshold,
        args.energy_window_ms,
    )
    alignment = best_aligned_window(reference, capture, cap_rate, args.correlation_step_ms)
    start_frame = 0
    if alignment["capture_start_seconds"] is not None:
        start_frame = int(round(alignment["capture_start_seconds"] * cap_rate))
    frames = min(len(reference), max(0, len(capture) - start_frame))
    aligned_capture = capture[start_frame:start_frame + frames]
    aligned_ref = reference[:frames]
    residual = aligned_capture - aligned_ref if frames > 0 else np.zeros((0, 2))
    if frames > 0:
        matrix, *_ = np.linalg.lstsq(aligned_ref, aligned_capture, rcond=None)
        predicted = aligned_ref @ matrix
        linear_residual = aligned_capture - predicted
    else:
        matrix = np.zeros((2, 2))
        predicted = np.zeros((0, 2))
        linear_residual = np.zeros((0, 2))
    tone_freqs = [110, 173, 440, 661, 997, 1663, 3137, 5003, 7210, 9181]

    aligned_snr_db = 20.0 * math.log10((rms(aligned_ref) + 1e-15) / (rms(residual) + 1e-15))
    linear_fit_snr_db = 20.0 * math.log10((rms(predicted) + 1e-15) / (rms(linear_residual) + 1e-15))
    linear_residual_over_capture_rms = rms(linear_residual) / (rms(aligned_capture) + 1e-15)
    gates = [
        gate(
            "first_energy_seconds",
            first_time is not None and first_time <= args.max_first_energy_seconds,
            first_time,
            f"<= {args.max_first_energy_seconds}",
            "physical energy must appear promptly after the wrapper pre-roll",
        ),
        gate(
            "best_abs_correlation",
            abs(alignment["correlation"]) >= args.min_abs_correlation,
            abs(alignment["correlation"]),
            f">= {args.min_abs_correlation}",
            "negative polarity can be diagnosed separately, but weak absolute correlation blocks readiness",
        ),
        gate(
            "aligned_snr_db",
            aligned_snr_db >= args.min_aligned_snr_db,
            aligned_snr_db,
            f">= {args.min_aligned_snr_db}",
        ),
        gate(
            "linear_fit_snr_db",
            linear_fit_snr_db >= args.min_linear_fit_snr_db,
            linear_fit_snr_db,
            f">= {args.min_linear_fit_snr_db}",
            "a static 2x2 gain/polarity fit is not enough if the residual remains high",
        ),
        gate(
            "linear_residual_over_capture_rms",
            linear_residual_over_capture_rms <= args.max_linear_residual_over_capture_rms,
            linear_residual_over_capture_rms,
            f"<= {args.max_linear_residual_over_capture_rms}",
        ),
    ]

    payload = {
        "schema": "opena8djcpp.physical-latency.v1",
        "result": "PASS" if all(item["result"] == "PASS" for item in gates) else "FAIL",
        "gates": gates,
        "reference": str(args.reference),
        "capture": str(args.capture),
        "sample_rate": ref_rate,
        "reference_frames": int(len(reference)),
        "capture_frames": int(len(capture)),
        "first_energy_seconds": first_time,
        "first_energy_rms": first_rms,
        "best_capture_start_seconds": alignment["capture_start_seconds"],
        "best_correlation": alignment["correlation"],
        "aligned_frames": int(frames),
        "aligned_reference_rms": rms(aligned_ref),
        "aligned_capture_rms": rms(aligned_capture),
        "aligned_residual_rms": rms(residual),
        "aligned_snr_db": aligned_snr_db,
        "linear_matrix_ref_to_capture": matrix.tolist(),
        "linear_predicted_rms": rms(predicted),
        "linear_residual_rms": rms(linear_residual),
        "linear_fit_snr_db": linear_fit_snr_db,
        "linear_residual_over_capture_rms": linear_residual_over_capture_rms,
        "capture_tone_amplitudes": band_tone_amplitudes(capture, cap_rate, start_frame, frames, tone_freqs),
    }
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
