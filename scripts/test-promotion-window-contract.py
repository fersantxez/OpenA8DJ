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


def run_evaluator(window: Path, include_known_good: bool, diagnostic: bool) -> dict:
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
    write_text(
        window / "window-manifest.txt",
        "\n".join(
            [
                "execute=1",
                "route_only=0",
                "candidate_only=0",
                "skip_known_good=0",
                f"allow_built_in_output_acoustic_diagnostic={1 if diagnostic else 0}",
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
            "skip_known_good": False,
            "allow_built_in_output_acoustic_diagnostic": diagnostic,
            "known_good_output_builtin_or_acoustic": diagnostic,
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


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="opena8djcpp-promotion-contract-") as temp:
        root = Path(temp)
        missing = run_evaluator(root / "missing-known-good", include_known_good=False, diagnostic=False)
        if gate(missing, "same_window_known_good_route_revalidated")["result"] != "FAIL":
            raise AssertionError("missing known-good route did not fail")
        if "known_good_route" not in missing["evidence_selection"]["physical_promotion_bundle"]["required_artifacts"]:
            raise AssertionError("known-good route is not a required bundle artifact")

        diagnostic = run_evaluator(root / "diagnostic-known-good", include_known_good=True, diagnostic=True)
        if gate(diagnostic, "same_window_known_good_route_revalidated")["result"] != "PASS":
            raise AssertionError("diagnostic fixture should still have same-window route file")
        if gate(diagnostic, "physical_window_not_diagnostic")["result"] != "FAIL":
            raise AssertionError("diagnostic physical window did not fail")

    print("promotion_window_contract=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
