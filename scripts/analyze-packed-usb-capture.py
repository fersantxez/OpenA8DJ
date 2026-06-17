#!/usr/bin/env python3
"""Compare OpenA8DJ packed OUT USB diagnostics against iRig capture."""

from __future__ import annotations

import argparse
import json
import math
import wave
from pathlib import Path

import numpy as np


STREAMS = 4
CHANNELS_PER_STREAM = 2
FRAME_BYTES = 6


def read_wav_pair(path: Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as wav:
        rate = wav.getframerate()
        channels = wav.getnchannels()
        frames = wav.getnframes()
        raw = wav.readframes(frames)
    data = np.frombuffer(raw, dtype="<i2").astype(np.float64).reshape(-1, channels) / 32768.0
    if channels == 1:
        data = np.column_stack([data[:, 0], data[:, 0]])
    return rate, data[:, :2]


def s24_to_float(values: bytes, native_order: bool) -> float:
    if native_order:
        raw = values[0] | (values[1] << 8) | (values[2] << 16)
    else:
        raw = (values[0] << 16) | (values[1] << 8) | values[2]
    if raw & 0x800000:
        raw |= ~0x00FFFFFF
    return float(raw) / 8388608.0


def mode2_check_byte(stream: int, byte_index: int) -> int:
    group = byte_index // 16
    return ((stream << 1) | ((~group) & 1)) & 0xFF


def decode_mode2_pair(
    data: bytes,
    *,
    pair_index: int,
    check_offset: int,
    start_byte: int,
    native_order: bool,
) -> dict:
    pending = [[0 for _ in range(FRAME_BYTES)] for _ in range(STREAMS)]
    present = [[False for _ in range(FRAME_BYTES)] for _ in range(STREAMS)]
    byte_position = start_byte
    lane_streams = 0
    frames: list[tuple[float, float]] = []
    checks = 0
    check_errors = 0
    panic_flags = 0
    for index, value in enumerate(data):
        group_offset = index % 16
        if check_offset <= group_offset < check_offset + STREAMS:
            stream = group_offset - check_offset
            checks += 1
            if value & 0x80:
                panic_flags += 1
            if (value & 0x3F) != mode2_check_byte(stream, index):
                check_errors += 1
            continue
        stream = group_offset % STREAMS
        if stream == 0 and byte_position == 0:
            present = [[False for _ in range(FRAME_BYTES)] for _ in range(STREAMS)]
            lane_streams = 0
        pending[stream][byte_position] = value
        present[stream][byte_position] = True
        lane_streams += 1
        if lane_streams == STREAMS:
            if byte_position == FRAME_BYTES - 1:
                complete = all(all(row) for row in present)
                if complete:
                    stream_bytes = bytes(pending[pair_index])
                    frames.append(
                        (
                            s24_to_float(stream_bytes[:3], native_order),
                            s24_to_float(stream_bytes[3:], native_order),
                        )
                    )
                present = [[False for _ in range(FRAME_BYTES)] for _ in range(STREAMS)]
            byte_position = (byte_position + 1) % FRAME_BYTES
            lane_streams = 0
    return {
        "frames": np.asarray(frames, dtype=np.float64),
        "checks": checks,
        "check_errors": check_errors,
        "panic_flags": panic_flags,
    }


def rms(values: np.ndarray) -> float:
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values))))


def mono(samples: np.ndarray) -> np.ndarray:
    return np.mean(samples, axis=1)


def best_alignment(reference: np.ndarray, capture: np.ndarray, step: int, max_start: int | None) -> dict:
    ref = mono(reference)
    ref = ref - np.mean(ref)
    ref_norm = float(np.sqrt(np.sum(ref * ref)))
    if ref_norm <= 1e-18:
        return {"start": 0, "correlation": 0.0}
    limit = len(capture) - len(reference)
    if max_start is not None:
        limit = min(limit, max_start)
    if limit < 0:
        return {"start": 0, "correlation": 0.0}
    cap_mono = mono(capture)
    best = {"start": 0, "correlation": 0.0}
    for start in range(0, limit + 1, max(1, step)):
        segment = cap_mono[start : start + len(ref)]
        segment = segment - np.mean(segment)
        denom = float(np.sqrt(np.sum(segment * segment)) * ref_norm)
        if denom <= 1e-18:
            continue
        corr = float(np.dot(ref, segment) / denom)
        if abs(corr) > abs(best["correlation"]):
            best = {"start": start, "correlation": corr}
    return best


def fit(reference: np.ndarray, capture: np.ndarray) -> dict:
    frames = min(len(reference), len(capture))
    if frames == 0:
        return {
            "frames": 0,
            "matrix": [[0.0, 0.0], [0.0, 0.0]],
            "linear_fit_snr_db": -999.0,
            "residual_over_capture_rms": 999.0,
        }
    ref = reference[:frames]
    cap = capture[:frames]
    matrix, *_ = np.linalg.lstsq(ref, cap, rcond=None)
    predicted = ref @ matrix
    residual = cap - predicted
    linear_snr = 20.0 * math.log10((rms(predicted) + 1e-15) / (rms(residual) + 1e-15))
    residual_ratio = rms(residual) / (rms(cap) + 1e-15)
    return {
        "frames": frames,
        "matrix": matrix.tolist(),
        "reference_rms": rms(ref),
        "capture_rms": rms(cap),
        "predicted_rms": rms(predicted),
        "residual_rms": rms(residual),
        "linear_fit_snr_db": linear_snr,
        "residual_over_capture_rms": residual_ratio,
    }


def parse_pair(value: str) -> int:
    pair = value.upper()
    if pair not in {"A", "B", "C", "D"}:
        raise argparse.ArgumentTypeError("pair must be A, B, C, or D")
    return ord(pair) - ord("A")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--packed-usb", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--pair", type=parse_pair, default=0)
    parser.add_argument("--check-offset", type=int, default=8)
    parser.add_argument("--start-byte", type=int, default=4)
    parser.add_argument("--native-order", action="store_true")
    parser.add_argument("--max-bytes", type=int, default=0)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    rate, reference = read_wav_pair(args.reference)
    capture_rate, capture = read_wav_pair(args.capture)
    if capture_rate != rate:
        raise SystemExit(f"sample-rate mismatch: reference={rate} capture={capture_rate}")

    raw = args.packed_usb.read_bytes()
    if args.max_bytes > 0:
        raw = raw[: args.max_bytes]
    decoded = decode_mode2_pair(
        raw,
        pair_index=args.pair,
        check_offset=args.check_offset,
        start_byte=args.start_byte,
        native_order=args.native_order,
    )
    usb_frames = decoded["frames"]
    usb_reference_alignment = best_alignment(
        reference,
        usb_frames,
        step=max(1, rate // 1000),
        max_start=rate * 5,
    )
    usb_reference_start = int(usb_reference_alignment["start"])
    usb_vs_reference = fit(
        reference,
        usb_frames[usb_reference_start : usb_reference_start + len(reference)],
    )
    usb_capture_alignment = best_alignment(
        usb_frames[: min(len(usb_frames), len(capture))],
        capture,
        step=max(1, rate // 200),
        max_start=rate * 5,
    )
    start = int(usb_capture_alignment["start"])
    usb_vs_capture = fit(usb_frames, capture[start : start + len(usb_frames)])
    payload = {
        "schema": "opena8djcpp.packed-usb-capture.v1",
        "reference": str(args.reference),
        "packed_usb": str(args.packed_usb),
        "capture": str(args.capture),
        "sample_rate": rate,
        "pair": chr(ord("A") + args.pair),
        "decoder": {
            "check_offset": args.check_offset,
            "start_byte": args.start_byte,
            "byte_order": "native" if args.native_order else "big",
            "raw_bytes": len(raw),
            "decoded_frames": int(len(usb_frames)),
            "checks": decoded["checks"],
            "check_errors": decoded["check_errors"],
            "panic_flags": decoded["panic_flags"],
        },
        "usb_vs_reference": {
            "usb_start_seconds": usb_reference_start / rate,
            "correlation": usb_reference_alignment["correlation"],
            **usb_vs_reference,
        },
        "usb_vs_capture": {
            "capture_start_seconds": start / rate,
            "correlation": usb_capture_alignment["correlation"],
            **usb_vs_capture,
        },
        "result": "PASS"
        if decoded["check_errors"] == 0
        and decoded["panic_flags"] == 0
        and usb_vs_reference["linear_fit_snr_db"] >= 75.0
        and usb_vs_capture["linear_fit_snr_db"] >= 35.0
        and usb_vs_capture["residual_over_capture_rms"] <= 0.10
        else "FAIL",
    }
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
