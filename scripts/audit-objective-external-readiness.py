#!/usr/bin/env python3
"""Audit external prerequisites for final objective and promotion claims.

This script is read-only. It does not acquire the hardware lock, play audio,
record audio, install/uninstall drivers, restart CoreAudio, reset USB, change
defaults, install Xcode, or mutate external worktrees.
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


def git_status(path: Path) -> dict[str, Any]:
    try:
        status = subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=path, text=True, stderr=subprocess.DEVNULL
        ).splitlines()
        head = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=path,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
        branch = subprocess.check_output(
            ["git", "branch", "--show-current"],
            cwd=path,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
        return {
            "path": str(path),
            "is_git_repo": True,
            "branch": branch,
            "head": head,
            "clean": len(status) == 0,
            "dirty_count": len(status),
            "status_sample": status[:20],
        }
    except (OSError, subprocess.CalledProcessError):
        return {
            "path": str(path),
            "is_git_repo": False,
            "branch": "",
            "head": "",
            "clean": False,
            "dirty_count": -1,
            "status_sample": [],
        }


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
    parser.add_argument("--mainline", default="/Users/fer/dev/opena8dj")
    parser.add_argument("--rust", default="/Users/fer/dev/audio8djrust")
    parser.add_argument("--evidence-dir", default=str(EVIDENCE))
    args = parser.parse_args()

    evidence = Path(args.evidence_dir)
    packet = load_json(evidence / "human-test-rc-packet.json")
    final_objective = load_json(evidence / "final-objective-readiness.json")
    driverkit = load_json(evidence / "driverkit-sdk-preflight-gate.json")
    route_plan = load_json(evidence / "physical-evidence-window-plan.json")
    timecode_plan = load_json(evidence / "timecode-physical-window-plan.json")

    cpp = git_status(ROOT)
    mainline = git_status(Path(args.mainline))
    rust = git_status(Path(args.rust))

    applications_free_gib = float(driverkit.get("applications_free_gib") or 0.0)
    xcode_min_gib = float(driverkit.get("xcode_install_minimum_free_gib") or 80.0)
    xcode_missing_gib = max(0.0, xcode_min_gib - applications_free_gib)
    same_session_ab_evidence_ready = route_plan.get("product_claim_allowed") is True
    timecode_ready = timecode_plan.get("ready_for_lock_gated_timecode_window") is True
    product_human_allowed = (
        packet.get("human_test", {}).get("product_human_test_allowed") is True
    )
    source_reference_policy_ready = True

    checks = [
        gate(
            "cpp_worktree_clean",
            cpp["clean"] is True,
            cpp,
            "C++ worktree must be clean before objective evidence can be claimable",
        ),
        gate(
            "mainline_reference_clean_for_promotion",
            mainline["clean"] is True,
            mainline,
            "mainline worktree is dirty; use a clean reference before Legacy/main promotion",
        ),
        gate(
            "rust_reference_clean",
            rust["clean"] is True,
            rust,
            "Rust oracle worktree is dirty; freeze/clean before using it as reference",
        ),
        gate(
            "driverkit_sdk_install_feasible_now",
            driverkit.get("noninteractive_xcode_install_prerequisites_met") is True,
            {
                "product_driverkit_build_allowed": driverkit.get(
                    "product_driverkit_build_allowed"
                ),
                "selected_full_xcode": driverkit.get("selected_full_xcode"),
                "xcode_app_present": driverkit.get("xcode_app_present"),
                "applications_free_gib": applications_free_gib,
                "xcode_install_minimum_free_gib": xcode_min_gib,
                "xcode_missing_gib": xcode_missing_gib,
                "xcode_install_disk_space_ok": driverkit.get("xcode_install_disk_space_ok"),
                "xcodes_cli_present": driverkit.get("xcodes_cli_present"),
                "xcodes_cli_usable": driverkit.get("xcodes_cli_usable"),
                "aria2_present": driverkit.get("aria2_present"),
            },
            "full Xcode/DriverKit SDK install is not feasible now; free disk and select full Xcode",
        ),
        gate(
            "driverkit_product_build_allowed",
            driverkit.get("product_driverkit_build_allowed") is True,
            driverkit,
            "real DriverKit/deXt build remains blocked by missing SDK/toolchain",
        ),
        gate(
            "source_reference_policy_ready",
            source_reference_policy_ready,
            {
                "policy": "original_music_file_or_original_tone_is_the_promotion_reference",
                "non_audio8_known_good_route_required": False,
                "physical_path_under_test": "Audio8_output_to_iRig_capture",
                "previous_known_good_route_plan": route_plan,
            },
            "source-reference physical comparison policy is not ready",
        ),
        gate(
            "same_session_ab_ready",
            same_session_ab_evidence_ready,
            route_plan,
            "same-session mainline/C++ source-reference A/B window is not ready",
        ),
        gate(
            "product_human_audio_allowed",
            product_human_allowed,
            packet.get("human_test", {}),
            "human product audio testing is still disallowed",
        ),
        gate(
            "timecode_physical_window_ready",
            timecode_ready,
            timecode_plan,
            "physical Traktor/Timecode Vinyl window is not ready",
        ),
        gate(
            "objective_achieved",
            final_objective.get("objective_achieved") is True,
            final_objective,
            "final objective remains unproven",
        ),
    ]

    failed = [item for item in checks if item["result"] != "PASS"]
    next_actions: list[str] = []
    if driverkit.get("product_driverkit_build_allowed") is not True:
        if driverkit.get("xcode_install_disk_space_ok") is not True:
            next_actions.append(f"FREE_AT_LEAST_{xcode_missing_gib:.1f}_GIB_FOR_FULL_XCODE_DRIVERKIT_SDK")
        next_actions.append("INSTALL_SELECT_FULL_XCODE_WITH_DRIVERKIT_SDK")
    if mainline["clean"] is not True:
        next_actions.append("PREPARE_CLEAN_MAINLINE_REFERENCE_BEFORE_PROMOTION")
    if not same_session_ab_evidence_ready:
        next_actions.append("RUN_LOCK_GATED_SOURCE_REFERENCE_MAINLINE_CPP_AB_AUDIO8_TO_IRIG")
    if not timecode_ready:
        next_actions.append("RUN_LOCK_GATED_TRAKTOR_TIMECODE_WINDOW_AFTER_SOURCE_REFERENCE_AB_PASS")

    output = {
        "schema": "opena8djcpp.objective-external-readiness.v1",
        "timestamp_utc": utc_now(),
        "safety": "read_only_existing_evidence_git_status_and_toolchain_state_no_audio_coreaudio_usb_driver_or_hardware_touch",
        "result": "PASS",
        "meaning": "PASS means external blockers were audited; objective_ready controls readiness.",
        "external_readiness_status": "READY" if not failed else "BLOCKED",
        "objective_ready": not failed,
        "promotion_allowed": not failed,
        "product_human_audio_allowed": product_human_allowed,
        "source_reference_policy_ready": source_reference_policy_ready,
        "non_audio8_known_good_route_required": False,
        "driverkit_install_or_build_attempt_allowed_now": (
            driverkit.get("noninteractive_xcode_install_prerequisites_met") is True
        ),
        "route_revalidation_allowed_now": source_reference_policy_ready,
        "checks": checks,
        "blockers": [item["blocker"] for item in failed if item["blocker"]],
        "next_required_actions": list(dict.fromkeys(next_actions)),
        "evidence": {
            "human_test_rc_packet": str(evidence / "human-test-rc-packet.json"),
            "final_objective": str(evidence / "final-objective-readiness.json"),
            "driverkit_sdk_preflight": str(evidence / "driverkit-sdk-preflight-gate.json"),
            "physical_evidence_window_plan": str(evidence / "physical-evidence-window-plan.json"),
            "timecode_physical_window_plan": str(evidence / "timecode-physical-window-plan.json"),
        },
    }

    out = Path(args.json_out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(output, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
