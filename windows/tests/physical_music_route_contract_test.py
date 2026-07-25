#!/usr/bin/env python3
"""Offline contracts for physical-music route evidence."""

from __future__ import annotations

import importlib.util
from importlib.machinery import SourceFileLoader
import json
import tempfile
from argparse import Namespace
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def load_gate():
    path = ROOT / "scripts" / "physical-music-quality-gate"
    spec = importlib.util.spec_from_loader(
        "physical_music_gate",
        SourceFileLoader("physical_music_gate", str(path)),
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def valid_route() -> tuple[dict, dict]:
    route = {
        "schema_version": 1,
        "platform": "windows",
        "capture_device": "Line In (iRig Stream)",
        "render_device": "Audio 8 DJ (8ch Out) (Audio 8 DJ)",
        "capture_hostapi": "Windows DirectSound",
        "render_hostapi": "Windows WASAPI",
        "capture_device_index": 20,
        "render_device_index": 33,
        "capture_is_irig_stream": True,
        "render_is_audio_8_dj": True,
        "capture_name_exact": True,
        "render_name_exact": True,
    }
    hardware = {
        "schema_version": 2,
        "audio8_usb_present_ok": True,
        "irig_usb_present_ok": True,
        "audio8_render_endpoint_present_ok": True,
        "irig_capture_endpoint_present_ok": True,
        "audio8_usb_instance_ids": ["USB\\VID_17CC&PID_1978\\A8DJ"],
        "irig_usb_instance_ids": ["USB\\VID_1963&PID_0059\\IRIG"],
        "audio8_usb_container_id": "{A8DJ}",
        "audio8_render_container_id": "{A8DJ}",
        "irig_usb_container_id": "{IRIG}",
        "irig_capture_container_id": "{IRIG}",
        "audio8_endpoint_matches_usb_container": True,
        "irig_endpoint_matches_usb_container": True,
    }
    return route, hardware


def main() -> int:
    gate = load_gate()
    with tempfile.TemporaryDirectory(prefix="opena8dj-route-contract-") as temp:
        run_dir = Path(temp)
        assert gate.verify_irig_route(run_dir)

        route, hardware = valid_route()
        write_json(run_dir / "route-proof.json", route)
        write_json(run_dir / "hardware-route-preflight.json", hardware)
        assert gate.verify_irig_route(run_dir) == []

        route["render_hostapi"] = "Windows DirectSound"
        write_json(run_dir / "route-proof.json", route)
        assert "render host API" in gate.verify_irig_route(run_dir)[0]

        route, hardware = valid_route()
        hardware["audio8_usb_instance_ids"] = ["USB\\VID_1234&PID_5678\\SPOOF"]
        write_json(run_dir / "route-proof.json", route)
        write_json(run_dir / "hardware-route-preflight.json", hardware)
        assert "Audio 8 DJ USB instance" in gate.verify_irig_route(run_dir)[0]

        route, hardware = valid_route()
        route["render_device"] = "Audio 8 DJ (8ch Out) spoof"
        write_json(run_dir / "route-proof.json", route)
        write_json(run_dir / "hardware-route-preflight.json", hardware)
        assert "render name" in gate.verify_irig_route(run_dir)[0]

        route, hardware = valid_route()
        hardware["audio8_render_container_id"] = "{UNRELATED}"
        hardware["audio8_endpoint_matches_usb_container"] = False
        write_json(run_dir / "route-proof.json", route)
        write_json(run_dir / "hardware-route-preflight.json", hardware)
        assert "audio8_endpoint_matches_usb_container" in gate.verify_irig_route(run_dir)[0]

        args = Namespace(
            min_alignment=0.925,
            min_windows_snr_db=8.0,
            max_windows_raw_clicks=0,
            max_windows_aligned_clicks=8,
            max_windows_dropouts=8,
            min_capture_peak=0.02,
            max_capture_peak=0.92,
            min_windows_capture_rms_dbfs=-36.0,
            max_capture_rms_dbfs=-10.0,
            windows_alignment_slack=0.02,
            windows_snr_slack_db=1.0,
        )
        mac_metrics = {
            "quality_alignment_score": 0.08,
            "capture_peak": 0.105,
            "capture_rms_dbfs": -33.0,
            "capture_clipped_frames_full": 0,
            "cpu_system_cpu_avg": 70.0,
            "cpu_system_cpu_p95": 100.0,
        }
        probe_metrics = {
            "aligned_alignment_score_mono": 0.94,
            "aligned_min_snr_db": 8.3,
            "aligned_click_outliers_total": 5,
            "raw_click_outliers": 0,
            "aligned_dropout_windows_total": 4,
            "playback_complete": True,
            "capture_complete": True,
            "status_event_count": 0,
            "callback_watchdog_expired": False,
        }
        baseline = {
            "alignment_score_mono": 0.944,
            "min_snr_db": 8.6,
            "raw_click_outliers": 0,
        }
        passed, errors, _ = gate.windows_two_clock_verdict(
            mac_metrics, probe_metrics, baseline, args
        )
        assert passed and errors == []
        probe_metrics["aligned_click_outliers_total"] = 10
        passed, errors, warnings = gate.windows_two_clock_verdict(
            mac_metrics, probe_metrics, baseline, args
        )
        assert passed and errors == []
        assert any("diagnostic only" in warning for warning in warnings)
        probe_metrics["raw_click_outliers"] = 1
        passed, errors, _ = gate.windows_two_clock_verdict(
            mac_metrics, probe_metrics, baseline, args
        )
        assert not passed and any("raw click" in error for error in errors)

    print("PASS: physical music route evidence contracts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
