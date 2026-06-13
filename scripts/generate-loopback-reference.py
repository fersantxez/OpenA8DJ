#!/usr/bin/env python3
import argparse
import math
import struct
import wave


def clamp(value):
    return max(-1.0, min(1.0, value))


def envelope(index, frames, rate):
    fade = int(rate * 0.050)
    if fade <= 0:
        return 1.0
    start = min(1.0, index / fade)
    end = min(1.0, (frames - 1 - index) / fade)
    return max(0.0, min(start, end))


def sample_channel(t, tones, pulse_offset):
    value = 0.0
    for freq, amp, phase in tones:
        value += amp * math.sin((2.0 * math.pi * freq * t) + phase)

    # Sparse deterministic transients catch byte slips and drop/replay artifacts.
    pulse_period = 0.733
    pulse_width = 0.004
    phase = (t + pulse_offset) % pulse_period
    if phase < pulse_width:
        value += 0.16 * (1.0 - phase / pulse_width)
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--peak", type=float, default=0.30)
    args = parser.parse_args()

    frames = max(1, int(round(args.rate * args.seconds)))
    left_tones = (
        (110.0, 0.16, 0.0),
        (440.0, 0.10, 0.3),
        (997.0, 0.08, 1.1),
        (3137.0, 0.05, 2.0),
        (7210.0, 0.025, 0.7),
    )
    right_tones = (
        (173.0, 0.14, 0.5),
        (661.0, 0.10, 1.3),
        (1663.0, 0.07, 0.1),
        (5003.0, 0.045, 2.2),
        (9181.0, 0.022, 1.0),
    )

    floats = []
    peak = 0.0
    for index in range(frames):
        t = index / args.rate
        env = envelope(index, frames, args.rate)
        left = env * sample_channel(t, left_tones, 0.000)
        right = env * sample_channel(t, right_tones, 0.231)
        floats.append((left, right))
        peak = max(peak, abs(left), abs(right))

    scale = args.peak / peak if peak > 0.0 else 1.0
    with wave.open(args.output, "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(args.rate)
        for left, right in floats:
            wav.writeframesraw(struct.pack(
                "<hh",
                int(round(clamp(left * scale) * 32767.0)),
                int(round(clamp(right * scale) * 32767.0)),
            ))

    print(f"reference={args.output}")
    print(f"rate={args.rate}")
    print(f"frames={frames}")
    print(f"channels=2")
    print(f"peak={args.peak:.6f}")


if __name__ == "__main__":
    main()
