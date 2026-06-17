#!/usr/bin/env python3
"""Estimate whether an LTI transfer function explains soundcheck captures.

Offline-only. Uses SciPy/NumPy from the local analysis environment when
available. The script aligns an existing reference/capture pair, estimates
per-channel transfer functions with Welch/CSD, reconstructs a predicted
capture, and reports residual/coherence by band.
"""

import argparse
import json
import math
import wave
from pathlib import Path

import numpy as np
from scipy import signal


def read_wav_pair(path):
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if width == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 3:
        bytes_ = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        vals = (bytes_[:, 0].astype(np.int32) |
                (bytes_[:, 1].astype(np.int32) << 8) |
                (bytes_[:, 2].astype(np.int32) << 16))
        vals = np.where(vals & 0x800000, vals | ~0xffffff, vals)
        data = vals.astype(np.float64) / 8388608.0
    elif width == 4:
        data = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise SystemExit(f"unsupported WAV width for {path}: {width * 8}")
    if channels <= 0:
        raise SystemExit(f"invalid channel count for {path}: {channels}")
    data = data.reshape(-1, channels)
    if channels == 1:
        pair = np.column_stack([data[:, 0], data[:, 0]])
    else:
        pair = data[:, :2]
    return rate, pair


def first_signal_index(pair):
    envelope = np.max(np.abs(pair), axis=1)
    if envelope.size == 0:
        return 0
    peak = float(np.max(envelope))
    threshold = max(0.0005, peak * 0.02)
    hits = np.flatnonzero(envelope >= threshold)
    return int(hits[0]) if hits.size else 0


def find_reference(run_dir):
    direct = run_dir / "fixture/reference.wav"
    if direct.exists():
        return direct
    prepare = run_dir / "prepare.log"
    if prepare.exists():
        for line in prepare.read_text().splitlines():
            if line.startswith("reference="):
                path = Path(line.split("=", 1)[1])
                if path.exists():
                    return path
                candidate = run_dir / path
                if candidate.exists():
                    return candidate
    raise SystemExit(f"cannot find reference WAV for {run_dir}")


def align_pair(ref_pair, got_pair, rate, max_lag):
    ref_start = first_signal_index(ref_pair)
    got_start = first_signal_index(got_pair)
    fit = min(len(ref_pair) - ref_start, len(got_pair) - got_start, int(rate * 1.0))
    if fit <= 0:
        raise SystemExit("not enough signal for alignment")
    ref = np.mean(ref_pair[ref_start:ref_start + fit], axis=1)
    got_region_start = max(0, got_start - max_lag)
    got_region_end = min(len(got_pair), got_start + fit + max_lag)
    got = np.mean(got_pair[got_region_start:got_region_end], axis=1)
    corr = signal.correlate(got, ref, mode="valid", method="fft")
    best = int(np.argmax(np.abs(corr)))
    got_aligned = got_region_start + best
    if got_aligned < 0:
        ref_start += -got_aligned
        got_aligned = 0
    return ref_start, got_aligned, got_aligned - ref_start


def rms(values):
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(values))))


def db(value):
    if value <= 0.0:
        return -240.0
    return 20.0 * math.log10(value)


def band_mask(freqs, low, high):
    return (freqs >= low) & (freqs < high)


def residual_metrics(ref, got, predicted, rate):
    usable = min(ref.size, got.size, predicted.size)
    ref = ref[:usable]
    got = got[:usable]
    predicted = predicted[:usable]
    scalar_gain = float(np.dot(ref, got) / np.dot(ref, ref)) if np.dot(ref, ref) > 0 else 0.0
    scalar_pred = scalar_gain * ref
    scalar_res = got - scalar_pred
    lti_res = got - predicted
    freqs, scalar_psd = signal.welch(scalar_res, fs=rate, nperseg=min(8192, usable))
    _, scalar_sig_psd = signal.welch(scalar_pred, fs=rate, nperseg=min(8192, usable))
    _, lti_psd = signal.welch(lti_res, fs=rate, nperseg=min(8192, usable))
    _, lti_sig_psd = signal.welch(predicted, fs=rate, nperseg=min(8192, usable))

    def band_ratio(res_psd, sig_psd, low, high):
        mask = band_mask(freqs, low, high)
        res = float(np.sqrt(np.mean(res_psd[mask]))) if np.any(mask) else 0.0
        sig = float(np.sqrt(np.mean(sig_psd[mask]))) if np.any(mask) else 0.0
        return res / sig if sig > 1e-18 else 0.0

    scalar_signal = rms(scalar_pred)
    scalar_residual = rms(scalar_res)
    lti_signal = rms(predicted)
    lti_residual = rms(lti_res)
    return {
        "scalar_gain": scalar_gain,
        "scalar_snr_db": db(scalar_signal / scalar_residual) if scalar_residual > 0 else 999.0,
        "lti_snr_db": db(lti_signal / lti_residual) if lti_residual > 0 else 999.0,
        "scalar_residual_rms": scalar_residual,
        "lti_residual_rms": lti_residual,
        "scalar_mid_ratio": band_ratio(scalar_psd, scalar_sig_psd, 1000.0, 5000.0),
        "lti_mid_ratio": band_ratio(lti_psd, lti_sig_psd, 1000.0, 5000.0),
        "scalar_high_ratio": band_ratio(scalar_psd, scalar_sig_psd, 5000.0, 12000.0),
        "lti_high_ratio": band_ratio(lti_psd, lti_sig_psd, 5000.0, 12000.0),
    }


def estimate_lti(ref, got, rate, nperseg, smooth_bins):
    nperseg = min(nperseg, ref.size, got.size)
    freqs, pxy = signal.csd(got, ref, fs=rate, nperseg=nperseg, noverlap=nperseg // 2)
    _, pxx = signal.welch(ref, fs=rate, nperseg=nperseg, noverlap=nperseg // 2)
    _, coh = signal.coherence(ref, got, fs=rate, nperseg=nperseg, noverlap=nperseg // 2)
    transfer = pxy / np.maximum(pxx, 1e-24)
    if smooth_bins > 1:
        kernel = np.ones(smooth_bins, dtype=np.float64) / float(smooth_bins)
        magnitude = np.convolve(np.abs(transfer), kernel, mode="same")
        phase = np.convolve(np.unwrap(np.angle(transfer)), kernel, mode="same")
        transfer = magnitude * np.exp(1j * phase)
    return freqs, transfer, coh


def predict_with_transfer(ref, rate, freqs, transfer):
    n = ref.size
    fft_freqs = np.fft.rfftfreq(n, d=1.0 / rate)
    magnitude = np.interp(fft_freqs, freqs, np.abs(transfer), left=abs(transfer[0]), right=abs(transfer[-1]))
    phase = np.interp(fft_freqs,
                      freqs,
                      np.unwrap(np.angle(transfer)),
                      left=np.angle(transfer[0]),
                      right=np.angle(transfer[-1]))
    shaped = np.fft.rfft(ref) * magnitude * np.exp(1j * phase)
    return np.fft.irfft(shaped, n=n)


def band_average(freqs, values, low, high):
    mask = band_mask(freqs, low, high)
    return float(np.mean(values[mask])) if np.any(mask) else 0.0


def analyze_run(run_dir, max_seconds, max_lag, nperseg, smooth_bins):
    reference = find_reference(run_dir)
    capture = run_dir / "captured.wav"
    rate, ref_pair = read_wav_pair(reference)
    got_rate, got_pair = read_wav_pair(capture)
    if rate != got_rate:
        raise SystemExit(f"rate mismatch for {run_dir}: reference={rate} capture={got_rate}")
    ref_start, got_start, lag = align_pair(ref_pair, got_pair, rate, max_lag)
    usable = min(len(ref_pair) - ref_start, len(got_pair) - got_start, int(max_seconds * rate))
    if usable <= rate:
        raise SystemExit(f"not enough aligned audio for {run_dir}: {usable}")
    ref = ref_pair[ref_start:ref_start + usable]
    got = got_pair[got_start:got_start + usable]
    rows = {}
    for channel, name in enumerate(("left", "right")):
        freqs, transfer, coherence = estimate_lti(ref[:, channel],
                                                  got[:, channel],
                                                  rate,
                                                  nperseg,
                                                  smooth_bins)
        predicted = predict_with_transfer(ref[:, channel], rate, freqs, transfer)
        metrics = residual_metrics(ref[:, channel], got[:, channel], predicted, rate)
        metrics.update({
            "coherence_low_mean": band_average(freqs, coherence, 40.0, 1000.0),
            "coherence_mid_mean": band_average(freqs, coherence, 1000.0, 5000.0),
            "coherence_high_mean": band_average(freqs, coherence, 5000.0, 12000.0),
            "transfer_mag_low_db": db(band_average(freqs, np.abs(transfer), 40.0, 1000.0)),
            "transfer_mag_mid_db": db(band_average(freqs, np.abs(transfer), 1000.0, 5000.0)),
            "transfer_mag_high_db": db(band_average(freqs, np.abs(transfer), 5000.0, 12000.0)),
            "lti_snr_delta_db": metrics["lti_snr_db"] - metrics["scalar_snr_db"],
            "lti_mid_ratio_delta": metrics["scalar_mid_ratio"] - metrics["lti_mid_ratio"],
            "lti_high_ratio_delta": metrics["scalar_high_ratio"] - metrics["lti_high_ratio"],
        })
        rows[name] = metrics
    return {
        "run_dir": str(run_dir),
        "reference": str(reference),
        "capture": str(capture),
        "rate": rate,
        "alignment_lag": lag,
        "compared_frames": usable,
        "compared_seconds": usable / rate,
        "left": rows["left"],
        "right": rows["right"],
        "min_lti_snr_delta_db": min(rows["left"]["lti_snr_delta_db"], rows["right"]["lti_snr_delta_db"]),
        "max_lti_mid_ratio_delta": max(rows["left"]["lti_mid_ratio_delta"], rows["right"]["lti_mid_ratio_delta"]),
        "min_mid_coherence": min(rows["left"]["coherence_mid_mean"], rows["right"]["coherence_mid_mean"]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--max-seconds", type=float, default=16.0)
    parser.add_argument("--max-lag", type=int, default=8192)
    parser.add_argument("--nperseg", type=int, default=16384)
    parser.add_argument("--smooth-bins", type=int, default=9)
    parser.add_argument("soundcheck_dirs", nargs="+")
    args = parser.parse_args()
    rows = [analyze_run(Path(path), args.max_seconds, args.max_lag, args.nperseg, args.smooth_bins)
            for path in args.soundcheck_dirs]
    result = {
        "schema": "opena8djcpp.lti-transfer-quality.v1",
        "result": "PASS_DIAGNOSTIC",
        "nperseg": args.nperseg,
        "smooth_bins": args.smooth_bins,
        "rows": rows,
    }
    out = Path(args.json_out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
