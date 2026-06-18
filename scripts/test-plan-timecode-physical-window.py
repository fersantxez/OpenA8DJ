#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def write_json(path: Path, payload: dict) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")


def run_case(tmp: Path, ready: bool) -> dict:
    timecode = tmp / "timecode.json"
    rc = tmp / "rc.json"
    physical = tmp / "physical.json"
    audio_list = tmp / "audio-list.txt"
    write_json(
        timecode,
        {
            "offline_timecode_pass": True,
            "product_timecode_ready": False,
        },
    )
    write_json(
        rc,
        {
            "status": "LOCK_GATED_FULL_AB_READY" if ready else "DIAGNOSTIC_RC_ARTIFACTS_READY_ROUTE_BLOCKED",
        },
    )
    write_json(
        physical,
        {
            "full_ab_ready": ready,
        },
    )
    audio_list.write_text(
        "device id=100 Open Audio 8 DJ uid=org.opena8dj.test in=8 out=8 rate=48000\n"
        if ready
        else "device id=200 Built-in Output uid=AppleHDA in=0 out=2 rate=48000\n",
        encoding="utf-8",
    )
    output = tmp / ("ready.json" if ready else "blocked.json")
    completed = subprocess.run(
        [
            str(ROOT / "scripts/plan-timecode-physical-window"),
            "--timecode-gate",
            str(timecode),
            "--rc-status",
            str(rc),
            "--physical-window-plan",
            str(physical),
            "--audio-list-fixture",
            str(audio_list),
            "--json-out",
            str(output),
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(completed.stderr or completed.stdout)
    return json.loads(output.read_text(encoding="utf-8"))


def main() -> int:
    failures: list[str] = []
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        blocked = run_case(tmp, ready=False)
        ready = run_case(tmp, ready=True)

    if blocked.get("status") != "BLOCKED":
        failures.append("blocked_case_not_blocked")
    if blocked.get("ready_for_lock_gated_timecode_window") is not False:
        failures.append("blocked_case_ready_flag_wrong")
    if blocked.get("future_command_argv"):
        failures.append("blocked_case_emitted_command")
    if "same_session_physical_ab_not_ready" not in blocked.get("blockers", []):
        failures.append("blocked_case_missing_ab_blocker")
    if ready.get("status") != "READY":
        failures.append("ready_case_not_ready")
    if ready.get("ready_for_lock_gated_timecode_window") is not True:
        failures.append("ready_case_ready_flag_wrong")
    if not ready.get("future_command_argv"):
        failures.append("ready_case_missing_command")
    if ready.get("product_claim_allowed") is not False:
        failures.append("ready_case_allows_product_claim")
    if ready.get("timecode_vinyl_certification_allowed") is not False:
        failures.append("ready_case_allows_certification_claim")

    print(
        json.dumps(
            {
                "schema": "opena8djcpp.test-timecode-physical-window-plan.v1",
                "result": "PASS" if not failures else "FAIL",
                "failures": failures,
                "blocked_status": blocked.get("status"),
                "ready_status": ready.get("status"),
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
