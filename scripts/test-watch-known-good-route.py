#!/usr/bin/env python3
"""Offline tests for watch-known-good-route."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


ROOT = Path(__file__).resolve().parents[1]


NO_ROUTE_AUDIO_LIST = """Dispositivos Core Audio: 4
  1  id=98  iRig Stream  uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1  in=2 out=2 rate=48000
  2  id=93  MacBook Air Microphone  uid=BuiltInMicrophoneDevice  in=1 out=0 rate=44100
  3  id=86  MacBook Air Speakers  uid=BuiltInSpeakerDevice  in=0 out=2 rate=44100
  4  id=82  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
"""


READY_AUDIO_LIST = """Dispositivos Core Audio: 5
  1  id=98  iRig Stream  uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1  in=2 out=2 rate=48000
  2  id=93  MacBook Air Microphone  uid=BuiltInMicrophoneDevice  in=1 out=0 rate=44100
  3  id=86  MacBook Air Speakers  uid=BuiltInSpeakerDevice  in=0 out=2 rate=44100
  4  id=82  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
  5  id=120  USB Audio DAC  uid=AppleUSBAudioEngine:Example:USB Audio DAC:1,2  in=0 out=2 rate=48000
"""


def run_case(audio_list: str) -> tuple[int, dict]:
    with TemporaryDirectory(prefix="opena8dj-watch-route-test-") as temp_dir:
        temp = Path(temp_dir)
        fixture = temp / "audio-list.txt"
        output = temp / "watch.json"
        fixture.write_text(audio_list, encoding="utf-8")
        completed = subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts/watch-known-good-route"),
                "--audio-list-file",
                str(fixture),
                "--timeout-seconds",
                "0",
                "--json-out",
                str(output),
            ],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        return completed.returncode, json.loads(output.read_text(encoding="utf-8"))


def main() -> int:
    blocked_rc, blocked = run_case(NO_ROUTE_AUDIO_LIST)
    ready_rc, ready = run_case(READY_AUDIO_LIST)
    failures: list[str] = []
    if blocked_rc != 2:
        failures.append(f"expected blocked rc 2, got {blocked_rc}")
    if blocked.get("status") != "BLOCKED":
        failures.append("blocked fixture did not report BLOCKED")
    if blocked.get("route_revalidation_ready") is not False:
        failures.append("blocked fixture unexpectedly route-ready")
    if ready_rc != 0:
        failures.append(f"expected ready rc 0, got {ready_rc}")
    if ready.get("status") != "READY":
        failures.append("ready fixture did not report READY")
    if ready.get("route_revalidation_ready") is not True:
        failures.append("ready fixture did not report route-ready")
    if not ready.get("command_argv"):
        failures.append("ready fixture did not expose lock-gated command argv")
    report = {
        "schema": "opena8djcpp.test-watch-known-good-route.v1",
        "result": "PASS" if not failures else "FAIL",
        "failures": failures,
        "blocked_status": blocked.get("status"),
        "ready_status": ready.get("status"),
        "ready_command_present": bool(ready.get("command_argv")),
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
