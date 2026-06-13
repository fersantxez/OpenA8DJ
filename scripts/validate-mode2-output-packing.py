#!/usr/bin/env python3
"""Validate OpenA8DJ mode-2 playback packing with synthetic 8-channel frames."""

import argparse
import math
import struct
import sys
from dataclasses import dataclass


STREAMS = 4
CHANNELS_PER_STREAM = 2
CHANNELS = STREAMS * CHANNELS_PER_STREAM
BYTES_PER_SAMPLE = 3
BYTES_PER_SAMPLE_USB = 4
FRAME_BYTES_PER_STREAM = CHANNELS_PER_STREAM * BYTES_PER_SAMPLE
GROUP_BYTES = STREAMS * BYTES_PER_SAMPLE_USB
CHECK_OFFSET = STREAMS * CHANNELS_PER_STREAM
S24_MAX = 8388607
S32_MAX = 2147483647
S32_MIN = -2147483648
DEFAULT_START_BYTE = BYTES_PER_SAMPLE + 1
DEFAULT_TRANSFER_BYTES = 352
STREAM_NAMES = ("A", "B", "C", "D")
SIDE_NAMES = ("left", "right")


def f32(value):
    return struct.unpack("<f", struct.pack("<f", float(value)))[0]


def float_to_output_i24(sample, gain):
    sample = f32(sample)
    sample = f32(sample * f32(gain))
    if sample >= 1.0:
        q31 = S32_MAX
    elif sample <= -1.0:
        q31 = S32_MIN
    else:
        q31 = int(round(f32(sample * f32(S32_MAX))))
    return q31 >> 8


def i24_to_native_bytes(value):
    raw = value & 0xFFFFFF
    return (raw & 0xFF, (raw >> 8) & 0xFF, (raw >> 16) & 0xFF)


def i24_to_big_endian_bytes(value):
    raw = value & 0xFFFFFF
    return ((raw >> 16) & 0xFF, (raw >> 8) & 0xFF, raw & 0xFF)


def native_i24_to_s32(bytes3):
    value = bytes3[0] | (bytes3[1] << 8) | (bytes3[2] << 16)
    if value & 0x800000:
        value -= 1 << 24
    return value


def big_endian_i24_to_s32(bytes3):
    value = (bytes3[0] << 16) | (bytes3[1] << 8) | bytes3[2]
    if value & 0x800000:
        value -= 1 << 24
    return value


def encode_i24(value, byte_order):
    if byte_order == "native":
        return i24_to_native_bytes(value)
    if byte_order == "big":
        return i24_to_big_endian_bytes(value)
    raise ValueError(f"unsupported byte order: {byte_order}")


def decode_i24(bytes3, byte_order):
    if byte_order == "native":
        return native_i24_to_s32(bytes3)
    if byte_order == "big":
        return big_endian_i24_to_s32(bytes3)
    raise ValueError(f"unsupported byte order: {byte_order}")


def mode2_check_byte(stream, byte_index):
    group = byte_index // GROUP_BYTES
    return (stream << 1) | ((~group) & 1)


def synthetic_s24_value(frame_index, channel):
    stream = channel // CHANNELS_PER_STREAM
    side = channel % CHANNELS_PER_STREAM
    magnitude = ((stream + 1) * 1_000_000) + (side * 250_000)
    magnitude += (frame_index % 8192) * 257
    return -magnitude if side else magnitude


def synthetic_frames(frame_count):
    frames = []
    for frame_index in range(frame_count):
        frame = [
            f32(synthetic_s24_value(frame_index, channel) / S24_MAX)
            for channel in range(CHANNELS)
        ]
        frames.append(tuple(frame))
    return frames


def stream_frame_bytes(frame, gain, byte_order):
    streams = []
    for stream in range(STREAMS):
        left = float_to_output_i24(frame[stream * 2], gain)
        right = float_to_output_i24(frame[stream * 2 + 1], gain)
        streams.append(tuple(encode_i24(left, byte_order) + encode_i24(right, byte_order)))
    return streams


class Mode2OutputPacker:
    def __init__(self, frames, start_byte, gain, byte_order):
        self.frames = frames
        self.start_byte = start_byte
        self.gain = gain
        self.byte_order = byte_order
        self.frame_index = 0
        self.output_byte_in_frame = start_byte
        self.output_frame_loaded = False
        self.output_frame_bytes = [(0, 0, 0, 0, 0, 0) for _ in range(STREAMS)]

    def load_next_output_frame_if_needed(self):
        if self.output_frame_loaded and self.output_byte_in_frame != 0:
            return
        if self.frame_index < len(self.frames):
            self.output_frame_bytes = stream_frame_bytes(
                self.frames[self.frame_index], self.gain, self.byte_order
            )
            self.frame_index += 1
        else:
            self.output_frame_bytes = [(0, 0, 0, 0, 0, 0) for _ in range(STREAMS)]
        self.output_frame_loaded = True

    def fill(self, length):
        out = bytearray()
        i = 0
        while i < length:
            if (i % GROUP_BYTES) == CHECK_OFFSET:
                for stream in range(STREAMS):
                    if i >= length:
                        break
                    out.append(mode2_check_byte(stream, i))
                    i += 1
                continue

            self.load_next_output_frame_if_needed()
            for stream in range(STREAMS):
                if i >= length:
                    break
                out.append(self.output_frame_bytes[stream][self.output_byte_in_frame])
                i += 1
            self.output_byte_in_frame += 1
            if self.output_byte_in_frame >= FRAME_BYTES_PER_STREAM:
                self.output_byte_in_frame = 0
        return bytes(out)


@dataclass
class DecodeResult:
    frames: list
    checks: int
    check_errors: int
    panic_flags: int
    sample_bytes: int


def decode_mode2_usb_bytes(data, start_byte, transfer_bytes, byte_order):
    pending = [[None] * FRAME_BYTES_PER_STREAM for _ in range(STREAMS)]
    decoded = []
    checks = 0
    check_errors = 0
    panic_flags = 0
    sample_bytes = 0
    lane_streams = 0
    byte_position = start_byte

    for index, value in enumerate(data):
        local_index = index % transfer_bytes
        group_offset = local_index % GROUP_BYTES
        if CHECK_OFFSET <= group_offset < CHECK_OFFSET + STREAMS:
            stream = group_offset - CHECK_OFFSET
            checks += 1
            if value & 0x80:
                panic_flags += 1
            expected = mode2_check_byte(stream, local_index)
            if (value & 0x3F) != expected:
                check_errors += 1
            continue

        stream = group_offset % STREAMS
        if stream == 0 and byte_position == 0:
            pending = [[None] * FRAME_BYTES_PER_STREAM for _ in range(STREAMS)]
            lane_streams = 0
        pending[stream][byte_position] = value
        sample_bytes += 1
        lane_streams += 1

        if lane_streams == STREAMS:
            if byte_position == FRAME_BYTES_PER_STREAM - 1:
                frame = []
                complete = True
                for stream_bytes in pending:
                    if any(byte is None for byte in stream_bytes):
                        complete = False
                        break
                    frame.append(decode_i24(stream_bytes[:3], byte_order))
                    frame.append(decode_i24(stream_bytes[3:6], byte_order))
                if complete:
                    decoded.append(tuple(frame))
                pending = [[None] * FRAME_BYTES_PER_STREAM for _ in range(STREAMS)]
            byte_position = (byte_position + 1) % FRAME_BYTES_PER_STREAM
            lane_streams = 0

    return DecodeResult(decoded, checks, check_errors, panic_flags, sample_bytes)


def expected_s24_frames(frames, gain):
    return [
        tuple(float_to_output_i24(frame[channel], gain) for channel in range(CHANNELS))
        for frame in frames
    ]


def conversion_self_test(byte_order):
    if byte_order == "native":
        vectors = (
            (0.0, (0x00, 0x00, 0x00), 0),
            (1.0, (0xFF, 0xFF, 0x7F), 0x7FFFFF),
            (-1.0, (0x00, 0x00, 0x80), -0x800000),
        )
    else:
        vectors = (
            (0.0, (0x00, 0x00, 0x00), 0),
            (1.0, (0x7F, 0xFF, 0xFF), 0x7FFFFF),
            (-1.0, (0x80, 0x00, 0x00), -0x800000),
        )
    errors = []
    for sample, expected_bytes, expected_value in vectors:
        quantized = float_to_output_i24(sample, 1.0)
        encoded = encode_i24(quantized, byte_order)
        decoded = decode_i24(encoded, byte_order)
        if encoded != expected_bytes or decoded != expected_value:
            errors.append(
                f"sample={sample} encoded={encoded} decoded={decoded} "
                f"expected_bytes={expected_bytes} expected_value={expected_value}"
            )

    return errors


def pack_until_comparable(frames, start_byte, transfer_bytes, gain, expected_count, byte_order):
    packer = Mode2OutputPacker(frames, start_byte, gain, byte_order)
    packed = bytearray()
    max_transfers = max(4, math.ceil((len(frames) + 8) * 32 / transfer_bytes) + 4)

    for _ in range(max_transfers):
        packed.extend(packer.fill(transfer_bytes))
        decoded = decode_mode2_usb_bytes(packed, start_byte, transfer_bytes, byte_order)
        if len(decoded.frames) >= expected_count:
            return bytes(packed), decoded

    raise RuntimeError("not enough decoded frames after packing synthetic data")


def compare_frames(expected, decoded, source_start_frame, compare_count):
    mismatches = []
    channel_counts = [0] * CHANNELS
    for rel_frame in range(compare_count):
        expected_frame = expected[source_start_frame + rel_frame]
        decoded_frame = decoded[rel_frame]
        for channel in range(CHANNELS):
            channel_counts[channel] += 1
            if expected_frame[channel] != decoded_frame[channel]:
                mismatches.append(
                    {
                        "frame": rel_frame,
                        "source_frame": source_start_frame + rel_frame,
                        "stream": channel // CHANNELS_PER_STREAM,
                        "side": channel % CHANNELS_PER_STREAM,
                        "expected": expected_frame[channel],
                        "decoded": decoded_frame[channel],
                    }
                )
                if len(mismatches) >= 10:
                    return mismatches, channel_counts
    return mismatches, channel_counts


def validate_start_byte(start_byte, args):
    frames = synthetic_frames(args.frames)
    expected = expected_s24_frames(frames, args.gain)
    source_start_frame = 0 if start_byte == 0 else 1
    expected_count = len(expected) - source_start_frame
    if expected_count <= 0:
        raise ValueError("use at least 2 frames when start_byte is not 0")

    packed, decoded = pack_until_comparable(
        frames, start_byte, args.transfer_bytes, args.gain, expected_count, args.byte_order
    )
    compare_count = min(expected_count, len(decoded.frames))
    mismatches, channel_counts = compare_frames(
        expected, decoded.frames, source_start_frame, compare_count
    )
    ok = (
        len(decoded.frames) >= expected_count
        and decoded.check_errors == 0
        and decoded.panic_flags == 0
        and not mismatches
    )
    return {
        "ok": ok,
        "start_byte": start_byte,
        "source_start_frame": source_start_frame,
        "packed_bytes": len(packed),
        "decoded_frames": len(decoded.frames),
        "compared_frames": compare_count,
        "checks": decoded.checks,
        "check_errors": decoded.check_errors,
        "panic_flags": decoded.panic_flags,
        "sample_bytes": decoded.sample_bytes,
        "mismatches": mismatches,
        "channel_counts": channel_counts,
        "first_expected": expected[source_start_frame],
        "first_decoded": decoded.frames[0] if decoded.frames else None,
    }


def parse_start_bytes(value):
    if value == "all":
        return list(range(FRAME_BYTES_PER_STREAM))
    try:
        start_byte = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected 0..5 or 'all'") from exc
    if not 0 <= start_byte < FRAME_BYTES_PER_STREAM:
        raise argparse.ArgumentTypeError("start byte must be between 0 and 5")
    return [start_byte]


def print_result(result, verbose):
    status = "PASS" if result["ok"] else "FAIL"
    print(
        f"{status} start_byte={result['start_byte']} "
        f"source_start_frame={result['source_start_frame']} "
        f"packed_bytes={result['packed_bytes']} "
        f"decoded_frames={result['decoded_frames']} "
        f"compared_frames={result['compared_frames']} "
        f"checks={result['checks']} "
        f"check_errors={result['check_errors']} "
        f"panic_flags={result['panic_flags']}"
    )

    if verbose or not result["ok"]:
        for channel, count in enumerate(result["channel_counts"]):
            stream = channel // CHANNELS_PER_STREAM
            side = channel % CHANNELS_PER_STREAM
            first_expected = result["first_expected"][channel]
            first_decoded = (
                "missing"
                if result["first_decoded"] is None
                else result["first_decoded"][channel]
            )
            print(
                f"  stream={STREAM_NAMES[stream]} channel={SIDE_NAMES[side]} "
                f"matched={count} first_expected_s24={first_expected} "
                f"first_decoded_s24={first_decoded}"
            )
    for mismatch in result["mismatches"]:
        stream = STREAM_NAMES[mismatch["stream"]]
        side = SIDE_NAMES[mismatch["side"]]
        print(
            "  mismatch "
            f"frame={mismatch['frame']} "
            f"source_frame={mismatch['source_frame']} "
            f"stream={stream} channel={side} "
            f"expected_s24={mismatch['expected']} "
            f"decoded_s24={mismatch['decoded']}"
        )


def build_parser():
    parser = argparse.ArgumentParser(
        description=(
            "Generate recognizable Float32 8-channel frames, pack them with the "
            "current mode-2 playback layout, decode the USB bytes, and verify "
            "that every stream/channel sequence round-trips."
        )
    )
    parser.add_argument(
        "--start-byte",
        default=str(DEFAULT_START_BYTE),
        type=parse_start_bytes,
        help="initial mode-2 output byte cursor: 0..5, or 'all' (default: 4)",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=64,
        help="synthetic source frames to generate (default: 64)",
    )
    parser.add_argument(
        "--transfer-bytes",
        type=int,
        default=DEFAULT_TRANSFER_BYTES,
        help="bytes passed to each simulated fillPlaybackBytes call (default: 352)",
    )
    parser.add_argument(
        "--gain",
        type=float,
        default=1.0,
        help="output gain applied before 24-bit quantization (default: 1.0)",
    )
    parser.add_argument(
        "--byte-order",
        choices=("big", "native"),
        default="big",
        help="24-bit sample byte order to validate (default: big)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="print per-stream/channel first-sample summaries",
    )
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.frames <= 0:
        parser.error("--frames must be positive")
    if args.transfer_bytes <= 0 or args.transfer_bytes % GROUP_BYTES != 0:
        parser.error(f"--transfer-bytes must be a positive multiple of {GROUP_BYTES}")

    conversion_errors = conversion_self_test(args.byte_order)
    if conversion_errors:
        print("FAIL conversion_vectors")
        for error in conversion_errors:
            print(f"  {error}")
        return 1
    print(f"PASS conversion_vectors {args.byte_order}_i24_output")

    all_ok = True
    for start_byte in args.start_byte:
        try:
            result = validate_start_byte(start_byte, args)
        except (RuntimeError, ValueError) as exc:
            print(f"FAIL start_byte={start_byte} error={exc}")
            all_ok = False
            continue
        print_result(result, args.verbose)
        all_ok = all_ok and result["ok"]
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
