#!/usr/bin/env python3
"""Build an offline per-window trace for an existing soundcheck run.

The script reads already captured WAV/evidence files. It does not open audio
devices, touch CoreAudio, query USB, install drivers, or mutate system state.
"""

import argparse
import importlib.util
import json
import statistics
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ANALYZER_PATH = ROOT / "scripts/analyze-soundcheck-capture.py"


def load_analyzer():
    spec = importlib.util.spec_from_file_location("soundcheck_analyzer", ANALYZER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def channel_residual_metrics(analyzer, ref, got, rate, low_hz, high_hz):
    gain, signal, residual = analyzer.channel_fit(ref, got)
    signal_rms = analyzer.rms(signal)
    residual_rms = analyzer.rms(residual)
    mid_signal = analyzer.band_rms(signal, rate, low_hz, high_hz)
    mid_residual = analyzer.band_rms(residual, rate, low_hz, high_hz)
    return {
        "gain": gain,
        "signal_rms": signal_rms,
        "residual_rms": residual_rms,
        "residual_ratio": residual_rms / signal_rms if signal_rms > 1e-9 else 0.0,
        "mid_band_signal_rms": mid_signal,
        "mid_band_residual_rms": mid_residual,
        "mid_band_residual_ratio": mid_residual / mid_signal if mid_signal > 1e-9 else 0.0,
    }


def pair_residual_metrics(analyzer, ref_pair, got_pair, rate, low_hz, high_hz):
    ref_left = [sample[0] for sample in ref_pair]
    ref_right = [sample[1] for sample in ref_pair]
    got_left = [sample[0] for sample in got_pair]
    got_right = [sample[1] for sample in got_pair]
    left = channel_residual_metrics(analyzer, ref_left, got_left, rate, low_hz, high_hz)
    right = channel_residual_metrics(analyzer, ref_right, got_right, rate, low_hz, high_hz)
    return {
        "left": left,
        "right": right,
        "residual_ratio": max(left["residual_ratio"], right["residual_ratio"]),
        "mid_band_residual_ratio": max(left["mid_band_residual_ratio"],
                                       right["mid_band_residual_ratio"]),
        "mid_band_residual_dbfs": analyzer.dbfs(max(left["mid_band_residual_rms"],
                                                    right["mid_band_residual_rms"])),
    }


def clamp_window(ref, got, ref_start, got_start, window_frames):
    if ref_start < 0 or got_start < 0:
        return [], []
    usable = min(window_frames, len(ref) - ref_start, len(got) - got_start)
    if usable <= 0:
        return [], []
    return ref[ref_start:ref_start + usable], got[got_start:got_start + usable]


def avg_cpu(analyzer, rows, columns, start_seconds, end_seconds):
    averaged = analyzer.average_cpu(rows, columns, start_seconds, end_seconds)
    return {
        key: averaged[key]
        for key in ("coreaudiod", "opena8dj_driver", "audio_tools", "windowserver", "total_audio_ui")
        if key in averaged
    }


def median(values):
    return statistics.median(values) if values else 0.0


def percentile(values, pct):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = int((len(ordered) - 1) * pct)
    return ordered[index]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--window-seconds", type=float, default=0.5)
    parser.add_argument("--hop-seconds", type=float, default=0.25)
    parser.add_argument("--max-local-lag", type=int, default=512)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    analyzer = load_analyzer()
    run_dir = args.run_dir
    metrics = json.loads((run_dir / "metrics.json").read_text(encoding="utf-8"))
    ref_rate, ref = analyzer.read_wav_pair(str(run_dir / "fixture/reference.wav"))
    got_rate, got = analyzer.read_wav_pair(str(run_dir / "captured.wav"))
    if got_rate != ref_rate:
      got = analyzer.resample_pair_linear(got, got_rate, ref_rate)
    rate = ref_rate

    ref_start = int(metrics["reference_start"])
    got_start = int(metrics["capture_start"])
    usable = int(metrics["compared_frames"])
    window_frames = max(64, int(rate * args.window_seconds))
    hop_frames = max(1, int(rate * args.hop_seconds))
    cpu_rows, cpu_columns = analyzer.load_cpu_profile(run_dir / "cpu-profile.tsv")
    ref_mono = analyzer.pair_to_mono(ref)
    got_mono = analyzer.pair_to_mono(got)

    rows = []
    for offset in range(0, max(0, usable - window_frames + 1), hop_frames):
        window_ref_start = ref_start + offset
        window_got_start = got_start + offset
        ref_window, raw_window = clamp_window(ref, got, window_ref_start, window_got_start, window_frames)
        if not ref_window or not raw_window:
            continue
        lag, best_score = analyzer.find_best_lag(ref_mono,
                                                 got_mono,
                                                 window_ref_start,
                                                 window_got_start,
                                                 args.max_local_lag,
                                                 len(ref_window),
                                                 max(1, len(ref_window) // 256))
        shifted_ref, shifted_got = clamp_window(ref,
                                                got,
                                                window_ref_start,
                                                window_got_start + lag,
                                                len(ref_window))
        if not shifted_ref or not shifted_got:
            shifted = pair_residual_metrics(analyzer, ref_window, raw_window, rate, 1000.0, 5000.0)
        else:
            shifted = pair_residual_metrics(analyzer, shifted_ref, shifted_got, rate, 1000.0, 5000.0)
        raw = pair_residual_metrics(analyzer, ref_window, raw_window, rate, 1000.0, 5000.0)
        raw_score = analyzer.correlation(ref_mono,
                                         got_mono,
                                         window_ref_start,
                                         window_got_start,
                                         len(ref_window))
        start_seconds = offset / rate
        end_seconds = (offset + len(ref_window)) / rate
        rows.append({
            "start_seconds": start_seconds,
            "end_seconds": end_seconds,
            "local_lag_frames": lag,
            "raw_correlation": raw_score,
            "lag_corrected_correlation": best_score,
            "raw_mid_band_residual_ratio": raw["mid_band_residual_ratio"],
            "lag_corrected_mid_band_residual_ratio": shifted["mid_band_residual_ratio"],
            "raw_mid_band_residual_dbfs": raw["mid_band_residual_dbfs"],
            "lag_corrected_mid_band_residual_dbfs": shifted["mid_band_residual_dbfs"],
            "raw_residual_ratio": raw["residual_ratio"],
            "lag_corrected_residual_ratio": shifted["residual_ratio"],
            "cpu": avg_cpu(analyzer, cpu_rows, cpu_columns, start_seconds, end_seconds),
        })

    lag_jumps = 0
    for left, right in zip(rows, rows[1:]):
        if abs(right["local_lag_frames"] - left["local_lag_frames"]) > 2:
            lag_jumps += 1
    raw_mid = [row["raw_mid_band_residual_ratio"] for row in rows]
    corrected_mid = [row["lag_corrected_mid_band_residual_ratio"] for row in rows]
    raw_correlation = [row["raw_correlation"] for row in rows]
    corrected_correlation = [row["lag_corrected_correlation"] for row in rows]
    abs_lag = [abs(row["local_lag_frames"]) for row in rows]
    driver_cpu = [
        row.get("cpu", {}).get("opena8dj_driver")
        for row in rows
        if row.get("cpu", {}).get("opena8dj_driver") is not None
    ]
    improvement = 0.0
    if raw_mid and analyzer.median(raw_mid) > 1e-9:
        improvement = 1.0 - (analyzer.median(corrected_mid) / analyzer.median(raw_mid))

    summary = {
        "result": "PASS" if rows else "FAIL",
        "run_dir": str(run_dir),
        "rate": rate,
        "window_seconds": args.window_seconds,
        "hop_seconds": args.hop_seconds,
        "windows": len(rows),
        "local_lag_min": min((row["local_lag_frames"] for row in rows), default=0),
        "local_lag_max": max((row["local_lag_frames"] for row in rows), default=0),
        "local_abs_lag_median": median(abs_lag),
        "local_abs_lag_p95": percentile(abs_lag, 0.95),
        "lag_jumps_gt_2_frames": lag_jumps,
        "raw_mid_band_residual_ratio_median": analyzer.median(raw_mid),
        "lag_corrected_mid_band_residual_ratio_median": analyzer.median(corrected_mid),
        "lag_correction_mid_ratio_improvement": improvement,
        "raw_correlation_median": median(raw_correlation),
        "lag_corrected_correlation_median": median(corrected_correlation),
        "opena8dj_driver_cpu_median": median(driver_cpu),
        "opena8dj_driver_cpu_p95": percentile(driver_cpu, 0.95),
        "opena8dj_driver_cpu_max": max(driver_cpu, default=0.0),
        "rows": rows,
    }

    text = json.dumps(summary, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    return 0 if rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
