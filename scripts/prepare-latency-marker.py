#!/usr/bin/env python3
"""Generate a deterministic stereo marker WAV for physical latency tests."""

from __future__ import annotations

import argparse
import json
import math
import wave
from pathlib import Path


def write_wav(path: Path, rate: int, samples: list[tuple[float, float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = bytearray()
    for left, right in samples:
        left_i = int(round(max(-1.0, min(1.0, left)) * 32767.0))
        right_i = int(round(max(-1.0, min(1.0, right)) * 32767.0))
        frames.extend(left_i.to_bytes(2, "little", signed=True))
        frames.extend(right_i.to_bytes(2, "little", signed=True))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(bytes(frames))


def add_tone_burst(
    samples: list[tuple[float, float]],
    rate: int,
    start_seconds: float,
    duration_seconds: float,
    freq_left: float,
    freq_right: float,
    peak: float,
) -> None:
    start = int(round(start_seconds * rate))
    frames = max(1, int(round(duration_seconds * rate)))
    for index in range(frames):
        pos = start + index
        if pos < 0 or pos >= len(samples):
            continue
        window = 0.5 - 0.5 * math.cos((2.0 * math.pi * index) / max(1, frames - 1))
        t = index / rate
        left = peak * window * math.sin(2.0 * math.pi * freq_left * t)
        right = peak * window * math.sin(2.0 * math.pi * freq_right * t)
        old_left, old_right = samples[pos]
        samples[pos] = (old_left + left, old_right + right)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--seconds", type=float, default=6.0)
    parser.add_argument("--peak", type=float, default=0.35)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    frames = max(1, int(round(args.seconds * args.rate)))
    samples = [(0.0, 0.0) for _ in range(frames)]
    markers = [
        {"start_seconds": 0.25, "duration_seconds": 0.040, "left_hz": 997.0, "right_hz": 1663.0},
        {"start_seconds": 1.25, "duration_seconds": 0.060, "left_hz": 1663.0, "right_hz": 997.0},
        {"start_seconds": 2.75, "duration_seconds": 0.080, "left_hz": 3137.0, "right_hz": 440.0},
        {"start_seconds": 4.50, "duration_seconds": 0.100, "left_hz": 440.0, "right_hz": 3137.0},
    ]
    for marker in markers:
        add_tone_burst(
            samples,
            args.rate,
            marker["start_seconds"],
            marker["duration_seconds"],
            marker["left_hz"],
            marker["right_hz"],
            args.peak,
        )

    reference = out_dir / "reference.wav"
    write_wav(reference, args.rate, samples)
    metadata = {
        "schema": "opena8djcpp.latency-marker.v1",
        "reference_wav": str(reference),
        "rate": args.rate,
        "seconds": args.seconds,
        "peak": args.peak,
        "markers": markers,
    }
    (out_dir / "source.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(metadata, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
