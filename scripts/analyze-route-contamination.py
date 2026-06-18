#!/usr/bin/env python3
import argparse
import json
import math
import re
import wave
from pathlib import Path


SCHEMA = "opena8djcpp.route-contamination-analysis.v1"


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def finite(value):
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def latest_record_log(root):
    best = None
    best_key = None
    if not root.is_dir():
        return None
    for path in root.rglob("record.log"):
        key = (path.stat().st_mtime, str(path))
        if best_key is None or key > best_key:
            best = path
            best_key = key
    return best


def parse_record_log(path):
    if path is None or not path.is_file():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    fields = {}
    for key, value in re.findall(r"([A-Za-z_][A-Za-z0-9_]*)=(\"[^\"]*\"|\S+)", text):
        if value.startswith('"') and value.endswith('"'):
            fields[key] = value[1:-1]
            continue
        try:
            fields[key] = float(value)
        except ValueError:
            fields[key] = value
    fields["record_log"] = str(path)
    return fields


def wav_rms_peak(path):
    if path is None or not path.is_file():
        return {}
    with wave.open(str(path), "rb") as handle:
        channels = handle.getnchannels()
        width = handle.getsampwidth()
        frames = handle.getnframes()
        raw = handle.readframes(frames)
    if width not in (2, 3, 4) or not raw:
        return {"wav_channels": channels, "wav_frames": frames}
    peak = 0.0
    acc = 0.0
    samples = 0
    step = width
    denom = float(1 << (8 * width - 1))
    for offset in range(0, len(raw) - step + 1, step):
        chunk = raw[offset : offset + step]
        if width == 3:
            sign = b"\xff" if chunk[2] & 0x80 else b"\x00"
            value = int.from_bytes(chunk + sign, byteorder="little", signed=True)
        else:
            value = int.from_bytes(chunk, byteorder="little", signed=True)
        scaled = value / denom
        peak = max(peak, abs(scaled))
        acc += scaled * scaled
        samples += 1
    rms = math.sqrt(acc / samples) if samples else 0.0
    return {
        "wav_channels": channels,
        "wav_frames": frames,
        "wav_rms": rms,
        "wav_peak": peak,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--direct-attribution",
        default="local-analysis/cpp-offline/direct-usb-path-attribution.json",
    )
    parser.add_argument("--idle-root", default="local-analysis/irig-capture-isolation")
    parser.add_argument("--idle-run")
    parser.add_argument("--json-out", required=True)
    args = parser.parse_args()

    direct_path = Path(args.direct_attribution)
    direct = load_json(direct_path)
    latest = direct.get("latest_run") or {}

    idle_run = Path(args.idle_run) if args.idle_run else None
    record_log = (idle_run / "record.log") if idle_run else latest_record_log(Path(args.idle_root))
    idle = parse_record_log(record_log)
    idle_dir = Path(record_log).parent if record_log else None
    wav_stats = wav_rms_peak(idle_dir / "captured.wav" if idle_dir else None)

    thresholds = {
        "min_clean_usb_alignment": 0.999999,
        "min_clean_usb_snr_db": 120.0,
        "min_capture_quality_alignment": 0.98,
        "min_capture_snr_db": 35.0,
        "max_timewarp_explanation_db": 3.0,
        "idle_non_silent_rms": 0.0003,
        "idle_non_silent_peak": 0.003,
    }

    usb_snr = latest.get("usb_snr_floor_db")
    usb_alignment = latest.get("usb_alignment_score")
    internal_usb_clean = (
        latest.get("internal_clean") is True
        and finite(usb_alignment)
        and usb_alignment >= thresholds["min_clean_usb_alignment"]
        and finite(usb_snr)
        and usb_snr >= thresholds["min_clean_usb_snr_db"]
        and latest.get("usb_check_errors") == 0
        and latest.get("usb_panic_flags") == 0
    )

    capture_quality = latest.get("capture_quality_alignment_score")
    capture_snr = latest.get("capture_snr_floor_db")
    physical_capture_failed = (
        latest.get("capture_failed") is True
        or (finite(capture_quality) and capture_quality < thresholds["min_capture_quality_alignment"])
        or (finite(capture_snr) and capture_snr < thresholds["min_capture_snr_db"])
        or latest.get("audiophile_wav_analysis_result") == "FAIL"
    )

    scalar_improvement = latest.get("timewarp_scalar_improvement_db")
    matrix_improvement = latest.get("timewarp_matrix_improvement_db")
    best_improvement = max(
        float(scalar_improvement) if finite(scalar_improvement) else float("-inf"),
        float(matrix_improvement) if finite(matrix_improvement) else float("-inf"),
    )
    timewarp_explains_failure = not (
        latest.get("timewarp_classification") == "fractional_time_warp_rejected"
        and best_improvement < thresholds["max_timewarp_explanation_db"]
    )

    idle_rms = idle.get("rms", wav_stats.get("wav_rms"))
    idle_peak = idle.get("peak", wav_stats.get("wav_peak"))
    first_energy_record_seconds = idle.get("first_energy_record_seconds")
    idle_capture_non_silent = (
        (finite(idle_rms) and idle_rms > thresholds["idle_non_silent_rms"])
        or (finite(idle_peak) and idle_peak > thresholds["idle_non_silent_peak"])
        or (finite(first_energy_record_seconds) and first_energy_record_seconds <= 0.0)
    )

    contamination_classified = (
        internal_usb_clean
        and physical_capture_failed
        and not timewarp_explains_failure
        and idle_capture_non_silent
    )
    if contamination_classified:
        classification = "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB"
        next_required_action = "VALIDATE_WIRED_NON_AUDIO8_KNOWN_GOOD_ROUTE_OR_FIX_IRIG_MIXER_MONITORING"
    elif internal_usb_clean and physical_capture_failed:
        classification = "DOWNSTREAM_CAPTURE_FAILURE_AFTER_CLEAN_USB_IDLE_UNKNOWN"
        next_required_action = "REPEAT_IDLE_CAPTURE_AND_VALIDATE_KNOWN_GOOD_ROUTE"
    elif internal_usb_clean:
        classification = "USB_CLEAN_CAPTURE_NOT_PROVEN_CONTAMINATED"
        next_required_action = "RUN_SAME_WINDOW_PRODUCT_PHYSICAL_AB_IF_ROUTE_VALIDATED"
    else:
        classification = "INTERNAL_USB_NOT_PROVEN_CLEAN"
        next_required_action = "FIX_OR_RECHECK_DIRECT_USB_DIAGNOSTICS"

    output = {
        "schema": SCHEMA,
        "safety": "offline_existing_evidence_only_no_audio_coreaudio_usb_driver_or_hardware_touch",
        "result": "PASS" if contamination_classified else "FAIL",
        "meaning": (
            "PASS means route contamination is objectively classified and product/human "
            "quality claims remain blocked; not product readiness"
        ),
        "classification": classification,
        "internal_usb_clean": internal_usb_clean,
        "physical_capture_failed": physical_capture_failed,
        "timewarp_explains_failure": timewarp_explains_failure,
        "idle_capture_non_silent": idle_capture_non_silent,
        "contamination_classified": contamination_classified,
        "product_claim_allowed": False,
        "branch_promotion_allowed": False,
        "timecode_vinyl_claim_allowed": False,
        "human_product_test_allowed": False,
        "diagnostic_rc_allowed": True,
        "next_required_action": next_required_action,
        "thresholds": thresholds,
        "direct_usb": {
            "evidence": str(direct_path),
            "latest_run_path": latest.get("path"),
            "attribution": latest.get("attribution"),
            "usb_alignment_score": usb_alignment,
            "usb_snr_floor_db": usb_snr,
            "usb_check_errors": latest.get("usb_check_errors"),
            "usb_panic_flags": latest.get("usb_panic_flags"),
            "capture_quality_alignment_score": capture_quality,
            "capture_snr_floor_db": capture_snr,
            "timewarp_classification": latest.get("timewarp_classification"),
            "timewarp_scalar_improvement_db": scalar_improvement,
            "timewarp_matrix_improvement_db": matrix_improvement,
            "audiophile_wav_analysis_result": latest.get("audiophile_wav_analysis_result"),
        },
        "idle_capture": {
            "record_log": str(record_log) if record_log else None,
            "run_dir": str(idle_dir) if idle_dir else None,
            "device": idle.get("device"),
            "uid": idle.get("uid"),
            "rate": idle.get("rate"),
            "channels": idle.get("channels"),
            "frames": idle.get("frames"),
            "rms": idle_rms,
            "peak": idle_peak,
            "clipped": idle.get("clipped"),
            "first_energy_record_seconds": first_energy_record_seconds,
            "first_energy_threshold": idle.get("first_energy_threshold"),
            **wav_stats,
        },
    }

    out_path = Path(args.json_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(output, indent=2, sort_keys=True))
    return 0 if contamination_classified else 1


if __name__ == "__main__":
    raise SystemExit(main())
