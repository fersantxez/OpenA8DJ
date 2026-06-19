#!/usr/bin/env python3
"""Compare a digital music reference with a physical capture.

The script is intentionally dependency-light: it uses Python stdlib for
alignment/residual math and ffmpeg only for band-limited RMS measurements.
"""

from __future__ import annotations

import argparse
import array
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def read_wav_mono(path: Path) -> tuple[int, list[float], float, int]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()
        raw = wav.readframes(frames)
    if sample_width != 2:
        raise SystemExit(f"{path}: expected 16-bit PCM WAV, got {sample_width * 8}-bit")
    samples = struct.unpack("<" + "h" * (len(raw) // 2), raw)
    mono: list[float] = []
    peak = 0.0
    clipped = 0
    for frame in range(frames):
        acc = 0.0
        for ch in range(channels):
            v = samples[frame * channels + ch]
            if abs(v) >= 32760:
                clipped += 1
            f = v / 32768.0
            peak = max(peak, abs(f))
            acc += f
        mono.append(acc / channels)
    return rate, mono, peak, clipped


def rms(values: list[float]) -> float:
    if not values:
        return 0.0
    return math.sqrt(sum(v * v for v in values) / len(values))


def db(value: float) -> float:
    if value <= 1e-15:
        return -300.0
    return 20.0 * math.log10(value)


def envelope(values: list[float], block: int) -> list[float]:
    out: list[float] = []
    for start in range(0, len(values) - block + 1, block):
        s = 0.0
        for v in values[start : start + block]:
            s += v * v
        out.append(math.sqrt(s / block))
    return out


def best_lag(ref: list[float], cap: list[float], rate: int, max_lag_seconds: float) -> int:
    block = max(1, rate // 100)
    r_env = envelope(ref, block)
    c_env = envelope(cap, block)
    max_lag_blocks = int(max_lag_seconds * 100)
    best_score = -1e300
    best = 0
    for lag in range(-max_lag_blocks, max_lag_blocks + 1):
        if lag >= 0:
            r0, c0 = 0, lag
            n = min(len(r_env), len(c_env) - lag)
        else:
            r0, c0 = -lag, 0
            n = min(len(r_env) + lag, len(c_env))
        if n < 200:
            continue
        rs = r_env[r0 : r0 + n]
        cs = c_env[c0 : c0 + n]
        mr = sum(rs) / n
        mc = sum(cs) / n
        num = sum((rs[i] - mr) * (cs[i] - mc) for i in range(n))
        den_r = math.sqrt(sum((v - mr) * (v - mr) for v in rs))
        den_c = math.sqrt(sum((v - mc) * (v - mc) for v in cs))
        score = num / (den_r * den_c + 1e-18)
        if score > best_score:
            best_score = score
            best = lag * block
    return best


def align(ref: list[float], cap: list[float], lag: int) -> tuple[list[float], list[float]]:
    if lag >= 0:
        r = ref[: max(0, min(len(ref), len(cap) - lag))]
        c = cap[lag : lag + len(r)]
    else:
        r = ref[-lag : -lag + min(len(ref) + lag, len(cap))]
        c = cap[: len(r)]
    n = min(len(r), len(c))
    return r[:n], c[:n]


def linear_fit(ref: list[float], cap: list[float]) -> tuple[float, float]:
    n = len(ref)
    if n == 0:
        return 0.0, 0.0
    mean_r = sum(ref) / n
    mean_c = sum(cap) / n
    num = sum((ref[i] - mean_r) * (cap[i] - mean_c) for i in range(n))
    den = sum((ref[i] - mean_r) * (ref[i] - mean_r) for i in range(n))
    gain = num / den if den > 1e-18 else 0.0
    offset = mean_c - gain * mean_r
    return gain, offset


def write_mono_wav(path: Path, rate: int, values: list[float]) -> None:
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        payload = bytearray()
        for v in values:
            v = max(-1.0, min(1.0, v))
            payload += struct.pack("<h", int(round(v * 32767.0)))
        wav.writeframes(bytes(payload))


def ffmpeg_band_rms(path: Path, low: int | None, high: int | None) -> float:
    filters = ["pan=mono|c0=c0"]
    if low:
        filters.append(f"highpass=f={low}")
    if high:
        filters.append(f"lowpass=f={high}")
    filters.append("aformat=sample_fmts=flt")
    cmd = [
        "ffmpeg",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(path),
        "-af",
        ",".join(filters),
        "-f",
        "f32le",
        "-",
    ]
    raw = subprocess.check_output(cmd)
    vals = array.array("f")
    vals.frombytes(raw)
    if sys.byteorder != "little":
        vals.byteswap()
    if not vals:
        return 0.0
    return math.sqrt(sum(float(v) * float(v) for v in vals) / len(vals))


def verdict(metrics: dict[str, float | int | str], baseline: dict[str, float] | None) -> tuple[str, list[str]]:
    reasons: list[str] = []
    if metrics["capture_peak_db"] > -0.5:
        reasons.append("capture too close to clipping")
    if metrics["capture_clipped_samples"] > 0:
        reasons.append("capture clipped")
    if baseline:
        for key, limit in (
            ("snr_db", -1.0),
            ("mid_residual_ratio", 0.25),
            ("high_residual_ratio", 0.25),
        ):
            delta = float(metrics[key]) - float(baseline[key])
            if key == "snr_db":
                if delta < limit:
                    reasons.append(f"{key} regressed by {-delta:.2f} dB vs baseline")
            elif delta > limit:
                reasons.append(f"{key} regressed by {delta:.4f} vs baseline")
        for key, limit_db in (
            ("mid_capture_to_ref_gain_db", 1.5),
            ("high_capture_to_ref_gain_db", 2.0),
        ):
            delta = abs(float(metrics[key]) - float(baseline[key]))
            if delta > limit_db:
                reasons.append(f"{key} shifted by {delta:.2f} dB vs baseline")
    else:
        if metrics["snr_db"] < 14.0:
            reasons.append("full-band residual SNR below 14 dB")
        if metrics["mid_residual_ratio"] > 0.28:
            reasons.append("1-5 kHz residual ratio above 0.28")
        if metrics["high_residual_ratio"] > 0.35:
            reasons.append("5-12 kHz residual ratio above 0.35")
    return ("FAIL" if reasons else "PASS"), reasons


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--max-lag-seconds", type=float, default=6.0)
    parser.add_argument("--baseline-json", type=Path)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    rate_r, ref, ref_peak, _ = read_wav_mono(args.reference)
    rate_c, cap, cap_peak, cap_clipped = read_wav_mono(args.capture)
    if rate_r != rate_c:
        raise SystemExit(f"sample-rate mismatch: {rate_r} vs {rate_c}")

    lag = best_lag(ref, cap, rate_r, args.max_lag_seconds)
    ref_a, cap_a = align(ref, cap, lag)
    if len(ref_a) < rate_r * 5:
        raise SystemExit("aligned overlap too short")

    gain, offset = linear_fit(ref_a, cap_a)
    fitted = [gain * v + offset for v in ref_a]
    residual = [cap_a[i] - fitted[i] for i in range(len(cap_a))]
    signal_rms = rms(fitted)
    residual_rms = rms(residual)

    with tempfile.TemporaryDirectory(prefix="opena8dj-music-") as td:
        signal_wav = Path(td) / "signal.wav"
        residual_wav = Path(td) / "residual.wav"
        write_mono_wav(signal_wav, rate_r, fitted)
        write_mono_wav(residual_wav, rate_r, residual)
        bands = {
            "low": (40, 250),
            "low_mid": (250, 1000),
            "mid": (1000, 5000),
            "high": (5000, 12000),
        }
        band_metrics: dict[str, float] = {}
        ref_wav = Path(td) / "ref.wav"
        cap_wav = Path(td) / "cap.wav"
        write_mono_wav(ref_wav, rate_r, ref_a)
        write_mono_wav(cap_wav, rate_r, cap_a)
        for name, (low, high) in bands.items():
            sig = ffmpeg_band_rms(signal_wav, low, high)
            res = ffmpeg_band_rms(residual_wav, low, high)
            ref_band = ffmpeg_band_rms(ref_wav, low, high)
            cap_band = ffmpeg_band_rms(cap_wav, low, high)
            band_metrics[f"{name}_signal_rms"] = sig
            band_metrics[f"{name}_residual_rms"] = res
            band_metrics[f"{name}_residual_ratio"] = res / (sig + 1e-15)
            band_metrics[f"{name}_reference_rms"] = ref_band
            band_metrics[f"{name}_capture_rms"] = cap_band
            band_metrics[f"{name}_capture_to_ref_gain_db"] = db(cap_band / (ref_band + 1e-15))

    baseline = None
    if args.baseline_json:
        baseline = json.loads(args.baseline_json.read_text())

    metrics: dict[str, float | int | str] = {
        "reference": str(args.reference),
        "capture": str(args.capture),
        "sample_rate": rate_r,
        "aligned_frames": len(ref_a),
        "lag_samples": lag,
        "lag_seconds": lag / rate_r,
        "reference_peak_db": db(ref_peak),
        "capture_peak_db": db(cap_peak),
        "capture_clipped_samples": cap_clipped,
        "fit_gain": gain,
        "fit_gain_db": db(abs(gain)),
        "fit_offset": offset,
        "signal_rms": signal_rms,
        "residual_rms": residual_rms,
        "snr_db": db(signal_rms / (residual_rms + 1e-15)),
    }
    metrics.update(band_metrics)
    status, reasons = verdict(metrics, baseline)
    metrics["verdict"] = status
    metrics["reasons"] = "; ".join(reasons)

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n")

    print(f"verdict={status}")
    for key in (
        "snr_db",
        "mid_residual_ratio",
        "high_residual_ratio",
        "mid_capture_to_ref_gain_db",
        "high_capture_to_ref_gain_db",
        "capture_peak_db",
        "capture_clipped_samples",
        "lag_seconds",
        "fit_gain_db",
    ):
        print(f"{key}={metrics[key]}")
    if reasons:
        for reason in reasons:
            print(f"reason={reason}")
    return 0 if status == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
