#!/usr/bin/env python3
"""Build a human-test RC decision packet from existing evidence.

This is read-only except for the requested output files. It does not acquire
the hardware lock, play audio, record audio, install/load/unload drivers,
restart CoreAudio, reset USB, change defaults, or touch external worktrees.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_ROOT = Path(__file__).resolve().parents[1]


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def load_json(path: Path) -> dict[str, Any]:
    try:
        parsed = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return parsed if isinstance(parsed, dict) else {}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_info(path: Path) -> dict[str, Any]:
    return {
        "path": str(path),
        "exists": path.is_file(),
        "bytes": path.stat().st_size if path.is_file() else 0,
        "sha256": sha256(path) if path.is_file() else "",
    }


def bundle_info(path: Path) -> dict[str, Any]:
    executable = path / "Contents/MacOS/OpenA8DJHAL"
    return {
        "path": str(path),
        "exists": path.is_dir(),
        "executable": str(executable),
        "executable_exists": executable.is_file(),
        "executable_bytes": executable.stat().st_size if executable.is_file() else 0,
        "executable_sha256": sha256(executable) if executable.is_file() else "",
        "complete": path.is_dir() and executable.is_file(),
    }


def git_text(root: Path, *args: str) -> str:
    try:
        return subprocess.check_output(
            ["git", *args], cwd=root, text=True, stderr=subprocess.DEVNULL
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def list_or_empty(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def shell_join(argv: list[Any]) -> str:
    def quote(raw: Any) -> str:
        value = str(raw)
        safe = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./:=+,-"
        if value and all(ch in safe for ch in value):
            return value
        return "'" + value.replace("'", "'\"'\"'") + "'"

    return " ".join(quote(item) for item in argv)


def build_packet(root: Path, evidence: Path) -> dict[str, Any]:
    current = load_json(evidence / "current-offline-gates.json")
    final = load_json(evidence / "final-objective-readiness.json")
    rc_status = load_json(evidence / "human-test-rc-status.json")
    rc_gate = load_json(evidence / "human-test-rc-gate.json")
    route_plan = load_json(evidence / "physical-evidence-window-plan.json")
    timecode_plan = load_json(evidence / "timecode-physical-window-plan.json")
    driverkit = load_json(evidence / "driverkit-sdk-preflight-gate.json")
    route_contamination = load_json(evidence / "route-contamination-analysis.json")
    watcher = load_json(evidence / "watch-known-good-route-live.json")
    if not watcher:
        watcher = load_json(evidence / "watch-known-good-route.json")

    pkg = file_info(root / "build/OpenA8DJ-0.3.25.pkg")
    dmg = file_info(root / "build/OpenA8DJ-0.3.25.dmg")
    hal = bundle_info(root / "build/OpenA8DJ.driver")
    artifacts_ready = bool(pkg["exists"] and dmg["exists"] and hal["complete"])
    objective_achieved = final.get("objective_achieved") is True
    product_human_allowed = rc_gate.get("product_human_test_allowed") is True
    route_ready = route_plan.get("route_only_ready") is True
    full_ab_ready = route_plan.get("full_ab_ready") is True
    timecode_ready = timecode_plan.get("ready_for_lock_gated_timecode_window") is True

    if objective_achieved:
        packet_status = "OBJECTIVE_READY_FOR_PROMOTION_REVIEW"
    elif artifacts_ready and rc_status.get("status", "").startswith("DIAGNOSTIC_RC_ARTIFACTS_READY"):
        packet_status = "DIAGNOSTIC_RC_PACKET_READY"
    else:
        packet_status = "BLOCKED"

    route_command = list_or_empty(route_plan.get("route_only_command_argv"))
    full_ab_command = list_or_empty(route_plan.get("full_ab_command_argv"))
    next_commands: list[dict[str, Any]] = []
    if route_ready and route_command:
        next_commands.append(
            {
                "name": "lock_gated_route_revalidation",
                "requires_lock": True,
                "allowed_now": True,
                "argv": route_command,
                "shell": shell_join(route_command),
            }
        )
    if full_ab_ready and full_ab_command:
        next_commands.append(
            {
                "name": "lock_gated_same_session_mainline_cpp_ab",
                "requires_lock": True,
                "allowed_now": True,
                "argv": full_ab_command,
                "shell": shell_join(full_ab_command),
            }
        )
    if not route_ready:
        next_commands.append(
            {
                "name": "read_only_route_watcher",
                "requires_lock": False,
                "allowed_now": True,
                "argv": [
                    "scripts/watch-known-good-route",
                    "--timeout-seconds",
                    "60",
                    "--interval-seconds",
                    "2",
                    "--json-out",
                    "local-analysis/cpp-offline/watch-known-good-route-live.json",
                ],
                "shell": (
                    "scripts/watch-known-good-route --timeout-seconds 60 "
                    "--interval-seconds 2 --json-out "
                    "local-analysis/cpp-offline/watch-known-good-route-live.json"
                ),
            }
        )

    return {
        "schema": "opena8djcpp.human-test-rc-packet.v1",
        "timestamp_utc": utc_now(),
        "safety": "existing_evidence_and_artifact_hashes_only_no_audio_coreaudio_usb_driver_or_hardware_touch",
        "result": "PASS",
        "packet_status": packet_status,
        "meaning": "A diagnostic RC packet is not product readiness or superiority over mainline.",
        "worktree": str(root),
        "branch": git_text(root, "branch", "--show-current"),
        "head_commit": git_text(root, "rev-parse", "--short", "HEAD"),
        "working_tree_clean": git_text(root, "status", "--short") == "",
        "artifacts_ready": artifacts_ready,
        "artifacts": {
            "pkg": pkg,
            "dmg": dmg,
            "hal_bundle": hal,
        },
        "offline": {
            "status": current.get("status"),
            "diagnostic_status": current.get("diagnostic_status"),
            "product_readiness_status": current.get("product_readiness_status"),
            "branch_promotion_allowed": current.get("branch_promotion_allowed"),
            "quality_claim_allowed": current.get("quality_claim_allowed"),
            "evidence": str(evidence / "current-offline-gates.json"),
        },
        "objective": {
            "status": final.get("objective_status"),
            "achieved": objective_achieved,
            "branch_promotion_allowed": final.get("branch_promotion_allowed"),
            "blockers": list_or_empty(final.get("blockers")),
            "next_required_action": final.get("next_required_action"),
            "evidence": str(evidence / "final-objective-readiness.json"),
        },
        "human_test": {
            "status": rc_status.get("status"),
            "diagnostic_artifacts_ready": rc_gate.get("diagnostic_rc_artifacts_ready"),
            "product_human_test_allowed": product_human_allowed,
            "allowed_window_types": list_or_empty(rc_status.get("allowed_window_types")),
            "disallowed_claims": list_or_empty(rc_status.get("disallowed_claims")),
            "blockers": list_or_empty(rc_status.get("blockers"))
            + list_or_empty(rc_gate.get("blockers")),
            "next_action": rc_status.get("next_action") or rc_gate.get("next_required_action"),
            "evidence": str(evidence / "human-test-rc-status.json"),
        },
        "route": {
            "watcher_status": watcher.get("status"),
            "watcher_ready": watcher.get("route_revalidation_ready"),
            "route_plan_status": route_plan.get("status"),
            "route_only_ready": route_ready,
            "full_ab_ready": full_ab_ready,
            "contamination_classification": route_contamination.get("classification"),
            "human_product_test_allowed": route_contamination.get("human_product_test_allowed"),
            "blockers": list_or_empty(route_plan.get("blockers")),
            "next_action": route_plan.get("next_action") or watcher.get("next_action"),
            "evidence": str(evidence / "physical-evidence-window-plan.json"),
        },
        "timecode": {
            "physical_window_status": timecode_plan.get("status"),
            "physical_window_ready": timecode_ready,
            "certification_allowed": timecode_plan.get("timecode_vinyl_certification_allowed"),
            "blockers": list_or_empty(timecode_plan.get("blockers")),
            "next_action": timecode_plan.get("next_action"),
            "evidence": str(evidence / "timecode-physical-window-plan.json"),
        },
        "driverkit": {
            "product_driverkit_build_allowed": driverkit.get("product_driverkit_build_allowed"),
            "driverkit_sdk_path": driverkit.get("driverkit_sdk_path"),
            "next_required_action": driverkit.get("next_required_action"),
            "evidence": str(evidence / "driverkit-sdk-preflight-gate.json"),
        },
        "promotion_policy": {
            "legacy_main_promotion_allowed": objective_achieved,
            "allowed_before_objective_achieved": False,
            "required_before_promotion": [
                "validated_wired_non_audio8_irig_route",
                "same_session_mainline_cpp_physical_ab_pass",
                "runtime_cpu_resource_superiority_pass",
                "physical_traktor_timecode_vinyl_pass",
                "real_driverkit_runtime_or_explicit_hal_only_release_decision",
                "clean_external_worktree_or_clean_reference_clone_for_promotion",
            ],
        },
        "next_commands": next_commands,
    }


def write_markdown(packet: dict[str, Any], path: Path) -> None:
    lines = [
        "# OpenA8DJ C++ Human-Test RC Packet",
        "",
        f"- Status: `{packet['packet_status']}`",
        f"- Commit: `{packet['head_commit']}`",
        f"- Product human test allowed: `{packet['human_test']['product_human_test_allowed']}`",
        f"- Objective achieved: `{packet['objective']['achieved']}`",
        f"- Branch promotion allowed: `{packet['objective']['branch_promotion_allowed']}`",
        "",
        "## Artifacts",
        "",
        f"- PKG: `{packet['artifacts']['pkg']['path']}`",
        f"  - SHA256: `{packet['artifacts']['pkg']['sha256']}`",
        f"- DMG: `{packet['artifacts']['dmg']['path']}`",
        f"  - SHA256: `{packet['artifacts']['dmg']['sha256']}`",
        f"- HAL executable SHA256: `{packet['artifacts']['hal_bundle']['executable_sha256']}`",
        "",
        "## Current Decision",
        "",
        f"- Human RC status: `{packet['human_test']['status']}`",
        f"- Route status: `{packet['route']['route_plan_status']}`",
        f"- Timecode physical status: `{packet['timecode']['physical_window_status']}`",
        f"- DriverKit build allowed: `{packet['driverkit']['product_driverkit_build_allowed']}`",
        "",
        "## Blockers",
        "",
    ]
    blockers = (
        packet["objective"]["blockers"]
        + packet["human_test"]["blockers"]
        + packet["route"]["blockers"]
        + packet["timecode"]["blockers"]
    )
    for blocker in dict.fromkeys(str(item) for item in blockers if item):
        lines.append(f"- `{blocker}`")
    lines.extend(["", "## Next Commands", ""])
    for command in packet["next_commands"]:
        lines.append(f"### {command['name']}")
        lines.append("")
        lines.append(f"- Requires lock: `{command['requires_lock']}`")
        lines.append("")
        lines.append("```sh")
        lines.append(command["shell"])
        lines.append("```")
        lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(DEFAULT_ROOT))
    parser.add_argument("--evidence-dir", default="")
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--markdown-out", required=True)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    evidence = Path(args.evidence_dir).resolve() if args.evidence_dir else root / "local-analysis/cpp-offline"
    packet = build_packet(root, evidence)
    json_out = Path(args.json_out)
    markdown_out = Path(args.markdown_out)
    json_out.parent.mkdir(parents=True, exist_ok=True)
    json_out.write_text(json.dumps(packet, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_markdown(packet, markdown_out)
    print(json.dumps(packet, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
