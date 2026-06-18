#!/usr/bin/env python3
"""Offline tests for known-good-route-selector."""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


NO_ROUTE_AUDIO_LIST = """\
  1  id=10  iRig Stream  uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1  in=2 out=2 rate=48000
  2  id=11  MacBook Air Speakers  uid=BuiltInSpeakerDevice  in=0 out=2 rate=44100
  3  id=12  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
"""


READY_AUDIO_LIST = """\
  1  id=10  iRig Stream  uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1  in=2 out=2 rate=48000
  2  id=11  USB Audio CODEC  uid=AppleUSBAudioEngine:Generic:USB Audio CODEC:12345:2,1  in=0 out=2 rate=48000
  3  id=12  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
"""


def run_selector(audio_list_text: str) -> dict[str, object]:
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "audio-list.txt"
        path.write_text(audio_list_text, encoding="utf-8")
        completed = subprocess.run(
            [
                str(ROOT / "scripts/known-good-route-selector"),
                "--audio-list-file",
                str(path),
                "--reference-wav",
                str(ROOT / "fixture.wav"),
            ],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    if completed.returncode != 0:
        raise AssertionError(
            f"selector failed rc={completed.returncode} stderr={completed.stderr}"
        )
    return json.loads(completed.stdout)


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    no_route = run_selector(NO_ROUTE_AUDIO_LIST)
    expect(no_route["result"] == "PASS", "no-route inventory should still be a classified PASS")
    expect(no_route["route_revalidation_ready"] is False, "no-route fixture became ready")
    expect(
        "non_audio8_non_builtin_known_good_output_not_visible" in no_route["blockers"],
        "no-route blocker missing",
    )
    expect(no_route["command_argv"] == [], "no-route fixture emitted a command")

    ready = run_selector(READY_AUDIO_LIST)
    expect(ready["result"] == "PASS", "ready fixture did not classify")
    expect(ready["route_revalidation_ready"] is True, "ready fixture was not ready")
    expect(ready["valid_known_good_output_count"] == 1, "ready fixture output count mismatch")
    expect(ready["irig_capture_count"] == 1, "ready fixture iRig count mismatch")
    argv = ready["command_argv"]
    expect("--output-device-uid" in argv, "ready command does not use output UID")
    expect("--capture-device-uid" in argv, "ready command does not use capture UID")
    expect(
        "AppleUSBAudioEngine:Generic:USB Audio CODEC:12345:2,1" in argv,
        "ready command missing known-good output UID",
    )
    expect(
        "AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1" in argv,
        "ready command missing iRig capture UID",
    )
    expect(
        ready["next_action"] == "ACQUIRE_LOCK_AND_RUN_KNOWN_GOOD_ROUTE_COMMAND",
        "ready next action mismatch",
    )

    print("known_good_route_selector_tests=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
