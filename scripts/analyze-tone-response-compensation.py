#!/usr/bin/env python3
"""Estimate whether physical tone response explains soundcheck residuals.

This tool is offline-only. It reads an existing channel-matrix tone analysis
and already-recorded soundcheck WAVs, then compares the normal scalar-gain
residual with a coarse per-band response model derived from the tone run.
"""

import argparse
import importlib.util
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ANALYZER_PATH = ROOT / "scripts/analyze-soundcheck-capture.py"


def load_analyzer():
    spec = importlib.util.spec_from_file_location("soundcheck_analyzer", ANALYZER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def find_reference(run_dir):
    direct = run_dir / "fixture/reference.wav"
    if direct.exists():
        return direct
    prepare = run_dir / "prepare.log"
    if prepare.exists():
        for line in prepare.read_text().splitlines():
            if line.startswith("reference="):
                path = Path(line.split("=", 1)[1])
                if path.exists():
                    return path
                candidate = run_dir / path
                if candidate.exists():
                    return candidate
    raise SystemExit(f"cannot find reference WAV for {run_dir}")


def gain_points(tone_data, channel_name):
    if channel_name == "left":
        ref = tone_data["reference"]["left_tones"]
        cap = tone_data["capture"]["left_channel_left_tones"]
    else:
        ref = tone_data["reference"]["right_tones"]
        cap = tone_data["capture"]["right_channel_right_tones"]
    points = []
    for freq_text, ref_amp in ref.items():
        freq = float(freq_text)
        cap_amp = float(cap.get(freq_text, 0.0))
        if ref_amp > 1e-12:
            points.append((freq, cap_amp / float(ref_amp)))
    return sorted(points)


def geometric_interp(points, frequency):
    if not points:
        return 1.0
    if frequency <= points[0][0]:
        return points[0][1]
    if frequency >= points[-1][0]:
        return points[-1][1]
    for (f0, g0), (f1, g1) in zip(points, points[1:]):
        if frequency <= f1:
            span = math.log(f1 / f0)
            if span <= 0.0:
                return g0
            t = math.log(frequency / f0) / span
            if g0 <= 0.0 or g1 <= 0.0:
                return g0 + (g1 - g0) * t
            return math.exp(math.log(g0) + (math.log(g1) - math.log(g0)) * t)
    return points[-1][1]


def split_bands(analyzer, values, rate):
    low = analyzer.apply_biquad(values, analyzer.biquad_coefficients("lowpass", rate, 1000.0))
    high = analyzer.apply_biquad(values, analyzer.biquad_coefficients("highpass", rate, 5000.0))
    mid_total = analyzer.bandpass(values, rate, 1000.0, 5000.0)
    return {
        "low": low,
        "mid": mid_total,
        "high": high,
    }


def combine_bands(bands, gains):
    length = min(len(value) for value in bands.values())
    out = []
    for index in range(length):
        out.append(
            bands["low"][index] * gains["low"] +
            bands["mid"][index] * gains["mid"] +
            bands["high"][index] * gains["high"]
        )
    return out


def channel_metrics(analyzer, ref, got, predicted, rate):
    usable = min(len(ref), len(got), len(predicted))
    ref = ref[:usable]
    got = got[:usable]
    predicted = predicted[:usable]
    scalar = analyzer.compare_channel(ref, got, rate, 1000.0, 5000.0, 5000.0, 12000.0)
    predicted_power = sum(value * value for value in predicted)
    predicted_gain = (sum(p * g for p, g in zip(predicted, got)) / predicted_power
                      if predicted_power > 0.0 else 0.0)
    response_signal = [predicted_gain * value for value in predicted]
    residual = [g - p for g, p in zip(got, response_signal)]
    signal_rms = analyzer.rms(response_signal)
    residual_rms = analyzer.rms(residual)
    mid_signal = analyzer.band_rms(predicted, rate, 1000.0, 5000.0)
    mid_residual = analyzer.band_rms(residual, rate, 1000.0, 5000.0)
    high_signal = analyzer.band_rms(predicted, rate, 5000.0, 12000.0)
    high_residual = analyzer.band_rms(residual, rate, 5000.0, 12000.0)
    click_threshold = max(0.01, residual_rms * 12.0)
    return {
        "scalar_gain": scalar["gain"],
        "scalar_snr_db": scalar["snr_db"],
        "scalar_mid_residual_ratio": scalar["mid_band_residual_ratio"],
        "scalar_high_residual_ratio": scalar["high_band_residual_ratio"],
        "scalar_residual_rms": scalar["residual_rms"],
        "response_scalar_gain": predicted_gain,
        "response_snr_db": 20.0 * math.log10(signal_rms / residual_rms)
        if signal_rms > 0.0 and residual_rms > 0.0 else 999.0,
        "response_mid_residual_ratio": mid_residual / mid_signal if mid_signal > 1e-9 else 0.0,
        "response_high_residual_ratio": high_residual / high_signal if high_signal > 1e-9 else 0.0,
        "response_residual_rms": residual_rms,
        "response_click_outliers": sum(1 for value in residual if abs(value) > click_threshold),
    }


def analyze_run(analyzer, tone_data, run_dir, max_seconds):
    reference_path = find_reference(run_dir)
    capture_path = run_dir / "captured.wav"
    if not capture_path.exists():
        raise SystemExit(f"missing capture WAV: {capture_path}")
    ref_rate, ref_pair = analyzer.read_wav_pair(str(reference_path))
    got_rate, got_pair = analyzer.read_wav_pair(str(capture_path))
    if ref_rate != got_rate:
        raise SystemExit(f"rate mismatch for {run_dir}: reference={ref_rate} capture={got_rate}")
    rate = ref_rate
    ref_start = analyzer.first_signal_index(ref_pair)
    got_rough_start = analyzer.first_signal_index(got_pair)
    fit_frames = min(len(ref_pair) - ref_start, int(rate * 0.1), int(max_seconds * rate))
    ref_mono = analyzer.pair_to_mono(ref_pair)
    got_mono = analyzer.pair_to_mono(got_pair)
    lag, _score = analyzer.find_best_lag(ref_mono,
                                         got_mono,
                                         ref_start,
                                         got_rough_start,
                                         8192,
                                         fit_frames,
                                         max(1, fit_frames // 512))
    got_start = got_rough_start + lag
    ref_start, got_start = analyzer.normalize_starts(ref_start, got_start)
    usable = min(len(ref_pair) - ref_start, len(got_pair) - got_start, int(max_seconds * rate))
    ref_window = ref_pair[ref_start:ref_start + usable]
    got_window = got_pair[got_start:got_start + usable]
    left_points = gain_points(tone_data, "left")
    right_points = gain_points(tone_data, "right")
    band_centers = {"low": 440.0, "mid": 3137.0, "high": 7210.0}
    left_gains = {name: geometric_interp(left_points, freq) for name, freq in band_centers.items()}
    right_gains = {name: geometric_interp(right_points, freq) for name, freq in band_centers.items()}
    ref_left = [sample[0] for sample in ref_window]
    ref_right = [sample[1] for sample in ref_window]
    got_left = [sample[0] for sample in got_window]
    got_right = [sample[1] for sample in got_window]
    predicted_left = combine_bands(split_bands(analyzer, ref_left, rate), left_gains)
    predicted_right = combine_bands(split_bands(analyzer, ref_right, rate), right_gains)
    left = channel_metrics(analyzer, ref_left, got_left, predicted_left, rate)
    right = channel_metrics(analyzer, ref_right, got_right, predicted_right, rate)
    return {
        "run_dir": str(run_dir),
        "reference": str(reference_path),
        "capture": str(capture_path),
        "rate": rate,
        "alignment_lag": got_start - ref_start,
        "compared_frames": usable,
        "compared_seconds": usable / rate,
        "left_response_gains": left_gains,
        "right_response_gains": right_gains,
        "left": left,
        "right": right,
        "max_scalar_mid_residual_ratio": max(left["scalar_mid_residual_ratio"],
                                             right["scalar_mid_residual_ratio"]),
        "max_response_mid_residual_ratio": max(left["response_mid_residual_ratio"],
                                               right["response_mid_residual_ratio"]),
        "max_scalar_high_residual_ratio": max(left["scalar_high_residual_ratio"],
                                              right["scalar_high_residual_ratio"]),
        "max_response_high_residual_ratio": max(left["response_high_residual_ratio"],
                                                right["response_high_residual_ratio"]),
        "min_scalar_snr_db": min(left["scalar_snr_db"], right["scalar_snr_db"]),
        "min_response_snr_db": min(left["response_snr_db"], right["response_snr_db"]),
        "response_click_outliers": left["response_click_outliers"] + right["response_click_outliers"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tone-matrix-json", required=True)
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--max-seconds", type=float, default=16.0)
    parser.add_argument("soundcheck_dirs", nargs="+")
    args = parser.parse_args()

    analyzer = load_analyzer()
    tone_data = json.loads(Path(args.tone_matrix_json).read_text())
    rows = [analyze_run(analyzer, tone_data, Path(path), args.max_seconds)
            for path in args.soundcheck_dirs]
    improvements = []
    for row in rows:
        scalar_mid = row["max_scalar_mid_residual_ratio"]
        response_mid = row["max_response_mid_residual_ratio"]
        scalar_snr = row["min_scalar_snr_db"]
        response_snr = row["min_response_snr_db"]
        improvements.append({
            "run_dir": row["run_dir"],
            "mid_residual_delta": scalar_mid - response_mid,
            "snr_delta_db": response_snr - scalar_snr,
        })
    result = {
        "schema": "opena8djcpp.tone-response-compensation.v1",
        "tone_matrix": args.tone_matrix_json,
        "model": "three_band_per_channel_gain_from_physical_tone_matrix",
        "result": "PASS_DIAGNOSTIC",
        "rows": rows,
        "improvements": improvements,
    }
    out = Path(args.json_out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
