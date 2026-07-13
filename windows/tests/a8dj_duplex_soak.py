#!/usr/bin/env python3
"""Bounded-memory WASAPI duplex soak for the physical Audio 8 DJ.

The probe continuously renders a low-level multi-tone on all eight channels,
consumes all eight capture channels, and records only online statistics.  It is
intended for long runs where retaining every sample would consume gigabytes.
"""

from __future__ import annotations

import argparse
import json
import math
import time
from pathlib import Path

import numpy as np
import psutil
import sounddevice as sd


PAIR_FREQUENCIES = (440.0, 833.0, 1200.0, 3190.0)


def hostapi_name(device: dict) -> str:
    return str(sd.query_hostapis(device["hostapi"])["name"])


def find_device(direction: str, name: str, hostapi: str, channels: int) -> int:
    key = f"max_{direction}_channels"
    matches = [
        index
        for index, device in enumerate(sd.query_devices())
        if device["name"] == name
        and hostapi_name(device) == hostapi
        and int(device[key]) >= channels
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected one {direction} device name={name!r} hostapi={hostapi!r} "
            f"channels>={channels}, found {matches}"
        )
    return matches[0]


def write_json(path: Path, value: object) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    # On Windows, a concurrent diagnostics reader can briefly hold the target
    # without FILE_SHARE_DELETE.  Retrying preserves atomic snapshots without
    # allowing an observer to terminate the audio soak by reading progress.
    for attempt in range(100):
        try:
            temporary.replace(path)
            return
        except OSError as error:
            if getattr(error, "winerror", None) not in (5, 32) or attempt == 99:
                raise
            time.sleep(0.050)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=300.0)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--amplitude", type=float, default=0.02)
    parser.add_argument("--input-name", default="Audio 8 DJ (8ch In) (Audio 8 DJ)")
    parser.add_argument("--output-name", default="Audio 8 DJ (8ch Out) (Audio 8 DJ)")
    parser.add_argument("--hostapi", default="Windows WASAPI")
    parser.add_argument("--exclusive", action="store_true")
    parser.add_argument("--latency", choices=("low", "high"), default="high")
    parser.add_argument("--progress-seconds", type=float, default=5.0)
    parser.add_argument("--callback-watchdog-seconds", type=float, default=5.0)
    parser.add_argument("--raw-click-threshold", type=float, default=0.075)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    if args.seconds <= 0.0 or args.blocksize <= 0 or args.rate not in (44100, 48000):
        parser.error("seconds/blocksize must be positive and rate must be 44100 or 48000")
    if not 0.0 < args.amplitude <= 0.10:
        parser.error("amplitude must be in (0, 0.10]")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    input_device = find_device("input", args.input_name, args.hostapi, 8)
    output_device = find_device("output", args.output_name, args.hostapi, 8)

    # Keep the callback representative of a native audio client.  Computing
    # four trigonometric vectors for every 512-frame callback can itself starve
    # PortAudio on the target tablet and confound driver timing with Python
    # workload.  One second is an exact period for the integer-Hz fixtures.
    tone_indexes = np.arange(args.rate, dtype=np.float64)
    tone_period = np.zeros((args.rate, 8), dtype=np.float32)
    for pair, frequency in enumerate(PAIR_FREQUENCIES):
        signal = (
            args.amplitude
            * np.sin(2.0 * math.pi * frequency * tone_indexes / args.rate)
        ).astype(np.float32)
        tone_period[:, pair * 2] = signal
        tone_period[:, pair * 2 + 1] = signal

    state = {
        "callbacks": 0,
        "render_frames": 0,
        "capture_frames": 0,
        "capture_peak": 0.0,
        "capture_clipped_frames": 0,
        "raw_click_outliers": 0,
        "status_events": [],
        "last_callback_monotonic": time.monotonic(),
        "previous_capture": np.zeros(8, dtype=np.float32),
        "callback_work_seconds": 0.0,
        "callback_work_max_seconds": 0.0,
    }
    abs_capture = np.empty((args.blocksize, 8), dtype=np.float32)
    capture_delta = np.empty((max(args.blocksize - 1, 1), 8), dtype=np.float32)
    frame_cursor = 0

    def callback(indata, outdata, frames, timing, status):
        nonlocal frame_cursor
        callback_started = time.perf_counter()
        del timing
        if status:
            state["status_events"].append(str(status))

        period_offset = frame_cursor % args.rate
        first_count = min(frames, args.rate - period_offset)
        outdata[:first_count, :] = tone_period[
            period_offset : period_offset + first_count, :
        ]
        if first_count < frames:
            outdata[first_count:, :] = tone_period[: frames - first_count, :]
        frame_cursor += frames

        capture_abs = abs_capture[:frames, :] if frames <= args.blocksize else np.abs(indata)
        if frames <= args.blocksize:
            np.abs(indata, out=capture_abs)
        peak = float(np.max(capture_abs)) if frames else 0.0
        state["capture_peak"] = max(float(state["capture_peak"]), peak)
        state["capture_clipped_frames"] += int(
            np.count_nonzero(np.any(capture_abs >= 0.999, axis=1))
        )
        if frames:
            first_delta = np.abs(indata[0] - state["previous_capture"])
            state["raw_click_outliers"] += int(np.count_nonzero(first_delta > args.raw_click_threshold))
            if frames > 1:
                if frames <= args.blocksize:
                    inner_delta = capture_delta[: frames - 1, :]
                    np.subtract(indata[1:, :], indata[:-1, :], out=inner_delta)
                    np.abs(inner_delta, out=inner_delta)
                else:
                    inner_delta = np.abs(np.diff(indata, axis=0))
                state["raw_click_outliers"] += int(
                    np.count_nonzero(inner_delta > args.raw_click_threshold)
                )
            state["previous_capture"][:] = indata[-1]
        state["callbacks"] += 1
        state["render_frames"] += frames
        state["capture_frames"] += frames
        state["last_callback_monotonic"] = time.monotonic()
        callback_work = time.perf_counter() - callback_started
        state["callback_work_seconds"] += callback_work
        state["callback_work_max_seconds"] = max(
            float(state["callback_work_max_seconds"]), callback_work
        )

    extra = None
    if args.exclusive:
        extra = (sd.WasapiSettings(exclusive=True), sd.WasapiSettings(exclusive=True))

    opened_started = time.monotonic()
    active_started = opened_started
    target_frames = int(round(args.rate * args.seconds))
    cadence_deadline_expired = False
    next_progress = opened_started
    watchdog_expired = False
    cpu_samples: list[float] = []

    with sd.Stream(
        device=(input_device, output_device),
        channels=(8, 8),
        samplerate=args.rate,
        blocksize=args.blocksize,
        dtype="float32",
        latency=args.latency,
        extra_settings=extra,
        callback=callback,
    ):
        active_started = time.monotonic()
        cadence_deadline = (
            active_started + args.seconds + args.callback_watchdog_seconds
        )
        next_progress = active_started
        while True:
            now = time.monotonic()
            if (
                int(state["render_frames"]) >= target_frames
                and int(state["capture_frames"]) >= target_frames
            ):
                break
            if now >= cadence_deadline:
                cadence_deadline_expired = True
                break
            if now - float(state["last_callback_monotonic"]) > args.callback_watchdog_seconds:
                watchdog_expired = True
                break
            cpu_samples.append(
                float(psutil.cpu_percent(interval=min(0.2, cadence_deadline - now)))
            )
            if now >= next_progress:
                progress = {
                    "elapsed_seconds": now - active_started,
                    "target_seconds": args.seconds,
                    "callbacks": state["callbacks"],
                    "render_frames": state["render_frames"],
                    "capture_frames": state["capture_frames"],
                    "capture_peak": state["capture_peak"],
                    "raw_click_outliers": state["raw_click_outliers"],
                    "status_event_count": len(state["status_events"]),
                    "cpu_last_percent": cpu_samples[-1] if cpu_samples else 0.0,
                }
                write_json(out_dir / "progress.json", progress)
                print(json.dumps(progress), flush=True)
                next_progress = now + args.progress_seconds

    elapsed = time.monotonic() - active_started
    summary = {
        "rate": args.rate,
        "blocksize": args.blocksize,
        "seconds_requested": args.seconds,
        "elapsed_seconds": elapsed,
        "input_device": input_device,
        "output_device": output_device,
        "exclusive": bool(args.exclusive),
        "latency": args.latency,
        "callbacks": state["callbacks"],
        "render_frames": state["render_frames"],
        "capture_frames": state["capture_frames"],
        "capture_peak": state["capture_peak"],
        "capture_clipped_frames": state["capture_clipped_frames"],
        "raw_click_threshold": args.raw_click_threshold,
        "raw_click_outliers": state["raw_click_outliers"],
        "status_events": state["status_events"],
        "watchdog_expired": watchdog_expired,
        "cadence_deadline_expired": cadence_deadline_expired,
        "target_frames": target_frames,
        "callback_work_avg_ms": (
            1000.0 * float(state["callback_work_seconds"]) / int(state["callbacks"])
            if int(state["callbacks"]) else 0.0
        ),
        "callback_work_max_ms": 1000.0 * float(state["callback_work_max_seconds"]),
        "cpu_avg_percent": float(np.mean(cpu_samples)) if cpu_samples else 0.0,
        "cpu_p95_percent": float(np.percentile(cpu_samples, 95)) if cpu_samples else 0.0,
        "cpu_max_percent": max(cpu_samples, default=0.0),
    }
    write_json(out_dir / "summary.json", summary)
    print(json.dumps(summary, indent=2), flush=True)

    hard_failure = (
        watchdog_expired
        or cadence_deadline_expired
        or int(state["render_frames"]) < target_frames
        or int(state["capture_frames"]) < target_frames
        or bool(state["status_events"])
        or int(state["capture_clipped_frames"]) != 0
        or int(state["raw_click_outliers"]) != 0
    )
    return 2 if args.strict and hard_failure else 0


if __name__ == "__main__":
    raise SystemExit(main())
