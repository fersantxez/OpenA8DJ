#!/usr/bin/env python3
"""Separate-stream physical tone probe for Audio 8 DJ -> mixer -> iRig.

This keeps playback and capture on their natural host APIs/rates so a weak
Windows tablet is not forced into a duplex configuration the hardware cannot
actually sustain.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import time
from datetime import datetime
from pathlib import Path

import numpy as np
import sounddevice as sd
import soundfile as sf

try:
    import psutil
except ImportError:  # pragma: no cover
    psutil = None


ROOT = Path(__file__).resolve().parents[2]


def find_device(kind: str, name_text: str, hostapi_text: str | None) -> int:
    name_needle = name_text.lower()
    hostapi_needle = hostapi_text.lower() if hostapi_text else None
    for index, device in enumerate(sd.query_devices()):
        hostapi = sd.query_hostapis(device["hostapi"])["name"]
        if hostapi_needle and hostapi_needle not in hostapi.lower():
            continue
        if name_needle not in device["name"].lower():
            continue
        if kind == "input" and int(device["max_input_channels"]) <= 0:
            continue
        if kind == "output" and int(device["max_output_channels"]) <= 0:
            continue
        return index
    raise SystemExit(f"no {kind} device matched name={name_text!r} hostapi={hostapi_text!r}")


def write_devices(path: Path) -> None:
    lines: list[str] = ["hostapis:"]
    for index, hostapi in enumerate(sd.query_hostapis()):
        lines.append(f"  {index}: {hostapi['name']}")
    lines.append("devices:")
    for index, device in enumerate(sd.query_devices()):
        hostapi = sd.query_hostapis(device["hostapi"])["name"]
        lines.append(
            f"{index:2d}: in={device['max_input_channels']} "
            f"out={device['max_output_channels']} "
            f"rate={device['default_samplerate']:.0f} "
            f"hostapi={hostapi} name={device['name']}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def make_signal(rate: int, seconds: float, frequency: float, amplitude: float, signal: str) -> np.ndarray:
    frames = int(round(rate * seconds))
    t = np.arange(frames, dtype=np.float64) / float(rate)
    if signal == "multi":
        mono = (
            0.58 * np.sin(2.0 * np.pi * frequency * t)
            + 0.27 * np.sin(2.0 * np.pi * 440.0 * t + 0.2)
            + 0.15 * np.sin(2.0 * np.pi * 3190.0 * t + 0.4)
        )
        peak = float(np.max(np.abs(mono)))
        if peak > 0:
            mono /= peak
    else:
        mono = np.sin(2.0 * np.pi * frequency * t)
    fade = max(1, int(round(rate * 0.050)))
    envelope = np.ones(frames, dtype=np.float64)
    ramp = np.linspace(0.0, 1.0, fade, dtype=np.float64)
    envelope[:fade] = ramp
    envelope[-fade:] = ramp[::-1]
    stereo = amplitude * mono[:, None] * envelope[:, None]
    return np.repeat(stereo, 2, axis=1).astype(np.float32)


def spectral_metrics(capture: np.ndarray, rate: int, targets: list[float]) -> dict[str, float | int]:
    mono = capture.mean(axis=1).astype(np.float64)
    if mono.size == 0:
        return {}
    peak = float(np.max(np.abs(capture)))
    rms = float(np.sqrt(np.mean(np.square(capture.astype(np.float64)))))
    clipped = int(np.sum(np.any(np.abs(capture) >= 0.999, axis=1)))
    near_clip = int(np.sum(np.any(np.abs(capture) >= 0.98, axis=1)))
    diff_abs = np.max(np.abs(np.diff(capture.astype(np.float64), axis=0)), axis=1) if len(capture) > 1 else np.array([])
    if diff_abs.size:
        sigma = float(np.median(np.abs(diff_abs - np.median(diff_abs))) * 1.4826)
        click_threshold = max(0.075, 12.0 * sigma)
        raw_click_outliers = int(np.sum(diff_abs > click_threshold))
    else:
        click_threshold = 0.075
        raw_click_outliers = 0

    mono -= float(np.mean(mono))
    window = np.hanning(mono.size)
    spectrum = np.fft.rfft(mono * window)
    freqs = np.fft.rfftfreq(mono.size, 1.0 / float(rate))
    mag = np.abs(spectrum)
    top_index = int(np.argmax(mag[1:]) + 1) if mag.size > 1 else 0

    metrics: dict[str, float | int] = {
        "capture_peak": peak,
        "capture_rms": rms,
        "capture_clipped_frames": clipped,
        "capture_near_clip_frames": near_clip,
        "raw_click_threshold": float(click_threshold),
        "raw_click_outliers": raw_click_outliers,
        "top_frequency_hz": float(freqs[top_index]) if top_index else 0.0,
        "top_magnitude": float(mag[top_index]) if top_index else 0.0,
    }
    for target in targets:
        index = int(np.argmin(np.abs(freqs - target)))
        key = f"mag_{int(round(target))}_hz"
        metrics[key] = float(mag[index])
    return metrics


def run_probe(args: argparse.Namespace) -> tuple[np.ndarray, np.ndarray, dict[str, float | int | str]]:
    input_device = args.input_device
    if input_device is None:
        input_device = find_device("input", args.input_name, args.input_hostapi)
    output_device = -1
    if not args.external_command:
        output_device = args.output_device
        if output_device is None:
            output_device = find_device("output", args.output_name, args.output_hostapi)
    playback = make_signal(args.output_rate, args.seconds, args.frequency, args.amplitude, args.signal)
    capture_frames = int(round((args.seconds + args.capture_extra_seconds) * args.input_rate))
    capture = np.zeros((capture_frames, 2), dtype=np.float32)
    playback_pos = 0
    capture_pos = 0
    playback_done = False
    status_events: list[str] = []
    cpu_samples: list[float] = []
    if psutil is not None:
        psutil.cpu_percent(interval=None)

    def output_callback(outdata, frames, time_info, status):  # noqa: ARG001
        nonlocal playback_pos, playback_done
        if status:
            status_events.append(f"output:{status}")
        outdata.fill(0)
        start = playback_pos
        end = min(start + frames, len(playback))
        if end > start:
            outdata[: end - start, :] = playback[start:end, :]
        playback_pos += frames
        if playback_pos >= len(playback):
            playback_done = True

    def input_callback(indata, frames, time_info, status):  # noqa: ARG001
        nonlocal capture_pos
        if status:
            status_events.append(f"input:{status}")
        start = capture_pos
        end = min(start + frames, len(capture))
        if end > start:
            capture[start:end, :] = indata[: end - start, :]
        capture_pos += frames

    started = time.perf_counter()
    with sd.InputStream(
        device=input_device,
        channels=2,
        samplerate=args.input_rate,
        blocksize=args.input_blocksize,
        latency=args.input_latency,
        dtype="float32",
        callback=input_callback,
    ):
        time.sleep(args.capture_lead_seconds)
        if args.external_command:
            completed = subprocess.run(args.external_command, shell=True, check=False)
            playback_done = True
            status_events.append(f"external_exit:{completed.returncode}")
        else:
            with sd.OutputStream(
                device=output_device,
                channels=2,
                samplerate=args.output_rate,
                blocksize=args.output_blocksize,
                latency=args.output_latency,
                dtype="float32",
                callback=output_callback,
            ):
                while not playback_done:
                    if psutil is not None:
                        cpu_samples.append(float(psutil.cpu_percent(interval=None)))
                    time.sleep(0.050)
        time.sleep(max(0.0, args.capture_extra_seconds - args.capture_lead_seconds))
    elapsed = time.perf_counter() - started

    metrics: dict[str, float | int | str] = {
        "output_device": int(output_device),
        "input_device": int(input_device),
        "output_rate": int(args.output_rate),
        "input_rate": int(args.input_rate),
        "frequency_hz": float(args.frequency),
        "signal": str(args.signal),
        "amplitude": float(args.amplitude),
        "elapsed_seconds": float(elapsed),
        "status_event_count": int(len(status_events)),
        "status_events": "; ".join(status_events[:12]),
        "playback_frames_written": int(min(playback_pos, len(playback))),
        "capture_frames_recorded": int(min(capture_pos, len(capture))),
        "cpu_monitor": "psutil" if psutil is not None else "unavailable",
    }
    if cpu_samples:
        cpu = np.asarray(cpu_samples, dtype=np.float64)
        metrics.update(
            {
                "system_cpu_samples": int(cpu.size),
                "system_cpu_avg_percent": float(np.mean(cpu)),
                "system_cpu_p95_percent": float(np.percentile(cpu, 95.0)),
                "system_cpu_max_percent": float(np.max(cpu)),
            }
        )
    return playback, capture, metrics


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=3.0)
    parser.add_argument("--frequency", type=float, default=1000.0)
    parser.add_argument("--amplitude", type=float, default=0.05)
    parser.add_argument("--signal", choices=["tone", "multi"], default="tone")
    parser.add_argument("--output-rate", type=int, default=48000)
    parser.add_argument("--input-rate", type=int, default=44100)
    parser.add_argument("--output-blocksize", type=int, default=480)
    parser.add_argument("--input-blocksize", type=int, default=2048)
    parser.add_argument("--output-latency", default="high")
    parser.add_argument("--input-latency", default="high")
    parser.add_argument("--capture-lead-seconds", type=float, default=0.5)
    parser.add_argument("--capture-extra-seconds", type=float, default=1.0)
    parser.add_argument("--output-name", default="Speakers (Audio 8 DJ)")
    parser.add_argument("--input-name", default="Line In (iRig Stream)")
    parser.add_argument("--output-device", type=int)
    parser.add_argument("--input-device", type=int)
    parser.add_argument("--output-hostapi", default="Windows WASAPI")
    parser.add_argument("--input-hostapi", default="Windows DirectSound")
    parser.add_argument("--out-dir")
    parser.add_argument("--external-command", default="")
    args = parser.parse_args()

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else ROOT / "local-analysis" / f"windows-irig-wasapi-tone-{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    write_devices(out_dir / "devices.txt")

    playback, capture, metrics = run_probe(args)
    metrics.update(spectral_metrics(capture, args.input_rate, [args.frequency, 440.0, 833.0, 836.0, 1000.0, 1200.0, 1333.0, 3190.0]))
    metrics["out_dir"] = str(out_dir)

    sf.write(out_dir / "playback.wav", playback, args.output_rate, subtype="PCM_24")
    sf.write(out_dir / "captured.wav", capture, args.input_rate, subtype="PCM_24")
    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (out_dir / "metrics.txt").write_text(
        "\n".join(f"{key}={value}" for key, value in sorted(metrics.items())) + "\n",
        encoding="utf-8",
    )

    for key in sorted(metrics):
        print(f"{key}={metrics[key]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
