#!/usr/bin/env python3
import json
import subprocess
import sys
import tempfile
import wave
from array import array
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/analyze-route-contamination.py"


def write_wav(path):
    samples = array("h", [800, -800] * 128)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(2)
        handle.setsampwidth(2)
        handle.setframerate(48000)
        handle.writeframes(samples.tobytes())


def main():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        direct = root / "direct.json"
        direct.write_text(
            json.dumps(
                {
                    "schema": "opena8djcpp.direct-usb-path-attribution.v1",
                    "result": "PASS",
                    "latest_run": {
                        "path": "local-analysis/direct-usb-soundcheck/example",
                        "attribution": (
                            "post_usb_device_analog_or_capture_route_dominant_timewarp_rejected"
                        ),
                        "internal_clean": True,
                        "capture_failed": True,
                        "capture_quality_alignment_score": 0.83,
                        "capture_snr_floor_db": -10.7,
                        "has_timewarp_evidence": True,
                        "timewarp_classification": "fractional_time_warp_rejected",
                        "timewarp_scalar_improvement_db": 1.8,
                        "timewarp_matrix_improvement_db": 1.9,
                        "audiophile_wav_analysis_result": "FAIL",
                        "usb_alignment_score": 1.0,
                        "usb_snr_floor_db": 999.0,
                        "usb_check_errors": 0,
                        "usb_panic_flags": 0,
                    },
                }
            ),
            encoding="utf-8",
        )
        idle = root / "idle"
        idle.mkdir()
        (idle / "record.log").write_text(
            'recorded path=captured.wav seconds=6 device="iRig Stream" '
            'uid="fixture" rate=48000 channels=1,2 frames=288000 '
            "rms=0.00065060 peak=0.01190186 clipped=0 "
            "first_energy_record_seconds=0.000000 first_energy_threshold=0.00030000\n",
            encoding="utf-8",
        )
        write_wav(idle / "captured.wav")
        out = root / "out.json"
        subprocess.check_call(
            [
                sys.executable,
                str(SCRIPT),
                "--direct-attribution",
                str(direct),
                "--idle-run",
                str(idle),
                "--json-out",
                str(out),
            ],
            stdout=subprocess.DEVNULL,
        )
        payload = json.loads(out.read_text(encoding="utf-8"))
        assert payload["result"] == "PASS"
        assert payload["classification"] == "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB"
        assert payload["internal_usb_clean"] is True
        assert payload["physical_capture_failed"] is True
        assert payload["idle_capture_non_silent"] is True
        assert payload["product_claim_allowed"] is False
        assert payload["branch_promotion_allowed"] is False
        assert payload["timecode_vinyl_claim_allowed"] is False
    print("route contamination analysis fixture PASS")


if __name__ == "__main__":
    main()
