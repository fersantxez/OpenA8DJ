"""Open the Audio 8 DJ output endpoint briefly with silence only.

This probe exercises the Windows audio render path without a tone, external
playback, USB reset, or control-state writes. A zero-sample result is valid;
the important gates are endpoint open, callback progress, and no host status
events.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path
from typing import Any

import numpy as np
import sounddevice as sd


def hostapi_name(device: dict[str, Any]) -> str:
    return str(sd.query_hostapis(int(device["hostapi"]))["name"])


def parse_diagnostics(text: str) -> dict[str, Any]:
    result: dict[str, Any] = {}
    patterns = {
        "streaming": r"^\s*streaming:\s+(\S+)",
        "render_frames": r"^\s*render-frames:\s+(\d+)",
        "packet_errors": r"^\s*packet-errors:\s+(\d+)",
        "late_completions": r"^\s*late-completions:\s+(\d+)",
        "underruns": r"^\s*underruns:\s+(\d+)",
        "overruns": r"^\s*overruns:\s+(\d+)",
    }
    for line in text.splitlines():
        for key, pattern in patterns.items():
            match = re.match(pattern, line)
            if match:
                value = match.group(1)
                result[key] = value if key == "streaming" else int(value)
    return result


def run_ctl(ctl: Path, *args: str, timeout_seconds: float = 5.0) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            [str(ctl), *args],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout_seconds,
        )
        return completed.returncode, completed.stdout
    except subprocess.TimeoutExpired as exc:
        partial = exc.stdout or ""
        if isinstance(partial, bytes):
            partial = partial.decode(errors="replace")
        return -2, f"control probe timeout after {timeout_seconds:g}s\n{partial}"


def discover_targets(
    name_substring: str,
    hostapis: set[str],
    min_output_channels: int,
    device_indices: set[int],
) -> list[dict[str, Any]]:
    targets: list[dict[str, Any]] = []
    for index, device in enumerate(sd.query_devices()):
        if device_indices and index not in device_indices:
            continue
        if name_substring.lower() not in str(device["name"]).lower():
            continue
        if int(device["max_output_channels"]) < min_output_channels:
            continue
        api = hostapi_name(device)
        if hostapis and api not in hostapis:
            continue
        targets.append(
            {
                "device_index": index,
                "hostapi": api,
                "name": str(device["name"]),
                "max_output_channels": int(device["max_output_channels"]),
                "default_samplerate": float(device["default_samplerate"]),
            }
        )
    return targets


def probe_target(target: dict[str, Any], seconds: float, rate: int, blocksize: int, channels: int) -> dict[str, Any]:
    device_index = int(target["device_index"])
    output_channels = min(channels, int(target["max_output_channels"]))
    frame_count = int(round(seconds * rate))
    cursor = 0
    status_events: list[str] = []

    def callback(outdata: np.ndarray, frames: int, _time_info: dict[str, Any], status: Any) -> None:
        nonlocal cursor
        if status:
            status_events.append(str(status))
        outdata.fill(0.0)
        cursor += frames
        if cursor >= frame_count:
            raise sd.CallbackStop

    started = time.perf_counter()
    ok = True
    error = ""
    try:
        with sd.OutputStream(
            device=device_index,
            channels=output_channels,
            samplerate=rate,
            blocksize=blocksize,
            latency="high",
            dtype="float32",
            callback=callback,
        ):
            while cursor < frame_count:
                time.sleep(0.02)
    except Exception as exc:  # noqa: BLE001 - reported as validation data
        ok = False
        error = repr(exc)

    return {
        **target,
        "requested_rate": rate,
        "requested_channels": output_channels,
        "seconds": seconds,
        "elapsed_seconds": time.perf_counter() - started,
        "ok": ok,
        "error": error,
        "frames_written": min(cursor, frame_count),
        "status_events": status_events,
        "status_event_count": len(status_events),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Probe Audio 8 DJ output endpoints with silence.")
    parser.add_argument("--ctl", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--channels", type=int, default=8)
    parser.add_argument("--name", default="Speakers (Audio 8 DJ)")
    parser.add_argument("--hostapi", action="append", default=[])
    parser.add_argument(
        "--min-output-channels",
        type=int,
        default=8,
        help="Minimum endpoint channel count; default 8 for the primary Audio 8 DJ endpoint.",
    )
    parser.add_argument(
        "--device-index",
        action="append",
        type=int,
        default=[],
        help="Restrict the diagnostic run to one or more PortAudio device indices.",
    )
    parser.add_argument(
        "--max-targets",
        type=int,
        default=0,
        help="Limit targets for a bounded diagnostic run; zero means all targets.",
    )
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    hostapis = set(args.hostapi or ["MME", "Windows DirectSound", "Windows WASAPI"])
    if args.min_output_channels <= 0:
        parser.error("--min-output-channels must be positive")
    instance_id = os.environ.get("OPENA8DJ_INSTANCE_ID", "")
    if instance_id.upper().startswith("ROOT\\") and "opena8dj virtual acx" not in args.name.lower():
        parser.error(
            "ROOT\\ instance probes must explicitly target the OpenA8DJ Virtual ACX endpoint name"
        )
    print(json.dumps({"stage": "target-discovery-start"}), flush=True)
    targets = discover_targets(
        args.name,
        hostapis,
        args.min_output_channels,
        set(args.device_index),
    )
    if args.max_targets > 0:
        targets = targets[: args.max_targets]
    print(
        json.dumps(
            {
                "stage": "targets-discovered",
                "target_count": len(targets),
                "targets": targets,
            }
        ),
        flush=True,
    )
    print(json.dumps({"stage": "diagnostics-before-start"}), flush=True)
    before_code, before_text = run_ctl(args.ctl, "diagnostics")
    before = parse_diagnostics(before_text)
    print(
        json.dumps(
            {
                "stage": "diagnostics-before-returned",
                "exit_code": before_code,
                "parsed_keys": sorted(before),
            }
        ),
        flush=True,
    )
    results = []
    for target in targets:
        target_rate = args.rate
        if target["hostapi"] in ("MME", "Windows DirectSound") and args.rate == 48000:
            target_rate = 44100
        target_with_rate = {**target, "effective_rate": target_rate}
        print(json.dumps({"stage": "target-open-start", "target": target_with_rate}), flush=True)
        result = probe_target(target, args.seconds, target_rate, args.blocksize, args.channels)
        results.append(result)
        print(json.dumps({"stage": "target-open-returned", "result": result}), flush=True)
    print(json.dumps({"stage": "diagnostics-after-start"}), flush=True)
    after_code, after_text = run_ctl(args.ctl, "diagnostics")
    after = parse_diagnostics(after_text)
    deltas = {
        key: int(after[key]) - int(before[key])
        for key in sorted(set(before) & set(after))
        if key != "streaming"
    }
    opened = [result for result in results if result["ok"]]
    failed = [result for result in results if not result["ok"]]
    status_event_count = sum(int(result["status_event_count"]) for result in results)
    summary = {
        "before_diagnostics_exit": before_code,
        "after_diagnostics_exit": after_code,
        "target_count": len(targets),
        "opened_count": len(opened),
        "failed_count": len(failed),
        "status_event_count": status_event_count,
        "frames_written": sum(int(result["frames_written"]) for result in results),
        "driver_deltas": deltas,
        "pass_endpoint_open": (
            len(targets) > 0
            and before_code == 0
            and after_code == 0
            and not failed
            and status_event_count == 0
        ),
        "silence_only": True,
    }
    (args.out_dir / "diagnostics-before.txt").write_text(before_text, encoding="utf-8")
    (args.out_dir / "diagnostics-after.txt").write_text(after_text, encoding="utf-8")
    (args.out_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    (args.out_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if summary["pass_endpoint_open"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
