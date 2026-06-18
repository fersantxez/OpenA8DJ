#!/usr/bin/env python3
"""Offline regression tests for physical promotion-window contract hardening."""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def write_json(path: Path, payload: dict) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return path


def write_text(path: Path, text: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def run_evaluator(
    window: Path,
    include_known_good: bool,
    built_in_diagnostic: bool,
    same_device_diagnostic: bool = False,
    skip_known_good: bool = False,
) -> dict:
    if "physical-superiority-window" not in window.parts:
        window = window / "local-analysis" / "physical-superiority-window" / "fixture"
    cpp = window / "cpp-soundcheck"
    known_good = window / "known-good-route"
    write_json(
        cpp / "metrics.json",
        {
            "result": "PASS",
            "quality_alignment_score": 1.0,
            "left_snr_db": 80.0,
            "right_snr_db": 80.0,
            "mid_band_residual_ratio": 0.001,
            "high_band_residual_ratio": 0.001,
            "quiet_mid_band_noise_dbfs": -80.0,
            "click_outliers": 0,
            "lag_jumps_gt_2_frames": 0,
            "capture_clipped_frames": 0,
        },
    )
    write_text(
        cpp / "cpu-profile.tsv",
        "time\topena8dj_driver\tcoreaudiod\n0\t1.0\t1.0\n1\t1.0\t1.0\n",
    )
    if include_known_good:
        write_json(
            known_good / "metrics.json",
            {
                "result": "PASS",
                "quality_alignment_score": 1.0,
                "left_snr_db": 80.0,
                "right_snr_db": 80.0,
                "lag_jumps_gt_2_frames": 0,
                "click_outliers": 0,
            },
        )
        write_json(
            known_good / "audiophile-wav-analysis-cpp.json",
            {
                "schema": "opena8djcpp.audiophile-wav-analysis-cpp.v1",
                "result": "PASS",
                "snr_db_min": 80.0,
            },
        )
        write_json(
            known_good / "audiophile-wav-analysis.json",
            {
                "schema": "opena8djcpp.audiophile-wav-analysis.v1",
                "result": "PASS",
                "snr_db_min": 80.0,
            },
        )
    write_text(
        window / "window-manifest.txt",
        "\n".join(
            [
                "execute=1",
                "route_only=0",
                "candidate_only=0",
                f"skip_known_good={1 if skip_known_good else 0}",
                f"allow_same_device_loopback_diagnostic={1 if same_device_diagnostic else 0}",
                f"allow_built_in_output_acoustic_diagnostic={1 if built_in_diagnostic else 0}",
                "",
            ]
        ),
    )
    write_json(
        window / "physical-window-preflight.json",
        {
            "result": "PASS",
            "ready_to_execute_physical_window": True,
            "route_only": False,
            "candidate_only": False,
            "skip_known_good": skip_known_good,
            "allow_same_device_loopback_diagnostic": same_device_diagnostic,
            "allow_built_in_output_acoustic_diagnostic": built_in_diagnostic,
            "known_good_output_same_as_capture": same_device_diagnostic,
            "known_good_output_builtin_or_acoustic": built_in_diagnostic,
        },
    )

    output = window / "promotion-readiness.json"
    command = [
        "python3",
        str(ROOT / "scripts/evaluate-promotion-readiness.py"),
        "--music",
        str(cpp / "metrics.json"),
        "--cpu",
        str(cpp / "cpu-profile.tsv"),
        "--known-good-route",
        str(known_good / "metrics.json"),
        "--known-good-audiophile-cpp",
        str(known_good / "audiophile-wav-analysis-cpp.json"),
        "--known-good-audiophile-python",
        str(known_good / "audiophile-wav-analysis.json"),
        "--same-session-compare",
        str(window / "same-session-physical-compare.json"),
        "--json-out",
        str(output),
    ]
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode not in (0, 1):
        raise AssertionError(completed.stderr or completed.stdout)
    return json.loads(output.read_text(encoding="utf-8"))


def gate(result: dict, name: str) -> dict:
    for item in result["gates"]:
        if item["name"] == name:
            return item
    raise AssertionError(f"missing gate {name}")


def run_known_good_request_fixture(root: Path) -> dict:
    root.mkdir(parents=True, exist_ok=True)
    audio_list = root / "audio-list.txt"
    audio_list.write_text(
        "\n".join(
            [
                "Dispositivos Core Audio: 3",
                "  1  id=91  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000",
                "  2  id=93  iRig Stream  uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1  in=2 out=2 rate=48000",
                "  3  id=81  Wired Test Output  uid=ExternalWiredOutput  in=0 out=2 rate=48000",
                "",
            ]
        ),
        encoding="utf-8",
    )
    output = root / "known-good-request.json"
    completed = subprocess.run(
        [
            "python3",
            str(ROOT / "scripts/validate-known-good-route-request.py"),
            "--audio-list-file",
            str(audio_list),
            "--output-device",
            "Open",
            "--capture-device",
            "iRig Stream",
            "--json-out",
            str(output),
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 1:
        raise AssertionError("Open Audio 8 route did not fail")
    return json.loads(output.read_text(encoding="utf-8"))


def run_ambiguous_known_good_request_fixture(root: Path) -> dict:
    root.mkdir(parents=True, exist_ok=True)
    audio_list = root / "audio-list.txt"
    audio_list.write_text(
        "\n".join(
            [
                "Dispositivos Core Audio: 3",
                "  1  id=93  iRig Stream  uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1  in=2 out=2 rate=48000",
                "  2  id=81  Wired Test Output  uid=ExternalWiredOutputA  in=0 out=2 rate=48000",
                "  3  id=82  Wired Test Output Backup  uid=ExternalWiredOutputB  in=0 out=2 rate=48000",
                "",
            ]
        ),
        encoding="utf-8",
    )
    output = root / "known-good-request.json"
    completed = subprocess.run(
        [
            "python3",
            str(ROOT / "scripts/validate-known-good-route-request.py"),
            "--audio-list-file",
            str(audio_list),
            "--output-device",
            "Wired Test Output",
            "--capture-device",
            "iRig Stream",
            "--json-out",
            str(output),
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 1:
        raise AssertionError("ambiguous known-good route selector did not fail")
    return json.loads(output.read_text(encoding="utf-8"))


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="opena8djcpp-promotion-contract-") as temp:
        root = Path(temp)
        missing = run_evaluator(root / "missing-known-good", include_known_good=False, built_in_diagnostic=False)
        if gate(missing, "same_window_known_good_route_revalidated")["result"] != "FAIL":
            raise AssertionError("missing known-good route did not fail")
        if "known_good_route" not in missing["evidence_selection"]["physical_promotion_bundle"]["required_artifacts"]:
            raise AssertionError("known-good route is not a required bundle artifact")
        if gate(missing, "same_window_known_good_audiophile_analyzers")["result"] != "FAIL":
            raise AssertionError("missing known-good audiophile analyzers did not fail")

        skipped = run_evaluator(
            root / "skip-known-good-window",
            include_known_good=False,
            built_in_diagnostic=False,
            skip_known_good=True,
        )
        if gate(skipped, "physical_window_not_diagnostic")["result"] != "FAIL":
            raise AssertionError("skip-known-good physical window did not fail")
        preflight_flags = gate(skipped, "physical_window_not_diagnostic")["metrics"]["preflight_flags"]
        if preflight_flags.get("skip_known_good") is not True:
            raise AssertionError("skip-known-good preflight flag was not preserved")
        manifest_flags = gate(skipped, "physical_window_not_diagnostic")["metrics"]["manifest_flags"]
        if manifest_flags.get("skip_known_good") != "1":
            raise AssertionError("skip-known-good manifest flag was not preserved")

        diagnostic = run_evaluator(root / "diagnostic-known-good", include_known_good=True, built_in_diagnostic=True)
        if gate(diagnostic, "same_window_known_good_route_revalidated")["result"] != "PASS":
            raise AssertionError("diagnostic fixture should still have same-window route file")
        if gate(diagnostic, "same_window_known_good_audiophile_analyzers")["result"] != "PASS":
            raise AssertionError("diagnostic fixture should include same-window audiophile analyzer files")
        if gate(diagnostic, "physical_window_not_diagnostic")["result"] != "FAIL":
            raise AssertionError("diagnostic physical window did not fail")

        same_device = run_evaluator(
            root / "same-device-diagnostic-known-good",
            include_known_good=True,
            built_in_diagnostic=False,
            same_device_diagnostic=True,
        )
        if gate(same_device, "same_window_known_good_route_revalidated")["result"] != "PASS":
            raise AssertionError("same-device diagnostic fixture should still have same-window route file")
        if gate(same_device, "physical_window_not_diagnostic")["result"] != "FAIL":
            raise AssertionError("same-device diagnostic physical window did not fail")

        known_good = run_known_good_request_fixture(root / "known-good-request")
        if gate(known_good, "output_not_audio8")["result"] != "FAIL":
            raise AssertionError("resolved Open Audio 8 DJ output was not rejected")
        if known_good["valid_for_promotion"] is not False:
            raise AssertionError("rejected Audio 8 route was considered valid")

        ambiguous = run_ambiguous_known_good_request_fixture(root / "ambiguous-known-good-request")
        if gate(ambiguous, "output_device_unambiguous")["result"] != "FAIL":
            raise AssertionError("ambiguous known-good selector was not rejected")
        if ambiguous["valid_for_promotion"] is not False:
            raise AssertionError("ambiguous known-good route was considered valid")

    print("promotion_window_contract=PASS")
    print("missing_known_good_route_blocked=true")
    print("skip_known_good_window_blocked=true")
    print("same_device_diagnostic_window_blocked=true")
    print("built_in_acoustic_diagnostic_window_blocked=true")
    print("audio8_known_good_output_rejected=true")
    print("ambiguous_known_good_output_rejected=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
