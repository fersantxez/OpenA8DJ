#!/usr/bin/env python3
"""Fixture tests for objective external readiness audit."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/audit-objective-external-readiness.py"


def write_json(path: Path, payload: dict) -> None:
    path.write_text(json.dumps(payload), encoding="utf-8")


def init_repo(path: Path, dirty: bool) -> None:
    path.mkdir(parents=True)
    subprocess.check_call(["git", "init", "-q"], cwd=path)
    subprocess.check_call(["git", "config", "user.email", "test@example.invalid"], cwd=path)
    subprocess.check_call(["git", "config", "user.name", "Test"], cwd=path)
    (path / "tracked.txt").write_text("clean\n", encoding="utf-8")
    subprocess.check_call(["git", "add", "tracked.txt"], cwd=path)
    subprocess.check_call(["git", "commit", "-q", "-m", "init"], cwd=path)
    if dirty:
        (path / "dirty.txt").write_text("dirty\n", encoding="utf-8")


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        mainline = root / "mainline"
        rust = root / "rust"
        evidence = root / "evidence"
        init_repo(mainline, dirty=True)
        init_repo(rust, dirty=False)
        evidence.mkdir()
        write_json(
            evidence / "human-test-rc-packet.json",
            {
                "human_test": {"product_human_test_allowed": False},
            },
        )
        write_json(
            evidence / "final-objective-readiness.json",
            {
                "objective_achieved": False,
            },
        )
        write_json(
            evidence / "driverkit-sdk-preflight-gate.json",
            {
                "noninteractive_xcode_install_prerequisites_met": False,
                "product_driverkit_build_allowed": False,
                "selected_full_xcode": False,
                "xcode_app_present": False,
                "applications_free_gib": 3.8,
                "xcode_install_minimum_free_gib": 80.0,
                "xcode_install_disk_space_ok": False,
                "xcodes_cli_present": True,
                "xcodes_cli_usable": True,
                "aria2_present": True,
            },
        )
        write_json(
            evidence / "physical-evidence-window-plan.json",
            {
                "route_only_ready": False,
                "full_ab_ready": False,
            },
        )
        write_json(
            evidence / "timecode-physical-window-plan.json",
            {
                "ready_for_lock_gated_timecode_window": False,
            },
        )
        out = root / "external-readiness.json"
        subprocess.check_call(
            [
                sys.executable,
                str(SCRIPT),
                "--json-out",
                str(out),
                "--mainline",
                str(mainline),
                "--rust",
                str(rust),
                "--evidence-dir",
                str(evidence),
            ],
            stdout=subprocess.DEVNULL,
        )
        payload = json.loads(out.read_text(encoding="utf-8"))
        assert payload["result"] == "PASS"
        assert payload["external_readiness_status"] == "BLOCKED"
        assert payload["objective_ready"] is False
        assert payload["promotion_allowed"] is False
        assert payload["driverkit_install_or_build_attempt_allowed_now"] is False
        assert "PREPARE_CLEAN_MAINLINE_REFERENCE_BEFORE_PROMOTION" in payload[
            "next_required_actions"
        ]
        assert any(
            action.startswith("FREE_AT_LEAST_") for action in payload["next_required_actions"]
        )
        assert "wired non-Audio8 known-good route is not ready" in payload["blockers"]
    print("objective external readiness fixture PASS")


if __name__ == "__main__":
    main()
