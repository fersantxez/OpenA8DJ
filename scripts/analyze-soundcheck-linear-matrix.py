#!/usr/bin/env python3
"""Fit a 2x2 linear L/R matrix for existing soundcheck captures.

This is an offline diagnostic. It reads existing evidence files only; it does
not open audio devices, touch CoreAudio, query USB, install drivers, or mutate
system state.
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


def column(values, index):
    return [sample[index] for sample in values]


def dot(left, right):
    return sum(lv * rv for lv, rv in zip(left, right))


def correlation(left, right):
    left_power = dot(left, left)
    right_power = dot(right, right)
    if left_power <= 0.0 or right_power <= 0.0:
        return 0.0
    return dot(left, right) / math.sqrt(left_power * right_power)


def gram_condition_number(a, b, d):
    trace = a + d
    determinant = a * d - b * b
    if determinant <= 0.0:
        return math.inf
    discriminant = max(0.0, trace * trace - 4.0 * determinant)
    root = math.sqrt(discriminant)
    low = 0.5 * (trace - root)
    high = 0.5 * (trace + root)
    if low <= 0.0:
        return math.inf
    return high / low


def solve_two_input_fit(ref_left, ref_right, got):
    ll = dot(ref_left, ref_left)
    lr = dot(ref_left, ref_right)
    rr = dot(ref_right, ref_right)
    lg = dot(ref_left, got)
    rg = dot(ref_right, got)
    determinant = ll * rr - lr * lr
    if abs(determinant) <= 1e-18:
        return 0.0, 0.0, math.inf
    a = (lg * rr - rg * lr) / determinant
    b = (rg * ll - lg * lr) / determinant
    return a, b, gram_condition_number(ll, lr, rr)


def score_pair_lag(ref_pair, got_pair, ref_start, got_start, lag, sample_count, stride):
    left_dot = 0.0
    right_dot = 0.0
    ref_left_energy = 0.0
    ref_right_energy = 0.0
    got_left_energy = 0.0
    got_right_energy = 0.0
    used = 0
    for offset in range(0, sample_count, max(1, stride)):
        ri = ref_start + offset
        gi = got_start + offset + lag
        if ri < 0 or gi < 0 or ri >= len(ref_pair) or gi >= len(got_pair):
            continue
        ref_left, ref_right = ref_pair[ri]
        got_left, got_right = got_pair[gi]
        left_dot += ref_left * got_left
        right_dot += ref_right * got_right
        ref_left_energy += ref_left * ref_left
        ref_right_energy += ref_right * ref_right
        got_left_energy += got_left * got_left
        got_right_energy += got_right * got_right
        used += 1
    if used == 0:
        return None
    left_score = 0.0
    if ref_left_energy > 0.0 and got_left_energy > 0.0:
        left_score = abs(left_dot) / math.sqrt(ref_left_energy * got_left_energy)
    right_score = 0.0
    if ref_right_energy > 0.0 and got_right_energy > 0.0:
        right_score = abs(right_dot) / math.sqrt(ref_right_energy * got_right_energy)
    return 0.5 * (left_score + right_score)


def scan_pair_lags(ref_pair,
                   got_pair,
                   ref_start,
                   got_start,
                   start_lag,
                   end_lag,
                   step,
                   sample_count,
                   stride):
    best_lag = 0
    best_score = None
    for lag in range(start_lag, end_lag + 1, max(1, step)):
        score = score_pair_lag(ref_pair,
                               got_pair,
                               ref_start,
                               got_start,
                               lag,
                               sample_count,
                               stride)
        if score is None:
            continue
        if best_score is None or score > best_score:
            best_lag = lag
            best_score = score
    return best_lag, best_score if best_score is not None else 0.0


def find_pair_lag(ref_pair, got_pair, ref_start, got_start, max_lag, nonnegative, sample_count):
    start = 0 if nonnegative else -max_lag
    coarse_step = max(1, max_lag // 2048)
    coarse_lag, _ = scan_pair_lags(ref_pair,
                                   got_pair,
                                   ref_start,
                                   got_start,
                                   start,
                                   max_lag,
                                   coarse_step,
                                   sample_count,
                                   64)
    fine_radius = max(8, coarse_step * 2)
    fine_start = max(start, coarse_lag - fine_radius)
    fine_end = min(max_lag, coarse_lag + fine_radius)
    return scan_pair_lags(ref_pair,
                          got_pair,
                          ref_start,
                          got_start,
                          fine_start,
                          fine_end,
                          1,
                          sample_count,
                          32)


def fit_matrix(analyzer, ref_pair, got_pair, rate):
    ref_left = column(ref_pair, 0)
    ref_right = column(ref_pair, 1)
    got_left = column(got_pair, 0)
    got_right = column(got_pair, 1)

    ll, lr, left_condition = solve_two_input_fit(ref_left, ref_right, got_left)
    rl, rr, right_condition = solve_two_input_fit(ref_left, ref_right, got_right)
    matrix = [[ll, lr], [rl, rr]]

    predicted_left = [ll * left + lr * right for left, right in zip(ref_left, ref_right)]
    predicted_right = [rl * left + rr * right for left, right in zip(ref_left, ref_right)]
    residual_left = [got - predicted for got, predicted in zip(got_left, predicted_left)]
    residual_right = [got - predicted for got, predicted in zip(got_right, predicted_right)]
    predicted = predicted_left + predicted_right
    got = got_left + got_right
    residual = residual_left + residual_right

    predicted_rms = analyzer.rms(predicted)
    got_rms = analyzer.rms(got)
    residual_rms = analyzer.rms(residual)
    input_corr = correlation(ref_left, ref_right)
    output_corr = correlation(got_left, got_right)
    condition = max(left_condition, right_condition)
    return {
        "matrix_ref_to_capture": matrix,
        "input_lr_correlation": input_corr,
        "capture_lr_correlation": output_corr,
        "gram_condition_number": condition,
        "predicted_rms": predicted_rms,
        "capture_rms": got_rms,
        "residual_rms": residual_rms,
        "residual_over_predicted_rms": residual_rms / predicted_rms if predicted_rms > 1e-12 else 0.0,
        "residual_over_capture_rms": residual_rms / got_rms if got_rms > 1e-12 else 0.0,
        "residual_dbfs": analyzer.dbfs(residual_rms),
    }


def clamp_pair(ref, got, ref_start, got_start, frames):
    if ref_start < 0:
        got_start += -ref_start
        ref_start = 0
    if got_start < 0:
        ref_start += -got_start
        got_start = 0
    usable = min(frames, len(ref) - ref_start, len(got) - got_start)
    if usable <= 0:
        return [], []
    return ref[ref_start:ref_start + usable], got[got_start:got_start + usable]


def analyze_run(analyzer,
                run_dir,
                max_lag,
                nonnegative_lag,
                analysis_seconds,
                correlation_warning,
                condition_warning):
    metrics_path = run_dir / "metrics.json"
    reference_path = run_dir / "fixture/reference.wav"
    capture_path = run_dir / "captured.wav"
    metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    ref_rate, ref = analyzer.read_wav_pair(str(reference_path))
    got_rate, got = analyzer.read_wav_pair(str(capture_path))
    if got_rate != ref_rate:
        got = analyzer.resample_pair_linear(got, got_rate, ref_rate)
    ref_start = int(metrics["reference_start"])
    got_start = int(metrics["capture_start"])
    compared_frames = int(metrics["compared_frames"])
    if max_lag > 0:
        global_lag, global_corr = find_pair_lag(ref,
                                                got,
                                                ref_start,
                                                got_start,
                                                max_lag,
                                                nonnegative_lag,
                                                min(compared_frames, ref_rate))
    else:
        ref_mono = analyzer.pair_to_mono(ref)
        got_mono = analyzer.pair_to_mono(got)
        global_lag = 0
        global_corr = analyzer.correlation(ref_mono,
                                           got_mono,
                                           ref_start,
                                           got_start,
                                           min(compared_frames, ref_rate))
    analysis_frames = compared_frames
    if analysis_seconds > 0.0:
        analysis_frames = min(analysis_frames, int(ref_rate * analysis_seconds))
    ref_window, got_window = clamp_pair(ref,
                                        got,
                                        ref_start,
                                        got_start + global_lag,
                                        analysis_frames)
    if not ref_window or not got_window:
        raise SystemExit(f"{run_dir}: no comparable audio after alignment")
    fit = fit_matrix(analyzer, ref_window, got_window, ref_rate)
    warnings = []
    if abs(fit["input_lr_correlation"]) >= correlation_warning:
        warnings.append("source_stereo_channels_are_correlated_matrix_is_diagnostic_not_crosstalk_proof")
    if fit["gram_condition_number"] >= condition_warning:
        warnings.append("fit_is_ill_conditioned_use_decorrelated_physical_fixture")
    if fit["residual_over_predicted_rms"] >= 0.25:
        warnings.append("linear_matrix_leaves_large_residual_non_linear_or_unmodelled_component")
    needs_decorrelated_fixture = (
        abs(fit["input_lr_correlation"]) >= correlation_warning or
        fit["gram_condition_number"] >= condition_warning
    )
    large_unmodelled_residual = fit["residual_over_predicted_rms"] >= 0.25

    return {
        "result": "PASS_DIAGNOSTIC",
        "run_dir": str(run_dir),
        "reference": str(reference_path),
        "capture": str(capture_path),
        "rate": ref_rate,
        "frames": len(ref_window),
        "analysis_seconds": len(ref_window) / ref_rate if ref_rate > 0 else 0.0,
        "alignment": {
            "reference_start": ref_start,
            "capture_start": got_start,
            "global_lag_frames": global_lag,
            "global_mono_correlation": global_corr,
        },
        "fit": fit,
        "warnings": warnings,
        "classification": (
            "needs_decorrelated_physical_matrix_fixture"
            if needs_decorrelated_fixture else
            "linear_matrix_rejected_large_physical_residual"
            if large_unmodelled_residual else
            "linear_matrix_estimate_well_conditioned"
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dirs", nargs="+", type=Path)
    parser.add_argument("--max-lag", type=int, default=0)
    parser.add_argument("--nonnegative-lag", action="store_true")
    parser.add_argument("--analysis-seconds", type=float, default=8.0)
    parser.add_argument("--correlation-warning", type=float, default=0.20)
    parser.add_argument("--condition-warning", type=float, default=25.0)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    analyzer = load_analyzer()
    runs = [
        analyze_run(analyzer,
                    run_dir,
                    args.max_lag,
                    args.nonnegative_lag,
                    args.analysis_seconds,
                    args.correlation_warning,
                    args.condition_warning)
        for run_dir in args.run_dirs
    ]
    summary = {
        "schema": "opena8djcpp.soundcheck-linear-matrix.v1",
        "result": "PASS_DIAGNOSTIC" if runs else "FAIL",
        "purpose": "Classify whether existing music capture residual can be explained by a 2x2 linear L/R mix.",
        "safety": "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch",
        "thresholds": {
            "source_correlation_warning": args.correlation_warning,
            "condition_number_warning": args.condition_warning,
            "large_residual_over_predicted_warning": 0.25,
        },
        "runs": runs,
    }
    text = json.dumps(summary, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    return 0 if runs else 1


if __name__ == "__main__":
    raise SystemExit(main())
