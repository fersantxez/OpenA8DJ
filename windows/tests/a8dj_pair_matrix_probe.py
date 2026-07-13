#!/usr/bin/env python3
"""Physical 8-channel Audio 8 DJ output matrix probe.

The test plays deterministic tones through each stereo pair of the primary
8-channel Windows endpoint, records the mixer/iRig return, and records both
driver diagnostics and simple capture metrics. It intentionally avoids USB
reset/recovery. The PowerShell wrapper owns the hardware preflight/monitoring.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import time
from pathlib import Path

import numpy as np
import psutil
import sounddevice as sd
import soundfile as sf


ROOT = Path(__file__).resolve().parents[2]
CTL = ROOT / "windows" / "dist" / "Release" / "x64" / "opena8djctl.exe"

STRICT_DRIVER_COUNTERS = (
    "underruns",
    "overruns",
    "packet_errors",
    "late_completions",
    "iso_out_empty",
    "iso_out_late",
    "iso_out_bad_start",
    "iso_out_other_err",
    "iso_output_panic",
    "iso_cap_late",
    "iso_cap_bad_start",
    "iso_cap_other_err",
    "rate_settle_fails",
)


def hostapi_name(device: dict) -> str:
    return sd.query_hostapis(device["hostapi"])["name"]


def find_device(kind: str, text: str, hostapi_text: str | None, min_channels: int) -> int:
    needle = text.lower()
    hostapi_needle = hostapi_text.lower() if hostapi_text else None
    matches: list[tuple[int, dict]] = []
    for index, device in enumerate(sd.query_devices()):
        hostapi = hostapi_name(device)
        if hostapi_needle and hostapi_needle not in hostapi.lower():
            continue
        if needle not in device["name"].lower():
            continue
        channels = int(device["max_input_channels"] if kind == "input" else device["max_output_channels"])
        if channels < min_channels:
            continue
        matches.append((index, device))
    if not matches:
        raise SystemExit(
            f"no {kind} device matched name={text!r} hostapi={hostapi_text!r} min_channels={min_channels}"
        )
    return matches[0][0]


def write_devices(path: Path) -> None:
    lines: list[str] = ["hostapis:"]
    for index, hostapi in enumerate(sd.query_hostapis()):
        lines.append(f"  {index}: {hostapi['name']}")
    lines.append("devices:")
    for index, device in enumerate(sd.query_devices()):
        lines.append(
            f"{index:2d}: in={device['max_input_channels']} out={device['max_output_channels']} "
            f"rate={device['default_samplerate']:.0f} hostapi={hostapi_name(device)} name={device['name']}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_diag(text: str) -> dict[str, int | str]:
    result: dict[str, int | str] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        match = re.match(r"acx-render-pair-(\d+): nonzero=(\d+) peak-s24=(\d+)", line)
        if match:
            result[f"render_pair_{match.group(1)}_nonzero"] = int(match.group(2))
            result[f"render_pair_{match.group(1)}_peak_s24"] = int(match.group(3))
            continue
        match = re.match(r"acx-capture-pair-(\d+): nonzero=(\d+) peak-s16=(\d+)", line)
        if match:
            result[f"capture_pair_{match.group(1)}_nonzero"] = int(match.group(2))
            result[f"capture_pair_{match.group(1)}_peak_s16"] = int(match.group(3))
            continue
        match = re.match(r"([a-z0-9-]+):\s+([0-9]+)$", line)
        if match:
            result[match.group(1).replace("-", "_")] = int(match.group(2))
    return result


def diagnostics() -> tuple[str, dict[str, int | str]]:
    text = subprocess.check_output([str(CTL), "diagnostics"], text=True, errors="replace")
    return text, parse_diag(text)


def make_playback(rate: int, seconds: float, pair: int | None, amplitude: float) -> np.ndarray:
    frames = int(rate * seconds)
    t = np.arange(frames, dtype=np.float64) / float(rate)
    playback = np.zeros((frames, 8), dtype=np.float32)
    fade = max(1, int(rate * 0.05))
    env = np.ones(frames, dtype=np.float64)
    ramp = np.linspace(0.0, 1.0, fade)
    env[:fade] = ramp
    env[-fade:] = ramp[::-1]
    pairs = range(4) if pair is None else (pair,)
    for p in pairs:
        left_ch = p * 2
        right_ch = left_ch + 1
        left_freq = 523.25 + p * 173.0
        right_freq = 659.25 + p * 211.0
        playback[:, left_ch] = (amplitude * env * np.sin(2.0 * math.pi * left_freq * t)).astype(np.float32)
        playback[:, right_ch] = (amplitude * env * np.sin(2.0 * math.pi * right_freq * t)).astype(np.float32)
    pre = np.zeros((int(rate * 0.5), 8), dtype=np.float32)
    post = np.zeros((int(rate * 0.5), 8), dtype=np.float32)
    return np.vstack([pre, playback, post])


def click_metrics(captured: np.ndarray, rate: int) -> dict[str, float | int]:
    active = captured[int(rate * 0.75) : -int(rate * 0.25), :]
    diff = np.diff(active, axis=0) if len(active) > 1 else np.zeros((0, 2), dtype=np.float32)
    diff_abs = np.max(np.abs(diff), axis=1) if len(diff) else np.zeros(0, dtype=np.float32)
    mad = float(np.median(np.abs(diff_abs - np.median(diff_abs)))) if len(diff_abs) else 0.0
    sigma = mad / 0.6745 if mad > 0.0 else 0.0
    threshold = max(0.075, 12.0 * sigma)
    return {
        "raw_click_threshold": threshold,
        "raw_click_outliers": int(np.sum(diff_abs > threshold)),
    }


def run_case(args: argparse.Namespace, name: str, pair: int | None, input_device: int, output_device: int) -> dict:
    playback = make_playback(args.rate, args.seconds, pair, args.amplitude)
    captured = np.zeros((playback.shape[0], args.input_channels), dtype=np.float32)
    cursor = 0
    status_events: list[str] = []
    cpu_samples: list[dict[str, float]] = []
    proc = psutil.Process()
    psutil.cpu_percent(interval=None)
    proc.cpu_percent(interval=None)

    case_dir = Path(args.out_dir) / name
    case_dir.mkdir(parents=True, exist_ok=True)
    before_text, before = diagnostics()
    (case_dir / "diagnostics-before.txt").write_text(before_text, encoding="utf-8")

    def callback(indata, outdata, frames, _time_info, status):
        nonlocal cursor
        if status:
            status_events.append(str(status))
        end = min(cursor + frames, playback.shape[0])
        count = end - cursor
        outdata.fill(0.0)
        if count:
            outdata[:count, :] = playback[cursor:end, :]
            captured[cursor:end, :] = indata[:count, :]
        cursor += frames
        if cursor >= playback.shape[0]:
            raise sd.CallbackStop

    start = time.perf_counter()
    watchdog_expired = False
    watchdog_deadline = (
        start
        + (playback.shape[0] / float(args.rate))
        + args.watchdog_grace_seconds
    )
    extra_settings = None
    if args.exclusive:
        extra_settings = (
            sd.WasapiSettings(exclusive=True),
            sd.WasapiSettings(exclusive=True),
        )
    with sd.Stream(
        samplerate=args.rate,
        blocksize=args.blocksize,
        dtype="float32",
        latency=args.latency,
        channels=(args.input_channels, 8),
        device=(input_device, output_device),
        extra_settings=extra_settings,
        callback=callback,
    ) as stream:
        while cursor < playback.shape[0]:
            if time.perf_counter() >= watchdog_deadline:
                watchdog_expired = True
                status_events.append(
                    f"watchdog timeout after {args.watchdog_grace_seconds:.1f}s grace"
                )
                stream.abort()
                break
            cpu_samples.append(
                {
                    "system": psutil.cpu_percent(interval=args.cpu_sample_interval),
                    "process": proc.cpu_percent(interval=None),
                }
            )
    elapsed = time.perf_counter() - start
    time.sleep(0.25)
    after_text, after = diagnostics()
    (case_dir / "diagnostics-after.txt").write_text(after_text, encoding="utf-8")
    silence_text = "skipped: pair matrix never arms or attempts iso-silence\n"
    silence_exit = -1
    (case_dir / "iso-silence-after.txt").write_text(silence_text, encoding="utf-8")

    deltas: dict[str, int] = {}
    for p in range(1, 5):
        key = f"render_pair_{p}_nonzero"
        deltas[f"render_pair_{p}_nonzero_delta"] = int(after.get(key, 0)) - int(before.get(key, 0))
        deltas[f"render_pair_{p}_peak_s24_after"] = int(after.get(f"render_pair_{p}_peak_s24", 0))

    capture_clipped_frames = int(np.sum(np.max(np.abs(captured), axis=1) >= 0.999))
    capture_click_metrics = click_metrics(captured, args.rate)
    driver_counter_deltas = {
        key: int(after.get(key, 0)) - int(before.get(key, 0))
        for key in STRICT_DRIVER_COUNTERS
    }
    hard_failure_reasons: list[str] = []
    if watchdog_expired:
        hard_failure_reasons.append("stream_watchdog_expired")
    if status_events:
        hard_failure_reasons.append("portaudio_status_events")
    if capture_clipped_frames != 0:
        hard_failure_reasons.append(f"capture_clipped_frames_{capture_clipped_frames}")
    if int(capture_click_metrics["raw_click_outliers"]) != 0:
        hard_failure_reasons.append(
            f"raw_click_outliers_{capture_click_metrics['raw_click_outliers']}"
        )
    # This read-mostly matrix intentionally never invokes the destructive
    # iso-silence command. Stream, watchdog, click, and counter gates are the
    # complete acceptance surface for this probe.
    for key, delta in driver_counter_deltas.items():
        if delta != 0:
            hard_failure_reasons.append(f"driver_{key}_delta_{delta}")

    result = {
        "name": name,
        "rate": args.rate,
        "seconds": args.seconds,
        "amplitude": args.amplitude,
        "output_device": output_device,
        "input_device": input_device,
        "output_channels": 8,
        "input_channels": args.input_channels,
        "exclusive": args.exclusive,
        "elapsed_seconds": elapsed,
        "status_events": status_events,
        "watchdog_expired": watchdog_expired,
        "watchdog_grace_seconds": args.watchdog_grace_seconds,
        "iso_silence_after_exit": silence_exit,
        "capture_peak": float(np.max(np.abs(captured))),
        "capture_rms": float(np.sqrt(np.mean(np.square(captured)))),
        "capture_clipped_frames": capture_clipped_frames,
        "driver_counter_deltas": driver_counter_deltas,
        "hard_failure_reasons": hard_failure_reasons,
        "cpu_system_avg_percent": float(np.mean([x["system"] for x in cpu_samples])) if cpu_samples else 0.0,
        "cpu_system_max_percent": float(np.max([x["system"] for x in cpu_samples])) if cpu_samples else 0.0,
        "cpu_process_avg_percent": float(np.mean([x["process"] for x in cpu_samples])) if cpu_samples else 0.0,
        "cpu_process_max_percent": float(np.max([x["process"] for x in cpu_samples])) if cpu_samples else 0.0,
        "cpu_sample_interval_seconds": args.cpu_sample_interval,
        **capture_click_metrics,
        **deltas,
    }
    if args.write_wavs:
        sf.write(case_dir / "playback.wav", playback, args.rate, subtype="PCM_24")
        sf.write(case_dir / "captured.wav", captured, args.rate, subtype="PCM_24")
    (case_dir / "metrics.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def run_case_with_status_retries(
    args: argparse.Namespace,
    name: str,
    pair: int | None,
    input_device: int,
    output_device: int,
) -> dict:
    attempts: list[dict] = []
    final_result: dict | None = None
    for attempt in range(args.status_retries + 1):
        case_name = name if attempt == 0 else f"{name}-retry{attempt}"
        result = run_case(args, case_name, pair, input_device, output_device)
        attempts.append(
            {
                "attempt": attempt,
                "case_dir": case_name,
                "status_event_count": len(result["status_events"]),
                "capture_clipped_frames": result["capture_clipped_frames"],
                "cpu_system_avg_percent": result["cpu_system_avg_percent"],
                "hard_failure_reasons": result["hard_failure_reasons"],
            }
        )
        final_result = result
        if not result["hard_failure_reasons"]:
            break
        if attempt < args.status_retries:
            time.sleep(args.retry_cooldown_seconds)

    assert final_result is not None
    final_result["name"] = name
    final_result["case_dir"] = case_name
    final_result["attempt_count"] = len(attempts)
    final_result["status_retry_attempts"] = attempts
    return final_result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=6.0)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--latency", default="high")
    parser.add_argument("--amplitude", type=float, default=0.10)
    parser.add_argument("--input-name", default="Line In (iRig Stream)")
    parser.add_argument("--output-name", default="Speakers (Audio 8 DJ)")
    parser.add_argument("--input-hostapi", default="MME")
    parser.add_argument("--output-hostapi", default="MME")
    parser.add_argument("--input-channels", type=int, default=2)
    parser.add_argument("--exclusive", action="store_true")
    parser.add_argument("--mode", choices=["full", "all-pairs-only"], default="full")
    parser.add_argument("--status-retries", type=int, default=1)
    parser.add_argument("--retry-cooldown-seconds", type=float, default=3.0)
    parser.add_argument("--cpu-sample-interval", type=float, default=0.20)
    parser.add_argument("--watchdog-grace-seconds", type=float, default=15.0)
    parser.add_argument("--inter-case-cooldown-seconds", type=float, default=1.0)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--write-wavs", action="store_true")
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    write_devices(out_dir / "devices.txt")
    input_device = find_device("input", args.input_name, args.input_hostapi, args.input_channels)
    output_device = find_device("output", args.output_name, args.output_hostapi, 8)

    cases = (
        [("all-pairs-long", None)]
        if args.mode == "all-pairs-only"
        else [(f"pair-{i + 1}", i) for i in range(4)] + [("all-pairs", None)]
    )
    results = []
    for case_index, (name, pair) in enumerate(cases):
        result = run_case_with_status_retries(args, name, pair, input_device, output_device)
        results.append(result)
        if args.strict and result["hard_failure_reasons"]:
            break
        if case_index + 1 < len(cases) and args.inter_case_cooldown_seconds > 0.0:
            time.sleep(args.inter_case_cooldown_seconds)

    summary = {
        "input_device": input_device,
        "output_device": output_device,
        "input_name": sd.query_devices(input_device)["name"],
        "output_name": sd.query_devices(output_device)["name"],
        "input_hostapi": hostapi_name(sd.query_devices(input_device)),
        "output_hostapi": hostapi_name(sd.query_devices(output_device)),
        "results": results,
    }
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    if args.strict and any(result["hard_failure_reasons"] for result in results):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
