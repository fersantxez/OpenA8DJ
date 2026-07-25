#!/usr/bin/env python3
"""Probe Audio 8 DJ capture endpoints without changing device state.

The probe opens each matching input endpoint, records a short float32 buffer,
and reports whether the endpoint opened cleanly, delivered frames, reported
host status events, and contained non-zero samples. A quiet result is not a
failure by itself because the Audio 8 DJ inputs may not have a known signal
cabled during unattended validation.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import time
from pathlib import Path
from typing import Any

import numpy as np
import sounddevice as sd
import soundfile as sf


def hostapi_name(device: dict[str, Any]) -> str:
    return str(sd.query_hostapis(int(device["hostapi"]))["name"])


def parse_diagnostics(text: str) -> dict[str, Any]:
    result: dict[str, Any] = {}
    patterns = {
        "streaming": r"^\s*streaming:\s+(\S+)",
        "capture_frames": r"^\s*capture-frames:\s+(\d+)",
        "packet_errors": r"^\s*packet-errors:\s+(\d+)",
        "late_completions": r"^\s*late-completions:\s+(\d+)",
        "underruns": r"^\s*underruns:\s+(\d+)",
        "overruns": r"^\s*overruns:\s+(\d+)",
        "acx_get_capture": r"^\s*acx-get-capture:\s+(\d+)",
        "acx_get_position": r"^\s*acx-get-position:\s+(\d+)",
    }
    pair_pattern = re.compile(r"^\s*acx-capture-pair-(\d+):\s+nonzero=(\d+)\s+peak-s16=(\d+)")
    for line in text.splitlines():
        for key, pattern in patterns.items():
            match = re.match(pattern, line)
            if match:
                value = match.group(1)
                result[key] = value if key == "streaming" else int(value)
        pair_match = pair_pattern.match(line)
        if pair_match:
            pair = pair_match.group(1)
            result[f"capture_pair_{pair}_nonzero"] = int(pair_match.group(2))
            result[f"capture_pair_{pair}_peak_s16"] = int(pair_match.group(3))
    return result


def run_ctl(ctl: Path, *args: str) -> tuple[int, str]:
    completed = subprocess.run(
        [str(ctl), *args],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return completed.returncode, completed.stdout


def discover_targets(name_substring: str, hostapis: set[str], include_wdm_ks: bool) -> list[dict[str, Any]]:
    targets: list[dict[str, Any]] = []
    for index, device in enumerate(sd.query_devices()):
        name = str(device["name"])
        api = hostapi_name(device)
        if name_substring.lower() not in name.lower():
            continue
        if int(device["max_input_channels"]) <= 0:
            continue
        if hostapis and api not in hostapis:
            continue
        if not include_wdm_ks and api == "Windows WDM-KS":
            continue
        targets.append(
            {
                "device_index": index,
                "hostapi": api,
                "name": name,
                "max_input_channels": int(device["max_input_channels"]),
                "default_samplerate": float(device["default_samplerate"]),
            }
        )
    return targets


def probe_target(
    target: dict[str, Any],
    seconds: float,
    rate: int,
    blocksize: int,
    channels: int,
    out_dir: Path,
    write_wavs: bool,
) -> dict[str, Any]:
    device_index = int(target["device_index"])
    capture_channels = min(channels, int(target["max_input_channels"]))
    frame_count = int(round(seconds * rate))
    capture = np.zeros((frame_count, capture_channels), dtype=np.float32)
    cursor = 0
    status_events: list[str] = []

    def callback(indata: np.ndarray, frames: int, time_info: dict[str, Any], status: Any) -> None:
        nonlocal cursor
        del time_info
        if status:
            status_events.append(str(status))
        start = cursor
        end = min(start + frames, frame_count)
        if end > start:
            capture[start:end, :] = indata[: end - start, :]
        cursor += frames

    started = time.perf_counter()
    ok = True
    error = ""
    try:
        with sd.InputStream(
            device=device_index,
            channels=capture_channels,
            samplerate=rate,
            blocksize=blocksize,
            latency="high",
            dtype="float32",
            callback=callback,
        ):
            time.sleep(seconds + 0.2)
    except Exception as exc:  # noqa: BLE001 - reported as data for driver validation
        ok = False
        error = repr(exc)
    elapsed = time.perf_counter() - started

    usable = capture[: min(cursor, frame_count), :]
    if usable.size:
        abs_capture = np.abs(usable)
        peak = float(np.max(abs_capture))
        rms = float(np.sqrt(np.mean(usable * usable)))
        nonzero_samples = int(np.count_nonzero(abs_capture > 1.0e-7))
        nonzero_frames = int(np.count_nonzero(np.max(abs_capture, axis=1) > 1.0e-7))
        clipped_frames = int(np.count_nonzero(np.max(abs_capture, axis=1) >= 0.999))
        near_clip_frames = int(np.count_nonzero(np.max(abs_capture, axis=1) >= 0.98))
        diff = np.diff(usable, axis=0)
        raw_click_outliers = int(np.count_nonzero(np.max(np.abs(diff), axis=1) > 0.075)) if len(usable) > 1 else 0
    else:
        peak = 0.0
        rms = 0.0
        nonzero_samples = 0
        nonzero_frames = 0
        clipped_frames = 0
        near_clip_frames = 0
        raw_click_outliers = 0

    channel_peak = np.max(np.abs(usable), axis=0).astype(float).tolist() if usable.size else []
    channel_rms = np.sqrt(np.mean(np.square(usable), axis=0)).astype(float).tolist() if usable.size else []
    wav_path = ""
    if write_wavs and usable.size:
        safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "-", str(target["name"])).strip("-")
        wav_name = f"device-{device_index:02d}-{safe_name}.wav"
        sf.write(out_dir / wav_name, usable, rate, subtype="PCM_24")
        wav_path = wav_name

    return {
        **target,
        "requested_rate": rate,
        "requested_channels": capture_channels,
        "seconds": seconds,
        "elapsed_seconds": elapsed,
        "ok": ok,
        "error": error,
        "frames_recorded": int(min(cursor, frame_count)),
        "status_events": status_events,
        "status_event_count": len(status_events),
        "capture_peak": peak,
        "capture_rms": rms,
        "channel_peak": channel_peak,
        "channel_rms": channel_rms,
        "wav_path": wav_path,
        "capture_clipped_frames": clipped_frames,
        "capture_near_clip_frames": near_clip_frames,
        "raw_click_outliers": raw_click_outliers,
        "nonzero_samples_gt_1e_7": nonzero_samples,
        "nonzero_frames_gt_1e_7": nonzero_frames,
    }


def metric_delta(before: dict[str, Any], after: dict[str, Any], key: str) -> int | None:
    if key not in before or key not in after:
        return None
    return int(after[key]) - int(before[key])


def main() -> int:
    parser = argparse.ArgumentParser(description="Probe Audio 8 DJ capture endpoints.")
    parser.add_argument("--ctl", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--seconds", type=float, default=3.0)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--channels", type=int, default=8)
    parser.add_argument(
        "--signal-threshold",
        type=float,
        default=0.001,
        help="Peak threshold for calling an input capture signal-like instead of low-level noise.",
    )
    parser.add_argument("--name", default="Audio 8 DJ")
    parser.add_argument(
        "--hostapi",
        action="append",
        default=[],
        help="Host API to include. Repeatable. Default: MME, Windows DirectSound, Windows WASAPI.",
    )
    parser.add_argument("--include-wdm-ks", action="store_true")
    parser.add_argument("--write-wavs", action="store_true")
    args = parser.parse_args()

    hostapis = set(args.hostapi or ["MME", "Windows DirectSound", "Windows WASAPI"])
    args.out_dir.mkdir(parents=True, exist_ok=True)

    before_code, before_text = run_ctl(args.ctl, "diagnostics")
    before = parse_diagnostics(before_text)
    targets = discover_targets(args.name, hostapis, args.include_wdm_ks)
    results = []
    for target in targets:
        target_rate = args.rate
        if target["hostapi"] in ("MME", "Windows DirectSound") and args.rate == 48000:
            target_rate = 44100
        results.append(
            probe_target(
                target,
                args.seconds,
                target_rate,
                args.blocksize,
                args.channels,
                args.out_dir,
                args.write_wavs,
            )
        )

    after_code, after_text = run_ctl(args.ctl, "diagnostics")
    after = parse_diagnostics(after_text)
    deltas = {
        key: metric_delta(before, after, key)
        for key in sorted(set(before.keys()) | set(after.keys()))
        if key != "streaming"
    }
    opened = [result for result in results if result["ok"]]
    failed = [result for result in results if not result["ok"]]
    status_event_count = sum(int(result["status_event_count"]) for result in results)
    low_level_endpoints = [
        result for result in opened if int(result["nonzero_frames_gt_1e_7"]) > 0 or float(result["capture_peak"]) > 0.0
    ]
    signal_like_endpoints = [result for result in opened if float(result["capture_peak"]) >= args.signal_threshold]

    summary = {
        "before_diagnostics_exit": before_code,
        "after_diagnostics_exit": after_code,
        "target_count": len(targets),
        "opened_count": len(opened),
        "failed_count": len(failed),
        "status_event_count": status_event_count,
        "low_level_nonzero_endpoint_count": len(low_level_endpoints),
        "signal_threshold": args.signal_threshold,
        "signal_like_endpoint_count": len(signal_like_endpoints),
        "driver_deltas": deltas,
        "pass_endpoint_open": len(targets) > 0 and len(failed) == 0 and status_event_count == 0,
        "pass_signal_threshold": len(signal_like_endpoints) > 0,
        "signal_note": (
            "pass_signal_threshold only means captured peak reached the configured threshold; "
            "it still does not prove calibrated Audio 8 DJ input quality without a known physical signal."
        ),
    }

    (args.out_dir / "diagnostics-before.txt").write_text(before_text, encoding="utf-8")
    (args.out_dir / "diagnostics-after.txt").write_text(after_text, encoding="utf-8")
    (args.out_dir / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    (args.out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))

    return 0 if summary["pass_endpoint_open"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
