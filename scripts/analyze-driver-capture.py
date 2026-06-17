#!/usr/bin/env python3
import argparse
import json
import math
import os
import re
import sys
import wave
from array import array
from collections import Counter, defaultdict


DEFAULT_META_NAME = "opena8dj-output-capture.txt"
DEFAULT_WRITTEN_NAME = "opena8dj-output-written-f32.raw"
DEFAULT_CONSUMED_NAME = "opena8dj-output-consumed-f32.raw"

USB_STREAMS = 4
USB_CHANNELS_PER_STREAM = 2
USB_BYTES_PER_SAMPLE = 3
USB_BYTES_PER_SAMPLE_ALIGNED = 4
USB_GROUP_BYTES = USB_STREAMS * USB_BYTES_PER_SAMPLE_ALIGNED
USB_FRAME_BYTES_PER_STREAM = USB_CHANNELS_PER_STREAM * USB_BYTES_PER_SAMPLE


def warn(message):
    print(f"warning={message}", file=sys.stderr)


def decode_pcm_sample(raw, width):
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
        frames = wav.readframes(wav.getnframes())
    pair = []
    step = channels * width
    for offset in range(0, len(frames), step):
        left = decode_pcm_sample(frames[offset:offset + width], width)
        if channels > 1:
            right_pos = offset + width
            right = decode_pcm_sample(frames[right_pos:right_pos + width], width)
        else:
            right = left
        pair.append((left, right))
    return rate, pair


def read_raw_pair(path, pair_index, channels=8):
    with open(path, "rb") as file:
        data = file.read()
    if len(data) < 4:
        return [], 0
    if len(data) % 4 != 0:
        warn(f"{path}:ignored_trailing_bytes={len(data) % 4}")
    usable_bytes = (len(data) // 4) * 4
    floats = array("f")
    floats.frombytes(data[:usable_bytes])
    if sys.byteorder != "little":
        floats.byteswap()
    frame_count = len(floats) // channels
    if len(floats) % channels != 0:
        warn(f"{path}:ignored_trailing_floats={len(floats) % channels}")
    out = []
    left = pair_index * 2
    right = left + 1
    if channels <= 0 or right >= channels:
        raise SystemExit(f"pair {pair_index} needs channels >= {right + 1}; got {channels}")
    for offset in range(0, frame_count * channels, channels):
        out.append((floats[offset + left], floats[offset + right]))
    return out, frame_count


def read_meta(path):
    meta = {}
    if path is None or not os.path.exists(path):
        return meta
    with open(path, "r", encoding="utf-8", errors="replace") as file:
        for line in file:
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            meta[key.strip()] = value.strip()
    return meta


def meta_int(meta, key, default):
    try:
        return int(meta.get(key, default))
    except (TypeError, ValueError):
        return default


def mono_abs(pair):
    return [max(abs(left), abs(right)) for left, right in pair]


def first_signal_index(pair):
    envelope = mono_abs(pair)
    if not envelope:
        return 0
    peak = max(envelope)
    threshold = max(0.0005, peak * 0.02)
    for index, value in enumerate(envelope):
        if value >= threshold:
            return index
    return 0


def pair_to_mono(pair):
    return [0.5 * (left + right) for left, right in pair]


def pair_name_to_index(text):
    pair = text.upper()
    if pair in ("A", "B", "C", "D"):
        return ord(pair) - ord("A")
    pair_index = int(pair)
    if pair_index < 0:
        raise SystemExit("pair must be A-D or a non-negative integer")
    return pair_index


def score_lag(ref_mono, got_mono, ref_start, got_start, lag, sample_count, stride):
    score = 0.0
    ref_energy = 0.0
    got_energy = 0.0
    used = 0
    ref_len = len(ref_mono)
    got_len = len(got_mono)
    for n in range(0, sample_count, max(1, stride)):
        ri = ref_start + n
        gi = got_start + n + lag
        if ri < 0 or gi < 0 or ri >= ref_len or gi >= got_len:
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
    if sample_count <= 0:
        return 0, 0.0
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
    if max_lag <= 512:
        return scan_lags(ref_mono,
                         got_mono,
                         ref_start,
                         got_start,
                         -max_lag,
                         max_lag,
                         1,
                         sample_count,
                         max(1, stride))

    coarse_step = max(8, max_lag // 2048)
    coarse_lag, _coarse_score = scan_lags(ref_mono,
                                          got_mono,
                                          ref_start,
                                          got_start,
                                          -max_lag,
                                          max_lag,
                                          coarse_step,
                                          sample_count,
                                          max(max(1, stride), coarse_step))
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
                     max(1, stride))


def normalize_starts(ref_start, got_start):
    if got_start < 0:
        ref_start += -got_start
        got_start = 0
    if ref_start < 0:
        got_start += -ref_start
        ref_start = 0
    return ref_start, got_start


def rms(values):
    if not values:
        return 0.0
    return math.sqrt(sum(value * value for value in values) / len(values))


def compare_channel(ref, got):
    dot = sum(r * g for r, g in zip(ref, got))
    ref_power = sum(r * r for r in ref)
    gain = dot / ref_power if ref_power > 0 else 0.0
    residual = [g - gain * r for r, g in zip(ref, got)]
    signal = [gain * r for r in ref]
    signal_rms = rms(signal)
    residual_rms = rms(residual)
    snr = 20.0 * math.log10(signal_rms / residual_rms) if residual_rms > 0 and signal_rms > 0 else 999.0
    peak_error = max((abs(value) for value in residual), default=0.0)
    click_threshold = max(0.01, residual_rms * 12.0)
    click_outliers = sum(1 for value in residual if abs(value) > click_threshold)
    return gain, signal_rms, residual_rms, snr, peak_error, click_outliers


def compare_pair(ref, got, rate, max_seconds, max_lag):
    ref_start = first_signal_index(ref)
    got_start = first_signal_index(got)
    ref_mono = pair_to_mono(ref)
    got_mono = pair_to_mono(got)
    coarse_lag, coarse_score = find_best_lag(ref_mono,
                                             got_mono,
                                             ref_start,
                                             got_start,
                                             max_lag,
                                             min(len(ref) - ref_start, int(rate * 1)),
                                             128)
    fine_lag, fine_score = find_best_lag(ref_mono,
                                         got_mono,
                                         ref_start,
                                         got_start + coarse_lag,
                                         min(512, max_lag),
                                         min(len(ref) - ref_start, int(rate * 0.5)),
                                         16)
    alignment_lag = coarse_lag + fine_lag
    got_start += alignment_lag
    ref_start, got_start = normalize_starts(ref_start, got_start)
    max_frames = int(max_seconds * rate) if max_seconds > 0 else len(ref)
    usable = min(len(ref) - ref_start, len(got) - got_start, max_frames)
    if usable <= max(1, rate // 2):
        raise ValueError(f"not enough aligned audio: usable={usable}")

    ref_window = ref[ref_start:ref_start + usable]
    got_window = got[got_start:got_start + usable]
    ref_left = [sample[0] for sample in ref_window]
    ref_right = [sample[1] for sample in ref_window]
    got_left = [sample[0] for sample in got_window]
    got_right = [sample[1] for sample in got_window]

    return {
        "reference_start": ref_start,
        "capture_start": got_start,
        "alignment_lag": alignment_lag,
        "alignment_score": fine_score if fine_score != 0.0 else coarse_score,
        "compared_frames": usable,
        "left": compare_channel(ref_left, got_left),
        "right": compare_channel(ref_right, got_right),
    }


def lag_profile(ref, got, rate, ref_start, got_start, usable, max_lag, window_seconds, hop_seconds):
    window = max(64, int(rate * window_seconds))
    hop = max(1, int(rate * hop_seconds))
    if usable < window:
        return []
    ref_mono = pair_to_mono(ref)
    got_mono = pair_to_mono(got)
    stride = max(1, window // 2048)
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


def compare_two_captures(left_capture, right_capture, rate, max_seconds, max_lag, window_seconds, hop_seconds):
    result = compare_pair(left_capture["pair"], right_capture["pair"], rate, max_seconds, max_lag)
    points = lag_profile(left_capture["pair"],
                         right_capture["pair"],
                         rate,
                         result["reference_start"],
                         result["capture_start"],
                         result["compared_frames"],
                         min(max_lag, 256),
                         window_seconds,
                         hop_seconds)
    lags = [point[1] for point in points]
    scores = [point[2] for point in points]
    jumps = sum(1 for prev, current in zip(lags, lags[1:]) if abs(current - prev) > 2)
    result.update({
        "lag_windows": len(points),
        "lag_min": min(lags) if lags else 0,
        "lag_max": max(lags) if lags else 0,
        "lag_first": lags[0] if lags else 0,
        "lag_last": lags[-1] if lags else 0,
        "lag_jumps_gt_2_frames": jumps,
        "lag_score_min": min(scores) if scores else 0.0,
        "lag_score_median": median(scores) if scores else 0.0,
    })
    return result


def median(values):
    if not values:
        return 0.0
    ordered = sorted(values)
    midpoint = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[midpoint]
    return 0.5 * (ordered[midpoint - 1] + ordered[midpoint])


def print_channel(prefix, channel_name, values):
    print(f"{prefix}{channel_name}_gain={values[0]:.8f}")
    print(f"{prefix}{channel_name}_signal_rms={values[1]:.8f}")
    print(f"{prefix}{channel_name}_residual_rms={values[2]:.8f}")
    print(f"{prefix}{channel_name}_snr_db={values[3]:.2f}")
    print(f"{prefix}{channel_name}_peak_error={values[4]:.8f}")
    print(f"{prefix}{channel_name}_click_outliers={values[5]}")


def print_comparison(prefix, result):
    print(f"{prefix}reference_start={result['reference_start']}")
    print(f"{prefix}capture_start={result['capture_start']}")
    print(f"{prefix}alignment_lag={result['alignment_lag']}")
    print(f"{prefix}alignment_score={result['alignment_score']:.6f}")
    print(f"{prefix}compared_frames={result['compared_frames']}")
    print_channel(prefix, "left", result["left"])
    print_channel(prefix, "right", result["right"])
    if "lag_windows" in result:
        print(f"{prefix}lag_windows={result['lag_windows']}")
        print(f"{prefix}lag_min={result['lag_min']}")
        print(f"{prefix}lag_max={result['lag_max']}")
        print(f"{prefix}lag_first={result['lag_first']}")
        print(f"{prefix}lag_last={result['lag_last']}")
        print(f"{prefix}lag_jumps_gt_2_frames={result['lag_jumps_gt_2_frames']}")
        print(f"{prefix}lag_score_min={result['lag_score_min']:.6f}")
        print(f"{prefix}lag_score_median={result['lag_score_median']:.6f}")


def s24be_to_float(bytes3):
    value = (bytes3[0] << 16) | (bytes3[1] << 8) | bytes3[2]
    if value & 0x800000:
        value |= ~0xffffff
    return value / 8388608.0


def s24native_to_float(bytes3):
    value = bytes3[0] | (bytes3[1] << 8) | (bytes3[2] << 16)
    if value & 0x800000:
        value |= ~0xffffff
    return value / 8388608.0


def s24_to_float(bytes3, byte_order):
    if byte_order == "native":
        return s24native_to_float(bytes3)
    return s24be_to_float(bytes3)


def mode2_check_byte(stream, byte_index):
    group = byte_index // USB_GROUP_BYTES
    return (stream << 1) | ((~group) & 1)


def score_mode2_check_bytes(data, check_offset):
    checks = 0
    errors = 0
    panic_flags = 0
    for index, value in enumerate(data):
        group_offset = index % USB_GROUP_BYTES
        if check_offset <= group_offset < check_offset + USB_STREAMS:
            stream = group_offset - check_offset
            checks += 1
            if value & 0x80:
                panic_flags += 1
            group = index // USB_GROUP_BYTES
            if group >= 4 and (value & 0x3f) != mode2_check_byte(stream, index):
                errors += 1
    return checks, errors, panic_flags


def decode_mode2_usb_bytes(data, check_offset, start_byte, byte_order):
    pending = [[None] * USB_FRAME_BYTES_PER_STREAM for _ in range(USB_STREAMS)]
    decoded = []
    checks = 0
    check_errors = 0
    panic_flags = 0
    sample_bytes = 0
    lane_streams = 0
    byte_position = start_byte
    for index, value in enumerate(data):
        group_offset = index % USB_GROUP_BYTES
        if check_offset <= group_offset < check_offset + USB_STREAMS:
            stream = group_offset - check_offset
            checks += 1
            if value & 0x80:
                panic_flags += 1
            group = index // USB_GROUP_BYTES
            if group >= 4 and (value & 0x3f) != mode2_check_byte(stream, index):
                check_errors += 1
            continue

        stream = group_offset % USB_STREAMS
        if stream == 0 and byte_position == 0:
            pending = [[None] * USB_FRAME_BYTES_PER_STREAM for _ in range(USB_STREAMS)]
            lane_streams = 0
        pending[stream][byte_position] = value
        sample_bytes += 1
        lane_streams += 1
        if lane_streams == USB_STREAMS:
            if byte_position == USB_FRAME_BYTES_PER_STREAM - 1:
                frame = []
                complete = True
                for stream_bytes in pending:
                    if any(byte is None for byte in stream_bytes):
                        complete = False
                        break
                    frame.append(s24_to_float(stream_bytes[:3], byte_order))
                    frame.append(s24_to_float(stream_bytes[3:6], byte_order))
                if complete:
                    decoded.append(tuple(frame))
                pending = [[None] * USB_FRAME_BYTES_PER_STREAM for _ in range(USB_STREAMS)]
            byte_position = (byte_position + 1) % USB_FRAME_BYTES_PER_STREAM
            lane_streams = 0
    return {
        "pair_frames": decoded,
        "checks": checks,
        "check_errors": check_errors,
        "panic_flags": panic_flags,
        "sample_bytes": sample_bytes,
    }


def select_usb_decode(data, args, ref, rate, pair_index):
    if args.usb_check_offset == "auto":
        candidates = []
        for offset in (0, 8):
            checks, errors, panic = score_mode2_check_bytes(data, offset)
            rate_value = errors / checks if checks else 1.0
            candidates.append((rate_value, errors, -checks, panic, offset))
        check_offset = sorted(candidates)[0][4]
    else:
        check_offset = int(args.usb_check_offset)

    if args.usb_start_byte == "auto":
        start_candidates = range(USB_FRAME_BYTES_PER_STREAM)
        selection_data = data[:max(USB_GROUP_BYTES, args.usb_auto_scan_bytes)]
    else:
        start_candidates = (int(args.usb_start_byte),)
        selection_data = data

    byte_order_candidates = ("big", "native") if args.usb_byte_order == "auto" else (args.usb_byte_order,)
    best = None
    for byte_order in byte_order_candidates:
        for start_byte in start_candidates:
            decoded = decode_mode2_usb_bytes(selection_data, check_offset, start_byte, byte_order)
            pair = [(frame[pair_index * 2], frame[pair_index * 2 + 1])
                    for frame in decoded["pair_frames"]
                    if pair_index * 2 + 1 < len(frame)]
            decoded["pair"] = pair
            score = -1.0
            if len(pair) > rate // 2:
                try:
                    comparison = compare_pair(
                        ref,
                        pair,
                        rate,
                        args.usb_compare_seconds,
                        args.max_lag,
                    )
                    score = comparison["alignment_score"]
                    decoded["comparison"] = comparison
                except ValueError:
                    pass
            default_bonus = 0.001 if ((check_offset == 0 and start_byte == 0) or
                                      (check_offset == 8 and start_byte == 4)) else 0.0
            check_penalty = decoded["check_errors"] / decoded["checks"] if decoded["checks"] else 1.0
            rank = (score + default_bonus) - check_penalty
            if best is None or rank > best[0]:
                best = (rank, start_byte, byte_order)

    start_byte = best[1]
    byte_order = best[2]
    decoded = decode_mode2_usb_bytes(data, check_offset, start_byte, byte_order)
    decoded["byte_order"] = byte_order
    pair = [(frame[pair_index * 2], frame[pair_index * 2 + 1])
            for frame in decoded["pair_frames"]
            if pair_index * 2 + 1 < len(frame)]
    decoded["pair"] = pair
    if len(pair) > rate // 2:
        try:
            decoded["comparison"] = compare_pair(
                ref,
                pair,
                rate,
                args.usb_compare_seconds,
                args.max_lag,
            )
        except ValueError:
            pass
    return check_offset, start_byte, decoded


def print_usb_analysis(path, data, check_offset, start_byte, decoded):
    checks = decoded["checks"]
    errors = decoded["check_errors"]
    print(f"usb_raw={path}")
    print(f"usb_bytes={len(data)}")
    print(f"usb_check_offset={check_offset}")
    print(f"usb_start_byte={start_byte}")
    print(f"usb_byte_order={decoded.get('byte_order', 'big')}")
    print(f"usb_check_bytes={checks}")
    print(f"usb_check_errors={errors}")
    print(f"usb_check_error_rate={(errors / checks if checks else 0.0):.8f}")
    print(f"usb_panic_flags={decoded['panic_flags']}")
    print(f"usb_sample_bytes={decoded['sample_bytes']}")
    print(f"usb_decoded_frames={len(decoded['pair_frames'])}")


def parse_event_line(line):
    stripped = line.strip()
    if not stripped:
        return None
    tab_fields = stripped.split("\t")
    if len(tab_fields) >= 9:
        if tab_fields[0] == "index" and tab_fields[1] == "type":
            return None
        return {
            "index": tab_fields[0],
            "type": tab_fields[1],
            "timeline": tab_fields[2],
            "host_time": tab_fields[3],
            "frame_number": tab_fields[4],
            "count": tab_fields[5],
            "flags": tab_fields[6],
            "value": tab_fields[7],
            "extra": tab_fields[8],
        }
    if len(tab_fields) >= 8:
        if tab_fields[0] == "index" and tab_fields[1] == "type":
            return None
        return {
            "index": tab_fields[0],
            "type": tab_fields[1],
            "host_time": tab_fields[2],
            "frame_number": tab_fields[3],
            "count": tab_fields[4],
            "flags": tab_fields[5],
            "value": tab_fields[6],
            "extra": tab_fields[7],
        }
    if stripped.startswith("{"):
        try:
            parsed = json.loads(stripped)
            if isinstance(parsed, dict):
                return parsed
        except json.JSONDecodeError:
            pass
    fields = {}
    for key, value in re.findall(r"([A-Za-z_][A-Za-z0-9_.-]*)=([^ \t]+)", stripped):
        fields[key] = value.rstrip(",")
    if fields:
        return fields
    return {"event": stripped.split()[0], "raw": stripped}


def maybe_number(value):
    try:
        if isinstance(value, (int, float)):
            return float(value)
        text = str(value)
        if text.startswith(("0x", "0X")):
            return float(int(text, 16))
        return float(text)
    except (TypeError, ValueError):
        return None


def print_event_summary(path):
    event_counts = Counter()
    numeric = defaultdict(list)
    total = 0
    with open(path, "r", encoding="utf-8", errors="replace") as file:
        for line in file:
            event = parse_event_line(line)
            if event is None:
                continue
            total += 1
            event_type = event.get("event") or event.get("type") or event.get("name") or "event"
            event_counts[str(event_type)] += 1
            for key, value in event.items():
                number = maybe_number(value)
                if number is not None:
                    numeric[key].append(number)
    print(f"events={path}")
    print(f"events_lines={total}")
    for event_type, count in event_counts.most_common(12):
        safe_type = re.sub(r"[^A-Za-z0-9_.-]+", "_", event_type)
        print(f"events_type_{safe_type}={count}")
    for key in sorted(numeric)[:16]:
        values = numeric[key]
        safe_key = re.sub(r"[^A-Za-z0-9_.-]+", "_", key)
        print(f"events_{safe_key}_min={min(values):.6f}")
        print(f"events_{safe_key}_max={max(values):.6f}")


def resolve_capture_paths(args):
    meta_path = args.meta
    written_path = args.written_raw
    consumed_path = args.consumed_raw
    if args.capture_dir:
        if meta_path is None:
            meta_path = os.path.join(args.capture_dir, DEFAULT_META_NAME)
        if written_path is None:
            candidate = os.path.join(args.capture_dir, DEFAULT_WRITTEN_NAME)
            if os.path.exists(candidate):
                written_path = candidate
        if consumed_path is None:
            candidate = os.path.join(args.capture_dir, DEFAULT_CONSUMED_NAME)
            if os.path.exists(candidate):
                consumed_path = candidate
    return meta_path, written_path, consumed_path


def main():
    parser = argparse.ArgumentParser(description="Compare a source WAV against OpenA8DJ diagnostic captures.")
    parser.add_argument("reference_wav")
    parser.add_argument("capture_raw", nargs="?", help="legacy single f32le-interleaved raw capture")
    parser.add_argument("--capture-dir", help="directory containing OpenA8DJ diagnostic capture files")
    parser.add_argument("--meta", help="capture metadata key=value file")
    parser.add_argument("--written-raw", help="f32le raw frames written by Core Audio into the USB engine")
    parser.add_argument("--consumed-raw", help="f32le raw frames consumed by the USB output packer")
    parser.add_argument("--usb-raw", help="optional packed mode-2 USB bytes")
    parser.add_argument("--usb-check-offset", default="auto", choices=("auto", "0", "8"))
    parser.add_argument("--usb-start-byte", default="auto", choices=("auto", "0", "1", "2", "3", "4", "5"))
    parser.add_argument("--usb-byte-order", default="big", choices=("auto", "big", "native"))
    parser.add_argument("--usb-auto-scan-bytes", type=int, default=2 * 1024 * 1024)
    parser.add_argument("--usb-compare-seconds", type=float, default=2.0)
    parser.add_argument("--events", help="optional JSONL or key=value diagnostic event log")
    parser.add_argument("--pair", default="A")
    parser.add_argument("--channels", type=int, default=8)
    parser.add_argument("--max-seconds", type=float, default=20.0)
    parser.add_argument("--max-lag", type=int, default=2048)
    parser.add_argument("--lag-window-seconds", type=float, default=0.5)
    parser.add_argument("--lag-hop-seconds", type=float, default=0.25)
    args = parser.parse_args()

    pair_index = pair_name_to_index(args.pair)
    if pair_index > 3:
        raise SystemExit("pair must be A-D or 0-3 for OpenA8DJ captures")

    rate, ref = read_wav_pair(args.reference_wav)
    meta_path, written_path, consumed_path = resolve_capture_paths(args)
    meta = read_meta(meta_path)
    channels = meta_int(meta, "channels", args.channels)
    if channels != args.channels and (args.written_raw or args.consumed_raw or args.capture_dir):
        warn(f"using_channels_from_meta={channels}")

    captures = []
    if args.capture_raw is not None:
        captures.append(("capture", args.capture_raw))
    if written_path is not None:
        captures.append(("written", written_path))
    if consumed_path is not None:
        captures.append(("consumed", consumed_path))
    if not captures and args.usb_raw is None and args.events is None:
        raise SystemExit("provide capture_raw, --capture-dir, --written-raw, --consumed-raw, --usb-raw, or --events")

    print(f"reference={args.reference_wav}")
    print(f"rate={rate}")
    print(f"pair={chr(ord('A') + pair_index)}")
    print(f"reference_frames={len(ref)}")
    print(f"channels={channels}")
    if meta_path is not None:
        print(f"meta={meta_path}")
        for key in sorted(meta):
            safe_key = re.sub(r"[^A-Za-z0-9_.-]+", "_", key)
            print(f"meta_{safe_key}={meta[key]}")

    loaded = {}
    legacy_single_capture = len(captures) == 1 and captures[0][0] == "capture"
    for name, path in captures:
        pair, frame_count = read_raw_pair(path, pair_index, channels)
        loaded[name] = {"path": path, "pair": pair, "frames": frame_count}
        prefix = "" if legacy_single_capture else f"{name}_"
        print(f"{prefix}capture={path}")
        print(f"{prefix}capture_frames={frame_count}")
        try:
            result = compare_pair(ref, pair, rate, args.max_seconds, args.max_lag)
            print_comparison(prefix, result)
        except ValueError as exc:
            print(f"{prefix}comparison_error={exc}")

    if "written" in loaded and "consumed" in loaded:
        try:
            result = compare_two_captures(loaded["written"],
                                          loaded["consumed"],
                                          rate,
                                          args.max_seconds,
                                          args.max_lag,
                                          args.lag_window_seconds,
                                          args.lag_hop_seconds)
            print_comparison("written_consumed_", result)
        except ValueError as exc:
            print(f"written_consumed_comparison_error={exc}")

    if args.usb_raw is not None:
        with open(args.usb_raw, "rb") as file:
            usb_data = file.read()
        check_offset, start_byte, decoded = select_usb_decode(usb_data, args, ref, rate, pair_index)
        print_usb_analysis(args.usb_raw, usb_data, check_offset, start_byte, decoded)
        if decoded.get("comparison") is not None:
            print_comparison("usb_", decoded["comparison"])

    if args.events is not None:
        print_event_summary(args.events)


if __name__ == "__main__":
    main()
