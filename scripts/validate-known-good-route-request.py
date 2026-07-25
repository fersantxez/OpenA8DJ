#!/usr/bin/env python3
"""Validate that a known-good route request cannot resolve to Audio 8.

Default mode reads an existing build/audio-list style text file and is safe for
offline tests. Live mode runs build/audio-list and must only be used by
lock-gated physical scripts after the hardware lock is acquired.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def audio8ish(value: str) -> bool:
    lowered = value.lower()
    return any(
        token in lowered
        for token in (
            "opena8dj",
            "open audio 8",
            "audio 8 dj",
            "org.opena8dj.audio8dj",
            "org.opena8dj.audio8",
        )
    )


def built_in_or_acoustic_output(value: str) -> bool:
    lowered = value.lower()
    return any(
        token in lowered
        for token in (
            "builtinspeakerdevice",
            "built-in output",
            "built in output",
            "macbook air speakers",
            "macbook speakers",
        )
    )


def looks_virtual_capture(value: str) -> bool:
    lowered = value.lower()
    return any(
        token in lowered
        for token in (
            "blackhole",
            "soundflower",
            "vb-cable",
            "vbcable",
            "background music",
            "loopback audio",
            "rogue amoeba loopback",
        )
    )


def looks_virtual_output(value: str) -> bool:
    lowered = value.lower()
    return any(
        token in lowered
        for token in (
            "blackhole",
            "soundflower",
            "vb-cable",
            "vbcable",
            "background music",
            "loopback audio",
            "rogue amoeba loopback",
        )
    )


def looks_irig_capture(value: str) -> bool:
    lowered = value.lower()
    return "irig" in lowered or "ik multimedia" in lowered


def parse_audio_list(text: str) -> list[dict[str, object]]:
    devices: list[dict[str, object]] = []
    pattern = re.compile(
        r"^\s*\d+\s+id=(?P<id>\d+)\s+(?P<name>.*?)\s+uid=(?P<uid>.*?)\s+"
        r"in=(?P<inputs>\d+)\s+out=(?P<outputs>\d+)\s+rate=(?P<rate>[0-9.]+)"
    )
    for line in text.splitlines():
        match = pattern.match(line)
        if not match:
            continue
        devices.append(
            {
                "id": int(match.group("id")),
                "name": match.group("name").strip(),
                "uid": match.group("uid"),
                "inputs": int(match.group("inputs")),
                "outputs": int(match.group("outputs")),
                "rate": float(match.group("rate")),
            }
        )
    return devices


def device_matches(device: dict[str, object], name: str, uid: str) -> bool:
    device_name = str(device.get("name", ""))
    device_uid = str(device.get("uid", ""))
    if uid:
        return device_uid == uid
    if name:
        return name.lower() in device_name.lower()
    return False


def find_devices(devices: list[dict[str, object]], name: str, uid: str) -> list[dict[str, object]]:
    return [device for device in devices if device_matches(device, name, uid)]


def unique_device(matches: list[dict[str, object]]) -> dict[str, object] | None:
    return matches[0] if len(matches) == 1 else None


def load_audio_list(args: argparse.Namespace) -> tuple[int, str, str]:
    if args.audio_list_file:
        path = Path(args.audio_list_file)
        return 0, path.read_text(encoding="utf-8", errors="replace"), ""
    completed = subprocess.run(
        [str(ROOT / "build/audio-list")],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        timeout=8,
    )
    return completed.returncode, completed.stdout, completed.stderr


def fail_gate(name: str, metrics: dict[str, object], reason: str) -> dict[str, object]:
    return {"name": name, "result": "FAIL", "metrics": metrics, "reason": reason}


def pass_gate(name: str, metrics: dict[str, object] | None = None) -> dict[str, object]:
    return {"name": name, "result": "PASS", "metrics": metrics or {}, "reason": ""}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-device", default="")
    parser.add_argument("--output-device-uid", default="")
    parser.add_argument("--capture-device", required=True)
    parser.add_argument("--capture-device-uid", default="")
    parser.add_argument("--allow-same-device-loopback-diagnostic", action="store_true")
    parser.add_argument("--allow-built-in-output-acoustic-diagnostic", action="store_true")
    parser.add_argument("--audio-list-file", default="")
    parser.add_argument("--json-out", default="")
    args = parser.parse_args()

    audio_rc, audio_stdout, audio_stderr = load_audio_list(args)
    devices = parse_audio_list(audio_stdout)
    output_matches = find_devices(devices, args.output_device, args.output_device_uid)
    capture_matches = find_devices(devices, args.capture_device, args.capture_device_uid)
    output = unique_device(output_matches)
    capture = unique_device(capture_matches)
    output_identity = " ".join(
        (
            args.output_device,
            args.output_device_uid,
            str((output or {}).get("name", "")),
            str((output or {}).get("uid", "")),
        )
    )
    capture_identity = " ".join(
        (
            args.capture_device,
            args.capture_device_uid,
            str((capture or {}).get("name", "")),
            str((capture or {}).get("uid", "")),
        )
    )
    same_device = (
        output is not None
        and capture is not None
        and (
            str(output.get("uid", "")) == str(capture.get("uid", ""))
            or str(output.get("name", "")).lower() == str(capture.get("name", "")).lower()
        )
    )
    resolved_audio8 = audio8ish(output_identity)
    built_in_output = built_in_or_acoustic_output(output_identity)
    virtual_output = looks_virtual_output(output_identity)
    virtual_capture = looks_virtual_capture(capture_identity)
    irig_capture = looks_irig_capture(capture_identity)

    gates = [
        pass_gate("audio_list_available", {"return_code": audio_rc, "device_count": len(devices)})
        if audio_rc == 0 and devices
        else fail_gate("audio_list_available", {"return_code": audio_rc}, audio_stderr.strip()),
        pass_gate("output_device_specified", {"name": args.output_device, "uid": args.output_device_uid})
        if args.output_device or args.output_device_uid
        else fail_gate("output_device_specified", {}, "output device name or UID is required"),
        pass_gate("output_device_unambiguous", {"match_count": len(output_matches), "matches": output_matches})
        if len(output_matches) == 1
        else fail_gate(
            "output_device_unambiguous",
            {"match_count": len(output_matches), "matches": output_matches},
            "output selector must resolve exactly one device; use UID when name is ambiguous",
        ),
        pass_gate("output_device_visible", {"device": output or {}})
        if output is not None and int(output.get("outputs", 0)) >= 2
        else fail_gate("output_device_visible", {"device": output or {}}, "output must be visible with at least two channels"),
        pass_gate("capture_device_unambiguous", {"match_count": len(capture_matches), "matches": capture_matches})
        if len(capture_matches) == 1
        else fail_gate(
            "capture_device_unambiguous",
            {"match_count": len(capture_matches), "matches": capture_matches},
            "capture selector must resolve exactly one device; use UID when name is ambiguous",
        ),
        pass_gate("capture_device_visible", {"device": capture or {}})
        if capture is not None and int(capture.get("inputs", 0)) >= 2
        else fail_gate("capture_device_visible", {"device": capture or {}}, "capture must be visible with at least two channels"),
        pass_gate("output_not_audio8", {"device": output or {}, "identity": output_identity})
        if not resolved_audio8
        else fail_gate("output_not_audio8", {"device": output or {}, "identity": output_identity}, "known-good output resolved to OpenA8DJ/Audio 8"),
        pass_gate("output_not_builtin_acoustic", {"device": output or {}})
        if not built_in_output or args.allow_built_in_output_acoustic_diagnostic
        else fail_gate("output_not_builtin_acoustic", {"device": output or {}}, "built-in/acoustic output cannot validate wired iRig route"),
        pass_gate("output_not_virtual", {"device": output or {}, "identity": output_identity})
        if not virtual_output or args.allow_same_device_loopback_diagnostic
        else fail_gate("output_not_virtual", {"device": output or {}, "identity": output_identity}, "virtual output cannot validate wired iRig route"),
        pass_gate("output_not_capture_device", {"output": output or {}, "capture": capture or {}})
        if not same_device or args.allow_same_device_loopback_diagnostic
        else fail_gate("output_not_capture_device", {"output": output or {}, "capture": capture or {}}, "output and capture devices are the same"),
        pass_gate("capture_not_virtual", {"identity": capture_identity})
        if not virtual_capture
        else fail_gate("capture_not_virtual", {"identity": capture_identity}, "capture appears virtual or pre-device"),
        pass_gate("capture_is_irig", {"identity": capture_identity})
        if irig_capture
        else fail_gate("capture_is_irig", {"identity": capture_identity}, "capture must resolve to the physical iRig/IK Multimedia route"),
    ]
    passed = all(gate["result"] == "PASS" for gate in gates)
    report = {
        "schema": "opena8djcpp.known-good-route-request.v1",
        "safety": "offline_audio_list_text_or_lock_gated_live_coreaudio_enumeration_only",
        "result": "PASS" if passed else "FAIL",
        "valid_for_promotion": passed
        and not args.allow_same_device_loopback_diagnostic
        and not args.allow_built_in_output_acoustic_diagnostic,
        "resolved_output_audio8": resolved_audio8,
        "resolved_output_name": str((output or {}).get("name", "")),
        "resolved_output_uid": str((output or {}).get("uid", "")),
        "resolved_capture_name": str((capture or {}).get("name", "")),
        "resolved_capture_uid": str((capture or {}).get("uid", "")),
        "output_match_count": len(output_matches),
        "capture_match_count": len(capture_matches),
        "output_builtin_or_acoustic": built_in_output,
        "output_virtual": virtual_output,
        "output_same_as_capture": same_device,
        "capture_virtual": virtual_capture,
        "capture_is_irig": irig_capture,
        "devices": devices,
        "gates": gates,
    }
    text = json.dumps(report, indent=2, sort_keys=True)
    print(text)
    if args.json_out:
        output_path = Path(args.json_out)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(text + "\n", encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
