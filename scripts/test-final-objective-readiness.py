#!/usr/bin/env python3
"""Fixture tests for final objective readiness evaluator."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/evaluate-final-objective-readiness.py"


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        offline = root / "offline.json"
        promotion = root / "promotion.json"
        offline.write_text(
            json.dumps(
                {
                    "status": "PASS",
                    "diagnostic_status": "PASS",
                    "base_commit": "fixture-head",
                    "branch_promotion_allowed": False,
                    "evidence_provenance_freshness_gate": {
                        "status": "PASS",
                        "claimable_current_candidate": True,
                    },
                    "human_test_rc_gate": {
                        "bundle_ready": True,
                        "package_present": True,
                    },
                    "driverkit_sdk_preflight_gate": {
                        "product_driverkit_build_allowed": False,
                    },
                    "driverkit_runtime_binding_gap_gate": {
                        "product_driverkit_runtime_ready": False,
                    },
                    "physical_evidence_window_plan": {"route_only_ready": False},
                    "route_contamination_analysis": {
                        "human_product_test_allowed": False,
                    },
                    "product_quality_claim_gate": {
                        "quality_claim_allowed": False,
                    },
                    "hal_transport_runtime_gate": {
                        "runtime_reduction_missing": True,
                        "product_claim_blocked": True,
                    },
                    "timecode_physical_window_plan": {
                        "ready_for_lock_gated_timecode_window": False,
                        "timecode_vinyl_certification_allowed": False,
                    },
                }
            ),
            encoding="utf-8",
        )
        promotion.write_text(
            json.dumps(
                {
                    "result": "FAIL",
                    "branch_promotion_allowed": False,
                    "gates": [{"name": "same_session_quality", "result": "FAIL"}],
                }
            ),
            encoding="utf-8",
        )
        out = root / "out.json"
        subprocess.check_call(
            [
                sys.executable,
                str(SCRIPT),
                "--offline",
                str(offline),
                "--promotion",
                str(promotion),
                "--mainline",
                str(ROOT),
                "--rust",
                str(ROOT),
                "--json-out",
                str(out),
            ],
            stdout=subprocess.DEVNULL,
        )
        payload = json.loads(out.read_text(encoding="utf-8"))
        assert payload["result"] == "PASS"
        assert payload["objective_achieved"] is False
        assert payload["objective_status"] == "NOT_READY"
        assert payload["branch_promotion_allowed"] is False
        assert "physical Traktor/Timecode Vinyl window is blocked or uncertified" in payload[
            "blockers"
        ]
    print("final objective readiness fixture PASS")


if __name__ == "__main__":
    main()
