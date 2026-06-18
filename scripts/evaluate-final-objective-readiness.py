#!/usr/bin/env python3
"""Evaluate the full thread objective without touching hardware.

PASS means the evaluator ran and produced a fail-closed objective decision.
It does not mean the C++ line is ready or better than mainline unless
objective_achieved is true.
"""

from __future__ import annotations

import argparse
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "local-analysis/cpp-offline"


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def load_json(path: Path) -> dict[str, Any]:
    try:
        parsed = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return parsed if isinstance(parsed, dict) else {}


def git_text(args: list[str], cwd: Path = ROOT) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def gate(name: str, passed: bool, evidence: Any, blocker: str) -> dict[str, Any]:
    return {
        "name": name,
        "result": "PASS" if passed else "FAIL",
        "evidence": evidence,
        "blocker": "" if passed else blocker,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--offline", default=str(EVIDENCE / "current-offline-gates.json"))
    parser.add_argument(
        "--promotion", default=str(EVIDENCE / "promotion-readiness-offline-check.json")
    )
    parser.add_argument("--mainline", default="/Users/fer/dev/opena8dj")
    parser.add_argument("--rust", default="/Users/fer/dev/audio8djrust")
    args = parser.parse_args()

    offline_path = Path(args.offline)
    promotion_path = Path(args.promotion)
    offline = load_json(offline_path)
    promotion = load_json(promotion_path)

    mainline_status = git_text(["status", "--porcelain"], Path(args.mainline))
    rust_status = git_text(["status", "--porcelain"], Path(args.rust))
    cpp_status = git_text(["status", "--porcelain"])
    cpp_head = git_text(["rev-parse", "--short", "HEAD"])
    mainline_head = git_text(["rev-parse", "--short", "HEAD"], Path(args.mainline))
    rust_head = git_text(["rev-parse", "--short", "HEAD"], Path(args.rust))

    human_status = offline.get("human_test_rc_status_report", {})
    route_contamination = offline.get("route_contamination_analysis", {})
    timecode_window = offline.get("timecode_physical_window_plan", {})
    provenance = offline.get("evidence_provenance_freshness_gate", {})
    driverkit_sdk = offline.get("driverkit_sdk_preflight_gate", {})
    driverkit_runtime_gap = offline.get("driverkit_runtime_binding_gap_gate", {})
    product_quality = offline.get("product_quality_claim_gate", {})
    hal_runtime = offline.get("hal_transport_runtime_gate", {})
    physical_window = offline.get("physical_evidence_window_plan", {})
    release_benchmark = offline.get("release_benchmark", {})

    checks = [
        gate(
            "worktree_isolation",
            not mainline_status and not rust_status and not cpp_status,
            {
                "cpp_status": cpp_status.splitlines(),
                "mainline_status": mainline_status.splitlines(),
                "rust_status": rust_status.splitlines(),
                "cpp_head": cpp_head,
                "mainline_head": mainline_head,
                "rust_head": rust_head,
            },
            "all worktrees must be clean before objective or promotion claims",
        ),
        gate(
            "offline_gates_current_and_fresh",
            offline.get("status") == "PASS"
            and offline.get("diagnostic_status") == "PASS"
            and provenance.get("status") == "PASS"
            and provenance.get("claimable_current_candidate") is True
            and offline.get("base_commit") == cpp_head,
            {
                "offline_status": offline.get("status"),
                "diagnostic_status": offline.get("diagnostic_status"),
                "base_commit": offline.get("base_commit"),
                "head_commit": cpp_head,
                "provenance": provenance,
            },
            "offline evidence must pass and be attributable to current clean HEAD",
        ),
        gate(
            "installable_rc_artifacts_available",
            offline.get("human_test_rc_gate", {}).get("bundle_ready") is True
            and offline.get("human_test_rc_gate", {}).get("package_present") is True,
            offline.get("human_test_rc_gate", {}),
            "installable HAL/PKG/DMG artifacts must exist",
        ),
        gate(
            "driverkit_runtime_ready",
            driverkit_sdk.get("product_driverkit_build_allowed") is True
            and driverkit_runtime_gap.get("product_driverkit_runtime_ready") is True,
            {
                "driverkit_sdk_preflight_gate": driverkit_sdk,
                "driverkit_runtime_binding_gap_gate": driverkit_runtime_gap,
            },
            "real DriverKit/dext build and runtime readiness are not proven on this host",
        ),
        gate(
            "physical_route_valid",
            physical_window.get("source_reference_policy_ready") is True
            and route_contamination.get("internal_usb_clean") is True
            and route_contamination.get("human_product_test_allowed") is True,
            {
                "physical_evidence_window_plan": physical_window,
                "route_contamination_analysis": route_contamination,
                "human_status": human_status,
            },
            "source-reference Audio8-to-iRig physical comparison is not validated",
        ),
        gate(
            "same_session_quality_beats_mainline",
            product_quality.get("quality_claim_allowed") is True
            and promotion.get("branch_promotion_allowed") is True,
            {
                "product_quality_claim_gate": product_quality,
                "promotion_result": promotion.get("result"),
                "branch_promotion_allowed": promotion.get("branch_promotion_allowed"),
                "failing_gates": [
                    item.get("name")
                    for item in promotion.get("gates", [])
                    if item.get("result") != "PASS"
                ],
            },
            "same-session physical quality has not beaten mainline",
        ),
        gate(
            "runtime_cpu_and_resource_beats_mainline",
            hal_runtime.get("runtime_reduction_missing") is False
            and hal_runtime.get("product_claim_blocked") is False
            and promotion.get("branch_promotion_allowed") is True,
            {
                "hal_transport_runtime_gate": hal_runtime,
                "release_benchmark": release_benchmark,
            },
            "runtime CPU/resource superiority over mainline is not physically proven",
        ),
        gate(
            "timecode_vinyl_physical_ready",
            timecode_window.get("ready_for_lock_gated_timecode_window") is True
            and timecode_window.get("timecode_vinyl_certification_allowed") is True,
            timecode_window,
            "physical Traktor/Timecode Vinyl window is blocked or uncertified",
        ),
        gate(
            "legacy_main_promotion_allowed",
            promotion.get("branch_promotion_allowed") is True
            and offline.get("branch_promotion_allowed") is True,
            {
                "offline_branch_promotion_allowed": offline.get("branch_promotion_allowed"),
                "promotion_branch_promotion_allowed": promotion.get("branch_promotion_allowed"),
                "promotion_result": promotion.get("result"),
            },
            "Legacy/main promotion remains forbidden until all objective evidence passes",
        ),
    ]

    failed = [item for item in checks if item["result"] != "PASS"]
    objective_achieved = not failed
    output = {
        "schema": "opena8djcpp.final-objective-readiness.v1",
        "timestamp_utc": utc_now(),
        "safety": "offline_existing_evidence_and_git_status_only_no_audio_coreaudio_usb_driver_or_hardware_touch",
        "result": "PASS",
        "meaning": "PASS means this evaluator ran and fail-closed the final objective decision; objective_achieved is the readiness claim.",
        "objective_achieved": objective_achieved,
        "objective_status": "ACHIEVED" if objective_achieved else "NOT_READY",
        "quality_superiority_proven": objective_achieved,
        "functionality_superiority_or_parity_proven": objective_achieved,
        "performance_superiority_proven": objective_achieved,
        "timecode_vinyl_physical_proven": objective_achieved,
        "legacy_main_promotion_plan_allowed": objective_achieved,
        "branch_promotion_allowed": objective_achieved,
        "checks": checks,
        "blockers": [item["blocker"] for item in failed if item["blocker"]],
        "next_required_action": (
            "PREPARE_LEGACY_MAIN_PROMOTION_WINDOW"
            if objective_achieved
            else "RUN_SOURCE_REFERENCE_MAINLINE_CPP_AB_CPU_TIMECODE"
        ),
        "evidence": {
            "offline": str(offline_path),
            "promotion": str(promotion_path),
            "mainline_worktree": args.mainline,
            "rust_worktree": args.rust,
        },
    }

    out_path = Path(args.json_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(output, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
