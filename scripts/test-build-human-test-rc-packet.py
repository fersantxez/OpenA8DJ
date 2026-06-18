#!/usr/bin/env python3
"""Fixture tests for the human-test RC packet builder."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/build-human-test-rc-packet.py"


def write_json(path: Path, payload: dict) -> None:
    path.write_text(json.dumps(payload), encoding="utf-8")


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence = root / "local-analysis/cpp-offline"
        evidence.mkdir(parents=True)
        hal_exe = root / "build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL"
        hal_exe.parent.mkdir(parents=True)
        hal_exe.write_bytes(b"hal")
        (root / "build/OpenA8DJ-0.3.25.pkg").write_bytes(b"pkg")
        (root / "build/OpenA8DJ-0.3.25.dmg").write_bytes(b"dmg")

        write_json(
            evidence / "current-offline-gates.json",
            {
                "status": "PASS",
                "diagnostic_status": "PASS",
                "product_readiness_status": "FAIL",
                "branch_promotion_allowed": False,
                "quality_claim_allowed": False,
            },
        )
        write_json(
            evidence / "final-objective-readiness.json",
            {
                "objective_status": "NOT_READY",
                "objective_achieved": False,
                "branch_promotion_allowed": False,
                "blockers": ["same-session physical quality has not beaten mainline"],
                "next_required_action": "VALIDATE_WIRED_NON_AUDIO8_ROUTE",
            },
        )
        write_json(
            evidence / "human-test-rc-status.json",
            {
                "status": "DIAGNOSTIC_RC_ARTIFACTS_READY_ROUTE_CONTAMINATED",
                "allowed_window_types": ["DIAGNOSTIC_PACKAGE_REVIEW_ONLY"],
                "disallowed_claims": ["mainline_superiority"],
                "blockers": ["known_good_physical_route_not_ready"],
                "next_action": "validate route",
            },
        )
        write_json(
            evidence / "human-test-rc-gate.json",
            {
                "diagnostic_rc_artifacts_ready": True,
                "product_human_test_allowed": False,
                "blockers": ["branch_promotion_forbidden"],
            },
        )
        write_json(
            evidence / "physical-evidence-window-plan.json",
            {
                "status": "BLOCKED",
                "route_only_ready": False,
                "full_ab_ready": False,
                "blockers": ["known_good_route_not_ready"],
                "next_action": "provision route",
            },
        )
        write_json(
            evidence / "timecode-physical-window-plan.json",
            {
                "status": "BLOCKED",
                "ready_for_lock_gated_timecode_window": False,
                "timecode_vinyl_certification_allowed": False,
                "blockers": ["same_session_physical_ab_not_ready"],
                "next_action": "validate ab first",
            },
        )
        write_json(
            evidence / "driverkit-sdk-preflight-gate.json",
            {
                "product_driverkit_build_allowed": False,
                "driverkit_sdk_path": "missing",
                "next_required_action": "install Xcode",
            },
        )
        write_json(
            evidence / "route-contamination-analysis.json",
            {
                "classification": "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB",
                "human_product_test_allowed": False,
            },
        )
        write_json(
            evidence / "watch-known-good-route.json",
            {"status": "BLOCKED", "route_revalidation_ready": False},
        )

        out_json = root / "packet.json"
        out_md = root / "packet.md"
        subprocess.check_call(
            [
                sys.executable,
                str(SCRIPT),
                "--root",
                str(root),
                "--evidence-dir",
                str(evidence),
                "--json-out",
                str(out_json),
                "--markdown-out",
                str(out_md),
            ],
            stdout=subprocess.DEVNULL,
        )
        payload = json.loads(out_json.read_text(encoding="utf-8"))
        assert payload["result"] == "PASS"
        assert payload["packet_status"] == "DIAGNOSTIC_RC_PACKET_READY"
        assert payload["objective"]["achieved"] is False
        assert payload["human_test"]["product_human_test_allowed"] is False
        assert payload["promotion_policy"]["legacy_main_promotion_allowed"] is False
        assert payload["next_commands"][0]["name"] == "read_only_route_watcher"
        assert out_md.read_text(encoding="utf-8").startswith(
            "# OpenA8DJ C++ Human-Test RC Packet"
        )
    print("human-test rc packet fixture PASS")


if __name__ == "__main__":
    main()
