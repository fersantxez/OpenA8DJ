#!/usr/bin/env python3
"""Detect marker burst peaks in reference/capture WAVs and estimate offset."""

from __future__ import annotations

import argparse
import json
import wave
from pathlib import Path

import numpy as np


def read_wav(path: Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as wav:
        rate = wav.getframerate()
        channels = wav.getnchannels()
        data = wav.readframes(wav.getnframes())
    samples = np.frombuffer(data, dtype="<i2").astype(np.float64).reshape(-1, channels) / 32768.0
    if channels == 1:
        samples = np.column_stack([samples[:, 0], samples[:, 0]])
    return rate, samples[:, :2]


def envelope(samples: np.ndarray, window_frames: int) -> np.ndarray:
    mono = np.mean(samples, axis=1)
    kernel = np.ones(max(1, window_frames), dtype=np.float64) / max(1, window_frames)
    return np.sqrt(np.convolve(mono * mono, kernel, mode="same"))


def merge_nearby_peaks(peaks: list[dict], merge_gap_seconds: float) -> list[dict]:
    if not peaks:
        return []
    merged = []
    group = [peaks[0]]
    for peak in peaks[1:]:
        if peak["seconds"] - group[-1]["seconds"] <= merge_gap_seconds:
            group.append(peak)
            continue
        merged.append(max(group, key=lambda item: item["rms"]))
        group = [peak]
    merged.append(max(group, key=lambda item: item["rms"]))
    return merged


def detect_peaks(
    samples: np.ndarray,
    rate: int,
    window_ms: float,
    relative_threshold: float,
    merge_gap_ms: float,
) -> dict:
    env = envelope(samples, max(1, int(round(rate * window_ms / 1000.0))))
    threshold = max(0.005, float(env.max()) * relative_threshold)
    peaks = []
    index = 0
    while index < len(env):
        if env[index] >= threshold:
            end = index
            while end < len(env) and env[end] >= threshold:
                end += 1
            peak_index = index + int(np.argmax(env[index:end]))
            peaks.append({
                "seconds": peak_index / rate,
                "rms": float(env[peak_index]),
            })
            index = end + 1
        else:
            index += 1
    merged_peaks = merge_nearby_peaks(peaks, merge_gap_ms / 1000.0)
    return {
        "max_rms": float(env.max()),
        "threshold": threshold,
        "raw_peaks": peaks,
        "peaks": merged_peaks,
        "raw_peak_count": len(peaks),
        "merged_peak_count": len(merged_peaks),
        "merge_gap_ms": merge_gap_ms,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--window-ms", type=float, default=5.0)
    parser.add_argument("--relative-threshold", type=float, default=0.25)
    parser.add_argument("--merge-gap-ms", type=float, default=120.0)
    parser.add_argument("--min-paired-peaks", type=int, default=4)
    parser.add_argument("--max-offset-std-seconds", type=float, default=0.025)
    parser.add_argument("--max-mean-offset-seconds", type=float, default=1.5)
    parser.add_argument("--record-preroll-seconds", type=float, default=0.0)
    parser.add_argument("--playback-lead-frames", type=int)
    parser.add_argument("--startup-silence-frames", type=int)
    args = parser.parse_args()

    ref_rate, reference = read_wav(args.reference)
    cap_rate, capture = read_wav(args.capture)
    if ref_rate != cap_rate:
        raise SystemExit(f"sample-rate mismatch: reference={ref_rate} capture={cap_rate}")

    ref = detect_peaks(reference, ref_rate, args.window_ms, args.relative_threshold, args.merge_gap_ms)
    cap = detect_peaks(capture, cap_rate, args.window_ms, args.relative_threshold, args.merge_gap_ms)
    pair_count = min(len(ref["peaks"]), len(cap["peaks"]))
    offsets = [
        cap["peaks"][index]["seconds"] - ref["peaks"][index]["seconds"]
        for index in range(pair_count)
    ]
    offset_mean = float(np.mean(offsets)) if offsets else None
    offset_std = float(np.std(offsets)) if offsets else None
    enough_peaks = pair_count >= args.min_paired_peaks
    stable_offset = enough_peaks and offset_std is not None and offset_std <= args.max_offset_std_seconds
    acceptable_offset = (
        stable_offset and
        offset_mean is not None and
        abs(offset_mean) <= args.max_mean_offset_seconds
    )
    lead_seconds = (
        args.playback_lead_frames / ref_rate
        if args.playback_lead_frames is not None
        else None
    )
    startup_silence_seconds = (
        args.startup_silence_frames / ref_rate
        if args.startup_silence_frames is not None
        else None
    )
    offset_after_record_preroll = (
        offset_mean - args.record_preroll_seconds
        if offset_mean is not None
        else None
    )
    expected_internal_seconds = None
    if lead_seconds is not None or startup_silence_seconds is not None:
        expected_internal_seconds = (lead_seconds or 0.0) + (startup_silence_seconds or 0.0)
    offset_after_preroll_and_internal = (
        offset_after_record_preroll - expected_internal_seconds
        if offset_after_record_preroll is not None and expected_internal_seconds is not None
        else None
    )
    payload = {
        "schema": "opena8djcpp.latency-marker-peaks.v1",
        "result": "PASS" if acceptable_offset else "FAIL",
        "stability_result": "PASS" if stable_offset else "FAIL",
        "readiness_result": "PASS" if acceptable_offset else "FAIL",
        "reference": str(args.reference),
        "capture": str(args.capture),
        "sample_rate": ref_rate,
        "reference_peaks": ref,
        "capture_peaks": cap,
        "paired_peaks": pair_count,
        "offsets_seconds": offsets,
        "offset_mean_seconds": offset_mean,
        "offset_std_seconds": offset_std,
        "record_preroll_seconds": args.record_preroll_seconds,
        "offset_mean_minus_record_preroll_seconds": offset_after_record_preroll,
        "playback_lead_frames": args.playback_lead_frames,
        "playback_lead_seconds": lead_seconds,
        "startup_silence_frames": args.startup_silence_frames,
        "startup_silence_seconds": startup_silence_seconds,
        "expected_internal_seconds": expected_internal_seconds,
        "offset_mean_minus_record_preroll_and_internal_seconds": offset_after_preroll_and_internal,
        "min_paired_peaks": args.min_paired_peaks,
        "max_offset_std_seconds": args.max_offset_std_seconds,
        "max_mean_offset_seconds": args.max_mean_offset_seconds,
        "gates": [
            {
                "name": "paired_marker_peaks",
                "result": "PASS" if enough_peaks else "FAIL",
                "value": pair_count,
                "threshold": f">= {args.min_paired_peaks}",
                "reason": "all expected marker bursts must be visible in the external capture",
            },
            {
                "name": "marker_offset_stability",
                "result": "PASS" if stable_offset else "FAIL",
                "value": offset_std,
                "threshold": f"<= {args.max_offset_std_seconds}",
                "reason": "stable offset is diagnostic evidence; it is not sufficient for readiness",
            },
            {
                "name": "marker_mean_offset",
                "result": "PASS" if acceptable_offset else "FAIL",
                "value": offset_mean,
                "threshold": f"abs(offset) <= {args.max_mean_offset_seconds}",
                "reason": "large stable latency blocks audiophile/interactive readiness",
            },
        ],
    }
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text, encoding="utf-8")
    print(text, end="")
    return 0 if payload["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
