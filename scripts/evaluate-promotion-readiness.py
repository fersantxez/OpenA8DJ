#!/usr/bin/env python3
"""Evaluate whether the C++ candidate is eligible to replace mainline C.

This script is intentionally offline/read-only with respect to hardware and the
mainline/Rust worktrees. It consumes existing evidence files and returns PASS
only when every promotion gate is already proven.
"""

import argparse
import csv
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


DEFAULTS = {
    "offline": ROOT / "local-analysis/cpp-offline/current-offline-gates.json",
    "simulated": ROOT / "local-analysis/simulated-output/2026-06-16T165629-sim-A-big-start4-gain05/metrics.json",
    "tone": ROOT / "local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long/start-byte-4/tone-analysis.txt",
    "music": ROOT / "local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/metrics.json",
    "cpu": ROOT / "local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/cpu-profile.tsv",
}


BASELINE = {
    "mainline_c_references": {
        "cpu_digital_stability": "0.3.135",
        "functional_timecode_topology": "0.3.25",
        "physical_tone_music_floor": "0.3.24",
    },
    "offline_pack_mib_s_floor": 100.0,
    "offline_decode_mib_s_floor": 100.0,
    "offline_route_frames_s_floor": 1_000_000.0,
    "simulated_alignment_min": 0.999999,
    "simulated_snr_db_min": 75.0,
    "tone_sideband_ratio_max": 0.004942,
    "tone_click_outliers_max": 0,
    "music_quality_alignment_min": 0.98,
    "music_snr_db_min": 35.0,
    "music_mid_residual_ratio_max": 1.36,
    "music_high_residual_ratio_max": 1.35,
    "music_quiet_mid_noise_dbfs_max": -58.0,
    "music_click_outliers_max": 0,
    "music_lag_jumps_max": 0,
    "music_capture_clipped_frames_max": 0,
    "driver_cpu_p95_max": 6.5,
    "coreaudiod_p95_max": 1.7,
}


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def parse_key_values(path):
    values = {}
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def as_float(mapping, key, default=math.nan):
    try:
        return float(mapping.get(key, default))
    except (TypeError, ValueError):
        return default


def percentile(values, fraction):
    if not values:
        return math.nan
    ordered = sorted(values)
    index = min(len(ordered) - 1, int(fraction * (len(ordered) - 1)))
    return ordered[index]


def cpu_profile(path):
    if not path.is_file():
        return {}
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
      rows = list(csv.DictReader(handle, delimiter="\t"))
    result = {}
    for key in ("opena8dj_driver", "coreaudiod"):
        values = []
        for row in rows:
            try:
                values.append(float(row.get(key, "") or 0.0))
            except ValueError:
                pass
        result[f"{key}_p95"] = percentile(values, 0.95)
        result[f"{key}_max"] = max(values) if values else math.nan
    return result


def gate(name, passed, metrics=None, reason=""):
    return {
        "name": name,
        "result": "PASS" if passed else "FAIL",
        "metrics": metrics or {},
        "reason": reason,
    }


def check_file(path, name):
    exists = path.is_file()
    return gate(name, exists, {"path": str(path)}, "" if exists else "required evidence file is missing")


def evaluate(args):
    paths = {
        "offline": Path(args.offline),
        "simulated": Path(args.simulated),
        "tone": Path(args.tone),
        "music": Path(args.music),
        "cpu": Path(args.cpu),
    }
    gates = []
    for name, path in paths.items():
        gates.append(check_file(path, f"evidence:{name}"))

    if any(item["result"] != "PASS" for item in gates):
        return {
            "result": "NOT_READY",
            "branch_promotion_allowed": False,
            "baseline": BASELINE,
            "gates": gates,
        }

    offline = load_json(paths["offline"])
    simulated = load_json(paths["simulated"])
    tone = parse_key_values(paths["tone"])
    music = load_json(paths["music"])
    cpu = cpu_profile(paths["cpu"])
    simulated_snr = min(as_float(simulated, "left_snr_db"), as_float(simulated, "right_snr_db"))
    music_snr = min(as_float(music, "left_snr_db"), as_float(music, "right_snr_db"))
    music_clicks = as_float(music, "click_outliers",
                            as_float(music, "window_click_outliers_max", 999))

    bench_path = ROOT / "local-analysis/cpp-offline/offline-bench-release.json"
    bench = load_json(bench_path) if bench_path.is_file() else {}

    gates.extend([
        gate("offline_all_gates",
             offline.get("status") == "PASS" and not offline.get("hardware_touched", True),
             {"status": offline.get("status"),
              "hardware_touched": offline.get("hardware_touched"),
              "coreaudio_touched": offline.get("coreaudio_touched"),
              "usb_touched": offline.get("usb_touched")}),
        gate("offline_timecode_signal_analysis",
             offline.get("timecode_signal_analysis", {}).get("status") == "PASS" and
             offline.get("timecode_signal_analysis", {}).get("rows", 0) >= 8 and
             offline.get("timecode_signal_analysis", {}).get("failures", 1) == 0,
             offline.get("timecode_signal_analysis", {})),
        gate("offline_dvs_packet_input_decode",
             offline.get("dvs_packet_input_decode", {}).get("status") == "PASS" and
             offline.get("dvs_packet_input_decode", {}).get("rows", 0) >= 24 and
             offline.get("dvs_packet_input_decode", {}).get("failures", 1) == 0 and
             offline.get("dvs_packet_input_decode", {}).get("playback_decode_off") == "PASS",
             offline.get("dvs_packet_input_decode", {})),
        gate("offline_driverkit_shell_contract",
             offline.get("driverkit_shell_contract", {}).get("status") == "PASS" and
             offline.get("driverkit_shell_contract", {}).get("device_model_valid") is True and
             offline.get("driverkit_shell_contract", {}).get("system_extension_activated") is False,
             offline.get("driverkit_shell_contract", {})),
        gate("offline_throughput_beats_floor",
             as_float(bench, "pack_mib_s") >= BASELINE["offline_pack_mib_s_floor"] and
             as_float(bench, "decode_mib_s") >= BASELINE["offline_decode_mib_s_floor"] and
             as_float(bench, "float_to_s24_frames_s") >=
             BASELINE["offline_route_frames_s_floor"] and
             as_float(bench, "route_frames_s") >= BASELINE["offline_route_frames_s_floor"] and
             as_float(bench, "route_reversed_frames_s") >=
             BASELINE["offline_route_frames_s_floor"] and
             as_float(bench, "decode_into_output_overflows", 1) == 0 and
             as_float(bench, "decode_into_check_errors", 1) == 0 and
             as_float(bench, "decode_into_panic_flags", 1) == 0 and
             as_float(bench, "check_errors", 1) == 0 and
             as_float(bench, "panic_flags", 1) == 0,
             {"pack_mib_s": bench.get("pack_mib_s"),
              "decode_mib_s": bench.get("decode_mib_s"),
              "decode_into_mib_s": bench.get("decode_into_mib_s"),
              "decode_allocating_mib_s": bench.get("decode_allocating_mib_s"),
              "decode_into_output_overflows": bench.get("decode_into_output_overflows"),
              "float_to_s24_frames_s": bench.get("float_to_s24_frames_s"),
              "route_frames_s": bench.get("route_frames_s"),
              "route_reversed_frames_s": bench.get("route_reversed_frames_s"),
              "check_errors": bench.get("check_errors"),
              "panic_flags": bench.get("panic_flags"),
              "decode_into_check_errors": bench.get("decode_into_check_errors"),
              "decode_into_panic_flags": bench.get("decode_into_panic_flags")}),
        gate("simulated_output_oracle",
             as_float(simulated, "alignment_score") >= BASELINE["simulated_alignment_min"] and
             simulated_snr >= BASELINE["simulated_snr_db_min"] and
             as_float(simulated, "mid_band_residual_ratio", 1.0) <= 0.001,
             {"alignment_score": simulated.get("alignment_score"),
              "snr_db_min": simulated_snr,
              "mid_band_residual_ratio": simulated.get("mid_band_residual_ratio")}),
        gate("physical_tone_beats_mainline_best",
             as_float(tone, "sideband_ratio") <= BASELINE["tone_sideband_ratio_max"] and
             as_float(tone, "click_outliers", 999) <= BASELINE["tone_click_outliers_max"] and
             as_float(tone, "peak", 2.0) < 0.98,
             {"sideband_ratio": tone.get("sideband_ratio"),
              "click_outliers": tone.get("click_outliers"),
              "peak": tone.get("peak"),
              "baseline_sideband_ratio_max": BASELINE["tone_sideband_ratio_max"]}),
        gate("physical_music_quality",
             as_float(music, "quality_alignment_score") >= BASELINE["music_quality_alignment_min"] and
             music_snr >= BASELINE["music_snr_db_min"] and
             as_float(music, "mid_band_residual_ratio") <= BASELINE["music_mid_residual_ratio_max"] and
             as_float(music, "high_band_residual_ratio") <= BASELINE["music_high_residual_ratio_max"] and
             as_float(music, "quiet_mid_band_noise_dbfs", 999.0) <=
             BASELINE["music_quiet_mid_noise_dbfs_max"] and
             music_clicks <= BASELINE["music_click_outliers_max"] and
             as_float(music, "lag_jumps_gt_2_frames", 999) <= BASELINE["music_lag_jumps_max"] and
             as_float(music, "capture_clipped_frames", 999) <= BASELINE["music_capture_clipped_frames_max"],
             {"quality_alignment_score": music.get("quality_alignment_score"),
              "snr_db_min": music_snr,
              "mid_band_residual_ratio": music.get("mid_band_residual_ratio"),
              "high_band_residual_ratio": music.get("high_band_residual_ratio"),
              "quiet_mid_band_noise_dbfs": music.get("quiet_mid_band_noise_dbfs"),
              "click_outliers": music_clicks,
              "lag_jumps_gt_2_frames": music.get("lag_jumps_gt_2_frames"),
              "capture_clipped_frames": music.get("capture_clipped_frames"),
              "alignment_min": BASELINE["music_quality_alignment_min"],
              "snr_db_min_required": BASELINE["music_snr_db_min"],
              "mid_residual_ratio_max": BASELINE["music_mid_residual_ratio_max"],
              "high_residual_ratio_max": BASELINE["music_high_residual_ratio_max"],
              "quiet_mid_noise_dbfs_max": BASELINE["music_quiet_mid_noise_dbfs_max"]}),
        gate("runtime_cpu_beats_mainline",
             as_float(cpu, "opena8dj_driver_p95") <= BASELINE["driver_cpu_p95_max"] and
             as_float(cpu, "coreaudiod_p95") <= BASELINE["coreaudiod_p95_max"],
             {"opena8dj_driver_p95": cpu.get("opena8dj_driver_p95"),
              "coreaudiod_p95": cpu.get("coreaudiod_p95"),
              "driver_cpu_p95_max": BASELINE["driver_cpu_p95_max"],
              "coreaudiod_p95_max": BASELINE["coreaudiod_p95_max"]}),
        gate("traktor_timecode_physical",
             False,
             {"status": "BLOCKED_UNVALIDATED_DVS"},
             "no physical Traktor/timecode-vinyl lock evidence has been recorded"),
    ])

    passed = all(item["result"] == "PASS" for item in gates)
    return {
        "result": "PASS" if passed else "FAIL",
        "branch_promotion_allowed": passed,
        "baseline": BASELINE,
        "evidence": {key: str(path) for key, path in paths.items()},
        "gates": gates,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", default=str(DEFAULTS["offline"]))
    parser.add_argument("--simulated", default=str(DEFAULTS["simulated"]))
    parser.add_argument("--tone", default=str(DEFAULTS["tone"]))
    parser.add_argument("--music", default=str(DEFAULTS["music"]))
    parser.add_argument("--cpu", default=str(DEFAULTS["cpu"]))
    parser.add_argument("--json-out")
    args = parser.parse_args()

    result = evaluate(args)
    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        Path(args.json_out).write_text(text + "\n", encoding="utf-8")
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
