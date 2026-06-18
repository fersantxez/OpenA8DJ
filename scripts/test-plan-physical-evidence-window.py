#!/usr/bin/env python3
"""Offline tests for plan-physical-evidence-window."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


ROOT = Path(__file__).resolve().parents[1]


def write_hal_bundle(path: Path, payload: bytes) -> None:
    executable = path / "Contents/MacOS/OpenA8DJHAL"
    executable.parent.mkdir(parents=True, exist_ok=True)
    executable.write_bytes(payload)
    (path / "Contents/Info.plist").write_text("<plist></plist>\n", encoding="utf-8")


def watcher_payload(route_ready: bool) -> dict:
    selector = {
        "blockers": [] if route_ready else ["non_audio8_non_builtin_known_good_output_not_visible"],
        "reference_wav": "",
        "selected_irig_capture": {
            "name": "iRig Stream",
            "uid": "AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1",
            "inputs": 2,
            "outputs": 2,
            "rate": 48000.0,
        }
        if route_ready
        else {},
        "selected_known_good_output": {
            "name": "USB Audio DAC",
            "uid": "AppleUSBAudioEngine:Example:USB Audio DAC:1,2",
            "inputs": 0,
            "outputs": 2,
            "rate": 48000.0,
        }
        if route_ready
        else {},
    }
    return {
        "schema": "opena8djcpp.watch-known-good-route.v1",
        "result": "PASS",
        "status": "READY" if route_ready else "BLOCKED",
        "route_revalidation_ready": route_ready,
        "last_selector": selector,
    }


def run_plan(temp: Path, watcher: dict, with_mainline: bool) -> tuple[int, dict]:
    temp.mkdir(parents=True, exist_ok=True)
    watcher_path = temp / "watcher.json"
    output_path = temp / "plan.json"
    candidate = temp / "cpp.driver"
    mainline = temp / "mainline.driver"
    reference = temp / "reference.wav"
    watcher_path.write_text(json.dumps(watcher), encoding="utf-8")
    reference.write_bytes(b"RIFF0000WAVEfmt ")
    write_hal_bundle(candidate, b"cpp")
    if with_mainline:
        write_hal_bundle(mainline, b"mainline")
    completed = subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts/plan-physical-evidence-window"),
            "--watcher-json",
            str(watcher_path),
            "--candidate",
            str(candidate),
            "--mainline-candidate",
            str(mainline),
            "--reference-wav",
            str(reference),
            "--run-dir",
            "local-analysis/physical-evidence-window/test",
            "--json-out",
            str(output_path),
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return completed.returncode, json.loads(output_path.read_text(encoding="utf-8"))


def main() -> int:
    failures: list[str] = []
    with TemporaryDirectory(prefix="opena8dj-window-plan-test-") as temp_dir:
        temp = Path(temp_dir)
        blocked_rc, blocked = run_plan(temp / "blocked", watcher_payload(False), True)
        route_rc, route_only = run_plan(temp / "route", watcher_payload(True), False)
        full_rc, full = run_plan(temp / "full", watcher_payload(True), True)
    if blocked_rc != 0 or blocked.get("status") != "BLOCKED":
        failures.append("blocked watcher did not produce PASS/BLOCKED plan")
    if blocked.get("route_only_ready") is not False:
        failures.append("blocked watcher unexpectedly route-ready")
    if route_rc != 0 or route_only.get("status") != "ROUTE_ONLY_READY":
        failures.append("ready watcher without mainline did not produce ROUTE_ONLY_READY")
    if route_only.get("full_ab_ready") is not False:
        failures.append("missing mainline unexpectedly allowed full A/B")
    if not route_only.get("route_only_command_argv"):
        failures.append("route-only plan did not include command argv")
    if full_rc != 0 or full.get("status") != "FULL_AB_READY":
        failures.append("ready watcher with both candidates did not produce FULL_AB_READY")
    if not full.get("full_ab_command_argv"):
        failures.append("full A/B plan did not include command argv")
    report = {
        "schema": "opena8djcpp.test-physical-evidence-window-plan.v1",
        "result": "PASS" if not failures else "FAIL",
        "failures": failures,
        "blocked_status": blocked.get("status"),
        "route_only_status": route_only.get("status"),
        "full_status": full.get("status"),
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
