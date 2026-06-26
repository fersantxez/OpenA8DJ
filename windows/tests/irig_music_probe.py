#!/usr/bin/env python3
"""Play a real music excerpt through Audio 8 DJ and capture it from iRig.

The output directory is compatible with scripts/physical-music-quality-gate:
it contains fixture/reference.wav and captured.wav.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

import numpy as np
import soundfile as sf

THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[1]
sys.path.insert(0, str(THIS_DIR))

import irig_quality_probe as probe  # noqa: E402


def find_ffmpeg() -> str:
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg:
        return ffmpeg
    candidates = sorted((Path.home() / "Documents" / "Codex" / "tools").rglob("ffmpeg.exe"))
    if candidates:
        return str(candidates[-1])
    raise SystemExit("ffmpeg.exe not found; cannot decode music source")


def convert_music(source: Path, dest: Path, rate: int, start: float, seconds: float) -> None:
    cmd = [
        find_ffmpeg(),
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-ss",
        f"{start:.3f}",
        "-i",
        str(source),
        "-t",
        f"{seconds:.3f}",
        "-ac",
        "2",
        "-ar",
        str(rate),
        "-sample_fmt",
        "s16",
        str(dest),
    ]
    subprocess.run(cmd, check=True)


def load_reference(path: Path, target_peak: float) -> np.ndarray:
    data, rate = sf.read(path, dtype="float32", always_2d=True)
    if rate <= 0:
        raise SystemExit(f"invalid sample rate in {path}")
    data = data[:, :2]
    if data.shape[1] == 1:
        data = np.repeat(data, 2, axis=1)
    peak = float(np.max(np.abs(data))) if len(data) else 0.0
    if peak > 0.0:
        data = data * (target_peak / peak)
    fade = min(len(data) // 2, max(1, int(rate * 0.020)))
    ramp = np.linspace(0.0, 1.0, fade, dtype=np.float32)
    data[:fade, :] *= ramp[:, None]
    data[-fade:, :] *= ramp[::-1, None]
    return data.astype(np.float32)


def run(args: argparse.Namespace) -> int:
    input_device = args.input_device
    output_device = args.output_device
    if input_device is None:
        input_device = probe.find_device("input", args.input_name, args.hostapi)
    if output_device is None:
        output_device = probe.find_device("output", args.output_name, args.hostapi)

    out_dir = args.out_dir
    if out_dir is None:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        out_dir = ROOT / "local-analysis" / f"windows-music-irig-{stamp}"
    out_dir = Path(out_dir).resolve()
    fixture_dir = out_dir / "fixture"
    fixture_dir.mkdir(parents=True, exist_ok=True)
    probe.write_device_list(out_dir / "device-list.txt")

    reference_wav = fixture_dir / "reference.wav"
    convert_music(Path(args.music_file), reference_wav, args.rate, args.start, args.seconds)
    reference = load_reference(reference_wav, args.target_peak)
    sf.write(reference_wav, reference, args.rate, subtype="PCM_16")

    pre = np.zeros((int(round(args.pre_roll * args.rate)), 2), dtype=np.float32)
    post = np.zeros((int(round(args.post_roll * args.rate)), 2), dtype=np.float32)
    playback = np.vstack([pre, reference, post])
    progress_hooks = []
    if args.trace_at is not None:
        ctl_path = Path(args.ctl_path)

        def capture_driver_traces(position: int, total: int) -> None:
            del position, total
            if not ctl_path.exists():
                (out_dir / "midplay-trace-error.txt").write_text(
                    f"missing ctl_path={ctl_path}\n",
                    encoding="utf-8",
                )
                return
            render_path = out_dir / "midplay-render-trace.txt"
            usb_text_path = out_dir / "midplay-usb-playback-trace.txt"
            usb_bin_path = out_dir / "midplay-usb-playback-trace.bin"
            with render_path.open("w", encoding="utf-8") as render_file:
                subprocess.run([str(ctl_path), "render-trace"], stdout=render_file, stderr=subprocess.STDOUT)
            with usb_text_path.open("w", encoding="utf-8") as usb_file:
                subprocess.run(
                    [str(ctl_path), "usb-playback-trace", str(usb_bin_path)],
                    stdout=usb_file,
                    stderr=subprocess.STDOUT,
                )

        progress_hooks.append((int(round(max(0.0, args.trace_at) * args.rate)), "driver-trace", capture_driver_traces))
    capture, status_events, elapsed, cpu_metrics = probe.play_and_record(
        playback,
        input_device,
        output_device,
        args.rate,
        args.blocksize,
        args.latency,
        progress_hooks=progress_hooks,
    )
    captured_wav = out_dir / "captured.wav"
    sf.write(captured_wav, capture, args.rate, subtype="PCM_16")

    metrics = probe.analyze(
        reference,
        capture,
        args.rate,
        status_events,
        elapsed,
        cpu_metrics,
        expected_offset_frames=int(round(args.pre_roll * args.rate)),
    )
    metrics.update(
        {
            "signal": "music",
            "music_file": str(Path(args.music_file).resolve()),
            "music_start_seconds": args.start,
            "music_seconds": args.seconds,
            "target_peak": args.target_peak,
            "input_device": input_device,
            "output_device": output_device,
            "input_device_name": probe.sd.query_devices(input_device)["name"],
            "output_device_name": probe.sd.query_devices(output_device)["name"],
            "out_dir": str(out_dir),
        }
    )
    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    for key, value in metrics.items():
        if isinstance(value, dict):
            for subkey, subvalue in value.items():
                print(f"{key}.{subkey}={subvalue}")
        else:
            print(f"{key}={value}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--music-file", required=True)
    parser.add_argument("--input-device", type=int)
    parser.add_argument("--output-device", type=int)
    parser.add_argument("--input-name", default="iRig Stream")
    parser.add_argument("--output-name", default="Audio 8 DJ")
    parser.add_argument("--hostapi")
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--seconds", type=float, default=20.0)
    parser.add_argument("--start", type=float, default=30.0)
    parser.add_argument("--pre-roll", type=float, default=1.0)
    parser.add_argument("--post-roll", type=float, default=1.0)
    parser.add_argument("--target-peak", type=float, default=0.08)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument("--latency", default="high")
    parser.add_argument("--out-dir", type=Path)
    parser.add_argument("--trace-at", type=float, help="capture driver traces this many seconds after stream start")
    parser.add_argument(
        "--ctl-path",
        default=str(ROOT / "windows" / "dist" / "Release" / "x64" / "opena8djctl.exe"),
    )
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
