#!/usr/bin/env python3
"""Compare a PCM WAV reference with an interleaved f32 loopback capture."""

import argparse
import math
import sys
import wave
from array import array
from statistics import median


DEFAULT_SAMPLE_RATE = 48000
DEFAULT_CHANNELS = 8
DEFAULT_PAIR = 0
DEFAULT_MIN_SNR_DB = 45.0
DEFAULT_MIN_CORRELATION = 0.98
DEFAULT_MAX_CLICKS = 0
DEFAULT_MAX_RIPPLE_DB = 12.0
MAX_LAG_SECONDS = 0.100
ALIGN_SECONDS = 2.0
SPECTRAL_SECONDS = 2.0
EPSILON = 1e-20


def die(message, code=2):
    print(f"error={sanitize_value(message)}")
    raise SystemExit(code)


def sanitize_value(value):
    return str(value).replace(" ", "_").replace(",", ";")


def read_reference_wav(path):
    try:
        wav = wave.open(path, "rb")
    except wave.Error as exc:
        die(f"invalid_wav:{exc}")
    except OSError as exc:
        die(f"cannot_open_reference:{exc}")

    with wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()
        comptype = wav.getcomptype()
        raw = wav.readframes(frames)

    if comptype != "NONE":
        die(f"unsupported_wav_compression:{comptype}")
    if channels not in (1, 2):
        die(f"unsupported_reference_channels:{channels}")
    if width != 2:
        die(f"unsupported_reference_bits:{width * 8}")

    out = []
    scale = 32768.0
    step = channels * width
    for offset in range(0, len(raw), step):
        left = int.from_bytes(raw[offset:offset + width], "little", signed=True) / scale
        if channels == 2:
            right_offset = offset + width
            right = int.from_bytes(raw[right_offset:right_offset + width], "little", signed=True) / scale
        else:
            right = left
        out.append((left, right))
    return rate, channels, out


def read_capture_f32(path, channels, pair):
    if channels <= 0:
        die(f"invalid_channels:{channels}")
    left_channel = pair * 2
    right_channel = left_channel + 1
    if pair < 0 or right_channel >= channels:
        die(f"invalid_pair:{pair}_for_channels:{channels}")

    try:
        with open(path, "rb") as file:
            data = file.read()
    except OSError as exc:
        die(f"cannot_open_capture:{exc}")

    trailing_bytes = len(data) % 4
    usable_bytes = len(data) - trailing_bytes
    floats = array("f")
    floats.frombytes(data[:usable_bytes])
    if sys.byteorder != "little":
        floats.byteswap()

    trailing_floats = len(floats) % channels
    frame_count = len(floats) // channels
    out = []
    nonfinite = 0
    for frame in range(frame_count):
        offset = frame * channels
        left = float(floats[offset + left_channel])
        right = float(floats[offset + right_channel])
        if not math.isfinite(left):
            nonfinite += 1
            left = 0.0
        if not math.isfinite(right):
            nonfinite += 1
            right = 0.0
        out.append((left, right))

    return {
        "pair": out,
        "bytes": len(data),
        "frames": frame_count,
        "trailing_bytes": trailing_bytes,
        "trailing_floats": trailing_floats,
        "nonfinite": nonfinite,
    }


def pair_to_mono(pair):
    return [0.5 * (left + right) for left, right in pair]


def first_signal_index(samples):
    if not samples:
        return 0
    peak = max(abs(value) for value in samples)
    if peak <= 0.0:
        return 0
    threshold = max(1e-5, peak * 0.01)
    for index, value in enumerate(samples):
        if abs(value) >= threshold:
            return index
    return 0


def normalized_dot(ref, got, ref_start, got_start, count, stride):
    dot = 0.0
    ref_power = 0.0
    got_power = 0.0
    used = 0
    for n in range(0, count, stride):
        ref_index = ref_start + n
        got_index = got_start + n
        if ref_index < 0 or got_index < 0 or ref_index >= len(ref) or got_index >= len(got):
            continue
        rv = ref[ref_index]
        gv = got[got_index]
        dot += rv * gv
        ref_power += rv * rv
        got_power += gv * gv
        used += 1
    if used == 0 or ref_power <= 0.0 or got_power <= 0.0:
        return -1.0
    return dot / math.sqrt(ref_power * got_power)


def scan_lags(ref, got, ref_start, got_start, start_lag, end_lag, step, count, stride):
    best_score = -1.0
    best = 0
    for lag in range(start_lag, end_lag + 1, max(1, step)):
        score = normalized_dot(ref, got, ref_start, got_start + lag, count, stride)
        if score > best_score:
            best_score = score
            best = lag
    return best, best_score


def best_lag(ref, got, ref_start, got_start, search_radius, count, stride):
    if search_radius <= 512:
        return scan_lags(ref,
                         got,
                         ref_start,
                         got_start,
                         -search_radius,
                         search_radius,
                         1,
                         count,
                         stride)
    coarse_step = max(8, search_radius // 512)
    coarse_lag, _coarse_score = scan_lags(ref,
                                          got,
                                          ref_start,
                                          got_start,
                                          -search_radius,
                                          search_radius,
                                          coarse_step,
                                          count,
                                          max(stride, coarse_step))
    fine_radius = coarse_step * 2
    return scan_lags(ref,
                     got,
                     ref_start,
                     got_start,
                     max(-search_radius, coarse_lag - fine_radius),
                     min(search_radius, coarse_lag + fine_radius),
                     1,
                     count,
                     stride)


def align_pair(reference, capture, rate):
    ref_mono = pair_to_mono(reference)
    cap_mono = pair_to_mono(capture)
    ref_start = first_signal_index(ref_mono)
    cap_start = first_signal_index(cap_mono)
    max_lag = max(1, int(rate * MAX_LAG_SECONDS))
    coarse_count = min(len(ref_mono) - ref_start, len(cap_mono) - cap_start, int(rate * ALIGN_SECONDS))
    if coarse_count <= max(128, rate // 100):
        die(f"not_enough_audio_for_alignment:{coarse_count}")

    coarse_stride = max(1, coarse_count // 1024)
    coarse_lag, coarse_score = best_lag(
        ref_mono,
        cap_mono,
        ref_start,
        cap_start,
        max_lag,
        coarse_count,
        coarse_stride,
    )
    fine_radius = min(max_lag, 512)
    fine_count = min(coarse_count, rate // 2)
    fine_stride = max(1, fine_count // 4096)
    fine_lag, fine_score = best_lag(
        ref_mono,
        cap_mono,
        ref_start,
        cap_start + coarse_lag,
        fine_radius,
        fine_count,
        fine_stride,
    )

    lag = coarse_lag + fine_lag
    cap_start += lag
    if cap_start < 0:
        ref_start += -cap_start
        cap_start = 0
    if ref_start < 0:
        cap_start += -ref_start
        ref_start = 0

    usable = min(len(reference) - ref_start, len(capture) - cap_start)
    if usable <= max(128, rate // 10):
        die(f"not_enough_aligned_audio:{usable}")

    return {
        "reference_start": ref_start,
        "capture_start": cap_start,
        "lag_frames": lag,
        "alignment_score": fine_score if fine_score >= 0.0 else coarse_score,
        "usable_frames": usable,
    }


def rms(values):
    if not values:
        return 0.0
    return math.sqrt(sum(value * value for value in values) / len(values))


def channel_metrics(ref, got):
    ref_power = sum(value * value for value in ref)
    got_power = sum(value * value for value in got)
    dot = sum(rv * gv for rv, gv in zip(ref, got))
    gain = dot / ref_power if ref_power > 0.0 else 0.0
    correlation = dot / math.sqrt(ref_power * got_power) if ref_power > 0.0 and got_power > 0.0 else 0.0
    residual = [gv - gain * rv for rv, gv in zip(ref, got)]
    signal_rms = abs(gain) * math.sqrt(ref_power / len(ref)) if ref else 0.0
    residual_rms = rms(residual)
    snr_db = 20.0 * math.log10(signal_rms / residual_rms) if signal_rms > 0.0 and residual_rms > 0.0 else 999.0
    return {
        "gain": gain,
        "correlation": correlation,
        "signal_rms": signal_rms,
        "capture_rms": math.sqrt(got_power / len(got)) if got else 0.0,
        "residual_rms": residual_rms,
        "residual_peak": max((abs(value) for value in residual), default=0.0),
        "snr_db": snr_db,
        "peak": max((abs(value) for value in got), default=0.0),
        "residual": residual,
    }


def robust_click_count(left_residual, right_residual):
    if len(left_residual) < 3 or len(right_residual) < 3:
        return 0, 0.0

    deltas = []
    frame_deltas = []
    for index in range(1, min(len(left_residual), len(right_residual))):
        left_delta = abs(left_residual[index] - left_residual[index - 1])
        right_delta = abs(right_residual[index] - right_residual[index - 1])
        value = max(left_delta, right_delta)
        frame_deltas.append(value)
        deltas.append(value)

    med = median(deltas)
    mad = median(abs(value - med) for value in deltas) or EPSILON
    threshold = max(0.02, med + 18.0 * mad)
    clicks = 0
    in_click = False
    quiet_frames = 0
    for value in frame_deltas:
        if value > threshold:
            if not in_click:
                clicks += 1
                in_click = True
            quiet_frames = 0
        elif in_click:
            quiet_frames += 1
            if quiet_frames >= 2:
                in_click = False
    return clicks, threshold


def goertzel_magnitude(samples, rate, frequency):
    count = len(samples)
    if count <= 1 or frequency <= 0.0 or frequency >= rate / 2.0:
        return 0.0
    omega = 2.0 * math.pi * frequency / rate
    coeff = 2.0 * math.cos(omega)
    s_prev = 0.0
    s_prev2 = 0.0
    for index, sample in enumerate(samples):
        window = 0.5 - 0.5 * math.cos((2.0 * math.pi * index) / (count - 1))
        s = sample * window + coeff * s_prev - s_prev2
        s_prev2 = s_prev
        s_prev = s
    power = s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2
    return math.sqrt(max(0.0, power))


def spectral_frequencies(rate):
    frequencies = []
    frequency = 40.0
    nyquist_limit = rate * 0.45
    ratio = 2.0 ** (1.0 / 6.0)
    while frequency <= nyquist_limit:
        frequencies.append(frequency)
        frequency *= ratio
    return frequencies


def spectral_metrics(ref, got, rate, gain):
    count = min(len(ref), len(got), int(rate * SPECTRAL_SECONDS))
    if count < max(256, rate // 20):
        return {"bins": 0, "ripple_db": 0.0, "min_db": 0.0, "max_db": 0.0}

    ref_window = ref[:count]
    got_window = got[:count]
    frequencies = spectral_frequencies(rate)
    ref_magnitudes = [(frequency, goertzel_magnitude(ref_window, rate, frequency)) for frequency in frequencies]
    peak_ref = max((magnitude for _frequency, magnitude in ref_magnitudes), default=0.0)
    if peak_ref <= EPSILON:
        return {"bins": 0, "ripple_db": 0.0, "min_db": 0.0, "max_db": 0.0}

    active = [(frequency, magnitude) for frequency, magnitude in ref_magnitudes if magnitude >= peak_ref * 0.03]
    if len(active) < 2:
        return {"bins": len(active), "ripple_db": 0.0, "min_db": 0.0, "max_db": 0.0}

    gain_abs = max(abs(gain), EPSILON)
    ratios_db = []
    for frequency, ref_magnitude in active:
        got_magnitude = goertzel_magnitude(got_window, rate, frequency) / gain_abs
        ratio = max(got_magnitude / max(ref_magnitude, EPSILON), EPSILON)
        ratios_db.append(20.0 * math.log10(ratio))

    return {
        "bins": len(ratios_db),
        "ripple_db": max(ratios_db) - min(ratios_db),
        "min_db": min(ratios_db),
        "max_db": max(ratios_db),
    }


def print_float(key, value, places=8):
    if math.isfinite(value):
        print(f"{key}={value:.{places}f}")
    else:
        print(f"{key}=nan")


def print_channel(prefix, metrics, spectral):
    print_float(f"{prefix}_gain", metrics["gain"])
    print_float(f"{prefix}_correlation", metrics["correlation"], 6)
    print_float(f"{prefix}_snr_db", metrics["snr_db"], 2)
    print_float(f"{prefix}_signal_rms", metrics["signal_rms"])
    print_float(f"{prefix}_capture_rms", metrics["capture_rms"])
    print_float(f"{prefix}_peak", metrics["peak"])
    print_float(f"{prefix}_residual_rms", metrics["residual_rms"])
    print_float(f"{prefix}_residual_peak", metrics["residual_peak"])
    print(f"{prefix}_spectral_bins={spectral['bins']}")
    print_float(f"{prefix}_spectral_ripple_db", spectral["ripple_db"], 2)
    print_float(f"{prefix}_spectral_min_db", spectral["min_db"], 2)
    print_float(f"{prefix}_spectral_max_db", spectral["max_db"], 2)


def main():
    parser = argparse.ArgumentParser(
        description="Analyze loopback quality by comparing a PCM WAV reference with an interleaved f32 capture."
    )
    parser.add_argument("--reference-wav", required=True)
    parser.add_argument("--captured-f32", required=True)
    parser.add_argument("--sample-rate", type=int, default=DEFAULT_SAMPLE_RATE)
    parser.add_argument("--channels", type=int, default=DEFAULT_CHANNELS)
    parser.add_argument("--pair", type=int, default=DEFAULT_PAIR)
    parser.add_argument("--min-snr-db", type=float, default=DEFAULT_MIN_SNR_DB)
    parser.add_argument("--min-correlation", type=float, default=DEFAULT_MIN_CORRELATION)
    parser.add_argument("--max-clicks", type=int, default=DEFAULT_MAX_CLICKS)
    parser.add_argument("--max-ripple-db", type=float, default=DEFAULT_MAX_RIPPLE_DB)
    args = parser.parse_args()

    if args.sample_rate <= 0:
        die(f"invalid_sample_rate:{args.sample_rate}")

    wav_rate, wav_channels, reference = read_reference_wav(args.reference_wav)
    if wav_rate != args.sample_rate:
        die(f"sample_rate_mismatch:wav={wav_rate}:expected={args.sample_rate}")
    capture_info = read_capture_f32(args.captured_f32, args.channels, args.pair)
    capture = capture_info["pair"]

    if not reference:
        die("empty_reference")
    if not capture:
        die("empty_capture")

    alignment = align_pair(reference, capture, args.sample_rate)
    ref_start = alignment["reference_start"]
    cap_start = alignment["capture_start"]
    usable = alignment["usable_frames"]
    reference_window = reference[ref_start:ref_start + usable]
    capture_window = capture[cap_start:cap_start + usable]

    ref_left = [sample[0] for sample in reference_window]
    ref_right = [sample[1] for sample in reference_window]
    got_left = [sample[0] for sample in capture_window]
    got_right = [sample[1] for sample in capture_window]

    left = channel_metrics(ref_left, got_left)
    right = channel_metrics(ref_right, got_right)
    left_spectral = spectral_metrics(ref_left, got_left, args.sample_rate, left["gain"])
    right_spectral = spectral_metrics(ref_right, got_right, args.sample_rate, right["gain"])
    clicks, click_threshold = robust_click_count(left["residual"], right["residual"])

    min_snr_db = min(left["snr_db"], right["snr_db"])
    min_correlation = min(left["correlation"], right["correlation"])
    peak = max(left["peak"], right["peak"])
    residual_rms = max(left["residual_rms"], right["residual_rms"])
    residual_peak = max(left["residual_peak"], right["residual_peak"])
    max_ripple_db = max(left_spectral["ripple_db"], right_spectral["ripple_db"])

    failures = []
    if min_snr_db < args.min_snr_db:
        failures.append("snr_db")
    if min_correlation < args.min_correlation:
        failures.append("correlation")
    if clicks > args.max_clicks:
        failures.append("clicks")
    if max_ripple_db > args.max_ripple_db:
        failures.append("spectral_ripple")
    if capture_info["nonfinite"] > 0:
        failures.append("nonfinite_capture")

    print(f"reference_wav={args.reference_wav}")
    print(f"captured_f32={args.captured_f32}")
    print(f"sample_rate={args.sample_rate}")
    print(f"reference_channels={wav_channels}")
    print(f"capture_channels={args.channels}")
    print(f"pair={args.pair}")
    print(f"reference_frames={len(reference)}")
    print(f"capture_frames={capture_info['frames']}")
    print(f"capture_bytes={capture_info['bytes']}")
    print(f"capture_trailing_bytes={capture_info['trailing_bytes']}")
    print(f"capture_trailing_floats={capture_info['trailing_floats']}")
    print(f"capture_nonfinite={capture_info['nonfinite']}")
    print(f"reference_start={alignment['reference_start']}")
    print(f"capture_start={alignment['capture_start']}")
    print(f"lag_frames={alignment['lag_frames']}")
    print_float("lag_seconds", alignment["lag_frames"] / args.sample_rate, 8)
    print_float("alignment_score", alignment["alignment_score"], 6)
    print(f"compared_frames={usable}")
    print_float("compared_seconds", usable / args.sample_rate, 6)
    print_channel("left", left, left_spectral)
    print_channel("right", right, right_spectral)
    print_float("min_snr_db", min_snr_db, 2)
    print_float("min_correlation", min_correlation, 6)
    print_float("peak", peak)
    print_float("residual_rms", residual_rms)
    print_float("residual_peak", residual_peak)
    print(f"click_outliers={clicks}")
    print_float("click_threshold", click_threshold)
    print_float("max_spectral_ripple_db", max_ripple_db, 2)
    print_float("threshold_min_snr_db", args.min_snr_db, 2)
    print_float("threshold_min_correlation", args.min_correlation, 6)
    print(f"threshold_max_clicks={args.max_clicks}")
    print_float("threshold_max_ripple_db", args.max_ripple_db, 2)
    print(f"failure_count={len(failures)}")
    print(f"failures={','.join(failures) if failures else 'none'}")
    print(f"pass={1 if not failures else 0}")

    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
