#!/usr/bin/env python3
"""Analyze decorrelated channel-matrix captures in the frequency domain.

The generated channel-matrix fixture uses disjoint L/R tone sets. Measuring
those tones is robust to analog latency and polarity, unlike sample-by-sample
linear fitting.
"""

import argparse
import importlib.util
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ANALYZER_PATH = ROOT / "scripts/analyze-soundcheck-capture.py"
LEFT_TONES = (110.0, 440.0, 997.0, 3137.0, 7210.0)
RIGHT_TONES = (173.0, 661.0, 1663.0, 5003.0, 9181.0)


def load_analyzer():
    spec = importlib.util.spec_from_file_location("soundcheck_analyzer", ANALYZER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def channel(pair, index):
    return [sample[index] for sample in pair]


def tone_amplitude(samples, rate, frequency):
    if not samples:
        return 0.0
    cosine = 0.0
    sine = 0.0
    for index, sample in enumerate(samples):
        phase = 2.0 * math.pi * frequency * (index / rate)
        cosine += sample * math.cos(phase)
        sine += sample * math.sin(phase)
    return 2.0 * math.hypot(cosine, sine) / len(samples)


def tone_set(samples, rate, tones):
    return {str(int(freq)): tone_amplitude(samples, rate, freq) for freq in tones}


def max_value(values):
    return max(values.values()) if values else 0.0


def ratio_db(numerator, denominator):
    if numerator <= 0.0:
        return -240.0
    if denominator <= 0.0:
        return 240.0
    return 20.0 * math.log10(numerator / denominator)


def analyze(run_dir, skip_seconds, analysis_seconds, max_leakage_db, min_expected_amp):
    analyzer = load_analyzer()
    ref_rate, ref = analyzer.read_wav_pair(str(run_dir / "fixture/reference.wav"))
    cap_rate, cap = analyzer.read_wav_pair(str(run_dir / "captured.wav"))
    if cap_rate != ref_rate:
        cap = analyzer.resample_pair_linear(cap, cap_rate, ref_rate)
    rate = ref_rate
    ref_frames = len(ref)
    start = max(0, int(round(skip_seconds * rate)))
    frames = ref_frames
    if analysis_seconds > 0.0:
        frames = min(frames, int(round(analysis_seconds * rate)))
    capture_window = cap[start:start + frames]
    reference_window = ref[:frames]

    ref_left = channel(reference_window, 0)
    ref_right = channel(reference_window, 1)
    cap_left = channel(capture_window, 0)
    cap_right = channel(capture_window, 1)

    ref_left_tones = tone_set(ref_left, rate, LEFT_TONES)
    ref_right_tones = tone_set(ref_right, rate, RIGHT_TONES)
    cap_left_left_tones = tone_set(cap_left, rate, LEFT_TONES)
    cap_left_right_tones = tone_set(cap_left, rate, RIGHT_TONES)
    cap_right_left_tones = tone_set(cap_right, rate, LEFT_TONES)
    cap_right_right_tones = tone_set(cap_right, rate, RIGHT_TONES)

    left_expected = max_value(cap_left_left_tones)
    right_expected = max_value(cap_right_right_tones)
    left_wrong_source = max_value(cap_left_right_tones)
    right_wrong_source = max_value(cap_right_left_tones)
    left_to_right_leakage = max_value(cap_right_left_tones)
    right_to_left_leakage = max_value(cap_left_right_tones)
    max_leakage = max(left_wrong_source, right_wrong_source)
    expected_floor = min(left_expected, right_expected)
    max_leakage_relative_db = ratio_db(max_leakage, max(left_expected, right_expected))
    left_leakage_db = ratio_db(left_to_right_leakage, left_expected)
    right_leakage_db = ratio_db(right_to_left_leakage, right_expected)
    clipped = sum(1 for left, right in capture_window
                  if abs(left) >= 0.999 or abs(right) >= 0.999)
    pass_gate = (
        expected_floor >= min_expected_amp and
        max(left_leakage_db, right_leakage_db, max_leakage_relative_db) <= max_leakage_db and
        clipped == 0
    )
    return {
        "schema": "opena8djcpp.channel-matrix-tones.v1",
        "result": "PASS" if pass_gate else "FAIL",
        "run_dir": str(run_dir),
        "rate": rate,
        "skip_seconds": skip_seconds,
        "analysis_seconds": len(capture_window) / rate if rate > 0 else 0.0,
        "frames": len(capture_window),
        "thresholds": {
            "max_leakage_db": max_leakage_db,
            "min_expected_amplitude": min_expected_amp,
        },
        "reference": {
            "left_tones": ref_left_tones,
            "right_tones": ref_right_tones,
        },
        "capture": {
            "left_channel_left_tones": cap_left_left_tones,
            "left_channel_right_tones": cap_left_right_tones,
            "right_channel_left_tones": cap_right_left_tones,
            "right_channel_right_tones": cap_right_right_tones,
        },
        "metrics": {
            "left_expected_max_amplitude": left_expected,
            "right_expected_max_amplitude": right_expected,
            "expected_floor_amplitude": expected_floor,
            "left_to_right_leakage_db": left_leakage_db,
            "right_to_left_leakage_db": right_leakage_db,
            "max_wrong_source_leakage_db": max_leakage_relative_db,
            "capture_clipped_frames": clipped,
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--skip-seconds", type=float, default=1.0)
    parser.add_argument("--analysis-seconds", type=float, default=8.0)
    parser.add_argument("--max-leakage-db", type=float, default=-45.0)
    parser.add_argument("--min-expected-amplitude", type=float, default=0.005)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    result = analyze(args.run_dir,
                     args.skip_seconds,
                     args.analysis_seconds,
                     args.max_leakage_db,
                     args.min_expected_amplitude)
    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
