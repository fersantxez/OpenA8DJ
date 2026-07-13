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
import shutil
import subprocess
import time
from datetime import datetime
from pathlib import Path

import numpy as np
import sounddevice as sd
import soundfile as sf

import irig_quality_probe as quality_probe

try:
    import psutil
except ImportError:  # pragma: no cover
    psutil = None


ROOT = Path(__file__).resolve().parents[2]


def find_device(kind: str, name_text: str, hostapi_text: str | None) -> int:
    name_needle = name_text.lower()
    hostapi_needle = hostapi_text.lower() if hostapi_text else None
    matches: list[int] = []
    for index, device in enumerate(sd.query_devices()):
        hostapi = sd.query_hostapis(device["hostapi"])["name"]
        if hostapi_needle and hostapi_needle not in hostapi.lower():
            continue
        if name_needle != device["name"].lower():
            continue
        if kind == "input" and int(device["max_input_channels"]) <= 0:
            continue
        if kind == "output" and int(device["max_output_channels"]) <= 0:
            continue
        matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"expected exactly one {kind} device matching name={name_text!r} "
            f"hostapi={hostapi_text!r}; found {matches}"
        )
    return matches[0]


def find_ffmpeg() -> str:
    command = shutil.which("ffmpeg")
    if command:
        return command
    tool_root = Path.home() / "Documents" / "Codex" / "tools"
    candidates = sorted(tool_root.rglob("ffmpeg.exe")) if tool_root.exists() else []
    if not candidates:
        raise RuntimeError("ffmpeg.exe is required for band-limited reference resampling")
    return str(candidates[-1])


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


def make_signal(
    rate: int,
    seconds: float,
    frequency: float,
    amplitude: float,
    signal: str,
    output_channels: int,
    output_pair: int,
    reference_file: str,
) -> np.ndarray:
    frames = int(round(rate * seconds))
    if reference_file:
        source, source_rate = sf.read(reference_file, dtype="float32", always_2d=True)
        if source.shape[1] == 1:
            source = np.repeat(source, 2, axis=1)
        source = source[:, :2]
        if source_rate != rate:
            target_frames = int(round(len(source) * rate / source_rate))
            source_positions = np.arange(len(source), dtype=np.float64)
            target_positions = np.arange(target_frames, dtype=np.float64) * source_rate / rate
            source = np.column_stack(
                [np.interp(target_positions, source_positions, source[:, channel]) for channel in range(2)]
            ).astype(np.float32)
        pair_signal = np.zeros((frames, 2), dtype=np.float32)
        copied = min(frames, len(source))
        pair_signal[:copied, :] = source[:copied, :]
        peak = float(np.max(np.abs(pair_signal))) if copied else 0.0
        if peak > 0.0:
            pair_signal *= amplitude / peak
    else:
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
        mono_signal = (amplitude * mono * envelope).astype(np.float32)
        pair_signal = np.column_stack([mono_signal, mono_signal])
    if output_channels <= 0 or output_channels % 2:
        raise ValueError("output_channels must be a positive even number")
    pair_count = output_channels // 2
    if output_pair < 1 or output_pair > pair_count:
        raise ValueError(f"output_pair must be between 1 and {pair_count}")
    playback = np.zeros((frames, output_channels), dtype=np.float32)
    first_channel = (output_pair - 1) * 2
    playback[:, first_channel : first_channel + 2] = pair_signal
    return playback


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


def tone_fidelity_metrics(capture: np.ndarray, rate: int, expected_frequency: float) -> dict[str, float | int]:
    mono = capture.mean(axis=1).astype(np.float64)
    window_frames = 1024
    usable = (len(mono) // window_frames) * window_frames
    if usable < rate:
        return {}
    window_rms = np.sqrt(np.mean(np.square(mono[:usable].reshape(-1, window_frames)), axis=1))
    active_threshold = max(0.005, float(np.percentile(window_rms, 95.0)) * 0.45)
    active = np.flatnonzero(window_rms >= active_threshold)
    if not len(active):
        return {}
    active_first = int(active[0] * window_frames)
    active_last = int(min(len(mono), (active[-1] + 1) * window_frames))
    segment_frames = min(rate, max(window_frames, active_last - active_first))
    segment_start = max(active_first, (active_first + active_last - segment_frames) // 2)
    segment = mono[segment_start : segment_start + segment_frames]
    segment -= float(np.mean(segment))
    if len(segment) < window_frames:
        return {}

    fft_size = 1 << (max(len(segment) * 8, 2) - 1).bit_length()
    tapered = segment * np.hanning(len(segment))
    spectrum = np.abs(np.fft.rfft(tapered, fft_size))
    freqs = np.fft.rfftfreq(fft_size, 1.0 / rate)
    search = np.flatnonzero((freqs >= expected_frequency - 10.0) & (freqs <= expected_frequency + 10.0))
    peak_index = int(search[int(np.argmax(spectrum[search]))])
    frequency = float(freqs[peak_index])
    if 0 < peak_index < len(spectrum) - 1:
        left = float(spectrum[peak_index - 1])
        center = float(spectrum[peak_index])
        right = float(spectrum[peak_index + 1])
        denominator = left - 2.0 * center + right
        if denominator != 0.0:
            frequency += 0.5 * (left - right) / denominator * (rate / fft_size)

    t = np.arange(len(segment), dtype=np.float64) / rate
    basis = np.column_stack(
        [np.sin(2.0 * np.pi * frequency * t), np.cos(2.0 * np.pi * frequency * t), np.ones(len(t))]
    )
    coefficients, _, _, _ = np.linalg.lstsq(basis, segment, rcond=None)
    fitted = basis @ coefficients
    residual = segment - fitted
    fitted_rms = float(np.sqrt(np.mean(np.square(fitted))))
    residual_rms = float(np.sqrt(np.mean(np.square(residual))))
    snr_db = 20.0 * math.log10(max(fitted_rms, 1e-15) / max(residual_rms, 1e-15))
    fundamental_amplitude = float(math.hypot(coefficients[0], coefficients[1]))
    harmonic_power = 0.0
    harmonic_count = 0
    for harmonic in range(2, 11):
        harmonic_frequency = frequency * harmonic
        if harmonic_frequency >= rate / 2.0:
            break
        harmonic_basis = np.column_stack(
            [
                np.sin(2.0 * np.pi * harmonic_frequency * t),
                np.cos(2.0 * np.pi * harmonic_frequency * t),
            ]
        )
        harmonic_coefficients, _, _, _ = np.linalg.lstsq(harmonic_basis, segment, rcond=None)
        harmonic_amplitude = float(math.hypot(harmonic_coefficients[0], harmonic_coefficients[1]))
        harmonic_power += harmonic_amplitude * harmonic_amplitude
        harmonic_count += 1
    thd_ratio = math.sqrt(harmonic_power) / max(fundamental_amplitude, 1e-15)
    thd_db = 20.0 * math.log10(max(thd_ratio, 1e-15))
    return {
        "tone_segment_start_frame": segment_start,
        "tone_segment_frames": len(segment),
        "tone_frequency_hz": frequency,
        "tone_frequency_error_hz": frequency - expected_frequency,
        "tone_fit_snr_db": snr_db,
        "tone_thd_ratio": thd_ratio,
        "tone_thd_db": thd_db,
        "tone_harmonics_measured": harmonic_count,
    }


def run_probe(
    args: argparse.Namespace,
) -> tuple[
    np.ndarray,
    np.ndarray,
    dict[str, float | int | str],
    list[tuple[float, float]],
]:
    input_device = args.input_device
    if input_device is None:
        input_device = find_device("input", args.input_name, args.input_hostapi)
    output_device = -1
    if not args.external_command:
        output_device = args.output_device
        if output_device is None:
            output_device = find_device("output", args.output_name, args.output_hostapi)
    playback = make_signal(
        args.output_rate,
        args.seconds,
        args.frequency,
        args.amplitude,
        args.signal,
        args.output_channels,
        args.output_pair,
        args.reference_file,
    )
    capture_frames = int(
        round(
            (args.seconds + args.capture_extra_seconds + args.output_drain_seconds)
            * args.input_rate
        )
    )
    capture = np.zeros((capture_frames, 2), dtype=np.float32)
    playback_pos = 0
    capture_pos = 0
    playback_done = False
    watchdog_expired = False
    status_events: list[str] = []
    cpu_samples: list[float] = []
    cpu_sample_trace: list[tuple[float, float]] = []
    if psutil is not None:
        psutil.cpu_percent(interval=None)
    last_output_callback = time.perf_counter()
    last_input_callback = time.perf_counter()
    first_output_callback_elapsed: float | None = None

    def output_callback(outdata, frames, time_info, status):  # noqa: ARG001
        nonlocal playback_pos, playback_done, last_output_callback, first_output_callback_elapsed
        callback_time = time.perf_counter()
        if first_output_callback_elapsed is None:
            first_output_callback_elapsed = callback_time - started
        if status:
            status_events.append(f"output:{status}")
        outdata.fill(0)
        start = playback_pos
        end = min(start + frames, len(playback))
        if end > start:
            outdata[: end - start, :] = playback[start:end, :]
        playback_pos += frames
        last_output_callback = callback_time
        if playback_pos >= len(playback):
            playback_done = True

    def input_callback(indata, frames, time_info, status):  # noqa: ARG001
        nonlocal capture_pos, last_input_callback
        if status:
            status_events.append(f"input:{status}")
        start = capture_pos
        end = min(start + frames, len(capture))
        if end > start:
            capture[start:end, :] = indata[: end - start, :]
        capture_pos += frames
        last_input_callback = time.perf_counter()

    started = time.perf_counter()
    with sd.InputStream(
        device=input_device,
        channels=2,
        samplerate=args.input_rate,
        blocksize=args.input_blocksize,
        latency=args.input_latency,
        dtype="float32",
        callback=input_callback,
        extra_settings=(sd.WasapiSettings(exclusive=True) if args.input_exclusive else None),
    ):
        time.sleep(args.capture_lead_seconds)
        if args.external_command:
            try:
                completed = subprocess.run(
                    args.external_command,
                    shell=True,
                    check=False,
                    timeout=args.seconds + args.output_drain_seconds + 10.0,
                )
            except subprocess.TimeoutExpired:
                watchdog_expired = True
                status_events.append("external_command_timeout")
                completed = None
            playback_done = True
            if completed is not None and completed.returncode != 0:
                status_events.append(f"external_exit:{completed.returncode}")
        else:
            output_extra_settings = (
                sd.WasapiSettings(exclusive=True) if args.output_exclusive else None
            )
            with sd.OutputStream(
                device=output_device,
                channels=args.output_channels,
                samplerate=args.output_rate,
                blocksize=args.output_blocksize,
                latency=args.output_latency,
                dtype="float32",
                callback=output_callback,
                extra_settings=output_extra_settings,
            ):
                global_deadline = (
                    time.perf_counter()
                    + args.seconds
                    + args.output_drain_seconds
                    + args.callback_watchdog_seconds
                    + 10.0
                )
                while not playback_done:
                    now = time.perf_counter()
                    if (
                        now >= global_deadline
                        or now - last_output_callback > args.callback_watchdog_seconds
                        or now - last_input_callback > args.callback_watchdog_seconds
                    ):
                        watchdog_expired = True
                        status_events.append("callback_watchdog_expired_during_playback")
                        break
                    if psutil is not None:
                        cpu_percent = float(psutil.cpu_percent(interval=None))
                        cpu_samples.append(cpu_percent)
                        cpu_sample_trace.append(
                            (time.perf_counter() - started, cpu_percent)
                        )
                    time.sleep(0.050)
                # PortAudio has accepted the last source frame, but high-latency
                # shared-mode buffers can still hold seconds of queued audio.
                # Keep the stream alive and feed zeroes until that tail reaches
                # the physical DAC instead of truncating the capture.
                drain_deadline = time.perf_counter() + args.output_drain_seconds
                while not watchdog_expired and time.perf_counter() < drain_deadline:
                    now = time.perf_counter()
                    if (
                        now >= global_deadline
                        or now - last_output_callback > args.callback_watchdog_seconds
                        or now - last_input_callback > args.callback_watchdog_seconds
                    ):
                        watchdog_expired = True
                        status_events.append("callback_watchdog_expired_during_drain")
                        break
                    if psutil is not None:
                        cpu_percent = float(psutil.cpu_percent(interval=None))
                        cpu_samples.append(cpu_percent)
                        cpu_sample_trace.append(
                            (time.perf_counter() - started, cpu_percent)
                        )
                    time.sleep(0.050)
        time.sleep(max(0.0, args.capture_extra_seconds - args.capture_lead_seconds))
    elapsed = time.perf_counter() - started

    metrics: dict[str, float | int | str] = {
        "output_device": int(output_device),
        "input_device": int(input_device),
        "output_device_name": (
            str(sd.query_devices(output_device)["name"])
            if output_device >= 0 else "external-command"
        ),
        "input_device_name": str(sd.query_devices(input_device)["name"]),
        "output_rate": int(args.output_rate),
        "input_rate": int(args.input_rate),
        "frequency_hz": float(args.frequency),
        "signal": str(args.signal),
        "reference_file": str(args.reference_file),
        "amplitude": float(args.amplitude),
        "output_channels": int(args.output_channels),
        "output_pair": int(args.output_pair),
        "output_exclusive": bool(args.output_exclusive),
        "input_exclusive": bool(args.input_exclusive),
        "elapsed_seconds": float(elapsed),
        "status_event_count": int(len(status_events)),
        "status_events": "; ".join(status_events[:12]),
        "playback_frames_written": int(min(playback_pos, len(playback))),
        "capture_frames_recorded": int(min(capture_pos, len(capture))),
        "playback_complete": bool(args.external_command or playback_pos >= len(playback)),
        "capture_complete": bool(capture_pos >= max(0, len(capture) - 2 * args.input_blocksize)),
        "callback_watchdog_expired": bool(watchdog_expired),
        "cpu_monitor": "psutil" if psutil is not None else "unavailable",
        "first_output_callback_elapsed_seconds": (
            float(first_output_callback_elapsed)
            if first_output_callback_elapsed is not None
            else -1.0
        ),
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
    return playback, capture, metrics, cpu_sample_trace, first_output_callback_elapsed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=3.0)
    parser.add_argument("--frequency", type=float, default=1000.0)
    parser.add_argument("--amplitude", type=float, default=0.05)
    parser.add_argument("--signal", choices=["tone", "multi"], default="tone")
    parser.add_argument("--output-rate", type=int, default=48000)
    parser.add_argument("--output-channels", type=int, default=2)
    parser.add_argument("--output-pair", type=int, default=1)
    parser.add_argument("--input-rate", type=int, default=44100)
    parser.add_argument("--output-blocksize", type=int, default=480)
    parser.add_argument("--input-blocksize", type=int, default=2048)
    parser.add_argument("--output-latency", default="high")
    parser.add_argument("--input-latency", default="high")
    parser.add_argument("--capture-lead-seconds", type=float, default=0.5)
    parser.add_argument("--capture-extra-seconds", type=float, default=1.0)
    parser.add_argument("--output-drain-seconds", type=float, default=2.0)
    parser.add_argument("--callback-watchdog-seconds", type=float, default=5.0)
    parser.add_argument("--output-name", default="Speakers (Audio 8 DJ)")
    parser.add_argument("--input-name", default="Line In (iRig Stream)")
    parser.add_argument("--output-device", type=int)
    parser.add_argument("--input-device", type=int)
    parser.add_argument("--output-hostapi", default="Windows WASAPI")
    parser.add_argument("--input-hostapi", default="Windows DirectSound")
    parser.add_argument("--out-dir")
    parser.add_argument("--external-command", default="")
    parser.add_argument("--output-exclusive", action="store_true")
    parser.add_argument("--input-exclusive", action="store_true")
    parser.add_argument("--reference-file", default="")
    args = parser.parse_args()

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else ROOT / "local-analysis" / f"windows-irig-wasapi-tone-{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    write_devices(out_dir / "devices.txt")

    playback, capture, metrics, cpu_sample_trace, first_output_callback_elapsed = run_probe(args)
    metrics.update(spectral_metrics(capture, args.input_rate, [args.frequency, 440.0, 833.0, 836.0, 1000.0, 1200.0, 1333.0, 3190.0]))
    if args.signal == "tone" and args.amplitude > 0.0 and not args.reference_file:
        metrics.update(tone_fidelity_metrics(capture, args.input_rate, args.frequency))
    if args.amplitude > 0.0 and not args.external_command:
        first_channel = (args.output_pair - 1) * 2
        reference = playback[:, first_channel : first_channel + 2].astype(np.float64)
        resampled_frames = int(round(len(reference) * args.input_rate / args.output_rate))
        source_positions = np.arange(len(reference), dtype=np.float64)
        target_positions = np.arange(resampled_frames, dtype=np.float64) * args.output_rate / args.input_rate
        resampled = np.column_stack(
            [
                np.interp(target_positions, source_positions, reference[:, channel])
                for channel in range(2)
            ]
        ).astype(np.float32)
        aligned = quality_probe.analyze(
            resampled,
            capture,
            args.input_rate,
            [],
            float(metrics["elapsed_seconds"]),
            {},
            expected_offset_frames=int(
                round(
                    (first_output_callback_elapsed or args.capture_lead_seconds)
                    * args.input_rate
                )
            ),
        )
        for key in (
            "alignment_offset_frames",
            "alignment_score_mono",
            "min_correlation",
            "min_snr_db",
            "click_outliers_total",
            "dropout_windows_total",
        ):
            metrics[f"aligned_{key}"] = aligned[key]
        for channel in aligned["channels"]:
            prefix = f"aligned_channel_{channel['channel']}"
            for key in ("gain", "correlation", "snr_db", "residual_rms", "click_outliers"):
                metrics[f"{prefix}_{key}"] = channel[key]
    metrics["out_dir"] = str(out_dir)

    sf.write(out_dir / "playback.wav", playback, args.output_rate, subtype="PCM_24")
    sf.write(out_dir / "captured.wav", capture, args.input_rate, subtype="PCM_24")
    fixture_dir = out_dir / "fixture"
    fixture_dir.mkdir(parents=True, exist_ok=True)
    first_channel = (args.output_pair - 1) * 2
    output_rate_reference = fixture_dir / "reference-output-rate.wav"
    sf.write(
        output_rate_reference,
        playback[:, first_channel : first_channel + 2],
        args.output_rate,
        subtype="PCM_24",
    )
    reference_path = fixture_dir / "reference.wav"
    if args.output_rate == args.input_rate:
        shutil.copyfile(output_rate_reference, reference_path)
        metrics["reference_resampler"] = "none"
    else:
        subprocess.run(
            [
                find_ffmpeg(),
                "-y",
                "-hide_banner",
                "-loglevel",
                "error",
                "-i",
                str(output_rate_reference),
                "-ar",
                str(args.input_rate),
                "-ac",
                "2",
                "-c:a",
                "pcm_s24le",
                str(reference_path),
            ],
            check=True,
            timeout=max(30.0, args.seconds * 4.0),
        )
        metrics["reference_resampler"] = "ffmpeg-band-limited"
    with (out_dir / "cpu-profile.tsv").open("w", encoding="utf-8", newline="") as cpu_file:
        cpu_file.write("elapsed_seconds\tsystem_cpu\n")
        for elapsed_seconds, cpu_percent in cpu_sample_trace:
            playback_elapsed = elapsed_seconds - (
                first_output_callback_elapsed
                if first_output_callback_elapsed is not None
                else args.capture_lead_seconds
            )
            if playback_elapsed >= 0.0:
                cpu_file.write(f"{playback_elapsed:.6f}\t{cpu_percent:.3f}\n")
    (out_dir / "route-proof.json").write_text(
        json.dumps(
            {
                "schema_version": 1,
                "platform": "windows",
                "capture_device": metrics["input_device_name"],
                "render_device": metrics["output_device_name"],
                "capture_hostapi": sd.query_hostapis(sd.query_devices(int(metrics["input_device"]))["hostapi"])["name"],
                "render_hostapi": (
                    sd.query_hostapis(sd.query_devices(int(metrics["output_device"]))["hostapi"])["name"]
                    if int(metrics["output_device"]) >= 0 else "external-command"
                ),
                "capture_device_index": metrics["input_device"],
                "render_device_index": metrics["output_device"],
                "capture_is_irig_stream": str(metrics["input_device_name"]).casefold()
                == args.input_name.casefold(),
                "render_is_audio_8_dj": str(metrics["output_device_name"]).casefold()
                == args.output_name.casefold(),
                "capture_name_exact": str(metrics["input_device_name"]).casefold()
                == "Line In (iRig Stream)".casefold(),
                "render_name_exact": str(metrics["output_device_name"]).casefold()
                == "Audio 8 DJ (8ch Out) (Audio 8 DJ)".casefold(),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (out_dir / "metrics.txt").write_text(
        "\n".join(f"{key}={value}" for key, value in sorted(metrics.items())) + "\n",
        encoding="utf-8",
    )

    for key in sorted(metrics):
        print(f"{key}={metrics[key]}")
    hard_failure = (
        bool(metrics["callback_watchdog_expired"])
        or int(metrics["status_event_count"]) != 0
        or not bool(metrics["playback_complete"])
        or not bool(metrics["capture_complete"])
    )
    return 2 if hard_failure else 0


if __name__ == "__main__":
    raise SystemExit(main())
