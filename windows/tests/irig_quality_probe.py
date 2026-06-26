#!/usr/bin/env python3
"""Physical Audio 8 DJ -> mixer -> iRig capture quality probe for Windows.

This is a baseline tool for the commercial Windows driver and later OpenA8DJ
Windows builds. It generates a deterministic stereo reference, plays it through
a selected Audio 8 DJ output device, records a selected iRig input, and writes
simple numeric quality metrics.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time
from datetime import datetime
from pathlib import Path

import numpy as np
import sounddevice as sd
import soundfile as sf

try:
    import psutil
except ImportError:  # pragma: no cover - optional runtime dependency
    psutil = None


ROOT = Path(__file__).resolve().parents[2]


def dbfs(value: float) -> float:
    if value <= 0:
        return -999.0
    return 20.0 * math.log10(value)


def next_power_of_two(value: int) -> int:
    return 1 << (value - 1).bit_length()


def make_reference(rate: int, seconds: float, amplitude: float, signal: str) -> np.ndarray:
    frames = int(round(rate * seconds))
    t = np.arange(frames, dtype=np.float64) / float(rate)

    if signal == "tone":
        left = np.sin(2.0 * np.pi * 1000.0 * t)
        right = np.sin(2.0 * np.pi * 1000.0 * t)
    else:
        left = (
            0.62 * np.sin(2.0 * np.pi * 1000.0 * t)
            + 0.25 * np.sin(2.0 * np.pi * 440.0 * t)
            + 0.13 * np.sin(2.0 * np.pi * 3190.0 * t)
        )
        right = (
            0.58 * np.sin(2.0 * np.pi * 997.0 * t + 0.35)
            + 0.27 * np.sin(2.0 * np.pi * 660.0 * t)
            + 0.15 * np.sin(2.0 * np.pi * 2710.0 * t)
        )

    # A slow envelope avoids start/stop pops while still exercising the route.
    fade_frames = max(1, int(rate * 0.050))
    envelope = np.ones(frames, dtype=np.float64)
    ramp = np.linspace(0.0, 1.0, fade_frames, dtype=np.float64)
    envelope[:fade_frames] = ramp
    envelope[-fade_frames:] = ramp[::-1]

    ref = np.column_stack([left, right]) * envelope[:, None]
    peak = float(np.max(np.abs(ref)))
    if peak > 0:
        ref *= amplitude / peak
    return ref.astype(np.float32)


def write_device_list(path: Path) -> None:
    lines: list[str] = []
    lines.append("hostapis:")
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


def find_device(kind: str, text: str, hostapi_text: str | None) -> int:
    matches: list[tuple[int, dict]] = []
    needle = text.lower()
    hostapi_needle = hostapi_text.lower() if hostapi_text else None
    for index, device in enumerate(sd.query_devices()):
        hostapi = sd.query_hostapis(device["hostapi"])["name"]
        if hostapi_needle and hostapi_needle not in hostapi.lower():
            continue
        if needle not in device["name"].lower():
            continue
        if kind == "input" and int(device["max_input_channels"]) <= 0:
            continue
        if kind == "output" and int(device["max_output_channels"]) <= 0:
            continue
        matches.append((index, device))

    if not matches:
        raise SystemExit(f"no {kind} device matched {text!r} hostapi={hostapi_text!r}")
    return matches[0][0]


def play_and_record(
    playback: np.ndarray,
    input_device: int,
    output_device: int,
    rate: int,
    blocksize: int,
    latency: str | float,
    progress_hooks: list[tuple[int, str, object]] | None = None,
) -> tuple[np.ndarray, list[str], float, dict[str, float | int | str]]:
    capture = np.zeros_like(playback, dtype=np.float32)
    status_events: list[str] = []
    position = 0
    total = len(playback)
    start_perf = time.perf_counter()
    cpu_samples: list[float] = []
    if psutil is not None:
        psutil.cpu_percent(interval=None)
    pending_hooks = sorted(progress_hooks or [], key=lambda item: item[0])

    def callback(indata, outdata, frames, time_info, status):  # noqa: ARG001
        nonlocal position
        if status:
            status_events.append(str(status))

        start = position
        end = min(position + frames, total)
        n = max(0, end - start)

        outdata.fill(0)
        if n:
            outdata[:n, :] = playback[start:end, :]
            capture[start:end, :] = indata[:n, :]

        position += frames

    with sd.Stream(
        samplerate=rate,
        blocksize=blocksize,
        latency=latency,
        dtype="float32",
        channels=(2, 2),
        device=(input_device, output_device),
        callback=callback,
    ):
        while position < total:
            while pending_hooks and position >= pending_hooks[0][0]:
                _, _, hook = pending_hooks.pop(0)
                hook(position, total)
            if psutil is not None:
                cpu_samples.append(float(psutil.cpu_percent(interval=None)))
            time.sleep(0.050)

    elapsed = time.perf_counter() - start_perf
    cpu_metrics: dict[str, float | int | str] = {"cpu_monitor": "psutil" if psutil is not None else "unavailable"}
    if cpu_samples:
        cpu_array = np.asarray(cpu_samples, dtype=np.float64)
        cpu_metrics.update(
            {
                "system_cpu_samples": int(len(cpu_samples)),
                "system_cpu_avg_percent": float(np.mean(cpu_array)),
                "system_cpu_p95_percent": float(np.percentile(cpu_array, 95)),
                "system_cpu_max_percent": float(np.max(cpu_array)),
            }
        )
    return capture, status_events, elapsed, cpu_metrics


def estimate_alignment(
    reference: np.ndarray,
    capture: np.ndarray,
    rate: int,
    expected_offset_frames: int | None = None,
) -> tuple[int, float]:
    ref = reference.mean(axis=1).astype(np.float64)
    cap = capture.mean(axis=1).astype(np.float64)
    ref -= float(np.mean(ref))
    cap -= float(np.mean(cap))

    decimation = max(1, int(rate // 4000))
    ref_d = ref[::decimation]
    cap_d = cap[::decimation]

    n = next_power_of_two(len(ref_d) + len(cap_d) - 1)
    corr = np.fft.irfft(
        np.fft.rfft(cap_d, n) * np.fft.rfft(ref_d[::-1], n),
        n,
    )
    corr = corr[: len(ref_d) + len(cap_d) - 1]
    first_valid = len(ref_d) - 1
    last_valid = len(cap_d) - 1
    if last_valid < first_valid:
        return 0, 0.0
    valid_corr = corr[first_valid : last_valid + 1]
    if expected_offset_frames is not None:
        expected_d = max(0, expected_offset_frames // decimation)
        # Keep the search near the known pre-roll. Periodic tones can create
        # false global maxima near frame 0 even when the captured signal starts
        # after the intentional silence.
        early_slack_d = max(1, int(rate * 0.20) // decimation)
        late_slack_d = max(1, int(rate * 1.00) // decimation)
        lo = max(0, expected_d - early_slack_d)
        hi = min(len(valid_corr), expected_d + late_slack_d + 1)
        lag_d = lo + int(np.argmax(valid_corr[lo:hi]))
    else:
        lag_d = int(np.argmax(valid_corr))
    rough = lag_d * decimation

    best_offset = rough
    best_score = -1.0
    search_radius = decimation * 4
    max_offset = max(0, len(cap) - len(reference))
    search_min = max(0, rough - search_radius)
    search_max = min(max_offset, rough + search_radius)
    if expected_offset_frames is not None:
        search_min = max(search_min, expected_offset_frames - int(rate * 0.20))
        search_max = min(search_max, expected_offset_frames + int(rate * 1.00))
    for offset in range(search_min, search_max + 1):
        segment = cap[offset : offset + len(reference)]
        denom = float(np.linalg.norm(segment) * np.linalg.norm(ref))
        score = 0.0 if denom == 0 else float(np.dot(segment, ref) / denom)
        if score > best_score:
            best_score = score
            best_offset = offset

    return best_offset, best_score


def analyze(
    reference: np.ndarray,
    capture: np.ndarray,
    rate: int,
    status_events: list[str],
    elapsed: float,
    cpu_metrics: dict[str, float | int | str],
    expected_offset_frames: int | None = None,
) -> dict:
    offset, alignment_score = estimate_alignment(reference, capture, rate, expected_offset_frames)
    aligned = capture[offset : offset + len(reference), :]
    if len(aligned) < len(reference):
        pad = np.zeros((len(reference) - len(aligned), 2), dtype=np.float32)
        aligned = np.vstack([aligned, pad])

    metrics: dict[str, object] = {
        "rate": rate,
        "reference_frames": int(len(reference)),
        "capture_frames": int(len(capture)),
        "elapsed_seconds": elapsed,
        "alignment_offset_frames": int(offset),
        "expected_alignment_offset_frames": expected_offset_frames,
        "alignment_score_mono": alignment_score,
        "portaudio_status_event_count": len(status_events),
        "portaudio_status_events": status_events[:20],
    }
    metrics.update(cpu_metrics)

    capture_abs = np.abs(capture)
    metrics["capture_peak"] = float(np.max(capture_abs))
    metrics["capture_peak_dbfs"] = dbfs(float(np.max(capture_abs)))
    metrics["capture_rms"] = float(np.sqrt(np.mean(np.square(capture))))
    metrics["capture_rms_dbfs"] = dbfs(float(metrics["capture_rms"]))
    metrics["capture_near_clip_frames"] = int(np.sum(np.any(capture_abs >= 0.98, axis=1)))
    metrics["capture_clipped_frames"] = int(np.sum(np.any(capture_abs >= 0.999, axis=1)))

    raw_window = 1024
    raw_usable = (len(capture) // raw_window) * raw_window
    if raw_usable:
        raw_win = capture[:raw_usable, :].reshape(-1, raw_window, 2)
        raw_win_rms = np.sqrt(np.mean(np.square(raw_win), axis=(1, 2)))
        active_threshold = max(0.003, float(np.percentile(raw_win_rms, 95)) * 0.08)
        active_indexes = np.flatnonzero(raw_win_rms > active_threshold)
        metrics["raw_active_threshold_rms"] = active_threshold
        metrics["raw_window_count"] = int(len(raw_win_rms))
        metrics["raw_active_window_count"] = int(len(active_indexes))
        if len(active_indexes):
            first_active = int(active_indexes[0])
            last_active = int(active_indexes[-1])
            active_span = raw_win_rms[first_active : last_active + 1]
            metrics["raw_active_first_window"] = first_active
            metrics["raw_active_last_window"] = last_active
            metrics["raw_low_windows_in_active_span"] = int(np.sum(active_span <= active_threshold))
        else:
            metrics["raw_active_first_window"] = -1
            metrics["raw_active_last_window"] = -1
            metrics["raw_low_windows_in_active_span"] = int(len(raw_win_rms))

        raw_diff = np.diff(capture[:raw_usable, :], axis=0)
        raw_diff_abs = np.max(np.abs(raw_diff), axis=1)
        raw_mad = float(np.median(np.abs(raw_diff_abs - np.median(raw_diff_abs))))
        raw_sigma = raw_mad / 0.6745 if raw_mad > 0.0 else 0.0
        raw_click_threshold = max(0.075, 12.0 * raw_sigma)
        metrics["raw_click_threshold"] = raw_click_threshold
        metrics["raw_click_outliers"] = int(np.sum(raw_diff_abs > raw_click_threshold))

    channel_metrics = []
    for channel in range(2):
        ref_ch = reference[:, channel].astype(np.float64)
        cap_ch = aligned[:, channel].astype(np.float64)
        denom = float(np.dot(ref_ch, ref_ch))
        gain = 0.0 if denom == 0.0 else float(np.dot(cap_ch, ref_ch) / denom)
        predicted = gain * ref_ch
        residual = cap_ch - predicted
        signal_rms = float(np.sqrt(np.mean(np.square(predicted))))
        noise_rms = float(np.sqrt(np.mean(np.square(residual))))
        corr_denom = float(np.linalg.norm(ref_ch) * np.linalg.norm(cap_ch))
        correlation = 0.0 if corr_denom == 0.0 else float(np.dot(ref_ch, cap_ch) / corr_denom)

        residual_diff = np.diff(residual)
        mad = float(np.median(np.abs(residual_diff - np.median(residual_diff)))) if len(residual_diff) else 0.0
        sigma = mad / 0.6745 if mad > 0.0 else 0.0
        click_threshold = max(0.050, 10.0 * sigma)
        click_outliers = int(np.sum(np.abs(residual_diff) > click_threshold))

        window = 1024
        usable = (len(cap_ch) // window) * window
        if usable:
            win = cap_ch[:usable].reshape(-1, window)
            win_rms = np.sqrt(np.mean(np.square(win), axis=1))
            median_rms = float(np.median(win_rms))
            dropout_windows = int(np.sum(win_rms < max(1e-6, median_rms * 0.10)))
        else:
            median_rms = 0.0
            dropout_windows = 0

        channel_metrics.append(
            {
                "channel": channel,
                "gain": gain,
                "correlation": correlation,
                "snr_db": dbfs(signal_rms / max(noise_rms, 1e-12)),
                "residual_rms": noise_rms,
                "click_threshold": click_threshold,
                "click_outliers": click_outliers,
                "dropout_windows": dropout_windows,
                "median_window_rms": median_rms,
            }
        )

    metrics["channels"] = channel_metrics
    metrics["min_correlation"] = min(float(ch["correlation"]) for ch in channel_metrics)
    metrics["min_snr_db"] = min(float(ch["snr_db"]) for ch in channel_metrics)
    metrics["click_outliers_total"] = sum(int(ch["click_outliers"]) for ch in channel_metrics)
    metrics["dropout_windows_total"] = sum(int(ch["dropout_windows"]) for ch in channel_metrics)
    return metrics


def write_metrics(path: Path, metrics: dict) -> None:
    lines = []
    for key, value in metrics.items():
        if key == "channels":
            continue
        if key == "portaudio_status_events":
            lines.append(f"{key}={json.dumps(value)}")
        else:
            lines.append(f"{key}={value}")
    for ch in metrics["channels"]:
        prefix = f"channel_{ch['channel']}"
        for key, value in ch.items():
            if key != "channel":
                lines.append(f"{prefix}.{key}={value}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-device", type=int)
    parser.add_argument("--output-device", type=int)
    parser.add_argument("--input-name", default="Line In (iRig Stream)")
    parser.add_argument("--output-name", default="Audio 8 DJ (Ch A, Out 1|2)")
    parser.add_argument("--hostapi", default="Windows DirectSound")
    parser.add_argument("--rate", type=int, default=44100)
    parser.add_argument("--seconds", type=float, default=20.0)
    parser.add_argument("--pre-roll", type=float, default=1.0)
    parser.add_argument("--post-roll", type=float, default=1.0)
    parser.add_argument("--amplitude", type=float, default=0.20)
    parser.add_argument("--signal", choices=("tone", "multitone"), default="multitone")
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--latency", default="high")
    parser.add_argument("--out-dir")
    parser.add_argument("--list-devices", action="store_true")
    args = parser.parse_args()

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else ROOT / "local-analysis" / f"windows-irig-quality-{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    write_device_list(out_dir / "devices.txt")

    if args.list_devices:
        print((out_dir / "devices.txt").read_text(encoding="utf-8"))
        return 0

    input_device = args.input_device
    if input_device is None:
        input_device = find_device("input", args.input_name, args.hostapi)
    output_device = args.output_device
    if output_device is None:
        output_device = find_device("output", args.output_name, args.hostapi)

    sd.check_input_settings(device=input_device, channels=2, samplerate=args.rate, dtype="float32")
    sd.check_output_settings(device=output_device, channels=2, samplerate=args.rate, dtype="float32")

    reference = make_reference(args.rate, args.seconds, args.amplitude, args.signal)
    pre = np.zeros((int(round(args.pre_roll * args.rate)), 2), dtype=np.float32)
    post = np.zeros((int(round(args.post_roll * args.rate)), 2), dtype=np.float32)
    playback = np.vstack([pre, reference, post])

    sf.write(out_dir / "reference.wav", reference, args.rate, subtype="PCM_24")
    sf.write(out_dir / "playback_with_silence.wav", playback, args.rate, subtype="PCM_24")

    print(f"input_device={input_device} {sd.query_devices(input_device)['name']}")
    print(f"output_device={output_device} {sd.query_devices(output_device)['name']}")
    print(
        f"rate={args.rate} seconds={args.seconds} blocksize={args.blocksize} "
        f"latency={args.latency} signal={args.signal} amplitude={args.amplitude}"
    )
    print(f"out_dir={out_dir}")

    capture, status_events, elapsed, cpu_metrics = play_and_record(
        playback,
        input_device=input_device,
        output_device=output_device,
        rate=args.rate,
        blocksize=args.blocksize,
        latency=args.latency,
    )
    sf.write(out_dir / "captured.wav", capture, args.rate, subtype="PCM_24")

    metrics = analyze(
        reference,
        capture,
        args.rate,
        status_events,
        elapsed,
        cpu_metrics,
        expected_offset_frames=len(pre),
    )
    metrics["signal"] = args.signal
    metrics["amplitude"] = args.amplitude
    metrics["input_device"] = input_device
    metrics["input_device_name"] = sd.query_devices(input_device)["name"]
    metrics["output_device"] = output_device
    metrics["output_device_name"] = sd.query_devices(output_device)["name"]
    metrics["out_dir"] = str(out_dir)

    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_metrics(out_dir / "metrics.txt", metrics)
    print((out_dir / "metrics.txt").read_text(encoding="utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
