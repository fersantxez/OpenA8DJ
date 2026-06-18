#!/usr/bin/env python3
import argparse
import array
import json
import math
import sys
import wave

DEFAULT_NOISE_BAND_LOW_HZ = 1000.0
DEFAULT_NOISE_BAND_HIGH_HZ = 5000.0
DEFAULT_HIGH_BAND_LOW_HZ = 5000.0
DEFAULT_HIGH_BAND_HIGH_HZ = 12000.0


def decode_sample(raw, width):
    if width == 1:
        return (raw[0] - 128) / 128.0
    if width == 2:
        return int.from_bytes(raw, "little", signed=True) / 32768.0
    if width == 3:
        value = raw[0] | (raw[1] << 8) | (raw[2] << 16)
        if value & 0x800000:
            value |= ~0xffffff
        return value / 8388608.0
    if width == 4:
        return int.from_bytes(raw, "little", signed=True) / 2147483648.0
    raise SystemExit(f"unsupported WAV sample width: {width * 8} bits")


def read_wav_pair(path):
    with wave.open(path, "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if width == 2:
        samples = array.array("h")
        samples.frombytes(raw)
        if sys.byteorder != "little":
            samples.byteswap()
        pair = []
        for offset in range(0, len(samples), channels):
            left = samples[offset] / 32768.0
            if channels > 1:
                right = samples[offset + 1] / 32768.0
            else:
                right = left
            pair.append((left, right))
        return rate, pair
    pair = []
    step = channels * width
    for offset in range(0, len(raw), step):
        left = decode_sample(raw[offset:offset + width], width)
        if channels > 1:
            right_offset = offset + width
            right = decode_sample(raw[right_offset:right_offset + width], width)
        else:
            right = left
        pair.append((left, right))
    return rate, pair


def pair_to_mono_s16(pair):
    mono_s16 = bytearray()
    for left, right in pair:
        mono = max(-1.0, min(1.0, 0.5 * (left + right)))
        mono_s16.extend(int(round(mono * 32767.0)).to_bytes(2, "little", signed=True))
    return bytes(mono_s16)


def resample_pair_linear(pair, source_rate, target_rate):
    if source_rate == target_rate or not pair:
        return pair
    target_frames = int(round(len(pair) * float(target_rate) / float(source_rate)))
    if target_frames <= 1:
        return pair[:1]
    scale = float(source_rate) / float(target_rate)
    last_index = len(pair) - 1
    resampled = []
    for index in range(target_frames):
        source_position = index * scale
        left_index = int(math.floor(source_position))
        right_index = min(left_index + 1, last_index)
        fraction = source_position - left_index
        left_a, right_a = pair[min(left_index, last_index)]
        left_b, right_b = pair[right_index]
        resampled.append((
            left_a + (left_b - left_a) * fraction,
            right_a + (right_b - right_a) * fraction,
        ))
    return resampled


def pair_to_mono(pair):
    return [0.5 * (left + right) for left, right in pair]


def rms(values):
    if not values:
        return 0.0
    return math.sqrt(sum(value * value for value in values) / len(values))


def dbfs(value):
    if value <= 0.0:
        return -240.0
    return 20.0 * math.log10(value)


def first_signal_index(pair):
    envelope = [max(abs(left), abs(right)) for left, right in pair]
    if not envelope:
        return 0
    peak = max(envelope)
    threshold = max(0.0005, peak * 0.02)
    for index, value in enumerate(envelope):
        if value >= threshold:
            return index
    return 0


def score_lag(ref_mono, got_mono, ref_start, got_start, lag, sample_count, stride):
    score = 0.0
    ref_energy = 0.0
    got_energy = 0.0
    used = 0
    for n in range(0, sample_count, max(1, stride)):
        ri = ref_start + n
        gi = got_start + n + lag
        if ri < 0 or gi < 0 or ri >= len(ref_mono) or gi >= len(got_mono):
            continue
        rv = ref_mono[ri]
        gv = got_mono[gi]
        score += rv * gv
        ref_energy += rv * rv
        got_energy += gv * gv
        used += 1
    if used == 0 or ref_energy <= 0.0 or got_energy <= 0.0:
        return None
    return score / math.sqrt(ref_energy * got_energy)


def scan_lags(ref_mono, got_mono, ref_start, got_start, start_lag, end_lag, step, sample_count, stride):
    best_lag = 0
    best_score = None
    for lag in range(start_lag, end_lag + 1, max(1, step)):
        normalized = score_lag(ref_mono,
                               got_mono,
                               ref_start,
                               got_start,
                               lag,
                               sample_count,
                               stride)
        if normalized is None:
            continue
        if best_score is None or normalized > best_score:
            best_score = normalized
            best_lag = lag
    return best_lag, best_score if best_score is not None else 0.0


def find_best_lag(ref_mono, got_mono, ref_start, got_start, max_lag, sample_count, stride):
    if max_lag <= 4096:
        return scan_lags(ref_mono,
                         got_mono,
                         ref_start,
                         got_start,
                         -max_lag,
                         max_lag,
                         1,
                         sample_count,
                         stride)

    coarse_step = max(8, max_lag // 2048)
    coarse_lag, _coarse_score = scan_lags(ref_mono,
                                          got_mono,
                                          ref_start,
                                          got_start,
                                          -max_lag,
                                          max_lag,
                                          coarse_step,
                                          sample_count,
                                          max(stride, coarse_step))
    fine_radius = coarse_step * 2
    fine_start = max(-max_lag, coarse_lag - fine_radius)
    fine_end = min(max_lag, coarse_lag + fine_radius)
    return scan_lags(ref_mono,
                     got_mono,
                     ref_start,
                     got_start,
                     fine_start,
                     fine_end,
                     1,
                     sample_count,
                     stride)


def correlation(ref_mono, got_mono, ref_start, got_start, sample_count):
    score = 0.0
    ref_energy = 0.0
    got_energy = 0.0
    for n in range(sample_count):
        ri = ref_start + n
        gi = got_start + n
        if ri < 0 or gi < 0 or ri >= len(ref_mono) or gi >= len(got_mono):
            continue
        rv = ref_mono[ri]
        gv = got_mono[gi]
        score += rv * gv
        ref_energy += rv * rv
        got_energy += gv * gv
    if ref_energy <= 0.0 or got_energy <= 0.0:
        return 0.0
    return score / math.sqrt(ref_energy * got_energy)


def normalize_starts(ref_start, got_start):
    if got_start < 0:
        ref_start += -got_start
        got_start = 0
    if ref_start < 0:
        got_start += -ref_start
        ref_start = 0
    return ref_start, got_start


def channel_fit(ref, got):
    dot = sum(r * g for r, g in zip(ref, got))
    ref_power = sum(r * r for r in ref)
    gain = dot / ref_power if ref_power > 0 else 0.0
    residual = [g - gain * r for r, g in zip(ref, got)]
    signal = [gain * r for r in ref]
    return gain, signal, residual


def quiet_segment_metrics(signal, capture, residual, rate, noise_band_low, noise_band_high):
    window = max(256, int(rate * 0.25))
    hop = max(1, window // 2)
    usable = min(len(signal), len(capture), len(residual))
    if usable < window:
        return {
            "windows": 0,
            "fullband_noise_rms": 0.0,
            "mid_band_noise_rms": 0.0,
            "capture_rms": 0.0,
        }
    signal_windows = []
    for start in range(0, usable - window + 1, hop):
        end = start + window
        signal_windows.append((rms(signal[start:end]), start, end))
    if not signal_windows:
        return {
            "windows": 0,
            "fullband_noise_rms": 0.0,
            "mid_band_noise_rms": 0.0,
            "capture_rms": 0.0,
        }
    ordered = sorted(level for level, _start, _end in signal_windows)
    quiet_cutoff = max(db_to_linear(-45.0), ordered[max(0, min(len(ordered) - 1, len(ordered) // 4))])
    selected = [(start, end) for level, start, end in signal_windows if level <= quiet_cutoff]
    if not selected:
        selected = [(start, end) for _level, start, end in sorted(signal_windows)[:1]]
    quiet_residual = []
    quiet_capture = []
    for start, end in selected:
        quiet_residual.extend(residual[start:end])
        quiet_capture.extend(capture[start:end])
    quiet_mid = bandpass(quiet_residual, rate, noise_band_low, noise_band_high)
    return {
        "windows": len(selected),
        "fullband_noise_rms": rms(quiet_residual),
        "mid_band_noise_rms": rms(quiet_mid),
        "capture_rms": rms(quiet_capture),
    }


def db_to_linear(db):
    return 10.0 ** (db / 20.0)


def compare_channel(ref, got, rate, noise_band_low, noise_band_high, high_band_low, high_band_high):
    gain, signal, residual = channel_fit(ref, got)
    signal_rms = rms(signal)
    residual_rms = rms(residual)
    snr = 20.0 * math.log10(signal_rms / residual_rms) if residual_rms > 0 and signal_rms > 0 else 999.0
    peak_error = max((abs(value) for value in residual), default=0.0)
    click_threshold = max(0.01, residual_rms * 12.0)
    click_outliers = sum(1 for value in residual if abs(value) > click_threshold)
    high_residual = high_band_proxy(residual)
    high_signal = high_band_proxy(signal)
    mid_residual = band_rms(residual, rate, noise_band_low, noise_band_high)
    mid_signal = band_rms(signal, rate, noise_band_low, noise_band_high)
    metrics = {
        "gain": gain,
        "signal_rms": signal_rms,
        "residual_rms": residual_rms,
        "snr_db": snr,
        "peak_error": peak_error,
        "click_outliers": click_outliers,
        "high_band_residual_ratio": high_residual / high_signal if high_signal > 0 else 0.0,
        "mid_band_signal_rms": mid_signal,
        "mid_band_residual_rms": mid_residual,
        "mid_band_residual_dbfs": dbfs(mid_residual),
        "mid_band_residual_ratio": mid_residual / mid_signal if mid_signal > 1e-9 else 0.0,
    }
    metrics.update(channel_noise_metrics(signal,
                                         got,
                                         residual,
                                         rate,
                                         noise_band_low,
                                         noise_band_high,
                                         high_band_low,
                                         high_band_high))
    return metrics


def sample_linear(values, position):
    if not values:
        return 0.0
    if position <= 0.0:
        return values[0]
    last_index = len(values) - 1
    if position >= last_index:
        return values[last_index]
    left_index = int(math.floor(position))
    fraction = position - left_index
    return values[left_index] + (values[left_index + 1] - values[left_index]) * fraction


def interpolated_lag(points, sample_offset):
    if not points:
        return 0.0
    if sample_offset <= points[0][0]:
        return float(points[0][1])
    for left, right in zip(points, points[1:]):
        left_offset, left_lag, _left_score = left
        right_offset, right_lag, _right_score = right
        if sample_offset <= right_offset:
            span = max(1, right_offset - left_offset)
            fraction = (sample_offset - left_offset) / span
            return left_lag + (right_lag - left_lag) * fraction
    return float(points[-1][1])


def warp_capture_pair(got, got_start, usable, lag_points):
    if not lag_points:
        return got[got_start:got_start + usable]
    left = [sample[0] for sample in got]
    right = [sample[1] for sample in got]
    warped = []
    for offset in range(usable):
        source_position = got_start + offset + interpolated_lag(lag_points, offset)
        warped.append((
            sample_linear(left, source_position),
            sample_linear(right, source_position),
        ))
    return warped


def channel_noise_metrics(ref_signal, got_signal, residual, rate, noise_band_low, noise_band_high, high_band_low, high_band_high):
    high_residual = band_rms(residual, rate, high_band_low, high_band_high)
    high_signal = band_rms(ref_signal, rate, high_band_low, high_band_high)
    quiet = quiet_segment_metrics(ref_signal, got_signal, residual, rate, noise_band_low, noise_band_high)
    return {
        "high_band_signal_rms": high_signal,
        "high_band_residual_rms": high_residual,
        "high_band_residual_dbfs": dbfs(high_residual),
        "high_band_residual_ratio": high_residual / high_signal if high_signal > 1e-9 else 0.0,
        "quiet_windows": quiet["windows"],
        "quiet_fullband_noise_rms": quiet["fullband_noise_rms"],
        "quiet_fullband_noise_dbfs": dbfs(quiet["fullband_noise_rms"]),
        "quiet_mid_band_noise_rms": quiet["mid_band_noise_rms"],
        "quiet_mid_band_noise_dbfs": dbfs(quiet["mid_band_noise_rms"]),
        "quiet_capture_rms": quiet["capture_rms"],
        "quiet_capture_dbfs": dbfs(quiet["capture_rms"]),
    }


def high_band_proxy(values):
    if len(values) < 2:
        return 0.0
    total = 0.0
    prev = values[0]
    for value in values[1:]:
        diff = value - prev
        total += diff * diff
        prev = value
    return math.sqrt(total / (len(values) - 1))


def biquad_coefficients(kind, rate, cutoff, q=0.7071067811865476):
    nyquist = rate * 0.5
    cutoff = max(1.0, min(float(cutoff), nyquist * 0.95))
    omega = 2.0 * math.pi * cutoff / rate
    sin_omega = math.sin(omega)
    cos_omega = math.cos(omega)
    alpha = sin_omega / (2.0 * q)
    if kind == "lowpass":
        b0 = (1.0 - cos_omega) * 0.5
        b1 = 1.0 - cos_omega
        b2 = (1.0 - cos_omega) * 0.5
    elif kind == "highpass":
        b0 = (1.0 + cos_omega) * 0.5
        b1 = -(1.0 + cos_omega)
        b2 = (1.0 + cos_omega) * 0.5
    else:
        raise ValueError(f"unknown biquad kind: {kind}")
    a0 = 1.0 + alpha
    a1 = -2.0 * cos_omega
    a2 = 1.0 - alpha
    return b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0


def apply_biquad(values, coefficients):
    b0, b1, b2, a1, a2 = coefficients
    x1 = x2 = y1 = y2 = 0.0
    output = []
    for x0 in values:
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        output.append(y0)
        x2 = x1
        x1 = x0
        y2 = y1
        y1 = y0
    return output


def bandpass(values, rate, low_hz, high_hz):
    if not values:
        return []
    nyquist = rate * 0.5
    low_hz = max(1.0, min(float(low_hz), nyquist * 0.90))
    high_hz = max(low_hz + 1.0, min(float(high_hz), nyquist * 0.95))
    highpassed = apply_biquad(values, biquad_coefficients("highpass", rate, low_hz))
    return apply_biquad(highpassed, biquad_coefficients("lowpass", rate, high_hz))


def band_rms(values, rate, low_hz, high_hz):
    return rms(bandpass(values, rate, low_hz, high_hz))


def lag_profile(ref, got, rate, ref_start, got_start, usable, max_lag, window_seconds, hop_seconds):
    window = max(64, int(rate * window_seconds))
    hop = max(1, int(rate * hop_seconds))
    if usable < window:
        return []
    ref_mono = pair_to_mono(ref)
    got_mono = pair_to_mono(got)
    stride = max(1, window // 256)
    points = []
    for start in range(0, usable - window + 1, hop):
        lag, score = find_best_lag(ref_mono,
                                   got_mono,
                                   ref_start + start,
                                   got_start + start,
                                   max_lag,
                                   window,
                                   stride)
        points.append((start, lag, score))
    return points


def median(values):
    if not values:
        return 0.0
    ordered = sorted(values)
    midpoint = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[midpoint]
    return 0.5 * (ordered[midpoint - 1] + ordered[midpoint])


def percentile(values, percent):
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * (percent / 100.0)
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def clipped_count(pair):
    return sum(1 for left, right in pair if abs(left) >= 0.999 or abs(right) >= 0.999)


def sanitize_metric_key(name):
    cleaned = []
    for char in name.strip().lower():
        cleaned.append(char if char.isalnum() else "_")
    return "_".join(part for part in "".join(cleaned).split("_") if part)


def load_cpu_profile(path):
    if not path:
        return [], []
    rows = []
    header = None
    with open(path, "r", encoding="utf-8") as file:
        for raw_line in file:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t") if "\t" in line else line.split()
            if header is None:
                header = [sanitize_metric_key(part) for part in parts]
                continue
            if len(parts) < len(header):
                continue
            row = {}
            for key, value in zip(header, parts):
                try:
                    row[key] = float(value)
                except ValueError:
                    pass
            if "time_epoch" in row and "sample_time_epoch" not in row:
                row["sample_time_epoch"] = row["time_epoch"]
            rows.append(row)
    if not rows:
        return [], []
    if "elapsed_seconds" not in rows[0]:
        first_epoch = next((row.get("sample_time_epoch") for row in rows if "sample_time_epoch" in row), None)
        if first_epoch is not None:
            for row in rows:
                if "sample_time_epoch" in row:
                    row["elapsed_seconds"] = row["sample_time_epoch"] - first_epoch
    columns = sorted({
        key
        for row in rows
        for key in row
        if key not in ("elapsed_seconds", "sample_time_epoch", "schema_version", "sample_index", "time_epoch")
    })
    return rows, columns


def average_cpu(rows, columns, start_seconds, end_seconds):
    if not rows or not columns:
        return {}
    in_window = [
        row for row in rows
        if start_seconds <= row.get("elapsed_seconds", -1e30) < end_seconds
    ]
    if not in_window:
        midpoint = 0.5 * (start_seconds + end_seconds)
        nearest = min(rows, key=lambda row: abs(row.get("elapsed_seconds", -1e30) - midpoint))
        if abs(nearest.get("elapsed_seconds", -1e30) - midpoint) <= max(0.5, end_seconds - start_seconds):
            in_window = [nearest]
    averaged = {}
    for column in columns:
        values = [row[column] for row in in_window if column in row]
        if values:
            averaged[column] = sum(values) / len(values)
    return averaged


def pearson(left, right):
    pairs = [(x, y) for x, y in zip(left, right) if x is not None and y is not None]
    if len(pairs) < 3:
        return 0.0
    xs = [pair[0] for pair in pairs]
    ys = [pair[1] for pair in pairs]
    x_mean = sum(xs) / len(xs)
    y_mean = sum(ys) / len(ys)
    x_var = sum((value - x_mean) ** 2 for value in xs)
    y_var = sum((value - y_mean) ** 2 for value in ys)
    if x_var <= 0.0 or y_var <= 0.0:
        return 0.0
    cov = sum((x - x_mean) * (y - y_mean) for x, y in pairs)
    return cov / math.sqrt(x_var * y_var)


def lagged_pearson(left, right, max_shift_windows):
    best = 0.0
    best_shift = 0
    for shift in range(-max_shift_windows, max_shift_windows + 1):
        if shift < 0:
            xs = left[-shift:]
            ys = right[:len(xs)]
        elif shift > 0:
            xs = left[:-shift]
            ys = right[shift:]
        else:
            xs = left
            ys = right
        corr = pearson(xs, ys)
        if corr > best:
            best = corr
            best_shift = shift
    return best, best_shift


def window_clicks(values):
    window_rms = rms(values)
    threshold = max(0.01, window_rms * 12.0)
    return sum(1 for value in values if abs(value) > threshold)


def build_coupling_profile(ref_left,
                           ref_right,
                           got_left,
                           got_right,
                           rate,
                           window_seconds,
                           hop_seconds,
                           noise_band_low,
                           noise_band_high,
                           cpu_rows,
                           cpu_columns):
    window = max(64, int(rate * window_seconds))
    hop = max(1, int(rate * hop_seconds))
    usable = min(len(ref_left), len(ref_right), len(got_left), len(got_right))
    if usable < window:
        return []

    _left_gain, left_signal, left_residual = channel_fit(ref_left[:usable], got_left[:usable])
    _right_gain, right_signal, right_residual = channel_fit(ref_right[:usable], got_right[:usable])
    left_mid_signal = bandpass(left_signal, rate, noise_band_low, noise_band_high)
    right_mid_signal = bandpass(right_signal, rate, noise_band_low, noise_band_high)
    left_mid_residual = bandpass(left_residual, rate, noise_band_low, noise_band_high)
    right_mid_residual = bandpass(right_residual, rate, noise_band_low, noise_band_high)

    points = []
    for start in range(0, usable - window + 1, hop):
        end = start + window
        left_mid_noise = rms(left_mid_residual[start:end])
        right_mid_noise = rms(right_mid_residual[start:end])
        left_mid_ref = rms(left_mid_signal[start:end])
        right_mid_ref = rms(right_mid_signal[start:end])
        mid_noise = max(left_mid_noise, right_mid_noise)
        mid_signal = max(left_mid_ref, right_mid_ref)
        residual_level = max(rms(left_residual[start:end]), rms(right_residual[start:end]))
        signal_level = max(rms(left_signal[start:end]), rms(right_signal[start:end]))
        point = {
            "start_seconds": start / rate,
            "end_seconds": end / rate,
            "mid_band_residual_rms": mid_noise,
            "mid_band_residual_dbfs": dbfs(mid_noise),
            "mid_band_signal_rms": mid_signal,
            "mid_band_residual_ratio": mid_noise / mid_signal if mid_signal > 1e-9 else 0.0,
            "residual_rms": residual_level,
            "signal_rms": signal_level,
            "residual_ratio": residual_level / signal_level if signal_level > 1e-9 else 0.0,
            "click_outliers": window_clicks(left_residual[start:end]) + window_clicks(right_residual[start:end]),
        }
        for column, value in average_cpu(cpu_rows, cpu_columns, point["start_seconds"], point["end_seconds"]).items():
            point[f"cpu_{column}"] = value
        points.append(point)
    return points


def add_coupling_metrics(metrics, profile, cpu_columns, max_cpu_lag_seconds):
    metrics["coupling_windows"] = len(profile)
    if not profile:
        return
    mid_rms = [point["mid_band_residual_rms"] for point in profile]
    mid_ratio = [point["mid_band_residual_ratio"] for point in profile]
    clicks = [point["click_outliers"] for point in profile]
    metrics["mid_band_window_residual_rms_max"] = max(mid_rms)
    metrics["mid_band_window_residual_rms_median"] = median(mid_rms)
    metrics["mid_band_window_residual_rms_p95"] = percentile(mid_rms, 95.0)
    metrics["mid_band_window_residual_ratio_max"] = max(mid_ratio)
    metrics["mid_band_window_residual_ratio_median"] = median(mid_ratio)
    metrics["mid_band_window_residual_ratio_p95"] = percentile(mid_ratio, 95.0)
    metrics["window_click_outliers_max"] = max(clicks)

    best_column = ""
    best_corr = 0.0
    if len(profile) >= 2:
        hop_seconds = max(0.001, profile[1]["start_seconds"] - profile[0]["start_seconds"])
    else:
        hop_seconds = 0.25
    max_shift_windows = max(0, int(round(max_cpu_lag_seconds / hop_seconds)))

    for column in cpu_columns:
        key = f"cpu_{column}"
        values = [point.get(key) for point in profile]
        corr, shift = lagged_pearson(values, mid_rms, max_shift_windows)
        metrics[f"mid_band_cpu_corr_{column}"] = corr
        metrics[f"mid_band_cpu_corr_shift_windows_{column}"] = shift
        if corr > best_corr:
            best_corr = corr
            best_column = column
    metrics["mid_band_cpu_corr_max"] = best_corr
    metrics["mid_band_cpu_corr_max_column"] = best_column


def compare_pair(ref,
                 got,
                 rate,
                 max_seconds,
                 max_lag,
                 time_warp,
                 time_warp_max_lag,
                 window_seconds,
                 hop_seconds,
                 drift_profile,
                 coupling_window_seconds,
                 coupling_hop_seconds,
                 noise_band_low,
                 noise_band_high,
                 high_band_low,
                 high_band_high,
                 cpu_rows,
                 cpu_columns):
    ref_start = first_signal_index(ref)
    got_rough_start = first_signal_index(got)
    fit_frames = min(len(ref) - ref_start,
                     int(max_seconds * rate) if max_seconds > 0 else len(ref),
                     int(rate * 1.0))
    if fit_frames < rate // 20:
        raise SystemExit("reference audio is too short for alignment")
    got_total_frames = len(got)
    slice_start = max(0, got_rough_start - max_lag)
    slice_end = min(got_total_frames, got_rough_start + fit_frames + max_lag)
    if slice_end - slice_start < fit_frames:
        raise SystemExit("capture audio is too short for alignment")
    ref_mono = pair_to_mono(ref)
    got_mono = pair_to_mono(got)
    local_lag, _local_score = find_best_lag(ref_mono,
                                            got_mono,
                                            ref_start,
                                            got_rough_start,
                                            max_lag,
                                            fit_frames,
                                            max(1, fit_frames // 512))
    got_start = got_rough_start + local_lag
    alignment_lag = got_start - ref_start
    ref_start, got_start = normalize_starts(ref_start, got_start)
    max_frames = int(max_seconds * rate) if max_seconds > 0 else len(ref)
    usable = min(len(ref) - ref_start, len(got) - got_start, max_frames)
    if usable <= max(1, rate // 2):
        raise SystemExit(f"not enough aligned audio: usable={usable}")
    alignment_score = correlation(ref_mono, got_mono, ref_start, got_start, min(usable, rate))

    lag_points = lag_profile(ref,
                             got,
                             rate,
                             ref_start,
                             got_start,
                             usable,
                             min(max_lag, 32),
                             window_seconds,
                             hop_seconds) if drift_profile else []
    warp_points = lag_profile(ref,
                              got,
                              rate,
                              ref_start,
                              got_start,
                              usable,
                              time_warp_max_lag,
                              window_seconds,
                              hop_seconds) if time_warp else []
    ref_window = ref[ref_start:ref_start + usable]
    raw_got_window = got[got_start:got_start + usable]
    got_window = warp_capture_pair(got, got_start, usable, warp_points) if time_warp else raw_got_window
    got_quality_mono = pair_to_mono(got_window)
    ref_left = [sample[0] for sample in ref_window]
    ref_right = [sample[1] for sample in ref_window]
    got_left = [sample[0] for sample in got_window]
    got_right = [sample[1] for sample in got_window]
    points = warp_points if time_warp else lag_points
    lags = [point[1] for point in points]
    scores = [point[2] for point in points]
    quality_alignment_score = correlation(ref_mono,
                                          got_quality_mono,
                                          ref_start,
                                          0,
                                          min(usable, rate))
    left = compare_channel(ref_left,
                           got_left,
                           rate,
                           noise_band_low,
                           noise_band_high,
                           high_band_low,
                           high_band_high)
    right = compare_channel(ref_right,
                            got_right,
                            rate,
                            noise_band_low,
                            noise_band_high,
                            high_band_low,
                            high_band_high)
    coupling = build_coupling_profile(ref_left,
                                      ref_right,
                                      got_left,
                                      got_right,
                                      rate,
                                      coupling_window_seconds,
                                      coupling_hop_seconds,
                                      noise_band_low,
                                      noise_band_high,
                                      cpu_rows,
                                      cpu_columns)
    result = {
        "reference_start": ref_start,
        "capture_start": got_start,
        "alignment_lag": alignment_lag,
        "alignment_score": alignment_score,
        "quality_alignment_score": quality_alignment_score,
        "compared_frames": usable,
        "compared_seconds": usable / rate,
        "time_warp_enabled": 1 if time_warp else 0,
        "time_warp_windows": len(warp_points),
        "time_warp_lag_min": min((point[1] for point in warp_points), default=0),
        "time_warp_lag_max": max((point[1] for point in warp_points), default=0),
        "time_warp_lag_first": warp_points[0][1] if warp_points else 0,
        "time_warp_lag_last": warp_points[-1][1] if warp_points else 0,
        "time_warp_lag_drift_frames": (warp_points[-1][1] - warp_points[0][1]) if len(warp_points) >= 2 else 0,
        "time_warp_score_min": min((point[2] for point in warp_points), default=0.0),
        "time_warp_score_median": median([point[2] for point in warp_points]),
        "noise_band_low_hz": noise_band_low,
        "noise_band_high_hz": noise_band_high,
        "high_band_low_hz": high_band_low,
        "high_band_high_hz": high_band_high,
        "mid_band_residual_ratio": max(left["mid_band_residual_ratio"], right["mid_band_residual_ratio"]),
        "mid_band_residual_rms": max(left["mid_band_residual_rms"], right["mid_band_residual_rms"]),
        "mid_band_residual_dbfs": max(left["mid_band_residual_dbfs"], right["mid_band_residual_dbfs"]),
        "high_band_residual_ratio": max(left["high_band_residual_ratio"], right["high_band_residual_ratio"]),
        "high_band_residual_rms": max(left["high_band_residual_rms"], right["high_band_residual_rms"]),
        "high_band_residual_dbfs": max(left["high_band_residual_dbfs"], right["high_band_residual_dbfs"]),
        "quiet_mid_band_noise_dbfs": max(left["quiet_mid_band_noise_dbfs"], right["quiet_mid_band_noise_dbfs"]),
        "quiet_fullband_noise_dbfs": max(left["quiet_fullband_noise_dbfs"], right["quiet_fullband_noise_dbfs"]),
        "left": left,
        "right": right,
        "lag_windows": len(points),
        "lag_min": min(lags) if lags else 0,
        "lag_max": max(lags) if lags else 0,
        "lag_first": lags[0] if lags else 0,
        "lag_last": lags[-1] if lags else 0,
        "lag_jumps_gt_2_frames": sum(1 for prev, cur in zip(lags, lags[1:]) if abs(cur - prev) > 2),
        "lag_score_min": min(scores) if scores else 0.0,
        "lag_score_median": median(scores),
        "capture_clipped_frames": clipped_count(raw_got_window),
    }
    return result, coupling


def flatten_metrics(result):
    flat = {}
    for key, value in result.items():
        if isinstance(value, dict):
            for subkey, subvalue in value.items():
                flat[f"{key}_{subkey}"] = subvalue
        else:
            flat[key] = value
    return flat


def verdict(metrics,
            min_alignment,
            min_snr,
            max_clicks,
            max_lag_jumps,
            max_mid_band_residual_ratio,
            max_high_band_residual_ratio,
            max_quiet_mid_band_noise_dbfs,
            max_mid_band_cpu_corr,
            min_mid_band_ratio_for_cpu_corr):
    errors = []
    alignment_metric = metrics.get("quality_alignment_score", metrics["alignment_score"])
    aligned = alignment_metric >= min_alignment
    metrics["mid_band_gate_evaluated"] = 1 if aligned else 0
    residual_gate_ready = alignment_metric >= 0.85
    metrics["residual_noise_gate_evaluated"] = 1 if residual_gate_ready else 0
    if not aligned:
        errors.append(f"quality_alignment_score={alignment_metric:.6f} < {min_alignment:.6f}")
    min_channel_snr = min(metrics["left_snr_db"], metrics["right_snr_db"])
    if min_channel_snr < min_snr:
        errors.append(f"snr_db={min_channel_snr:.2f} < {min_snr:.2f}")
    clicks = metrics["left_click_outliers"] + metrics["right_click_outliers"]
    if clicks > max_clicks:
        errors.append(f"click_outliers={clicks} > {max_clicks}")
    if metrics["lag_jumps_gt_2_frames"] > max_lag_jumps:
        errors.append(f"lag_jumps_gt_2_frames={metrics['lag_jumps_gt_2_frames']} > {max_lag_jumps}")
    if metrics["capture_clipped_frames"] > 0:
        errors.append(f"capture_clipped_frames={metrics['capture_clipped_frames']}")
    if residual_gate_ready:
        mid_ratio = metrics.get("mid_band_residual_ratio", 0.0)
        if mid_ratio > max_mid_band_residual_ratio:
            errors.append(f"mid_band_residual_ratio={mid_ratio:.6f} > {max_mid_band_residual_ratio:.6f}")
        high_ratio = metrics.get("high_band_residual_ratio", 0.0)
        if high_ratio > max_high_band_residual_ratio:
            errors.append(f"high_band_residual_ratio={high_ratio:.6f} > {max_high_band_residual_ratio:.6f}")
        quiet_mid = metrics.get("quiet_mid_band_noise_dbfs", -240.0)
        if quiet_mid > max_quiet_mid_band_noise_dbfs:
            errors.append(f"quiet_mid_band_noise_dbfs={quiet_mid:.2f} > {max_quiet_mid_band_noise_dbfs:.2f}")
        cpu_corr = metrics.get("mid_band_cpu_corr_max", 0.0)
        cpu_corr_column = metrics.get("mid_band_cpu_corr_max_column", "")
        window_mid_ratio = metrics.get("mid_band_window_residual_ratio_max", mid_ratio)
        if cpu_corr > max_mid_band_cpu_corr and window_mid_ratio > min_mid_band_ratio_for_cpu_corr:
            errors.append(
                f"mid_band_cpu_corr={cpu_corr:.6f} ({cpu_corr_column}) > {max_mid_band_cpu_corr:.6f}"
            )
    return not errors, errors


def print_metrics(metrics):
    for key in sorted(metrics):
        value = metrics[key]
        if isinstance(value, float):
            print(f"{key}={value:.8f}")
        else:
            print(f"{key}={value}")


def main():
    parser = argparse.ArgumentParser(description="Compare a soundcheck reference WAV with an analog capture WAV.")
    parser.add_argument("reference_wav")
    parser.add_argument("capture_wav")
    parser.add_argument("--max-seconds", type=float, default=20.0)
    parser.add_argument("--max-lag", type=int, default=1024)
    parser.add_argument("--time-warp", action="store_true")
    parser.add_argument("--time-warp-max-lag", type=int, default=128)
    parser.add_argument("--lag-window-seconds", type=float, default=0.5)
    parser.add_argument("--lag-hop-seconds", type=float, default=0.25)
    parser.add_argument("--drift-profile", action="store_true")
    parser.add_argument("--min-alignment", type=float, default=0.98)
    parser.add_argument("--min-snr-db", type=float, default=35.0)
    parser.add_argument("--max-clicks", type=int, default=10)
    parser.add_argument("--max-lag-jumps", type=int, default=0)
    parser.add_argument("--noise-band-low-hz", type=float, default=DEFAULT_NOISE_BAND_LOW_HZ)
    parser.add_argument("--noise-band-high-hz", type=float, default=DEFAULT_NOISE_BAND_HIGH_HZ)
    parser.add_argument("--high-band-low-hz", type=float, default=DEFAULT_HIGH_BAND_LOW_HZ)
    parser.add_argument("--high-band-high-hz", type=float, default=DEFAULT_HIGH_BAND_HIGH_HZ)
    parser.add_argument("--coupling-window-seconds", type=float, default=0.5)
    parser.add_argument("--coupling-hop-seconds", type=float, default=0.25)
    parser.add_argument("--max-cpu-correlation-lag-seconds", type=float, default=1.0)
    parser.add_argument("--cpu-profile")
    parser.add_argument("--coupling-profile-out")
    parser.add_argument("--max-mid-band-residual-ratio", type=float, default=0.04)
    parser.add_argument("--max-high-band-residual-ratio", type=float, default=0.08)
    parser.add_argument("--max-quiet-mid-band-noise-dbfs", type=float, default=-58.0)
    parser.add_argument("--max-mid-band-cpu-corr", type=float, default=0.60)
    parser.add_argument("--min-mid-band-ratio-for-cpu-corr", type=float, default=0.02)
    parser.add_argument("--json-out")
    args = parser.parse_args()

    ref_rate, ref = read_wav_pair(args.reference_wav)
    got_rate, got = read_wav_pair(args.capture_wav)
    cpu_rows, cpu_columns = load_cpu_profile(args.cpu_profile)
    original_got_rate = got_rate
    if ref_rate != got_rate:
        got = resample_pair_linear(got, got_rate, ref_rate)
        got_rate = ref_rate
    result, coupling = compare_pair(ref,
                                    got,
                                    ref_rate,
                                    args.max_seconds,
                                    args.max_lag,
                                    args.time_warp,
                                    args.time_warp_max_lag,
                                    args.lag_window_seconds,
                                    args.lag_hop_seconds,
                                    args.drift_profile,
                                    args.coupling_window_seconds,
                                    args.coupling_hop_seconds,
                                    args.noise_band_low_hz,
                                    args.noise_band_high_hz,
                                    args.high_band_low_hz,
                                    args.high_band_high_hz,
                                    cpu_rows,
                                    cpu_columns)
    metrics = flatten_metrics(result)
    add_coupling_metrics(metrics, coupling, cpu_columns, args.max_cpu_correlation_lag_seconds)
    passed, errors = verdict(metrics,
                             args.min_alignment,
                             args.min_snr_db,
                             args.max_clicks,
                             args.max_lag_jumps,
                             args.max_mid_band_residual_ratio,
                             args.max_high_band_residual_ratio,
                             args.max_quiet_mid_band_noise_dbfs,
                             args.max_mid_band_cpu_corr,
                             args.min_mid_band_ratio_for_cpu_corr)
    metrics["reference"] = args.reference_wav
    metrics["capture"] = args.capture_wav
    metrics["rate"] = ref_rate
    metrics["capture_original_rate"] = original_got_rate
    metrics["cpu_profile"] = args.cpu_profile or ""
    metrics["cpu_profile_samples"] = len(cpu_rows)
    metrics["verdict"] = "PASS" if passed else "FAIL"
    metrics["errors"] = errors

    print_metrics(metrics)
    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as file:
            json.dump(metrics, file, indent=2)
            file.write("\n")
    if args.coupling_profile_out:
        with open(args.coupling_profile_out, "w", encoding="utf-8") as file:
            json.dump({
                "noise_band_low_hz": args.noise_band_low_hz,
                "noise_band_high_hz": args.noise_band_high_hz,
                "cpu_columns": cpu_columns,
                "windows": coupling,
            }, file, indent=2)
            file.write("\n")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
