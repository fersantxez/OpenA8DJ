#!/usr/bin/env python3
"""Evaluate whether the current macOS driver candidate is release-ready.

This script is intentionally offline/read-only with respect to hardware and the
legacy/Rust worktrees. It consumes existing evidence files and returns PASS
only when every promotion gate is already proven.
"""

import argparse
import csv
import json
import math
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def latest_file(pattern, fallback):
    matches = [path for path in ROOT.glob(pattern) if path.is_file()]
    if not matches:
        return fallback
    return max(matches, key=lambda path: (path.stat().st_mtime, str(path)))


def latest_file_any(patterns, fallback):
    matches = []
    for pattern in patterns:
        matches.extend(path for path in ROOT.glob(pattern) if path.is_file())
    if not matches:
        return fallback
    return max(matches, key=lambda path: (path.stat().st_mtime, str(path)))


def latest_complete_soundcheck_run(fallback):
    """Return the latest product run with music and CPU from one run directory."""
    matches = []
    for metrics_path in ROOT.glob("local-analysis/soundcheck/*/metrics.json"):
        run_dir = metrics_path.parent
        cpu_path = run_dir / "cpu-profile.tsv"
        if not cpu_path.is_file():
            continue
        newest_ns = max(metrics_path.stat().st_mtime_ns,
                        cpu_path.stat().st_mtime_ns)
        matches.append((newest_ns, str(run_dir), run_dir))
    if not matches:
        return fallback
    return max(matches)[2]


DEFAULT_PRODUCT_RUN = latest_complete_soundcheck_run(
    ROOT / "local-analysis/soundcheck/20260616-185543-irig-pairA-24s-cpp-hal")


DEFAULTS = {
    "offline": ROOT / "local-analysis/cpp-offline/current-offline-gates.json",
    "simulated": ROOT / "local-analysis/simulated-output/2026-06-16T165629-sim-A-big-start4-gain05/metrics.json",
    "tone": ROOT / "local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long/start-byte-4/tone-analysis.txt",
    "music": DEFAULT_PRODUCT_RUN / "metrics.json",
    "cpu": DEFAULT_PRODUCT_RUN / "cpu-profile.tsv",
    "physical_investigation": ROOT / "local-analysis/usb-physical-investigation-summary.json",
    "physical_latency": latest_file(
        "local-analysis/**/physical-latency.json",
        ROOT / "local-analysis/channel-matrix/20260617-latency-measure-pairA-postroll8-3s/physical-latency.json",
    ),
    "physical_marker": latest_file(
        "local-analysis/**/marker-peak-summary.json",
        ROOT / "local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/marker-peak-summary.json",
    ),
    "usb_integrity": latest_file(
        "local-analysis/direct-usb-soundcheck/*/driver-diagnostics-analysis.txt",
        ROOT / "local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/driver-capture-analysis-explicit-usb-12s.txt",
    ),
    "physical_matrix": latest_file_any(
        [
            "local-analysis/direct-usb-soundcheck/*/tone-matrix.json",
            "local-analysis/channel-matrix/*/tone-matrix.json",
        ],
        ROOT / "local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/tone-matrix.json",
    ),
    "known_good_route": latest_file(
        "local-analysis/physical-superiority-window/**/known-good-route/metrics.json",
        ROOT / "local-analysis/physical-superiority-window/REQUIRED-known-good-route-metrics.json",
    ),
    "known_good_audiophile_cpp": latest_file(
        "local-analysis/physical-superiority-window/**/known-good-route/audiophile-wav-analysis-cpp.json",
        ROOT / "local-analysis/physical-superiority-window/REQUIRED-known-good-audiophile-cpp.json",
    ),
    "known_good_audiophile_python": latest_file(
        "local-analysis/physical-superiority-window/**/known-good-route/audiophile-wav-analysis.json",
        ROOT / "local-analysis/physical-superiority-window/REQUIRED-known-good-audiophile-python.json",
    ),
    "same_session_compare": latest_file(
        "local-analysis/physical-superiority-window/**/same-session-physical-compare.json",
        ROOT / "local-analysis/physical-superiority-window/REQUIRED-same-session-physical-compare.json",
    ),
    "capture_route_health": ROOT / "local-analysis/cpp-offline/capture-route-health-gate.json",
    "direct_usb_attribution": ROOT / "local-analysis/cpp-offline/direct-usb-path-attribution.json",
    "hal_candidate_safety": ROOT / "local-analysis/cpp-offline/hal-candidate-safety-gate.json",
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
    "physical_latency_first_energy_seconds_max": 1.5,
    "physical_latency_abs_correlation_min": 0.98,
    "physical_latency_aligned_snr_db_min": 35.0,
    "physical_latency_linear_fit_snr_db_min": 35.0,
    "physical_latency_linear_residual_over_capture_rms_max": 0.10,
    "physical_marker_min_paired_peaks": 4,
    "physical_marker_offset_std_seconds_max": 0.025,
    "physical_marker_mean_offset_seconds_max": 1.5,
    "usb_integrity_alignment_min": 0.999999,
    "usb_integrity_snr_db_min": 90.0,
    "physical_matrix_max_wrong_source_leakage_db_max": -45.0,
    "physical_matrix_expected_floor_amplitude_min": 0.005,
}


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def load_json_or_empty(path):
    if not path.is_file():
        return {}
    return load_json(path)


def parse_key_values(path):
    if not path.is_file():
        return {}
    values = {}
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def window_file(paths, name):
    root = promotion_window_root(paths["music"])
    if root is None:
        return ROOT / "local-analysis/physical-superiority-window" / f"REQUIRED-{name}"
    return root / name


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


def current_head_short():
    try:
        return subprocess.check_output(
            ["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"],
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def check_file(path, name):
    exists = path.is_file()
    return gate(name, exists, {"path": str(path)}, "" if exists else "required evidence file is missing")


def evidence_metadata(path):
    if not path.is_file():
        return {"path": str(path), "exists": False}
    return {
        "path": str(path),
        "exists": True,
        "mtime_ns": path.stat().st_mtime_ns,
        "parent": str(path.parent),
    }


def promotion_window_root(path):
    parts = list(path.resolve().parts)
    marker = "physical-superiority-window"
    if marker not in parts:
        return None
    index = parts.index(marker)
    if index + 1 >= len(parts):
        return None
    return Path(*parts[:index + 2])


def promotion_bundle(paths):
    product_run = paths["music"].parent if paths["music"].parent == paths["cpu"].parent else None
    product_window = promotion_window_root(paths["music"]) if product_run else None
    required = {
        "music": paths["music"],
        "cpu": paths["cpu"],
        "tone": paths["tone"],
        "physical_latency": paths["physical_latency"],
        "physical_marker": paths["physical_marker"],
        "usb_integrity": paths["usb_integrity"],
        "physical_matrix": paths["physical_matrix"],
        "known_good_route": paths["known_good_route"],
        "known_good_audiophile_cpp": paths["known_good_audiophile_cpp"],
        "known_good_audiophile_python": paths["known_good_audiophile_python"],
        "same_session_compare": paths["same_session_compare"],
    }
    roots = {name: promotion_window_root(path) for name, path in required.items()}
    missing_or_mixed = [
        name for name, root in roots.items()
        if product_window is None or root is None or root != product_window
    ]
    return {
        "product_run": str(product_run) if product_run else None,
        "product_window": str(product_window) if product_window else None,
        "artifact_windows": {
            name: str(root) if root else None for name, root in roots.items()
        },
        "missing_or_mixed_artifacts": missing_or_mixed,
        "required_artifacts": sorted(required.keys()),
        "same_window": product_window is not None and not missing_or_mixed,
    }


def evaluate(args):
    paths = {
        "offline": Path(args.offline),
        "simulated": Path(args.simulated),
        "tone": Path(args.tone),
        "music": Path(args.music),
        "cpu": Path(args.cpu),
        "physical_investigation": Path(args.physical_investigation),
        "physical_latency": Path(args.physical_latency),
        "physical_marker": Path(args.physical_marker),
        "usb_integrity": Path(args.usb_integrity),
        "physical_matrix": Path(args.physical_matrix),
        "known_good_route": Path(args.known_good_route),
        "known_good_audiophile_cpp": Path(args.known_good_audiophile_cpp),
        "known_good_audiophile_python": Path(args.known_good_audiophile_python),
        "same_session_compare": Path(args.same_session_compare),
        "capture_route_health": Path(args.capture_route_health),
        "direct_usb_attribution": Path(args.direct_usb_attribution),
        "hal_candidate_safety": Path(args.hal_candidate_safety),
    }
    gates = []
    for name, path in paths.items():
        gates.append(check_file(path, f"evidence:{name}"))

    offline = load_json_or_empty(paths["offline"])
    simulated = load_json_or_empty(paths["simulated"])
    tone = parse_key_values(paths["tone"])
    music = load_json_or_empty(paths["music"])
    physical_investigation = load_json_or_empty(paths["physical_investigation"])
    physical_latency = load_json_or_empty(paths["physical_latency"])
    physical_marker = load_json_or_empty(paths["physical_marker"])
    usb_integrity = parse_key_values(paths["usb_integrity"])
    physical_matrix = load_json_or_empty(paths["physical_matrix"])
    known_good_route = load_json_or_empty(paths["known_good_route"])
    known_good_audiophile_cpp = load_json_or_empty(paths["known_good_audiophile_cpp"])
    known_good_audiophile_python = load_json_or_empty(paths["known_good_audiophile_python"])
    same_session_compare = load_json_or_empty(paths["same_session_compare"])
    capture_route_health = load_json_or_empty(paths["capture_route_health"])
    direct_usb_attribution = load_json_or_empty(paths["direct_usb_attribution"])
    hal_candidate_safety = load_json_or_empty(paths["hal_candidate_safety"])
    cpu = cpu_profile(paths["cpu"])
    head_commit = current_head_short()
    provenance = offline.get("evidence_provenance_freshness_gate", {})
    simulated_snr = min(as_float(simulated, "left_snr_db"), as_float(simulated, "right_snr_db"))
    music_snr = min(as_float(music, "left_snr_db"), as_float(music, "right_snr_db"))
    music_clicks = as_float(music, "click_outliers",
                            as_float(music, "window_click_outliers_max", 999))

    bench_path = ROOT / "local-analysis/cpp-offline/offline-bench-release.json"
    bench = load_json(bench_path) if bench_path.is_file() else {}
    bundle = promotion_bundle(paths)
    window_manifest_path = window_file(paths, "window-manifest.txt")
    window_preflight_path = window_file(paths, "physical-window-preflight.json")
    window_manifest = parse_key_values(window_manifest_path)
    window_preflight = load_json_or_empty(window_preflight_path)
    source_reference_promotion = (
        args.source_reference_promotion
        or window_manifest.get("source_reference_promotion") == "1"
        or window_preflight.get("source_reference_promotion") is True
    )
    known_good_policy_ok = (
        source_reference_promotion
        or (
            window_manifest.get("skip_known_good") == "0"
            and window_preflight.get("skip_known_good") is False
            and window_preflight.get("known_good_output_same_as_capture") is False
            and window_preflight.get("known_good_output_builtin_or_acoustic") is False
        )
    )
    window_not_diagnostic = (
        window_manifest.get("execute") == "1" and
        window_manifest.get("route_only") == "0" and
        window_manifest.get("candidate_only") == "0" and
        known_good_policy_ok and
        window_manifest.get("allow_same_device_loopback_diagnostic") == "0" and
        window_manifest.get("allow_built_in_output_acoustic_diagnostic") == "0" and
        window_preflight.get("result") == "PASS" and
        window_preflight.get("ready_to_execute_physical_window") is True and
        window_preflight.get("route_only") is False and
        window_preflight.get("candidate_only") is False and
        window_preflight.get("allow_same_device_loopback_diagnostic") is False and
        window_preflight.get("allow_built_in_output_acoustic_diagnostic") is False
    )
    known_good_route_ok = (
        source_reference_promotion
        or (
            known_good_route.get("result") == "PASS"
            and paths["known_good_route"].parent.name == "known-good-route"
            and promotion_window_root(paths["known_good_route"])
            == promotion_window_root(paths["music"])
        )
    )
    known_good_analyzers_ok = (
        source_reference_promotion
        or (
            known_good_audiophile_cpp.get("result") == "PASS"
            and known_good_audiophile_python.get("result") == "PASS"
            and paths["known_good_audiophile_cpp"].parent == paths["known_good_route"].parent
            and paths["known_good_audiophile_python"].parent
            == paths["known_good_route"].parent
            and promotion_window_root(paths["known_good_audiophile_cpp"])
            == promotion_window_root(paths["music"])
            and promotion_window_root(paths["known_good_audiophile_python"])
            == promotion_window_root(paths["music"])
        )
    )

    gates.extend([
        gate("latest_music_cpu_pair",
             paths["music"].parent == paths["cpu"].parent,
             {"music": evidence_metadata(paths["music"]),
              "cpu": evidence_metadata(paths["cpu"])},
             "music metrics and CPU profile must come from the same soundcheck run"),
        gate("single_physical_promotion_evidence_bundle",
             bundle["same_window"],
             bundle,
             "promotion evidence must come from one lock-gated physical window with the same route, candidate, baseline, and run context"),
        gate("same_window_known_good_route_revalidated",
             known_good_route_ok,
             {"path": str(paths["known_good_route"]),
              "result": known_good_route.get("result"),
              "source_reference_promotion": source_reference_promotion,
              "quality_alignment_score": known_good_route.get("quality_alignment_score"),
              "snr_db_min": min(as_float(known_good_route, "left_snr_db"),
                                as_float(known_good_route, "right_snr_db")),
              "lag_jumps_gt_2_frames": known_good_route.get("lag_jumps_gt_2_frames"),
              "click_outliers": known_good_route.get("click_outliers"),
              "known_good_window": str(promotion_window_root(paths["known_good_route"])),
              "product_window": str(promotion_window_root(paths["music"]))},
             "the physical reference policy must be valid in the same promotion window"),
        gate("same_window_known_good_audiophile_analyzers",
             known_good_analyzers_ok,
             {"cpp": evidence_metadata(paths["known_good_audiophile_cpp"]),
              "python": evidence_metadata(paths["known_good_audiophile_python"]),
              "source_reference_promotion": source_reference_promotion,
              "cpp_result": known_good_audiophile_cpp.get("result"),
              "python_result": known_good_audiophile_python.get("result")},
             "known-good route revalidation must include passing C++ and Python audiophile WAV analyzers in the same window"),
        gate("physical_window_not_diagnostic",
             window_not_diagnostic,
             {"manifest": evidence_metadata(window_manifest_path),
              "preflight": evidence_metadata(window_preflight_path),
              "manifest_flags": {
                  "execute": window_manifest.get("execute"),
                  "route_only": window_manifest.get("route_only"),
                  "candidate_only": window_manifest.get("candidate_only"),
                  "skip_known_good": window_manifest.get("skip_known_good"),
                  "source_reference_promotion":
                  window_manifest.get("source_reference_promotion"),
                  "allow_same_device_loopback_diagnostic":
                  window_manifest.get("allow_same_device_loopback_diagnostic"),
                  "allow_built_in_output_acoustic_diagnostic":
                  window_manifest.get("allow_built_in_output_acoustic_diagnostic"),
              },
              "preflight_flags": {
                  "result": window_preflight.get("result"),
                  "ready_to_execute_physical_window":
                  window_preflight.get("ready_to_execute_physical_window"),
                  "route_only": window_preflight.get("route_only"),
                  "candidate_only": window_preflight.get("candidate_only"),
                  "skip_known_good": window_preflight.get("skip_known_good"),
                  "source_reference_promotion":
                  window_preflight.get("source_reference_promotion"),
                  "allow_same_device_loopback_diagnostic":
                  window_preflight.get("allow_same_device_loopback_diagnostic"),
                  "allow_built_in_output_acoustic_diagnostic":
                  window_preflight.get("allow_built_in_output_acoustic_diagnostic"),
                  "known_good_output_same_as_capture":
                  window_preflight.get("known_good_output_same_as_capture"),
                  "known_good_output_builtin_or_acoustic":
                  window_preflight.get("known_good_output_builtin_or_acoustic"),
              }},
             "diagnostic, route-only, candidate-only, invalid reference-policy, same-device loopback, or built-in/acoustic windows cannot support promotion"),
        gate("offline_all_gates",
             offline.get("status") == "PASS" and not offline.get("hardware_touched", True),
             {"status": offline.get("status"),
              "hardware_touched": offline.get("hardware_touched"),
              "coreaudio_touched": offline.get("coreaudio_touched"),
              "usb_touched": offline.get("usb_touched")}),
        gate("candidate_evidence_matches_claimed_commit",
             offline.get("base_commit") == head_commit and
             provenance.get("claimable_current_candidate") is True,
             {"offline_base_commit": offline.get("base_commit"),
              "head_commit": head_commit,
              "provenance_status": provenance.get("status"),
              "claimable_current_candidate": provenance.get("claimable_current_candidate"),
              "working_tree_clean_for_claim": provenance.get("working_tree_clean_for_claim")},
             "offline evidence must be freshly attributable to the current HEAD before promotion"),
        gate("offline_timecode_signal_analysis",
             offline.get("timecode_signal_analysis", {}).get("status") == "PASS" and
             offline.get("timecode_signal_analysis", {}).get("rows", 0) >= 8 and
             offline.get("timecode_signal_analysis", {}).get("failures", 1) == 0,
             offline.get("timecode_signal_analysis", {})),
        gate("offline_protocol_contract",
             offline.get("protocol_contract", {}).get("status") == "PASS" and
             offline.get("protocol_contract", {}).get("vendor_id") == "0x17cc" and
             offline.get("protocol_contract", {}).get("product_id") == "0x1978" and
             offline.get("protocol_contract", {}).get("input_channels") == 8 and
             offline.get("protocol_contract", {}).get("output_channels") == 8 and
             offline.get("protocol_contract", {}).get("mode2_check_cadence_bytes") == 16 and
             offline.get("protocol_contract", {}).get("mode2_full_frame_bytes") == 32 and
             offline.get("protocol_contract", {}).get("default_start_byte") == 4,
             offline.get("protocol_contract", {})),
        gate("offline_simulated_output_matrix",
             offline.get("simulated_output_matrix", {}).get("status") == "PASS" and
             offline.get("simulated_output_matrix", {}).get("rows", 0) >= 48 and
             offline.get("simulated_output_matrix", {}).get("failures", 1) == 0 and
             as_float(offline.get("simulated_output_matrix", {}), "min_alignment_score") >=
             BASELINE["simulated_alignment_min"] and
             as_float(offline.get("simulated_output_matrix", {}), "min_snr_db") >=
             BASELINE["simulated_snr_db_min"] and
             as_float(offline.get("simulated_output_matrix", {}), "max_residual_ratio", 1.0) <=
             0.001 and
             as_float(offline.get("simulated_output_matrix", {}), "max_leakage_dbfs", 999.0) <=
             -120.0,
             offline.get("simulated_output_matrix", {})),
        gate("offline_mode2_cross_oracle_parity",
             offline.get("mode2_cross_oracle_parity", {}).get("status") == "PASS" and
             offline.get("mode2_cross_oracle_parity", {}).get("rows", 0) >= 72 and
             offline.get("mode2_cross_oracle_parity", {}).get("failures", 1) == 0 and
             offline.get("mode2_cross_oracle_parity", {}).get("byte_order") == "big" and
             as_float(offline.get("mode2_cross_oracle_parity", {}), "max_byte_mismatches", 1) == 0 and
             as_float(offline.get("mode2_cross_oracle_parity", {}), "max_length_delta", 1) == 0 and
             as_float(offline.get("mode2_cross_oracle_parity", {}), "total_check_errors", 1) == 0 and
             as_float(offline.get("mode2_cross_oracle_parity", {}), "total_panic_flags", 1) == 0,
             offline.get("mode2_cross_oracle_parity", {})),
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
        gate("offline_hardware_lock_policy",
             offline.get("hardware_lock_policy", {}).get("status") == "PASS" and
             offline.get("hardware_lock_policy", {}).get("audited_scripts", 0) >= 4 and
             offline.get("hardware_lock_policy", {}).get("missing_requirements", 1) == 0,
             offline.get("hardware_lock_policy", {})),
        gate("capture_route_measurement_valid_for_promotion",
             capture_route_health.get("measurement_valid_for_promotion") is True and
             not capture_route_health.get("promotion_blockers") and
             capture_route_health.get("direct_usb_capture_failed_after_clean_payload") is not True,
             {"path": str(paths["capture_route_health"]),
              "measurement_valid_for_promotion":
              capture_route_health.get("measurement_valid_for_promotion"),
              "direct_usb_capture_failed_after_clean_payload":
              capture_route_health.get("direct_usb_capture_failed_after_clean_payload"),
              "direct_usb_attribution": capture_route_health.get("direct_usb_attribution"),
              "promotion_blockers": capture_route_health.get("promotion_blockers", []),
              "required_physical_experiments":
              capture_route_health.get("required_physical_experiments", [])},
             "physical capture route must be validated before audio quality, CPU, timecode, or branch promotion claims"),
        gate("direct_usb_capture_route_not_failed_after_clean_payload",
             direct_usb_attribution.get("latest_run", {}).get("internal_clean") is True and
             direct_usb_attribution.get("latest_run", {}).get("capture_failed") is not True,
             {"path": str(paths["direct_usb_attribution"]),
              "readiness_claim": direct_usb_attribution.get("readiness_claim"),
              "latest_run": direct_usb_attribution.get("latest_run", {})},
             "a clean written/consumed/packed USB payload followed by failed physical capture blocks promotion"),
        gate("hal_candidate_safety_for_physical_window",
             hal_candidate_safety.get("result") == "PASS" and
             hal_candidate_safety.get("audio8_enumerated_8x8") is True and
             hal_candidate_safety.get("required_device_pass") is True and
             hal_candidate_safety.get("irig_preserved_during_guard") is True and
             hal_candidate_safety.get("post_unload_coreaudio_clean") is True and
             hal_candidate_safety.get("driver_installed_or_activated_now") is False,
             {"path": str(paths["hal_candidate_safety"]),
              "latest_run": hal_candidate_safety.get("latest_run"),
              "audio8_enumerated_8x8": hal_candidate_safety.get("audio8_enumerated_8x8"),
              "required_device_pass": hal_candidate_safety.get("required_device_pass"),
              "irig_preserved_during_guard":
              hal_candidate_safety.get("irig_preserved_during_guard"),
              "post_unload_coreaudio_clean":
              hal_candidate_safety.get("post_unload_coreaudio_clean"),
              "promotion_blockers": hal_candidate_safety.get("promotion_blockers", [])},
             "HAL install/reload must be proven safe and unload cleanly before physical promotion windows"),
        gate("offline_transfer_pool_model",
             offline.get("transfer_pool_model", {}).get("status") == "PASS" and
             offline.get("transfer_pool_model", {}).get("rows", 0) >= 6 and
             offline.get("transfer_pool_model", {}).get("failures", 1) == 0 and
             "capture_pool_leak_rejected" in
             offline.get("transfer_pool_model", {}).get("fallback_rejected_scenarios", []) and
             "playback_pool_leak_rejected" in
             offline.get("transfer_pool_model", {}).get("fallback_rejected_scenarios", []),
             offline.get("transfer_pool_model", {}),
             "transfer-pool model must prove healthy queueing and reject fallback-allocation scenarios"),
        gate("offline_throughput_beats_floor",
             as_float(bench, "pack_mib_s") >= BASELINE["offline_pack_mib_s_floor"] and
             as_float(bench, "decode_mib_s") >= BASELINE["offline_decode_mib_s_floor"] and
             as_float(bench, "float_to_s24_frames_s") >=
             BASELINE["offline_route_frames_s_floor"] and
             as_float(bench, "route_frames_s") >= BASELINE["offline_route_frames_s_floor"] and
             as_float(bench, "route_reversed_frames_s") >=
             BASELINE["offline_route_frames_s_floor"] and
             as_float(bench, "route_advanced_frames_s") >=
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
              "route_advanced_frames_s": bench.get("route_advanced_frames_s"),
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
        gate("physical_latency_alignment",
             physical_latency.get("result") == "PASS" and
             as_float(physical_latency, "first_energy_seconds", 999.0) <=
             BASELINE["physical_latency_first_energy_seconds_max"] and
             abs(as_float(physical_latency, "best_correlation", 0.0)) >=
             BASELINE["physical_latency_abs_correlation_min"] and
             as_float(physical_latency, "aligned_snr_db") >=
             BASELINE["physical_latency_aligned_snr_db_min"] and
             as_float(physical_latency, "linear_fit_snr_db") >=
             BASELINE["physical_latency_linear_fit_snr_db_min"] and
             as_float(physical_latency, "linear_residual_over_capture_rms", 999.0) <=
             BASELINE["physical_latency_linear_residual_over_capture_rms_max"],
             {"result": physical_latency.get("result"),
              "first_energy_seconds": physical_latency.get("first_energy_seconds"),
              "first_energy_seconds_max": BASELINE["physical_latency_first_energy_seconds_max"],
              "best_correlation": physical_latency.get("best_correlation"),
              "abs_correlation_min": BASELINE["physical_latency_abs_correlation_min"],
              "aligned_snr_db": physical_latency.get("aligned_snr_db"),
              "aligned_snr_db_min": BASELINE["physical_latency_aligned_snr_db_min"],
              "linear_fit_snr_db": physical_latency.get("linear_fit_snr_db"),
              "linear_fit_snr_db_min": BASELINE["physical_latency_linear_fit_snr_db_min"],
              "linear_residual_over_capture_rms": physical_latency.get("linear_residual_over_capture_rms"),
              "linear_residual_over_capture_rms_max":
              BASELINE["physical_latency_linear_residual_over_capture_rms_max"],
              "path": str(paths["physical_latency"])},
             "physical output must align promptly and cleanly before branch promotion"),
        gate("physical_marker_latency",
             physical_marker.get("result") == "PASS" and
             physical_marker.get("readiness_result", physical_marker.get("result")) == "PASS" and
             as_float(physical_marker, "paired_peaks") >=
             BASELINE["physical_marker_min_paired_peaks"] and
             as_float(physical_marker, "offset_std_seconds", 999.0) <=
             BASELINE["physical_marker_offset_std_seconds_max"] and
             abs(as_float(physical_marker, "offset_mean_seconds", 999.0)) <=
             BASELINE["physical_marker_mean_offset_seconds_max"],
             {"result": physical_marker.get("result"),
              "stability_result": physical_marker.get("stability_result"),
              "readiness_result": physical_marker.get("readiness_result"),
              "paired_peaks": physical_marker.get("paired_peaks"),
              "paired_peaks_min": BASELINE["physical_marker_min_paired_peaks"],
              "offset_mean_seconds": physical_marker.get("offset_mean_seconds"),
              "offset_mean_seconds_abs_max": BASELINE["physical_marker_mean_offset_seconds_max"],
              "offset_std_seconds": physical_marker.get("offset_std_seconds"),
              "offset_std_seconds_max": BASELINE["physical_marker_offset_std_seconds_max"],
              "path": str(paths["physical_marker"])},
             "stable marker offset is useful diagnosis, but large stable latency blocks readiness"),
        gate("direct_usb_internal_integrity",
             as_float(usb_integrity, "written_alignment_score") >=
             BASELINE["usb_integrity_alignment_min"] and
             as_float(usb_integrity, "consumed_alignment_score") >=
             BASELINE["usb_integrity_alignment_min"] and
             as_float(usb_integrity, "usb_alignment_score") >=
             BASELINE["usb_integrity_alignment_min"] and
             as_float(usb_integrity, "written_left_snr_db") >=
             BASELINE["usb_integrity_snr_db_min"] and
             as_float(usb_integrity, "written_right_snr_db") >=
             BASELINE["usb_integrity_snr_db_min"] and
             as_float(usb_integrity, "consumed_left_snr_db") >=
             BASELINE["usb_integrity_snr_db_min"] and
             as_float(usb_integrity, "consumed_right_snr_db") >=
             BASELINE["usb_integrity_snr_db_min"] and
             as_float(usb_integrity, "usb_left_snr_db") >=
             BASELINE["usb_integrity_snr_db_min"] and
             as_float(usb_integrity, "usb_right_snr_db") >=
             BASELINE["usb_integrity_snr_db_min"] and
             as_float(usb_integrity, "usb_check_errors", 1) == 0 and
             as_float(usb_integrity, "usb_panic_flags", 1) == 0,
             {"path": str(paths["usb_integrity"]),
              "written_alignment_score": usb_integrity.get("written_alignment_score"),
              "consumed_alignment_score": usb_integrity.get("consumed_alignment_score"),
              "usb_alignment_score": usb_integrity.get("usb_alignment_score"),
              "written_left_snr_db": usb_integrity.get("written_left_snr_db"),
              "written_right_snr_db": usb_integrity.get("written_right_snr_db"),
              "consumed_left_snr_db": usb_integrity.get("consumed_left_snr_db"),
              "consumed_right_snr_db": usb_integrity.get("consumed_right_snr_db"),
              "usb_left_snr_db": usb_integrity.get("usb_left_snr_db"),
              "usb_right_snr_db": usb_integrity.get("usb_right_snr_db"),
              "usb_check_errors": usb_integrity.get("usb_check_errors"),
              "usb_panic_flags": usb_integrity.get("usb_panic_flags")}),
        gate("physical_decorrelated_matrix_routing",
             physical_matrix.get("result") == "PASS" and
             as_float(physical_matrix.get("metrics", {}), "max_wrong_source_leakage_db", 999.0) <=
             BASELINE["physical_matrix_max_wrong_source_leakage_db_max"] and
             as_float(physical_matrix.get("metrics", {}), "expected_floor_amplitude", 0.0) >=
             BASELINE["physical_matrix_expected_floor_amplitude_min"] and
             as_float(physical_matrix.get("metrics", {}), "capture_clipped_frames", 999.0) <=
             BASELINE["music_capture_clipped_frames_max"],
             {"path": str(paths["physical_matrix"]),
              "result": physical_matrix.get("result"),
              "max_wrong_source_leakage_db":
              physical_matrix.get("metrics", {}).get("max_wrong_source_leakage_db"),
              "max_wrong_source_leakage_db_max":
              BASELINE["physical_matrix_max_wrong_source_leakage_db_max"],
              "expected_floor_amplitude":
              physical_matrix.get("metrics", {}).get("expected_floor_amplitude"),
              "expected_floor_amplitude_min":
              BASELINE["physical_matrix_expected_floor_amplitude_min"],
              "capture_clipped_frames":
              physical_matrix.get("metrics", {}).get("capture_clipped_frames")}),
        gate("same_session_mainline_cpp_physical_compare",
             same_session_compare.get("result") == "PASS" and
             same_session_compare.get("branch_promotion_supported") is True and
             same_session_compare.get("mode") == "candidate_vs_baseline_run" and
             paths["same_session_compare"].parent == paths["music"].parent.parent,
             {"path": str(paths["same_session_compare"]),
              "result": same_session_compare.get("result"),
              "branch_promotion_supported": same_session_compare.get("branch_promotion_supported"),
              "mode": same_session_compare.get("mode"),
              "compare_parent": str(paths["same_session_compare"].parent),
              "product_window_parent": str(paths["music"].parent.parent)},
             "promotion requires a same-window mainline/C++ physical comparison matching the product run"),
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
        gate("latest_physical_investigation",
             physical_investigation.get("result") == "PASS_READY",
             {"result": physical_investigation.get("result"),
              "date": physical_investigation.get("date"),
              "decision": physical_investigation.get("decision"),
              "hal_final_state": physical_investigation.get("hal_final_state"),
              "lock_final_state": physical_investigation.get("lock_final_state")},
             "latest physical investigation has not cleared hardware readiness"),
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
        "evidence_selection": {
            "product_run": str(paths["music"].parent)
            if paths["music"].parent == paths["cpu"].parent else None,
            "physical_promotion_bundle": bundle,
            "window_manifest": evidence_metadata(window_manifest_path),
            "window_preflight": evidence_metadata(window_preflight_path),
            "known_good_route": evidence_metadata(paths["known_good_route"]),
            "known_good_audiophile_cpp": evidence_metadata(paths["known_good_audiophile_cpp"]),
            "known_good_audiophile_python": evidence_metadata(paths["known_good_audiophile_python"]),
            "music": evidence_metadata(paths["music"]),
            "cpu": evidence_metadata(paths["cpu"]),
        },
        "gates": gates,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", default=str(DEFAULTS["offline"]))
    parser.add_argument("--simulated", default=str(DEFAULTS["simulated"]))
    parser.add_argument("--tone", default=str(DEFAULTS["tone"]))
    parser.add_argument("--music", default=str(DEFAULTS["music"]))
    parser.add_argument("--cpu", default=str(DEFAULTS["cpu"]))
    parser.add_argument("--physical-investigation", default=str(DEFAULTS["physical_investigation"]))
    parser.add_argument("--physical-latency", default=str(DEFAULTS["physical_latency"]))
    parser.add_argument("--physical-marker", default=str(DEFAULTS["physical_marker"]))
    parser.add_argument("--usb-integrity", default=str(DEFAULTS["usb_integrity"]))
    parser.add_argument("--physical-matrix", default=str(DEFAULTS["physical_matrix"]))
    parser.add_argument("--known-good-route", default=str(DEFAULTS["known_good_route"]))
    parser.add_argument("--known-good-audiophile-cpp", default=str(DEFAULTS["known_good_audiophile_cpp"]))
    parser.add_argument("--known-good-audiophile-python", default=str(DEFAULTS["known_good_audiophile_python"]))
    parser.add_argument("--same-session-compare", default=str(DEFAULTS["same_session_compare"]))
    parser.add_argument("--capture-route-health", default=str(DEFAULTS["capture_route_health"]))
    parser.add_argument("--direct-usb-attribution", default=str(DEFAULTS["direct_usb_attribution"]))
    parser.add_argument("--hal-candidate-safety", default=str(DEFAULTS["hal_candidate_safety"]))
    parser.add_argument("--source-reference-promotion", action="store_true")
    parser.add_argument("--json-out")
    args = parser.parse_args()

    result = evaluate(args)
    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        output_path = Path(args.json_out)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(text + "\n", encoding="utf-8")
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
