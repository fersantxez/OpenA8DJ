#!/usr/bin/env python3
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import wave
from array import array
from pathlib import Path


SUPPORTED_EXTENSIONS = {".wav", ".aif", ".aiff", ".mp3", ".m4a", ".aac", ".flac"}
SCAN_STRIDE_FRAMES = 64


def find_music_file(paths, max_files):
    candidates = []
    for root in paths:
        if not root:
            continue
        root_path = Path(root).expanduser()
        if root_path.is_file() and root_path.suffix.lower() in SUPPORTED_EXTENSIONS:
            return root_path
        if not root_path.is_dir():
            continue
        for path in root_path.rglob("*"):
            if path.is_file() and path.suffix.lower() in SUPPORTED_EXTENSIONS:
                candidates.append(path)
                if len(candidates) >= max_files:
                    break
        if candidates:
            break
    if not candidates:
        raise SystemExit("no supported local music files found")
    return sorted(candidates, key=lambda p: str(p).lower())[0]


def run_checked(command):
    try:
        subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(stderr or f"command failed: {' '.join(command)}") from exc


def convert_to_wav(source, dest, rate, duration=None):
    afconvert = shutil.which("afconvert")
    ffmpeg = shutil.which("ffmpeg")
    errors = []
    if duration is not None and ffmpeg is not None:
        command = [
            ffmpeg,
            "-y",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-t",
            f"{duration:.3f}",
            "-ac",
            "2",
            "-ar",
            str(rate),
            "-sample_fmt",
            "s16",
            str(dest),
        ]
        try:
            run_checked(command)
            return "ffmpeg-partial"
        except RuntimeError as exc:
            errors.append(f"ffmpeg-partial: {exc}")
    if afconvert is not None:
        command = [
            afconvert,
            "-f",
            "WAVE",
            "-d",
            f"LEI16@{rate}",
            "-c",
            "2",
            str(source),
            str(dest),
        ]
        try:
            run_checked(command)
            return "afconvert"
        except RuntimeError as exc:
            errors.append(f"afconvert: {exc}")
    if ffmpeg is not None:
        command = [
            ffmpeg,
            "-y",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-ac",
            "2",
            "-ar",
            str(rate),
            "-sample_fmt",
            "s16",
            str(dest),
        ]
        try:
            run_checked(command)
            return "ffmpeg"
        except RuntimeError as exc:
            errors.append(f"ffmpeg: {exc}")
    raise SystemExit("could not convert source audio: " + " | ".join(errors))


def read_wav(path):
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if channels != 2 or width != 2:
        raise SystemExit(f"expected converted stereo PCM16 WAV, got channels={channels} width={width}")
    samples = array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    return rate, samples, len(samples) // 2


def write_excerpt_wav(path, rate, samples, start, length, gain):
    fade_frames = min(length // 4, max(1, int(rate * 0.01)))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        frames = bytearray()
        for frame in range(length):
            fade = 1.0
            if frame < fade_frames:
                fade = frame / fade_frames
            elif frame >= length - fade_frames:
                fade = (length - 1 - frame) / fade_frames
            scale = gain * max(0.0, min(1.0, fade))
            index = (start + frame) * 2
            left = max(-1.0, min(1.0, samples[index] / 32768.0 * scale))
            right = max(-1.0, min(1.0, samples[index + 1] / 32768.0 * scale))
            frames.extend(int(round(left * 32767.0)).to_bytes(2, "little", signed=True))
            frames.extend(int(round(right * 32767.0)).to_bytes(2, "little", signed=True))
        wav.writeframes(bytes(frames))


def window_stats(samples, start, length, stride=1):
    total = 0.0
    high_total = 0.0
    window_peak = 0.0
    prev = None
    count = 0
    for frame in range(start, start + length, max(1, stride)):
        index = frame * 2
        left = samples[index] / 32768.0
        right = samples[index + 1] / 32768.0
        mono = 0.5 * (left + right)
        total += mono * mono
        window_peak = max(window_peak, abs(left), abs(right))
        if prev is not None:
            diff = mono - prev
            high_total += diff * diff
        prev = mono
        count += 1
    window_rms = math.sqrt(total / count) if count > 0 else 0.0
    high = math.sqrt(high_total / max(1, count - 1)) if count > 1 else 0.0
    return window_rms, window_peak, high


def score_window(samples, start, length, mode):
    window_rms, window_peak, high = window_stats(samples, start, length, SCAN_STRIDE_FRAMES)
    if mode == "dense":
        return window_rms
    if mode == "transient":
        if window_rms < 0.002:
            return 0.0
        return window_peak / window_rms
    if mode == "wideband":
        if window_rms < 0.002:
            return 0.0
        return high
    if mode == "start":
        return -start
    raise ValueError(mode)


def choose_start(samples, frames, rate, seconds, mode):
    length = int(rate * seconds)
    if frames < length:
        raise SystemExit(f"source is too short: {frames / rate:.2f}s < {seconds:.2f}s")
    if mode == "start":
        return 0
    hop = max(1, int(rate * 0.5), length // 2)
    best_start = 0
    best_score = None
    last_start = frames - length
    starts = list(range(0, last_start + 1, hop))
    if starts[-1] != last_start:
        starts.append(last_start)
    for start in starts:
        current = score_window(samples, start, length, mode)
        if best_score is None or current > best_score:
            best_score = current
            best_start = start
    return best_start


def excerpt_peak(samples, start, length):
    current = 0.0
    for frame in range(start, start + length):
        index = frame * 2
        current = max(current, abs(samples[index] / 32768.0), abs(samples[index + 1] / 32768.0))
    return current


def excerpt_rms(samples, start, length, gain):
    total = 0.0
    for frame in range(start, start + length):
        index = frame * 2
        left = samples[index] / 32768.0 * gain
        right = samples[index + 1] / 32768.0 * gain
        mono = 0.5 * (left + right)
        total += mono * mono
    return math.sqrt(total / length) if length > 0 else 0.0


def normalization_gain(samples, start, length, target_peak_db):
    source_peak = excerpt_peak(samples, start, length)
    target_peak = 10.0 ** (target_peak_db / 20.0)
    gain = target_peak / source_peak if source_peak > 0.0 else 1.0
    return gain, source_peak, target_peak


def main():
    parser = argparse.ArgumentParser(description="Prepare a normalized real-music WAV fixture for OpenA8DJ soundchecks.")
    parser.add_argument("--music-file")
    parser.add_argument("--music-dir", action="append")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--seconds", type=float, default=20.0)
    parser.add_argument("--mode", choices=("dense", "transient", "wideband", "start"), default="dense")
    parser.add_argument("--target-peak-db", type=float, default=-12.0)
    parser.add_argument("--max-files", type=int, default=128)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    source = Path(args.music_file).expanduser() if args.music_file else find_music_file(
        args.music_dir or [Path.home() / "Music", Path.home() / "Downloads"],
        args.max_files,
    )
    if not source.exists():
        raise SystemExit(f"music file not found: {source}")

    converted = out_dir / "converted-source.wav"
    reference = out_dir / "reference.wav"
    partial_duration = args.seconds if args.mode == "start" else None
    backend = convert_to_wav(source, converted, args.rate, partial_duration)
    rate, samples, frames = read_wav(converted)
    start = choose_start(samples, frames, rate, args.seconds, args.mode)
    length = int(rate * args.seconds)
    gain, source_peak, target_peak = normalization_gain(samples, start, length, args.target_peak_db)
    write_excerpt_wav(reference, rate, samples, start, length, gain)

    metadata = {
        "source_path": str(source),
        "source_name": source.name,
        "conversion_backend": backend,
        "reference_wav": str(reference),
        "mode": args.mode,
        "rate": rate,
        "seconds": args.seconds,
        "start_frame": start,
        "start_seconds": start / rate,
        "frames": length,
        "source_peak": source_peak,
        "target_peak": target_peak,
        "gain": gain,
        "rms": excerpt_rms(samples, start, length, gain),
        "peak": min(1.0, source_peak * gain),
    }
    metadata_path = out_dir / "source.json"
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"reference={reference}")
    print(f"source={source}")
    print(f"mode={args.mode}")
    print(f"rate={rate}")
    print(f"seconds={args.seconds:.3f}")
    print(f"start_seconds={metadata['start_seconds']:.3f}")
    print(f"peak={metadata['peak']:.8f}")
    print(f"rms={metadata['rms']:.8f}")


if __name__ == "__main__":
    main()
