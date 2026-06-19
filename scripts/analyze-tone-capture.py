#!/usr/bin/env python3
import argparse
import math
import statistics
import wave


def read_wav_mono(path):
    with wave.open(path, "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())
    if width != 2:
        raise SystemExit(f"unsupported sample width: {width * 8} bits")
    samples = []
    scale = 32768.0
    step = channels * width
    for offset in range(0, len(frames), step):
        acc = 0.0
        for channel in range(channels):
            pos = offset + channel * width
            raw = int.from_bytes(frames[pos:pos + width], "little", signed=True)
            acc += raw / scale
        samples.append(acc / max(1, channels))
    return rate, samples


def rms(values):
    if not values:
        return 0.0
    return math.sqrt(sum(value * value for value in values) / len(values))


def choose_active_window(samples, rate, window_seconds=0.25, threshold_ratio=0.35, edge_trim_seconds=0.25):
    window_size = max(1, int(window_seconds * rate))
    if len(samples) < window_size:
        return 0, len(samples), 0.0

    windows = []
    for start in range(0, len(samples), window_size):
        end = min(len(samples), start + window_size)
        value = rms(samples[start:end])
        windows.append((start, end, value))
    max_rms = max((value for _start, _end, value in windows), default=0.0)
    if max_rms <= 0.0:
        return 0, len(samples), 0.0

    threshold = max_rms * threshold_ratio
    best_start = 0
    best_end = 0
    current_start = None
    current_end = None
    for start, end, value in windows:
        if value >= threshold:
            if current_start is None:
                current_start = start
            current_end = end
        elif current_start is not None:
            if current_end - current_start > best_end - best_start:
                best_start, best_end = current_start, current_end
            current_start = None
            current_end = None
    if current_start is not None and current_end - current_start > best_end - best_start:
        best_start, best_end = current_start, current_end

    if best_end <= best_start:
        return 0, len(samples), threshold

    edge_trim = int(edge_trim_seconds * rate)
    trimmed_start = min(best_end, best_start + edge_trim)
    trimmed_end = max(trimmed_start, best_end - edge_trim)
    if trimmed_end - trimmed_start >= rate:
        return trimmed_start, trimmed_end, threshold
    return best_start, best_end, threshold


def dbfs(value):
    if value <= 0.0:
        return -240.0
    return 20.0 * math.log10(value)


def fit_sine_power(samples, rate, frequency):
    if not samples:
        return 0.0, 0.0, 0.0
    sin_sum = 0.0
    cos_sum = 0.0
    for index, sample in enumerate(samples):
        phase = 2.0 * math.pi * frequency * index / rate
        sin_sum += sample * math.sin(phase)
        cos_sum += sample * math.cos(phase)
    a = 2.0 * sin_sum / len(samples)
    b = 2.0 * cos_sum / len(samples)
    amplitude = math.sqrt(a * a + b * b)
    power = (amplitude * amplitude) / 2.0
    return amplitude, power, math.atan2(b, a)


def click_score(samples):
    if len(samples) < 3:
        return 0, 0.0
    deltas = [abs(samples[i] - samples[i - 1]) for i in range(1, len(samples))]
    med = statistics.median(deltas)
    mad = statistics.median([abs(delta - med) for delta in deltas]) or 1e-9
    threshold = med + 18.0 * mad
    outliers = sum(1 for delta in deltas if delta > threshold and delta > 0.015)
    return outliers, threshold


def sideband_metrics(samples, rate, frequency, spacing, count, fundamental_amp):
    strongest = {
        "frequency": 0.0,
        "amp": 0.0,
        "relative_db": -240.0,
    }
    total_power = 0.0
    rows = []
    for index in range(1, count + 1):
        for sign in (-1, 1):
            candidate = frequency + sign * spacing * index
            if candidate <= 0.0 or candidate >= rate / 2:
                continue
            amplitude, power, _phase = fit_sine_power(samples, rate, candidate)
            relative_db = 20.0 * math.log10(amplitude / fundamental_amp) if fundamental_amp > 0.0 and amplitude > 0.0 else -240.0
            row = {
                "frequency": candidate,
                "amp": amplitude,
                "relative_db": relative_db,
            }
            rows.append(row)
            total_power += power
            if amplitude > strongest["amp"]:
                strongest = row
    sideband_rms = math.sqrt(total_power)
    return rows, strongest, sideband_rms


def percentile(sorted_values, percentile_value):
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return sorted_values[0]
    rank = (len(sorted_values) - 1) * percentile_value
    lower = int(math.floor(rank))
    upper = int(math.ceil(rank))
    if lower == upper:
        return sorted_values[lower]
    weight = rank - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def segment_metrics(samples, rate, frequency, spacing, count, segment_seconds=1.0, hop_seconds=1.0):
    segment_size = max(1, int(segment_seconds * rate))
    hop_size = max(1, int(hop_seconds * rate))
    if len(samples) < segment_size:
        return {}

    ratios = []
    strongest_values = []
    clicks_per_second = []
    dirty_segments = 0
    segment_count = 0
    for start in range(0, len(samples) - segment_size + 1, hop_size):
        segment = samples[start:start + segment_size]
        fundamental_amp, _power, _phase = fit_sine_power(segment, rate, frequency)
        if fundamental_amp <= 0.0:
            continue
        _rows, strongest, sideband_rms = sideband_metrics(segment,
                                                          rate,
                                                          frequency,
                                                          spacing,
                                                          count,
                                                          fundamental_amp)
        ratio = sideband_rms / fundamental_amp
        ratios.append(ratio)
        strongest_values.append(strongest["relative_db"])
        clicks, _threshold = click_score(segment)
        clicks_per_second.append(clicks / segment_seconds)
        if ratio > 0.08 or strongest["relative_db"] > -25.0:
            dirty_segments += 1
        segment_count += 1

    if not ratios:
        return {}
    ratios.sort()
    strongest_sorted = sorted(strongest_values)
    clicks_sorted = sorted(clicks_per_second)
    return {
        "segment_count": segment_count,
        "segment_seconds": segment_seconds,
        "segment_sideband_ratio_min": ratios[0],
        "segment_sideband_ratio_median": statistics.median(ratios),
        "segment_sideband_ratio_p95": percentile(ratios, 0.95),
        "segment_sideband_ratio_max": ratios[-1],
        "segment_strongest_sideband_relative_db_median": statistics.median(strongest_values),
        "segment_strongest_sideband_relative_db_p95": percentile(strongest_sorted, 0.95),
        "segment_strongest_sideband_relative_db_max": strongest_sorted[-1],
        "segment_clicks_per_second_median": statistics.median(clicks_per_second),
        "segment_clicks_per_second_p95": percentile(clicks_sorted, 0.95),
        "segment_clicks_per_second_max": clicks_sorted[-1],
        "segment_dirty_count": dirty_segments,
        "segment_dirty_seconds": dirty_segments * segment_seconds,
    }


def main():
    parser = argparse.ArgumentParser(description="Analyze a microphone recording of a pure tone playback test.")
    parser.add_argument("wav")
    parser.add_argument("--frequency", type=float, default=1000.0)
    parser.add_argument("--sideband-spacing", type=float, default=60.0)
    parser.add_argument("--sideband-count", type=int, default=5)
    parser.add_argument("--trim-start", type=float, default=1.0)
    parser.add_argument("--trim-end", type=float, default=0.5)
    parser.add_argument("--auto-window", action="store_true",
                        help="Analyze the detected active tone region instead of fixed start/end trims.")
    args = parser.parse_args()

    rate, samples = read_wav_mono(args.wav)
    active_threshold = 0.0
    if args.auto_window:
        start, end, active_threshold = choose_active_window(samples, rate)
    else:
        start = min(len(samples), int(args.trim_start * rate))
        end = max(start, len(samples) - int(args.trim_end * rate))
    window = samples[start:end]
    if len(window) < rate:
        raise SystemExit("recording is too short after trimming")

    total_power = rms(window) ** 2
    explained_power = 0.0
    fundamental_amp = 0.0
    for harmonic in range(1, 7):
        frequency = args.frequency * harmonic
        if frequency >= rate / 2:
            break
        amplitude, power, _phase = fit_sine_power(window, rate, frequency)
        if harmonic == 1:
            fundamental_amp = amplitude
        explained_power += power
    residual_power = max(0.0, total_power - explained_power)
    residual_rms = math.sqrt(residual_power)
    total_rms = math.sqrt(total_power)
    residual_ratio = residual_rms / total_rms if total_rms > 0 else 0.0
    outliers, delta_threshold = click_score(window)
    peak = max((abs(sample) for sample in window), default=0.0)
    sidebands, strongest_sideband, sideband_rms = sideband_metrics(window,
                                                                  rate,
                                                                  args.frequency,
                                                                  args.sideband_spacing,
                                                                  args.sideband_count,
                                                                  fundamental_amp)
    segments = segment_metrics(window,
                               rate,
                               args.frequency,
                               args.sideband_spacing,
                               args.sideband_count)

    print(f"path={args.wav}")
    print(f"rate={rate}")
    print(f"samples={len(window)}")
    print(f"window_mode={'auto' if args.auto_window else 'fixed'}")
    print(f"window_start_seconds={start / rate:.6f}")
    print(f"window_end_seconds={end / rate:.6f}")
    if args.auto_window:
        print(f"active_rms_threshold={active_threshold:.8f}")
    print(f"rms={total_rms:.8f}")
    print(f"peak={peak:.8f}")
    print(f"fundamental_amp={fundamental_amp:.8f}")
    print(f"fundamental_dbfs={dbfs(fundamental_amp):.2f}")
    print(f"residual_rms={residual_rms:.8f}")
    print(f"residual_ratio={residual_ratio:.6f}")
    print(f"sideband_spacing_hz={args.sideband_spacing:.2f}")
    print(f"sideband_count={args.sideband_count}")
    print(f"sideband_rms={sideband_rms:.8f}")
    print(f"sideband_rms_dbfs={dbfs(sideband_rms):.2f}")
    print(f"sideband_ratio={sideband_rms / fundamental_amp if fundamental_amp > 0.0 else 0.0:.6f}")
    print(f"strongest_sideband_hz={strongest_sideband['frequency']:.2f}")
    print(f"strongest_sideband_amp={strongest_sideband['amp']:.8f}")
    print(f"strongest_sideband_relative_db={strongest_sideband['relative_db']:.2f}")
    for index, sideband in enumerate(sorted(sidebands, key=lambda row: row["amp"], reverse=True)[:8], 1):
        print(f"sideband_{index}_hz={sideband['frequency']:.2f}")
        print(f"sideband_{index}_amp={sideband['amp']:.8f}")
        print(f"sideband_{index}_relative_db={sideband['relative_db']:.2f}")
    print(f"click_outliers={outliers}")
    print(f"delta_threshold={delta_threshold:.8f}")
    for key, value in segments.items():
        if isinstance(value, float):
            print(f"{key}={value:.6f}")
        else:
            print(f"{key}={value}")


if __name__ == "__main__":
    main()
