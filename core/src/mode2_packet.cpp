#include "opena8djcpp/mode2_packet.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opena8djcpp {

namespace {

std::array<std::uint8_t, kMode2FrameBytesPerStream> stream_frame_bytes(const S24Frame& frame,
                                                                       std::uint32_t stream) {
  const auto left = encode_s24_big_endian(frame[(stream * kChannelsPerPair) + 0]);
  const auto right = encode_s24_big_endian(frame[(stream * kChannelsPerPair) + 1]);
  return {left[0], left[1], left[2], right[0], right[1], right[2]};
}

}  // namespace

std::uint8_t mode2_check_byte(std::uint32_t stream, std::uint32_t byte_index) {
  const auto group = byte_index / kMode2GroupBytes;
  return static_cast<std::uint8_t>((stream << 1U) | ((~group) & 1U));
}

std::array<std::uint8_t, 3> encode_s24_big_endian(std::int32_t sample) {
  const auto clamped = std::clamp(sample, kS24Min, kS24Max);
  const auto raw = static_cast<std::uint32_t>(clamped) & 0x00FFFFFFU;
  return {
      static_cast<std::uint8_t>((raw >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((raw >> 8U) & 0xFFU),
      static_cast<std::uint8_t>(raw & 0xFFU),
  };
}

std::int32_t decode_s24_big_endian(std::span<const std::uint8_t, 3> bytes) {
  std::int32_t value = (static_cast<std::int32_t>(bytes[0]) << 16) |
                       (static_cast<std::int32_t>(bytes[1]) << 8) |
                       static_cast<std::int32_t>(bytes[2]);
  if ((value & 0x00800000) != 0) {
    value -= 0x01000000;
  }
  return value;
}

std::int32_t float_to_s24(float sample, float gain) {
  const float scaled = sample * static_cast<float>(gain);
  if (!std::isfinite(scaled)) {
    return 0;
  }
  if (scaled >= 1.0F) {
    return kS24Max;
  }
  if (scaled <= -1.0F) {
    return kS24Min;
  }
  const float q31_float =
      scaled * static_cast<float>(std::numeric_limits<std::int32_t>::max());
  const auto q31 = static_cast<std::int64_t>(std::lrint(static_cast<double>(q31_float)));
  return static_cast<std::int32_t>(q31 >> 8);
}

Mode2OutputPacker::Mode2OutputPacker(std::span<const S24Frame> frames, std::uint32_t start_byte)
    : frames_(frames), output_byte_in_frame_(start_byte) {}

std::vector<std::uint8_t> Mode2OutputPacker::fill(std::uint32_t length) {
  std::vector<std::uint8_t> out;
  out.reserve(length);
  out.resize(length);
  const auto written = fill_into(out);
  out.resize(written);
  return out;
}

std::uint32_t Mode2OutputPacker::fill_into(std::span<std::uint8_t> output) {
  std::uint32_t written = 0;

  std::uint32_t index = 0;
  while (index < output.size()) {
    if ((index % kMode2GroupBytes) == kMode2CheckOffset) {
      for (std::uint32_t stream = 0; stream < kMode2Streams && index < output.size(); ++stream) {
        output[index] = mode2_check_byte(stream, index);
        ++index;
        ++written;
      }
      continue;
    }

    load_next_frame_if_needed();
    for (std::uint32_t stream = 0; stream < kMode2Streams && index < output.size(); ++stream) {
      output[index] = output_frame_bytes_[stream][output_byte_in_frame_];
      ++index;
      ++written;
    }
    output_byte_in_frame_ += 1;
    if (output_byte_in_frame_ >= kMode2FrameBytesPerStream) {
      output_byte_in_frame_ = 0;
    }
  }

  return written;
}

void Mode2OutputPacker::load_next_frame_if_needed() {
  if (frame_loaded_ && output_byte_in_frame_ != 0) {
    return;
  }

  if (frame_index_ < frames_.size()) {
    for (std::uint32_t stream = 0; stream < kMode2Streams; ++stream) {
      output_frame_bytes_[stream] = stream_frame_bytes(frames_[frame_index_], stream);
    }
    ++frame_index_;
  } else {
    for (auto& stream : output_frame_bytes_) {
      stream.fill(0);
    }
  }
  frame_loaded_ = true;
}

Mode2DecodeResult decode_mode2_usb_bytes(std::span<const std::uint8_t> data,
                                         std::uint32_t start_byte,
                                         std::uint32_t transfer_bytes) {
  Mode2DecodeResult result;
  result.frames.resize(data.size() / kMode2GroupBytes + 2);
  const auto into = decode_mode2_usb_bytes_into(data, start_byte, transfer_bytes, result.frames);
  result.stats = into.stats;
  result.frames.resize(static_cast<std::size_t>(into.stats.decoded_frames));
  return result;
}

Mode2DecodeIntoResult decode_mode2_usb_bytes_into(std::span<const std::uint8_t> data,
                                                  std::uint32_t start_byte,
                                                  std::uint32_t transfer_bytes,
                                                  std::span<S24Frame> output_frames) {
  Mode2DecodeIntoResult result;
  if (transfer_bytes == 0) {
    return result;
  }

  std::array<std::array<std::uint8_t, kMode2FrameBytesPerStream>, kMode2Streams> pending{};
  std::array<std::uint8_t, kMode2Streams> present_masks{};
  std::uint32_t lane_streams = 0;
  std::uint32_t byte_position = start_byte;
  std::uint32_t local_index = 0;
  std::uint32_t group_offset = 0;
  std::uint32_t group_index = 0;
  constexpr std::uint8_t kCompleteFrameMask =
      static_cast<std::uint8_t>((1U << kMode2FrameBytesPerStream) - 1U);

  for (const auto value : data) {
    const auto current_group_offset = group_offset;

    local_index += 1;
    group_offset += 1;
    if (local_index == transfer_bytes) {
      local_index = 0;
      group_offset = 0;
      group_index = 0;
    } else if (group_offset == kMode2GroupBytes) {
      group_offset = 0;
      group_index += 1;
    }

    if (current_group_offset >= kMode2CheckOffset &&
        current_group_offset < kMode2CheckOffset + kMode2Streams) {
      const auto stream = current_group_offset - kMode2CheckOffset;
      result.stats.checks += 1;
      if ((value & 0x80U) != 0) {
        result.stats.panic_flags += 1;
      }
      const auto expected =
          static_cast<std::uint8_t>((stream << 1U) | ((~group_index) & 1U));
      if ((value & 0x3FU) != expected) {
        result.stats.check_errors += 1;
      }
      continue;
    }

    const auto stream = current_group_offset & (kMode2Streams - 1U);
    if (stream == 0 && byte_position == 0) {
      present_masks.fill(0);
      lane_streams = 0;
    }

    pending[stream][byte_position] = value;
    present_masks[stream] |= static_cast<std::uint8_t>(1U << byte_position);
    result.stats.sample_bytes += 1;
    lane_streams += 1;

    if (lane_streams == kMode2Streams) {
      if (byte_position == kMode2FrameBytesPerStream - 1) {
        const bool complete =
            std::all_of(present_masks.begin(), present_masks.end(), [](std::uint8_t mask) {
              return mask == kCompleteFrameMask;
            });

        if (complete) {
          S24Frame frame{};
          for (std::uint32_t stream_index = 0; stream_index < kMode2Streams; ++stream_index) {
            const auto left = std::span<const std::uint8_t, 3>(pending[stream_index].data(), 3);
            const auto right =
                std::span<const std::uint8_t, 3>(pending[stream_index].data() + 3, 3);
            frame[(stream_index * kChannelsPerPair) + 0] = decode_s24_big_endian(left);
            frame[(stream_index * kChannelsPerPair) + 1] = decode_s24_big_endian(right);
          }
          if (result.stats.decoded_frames < output_frames.size()) {
            output_frames[static_cast<std::size_t>(result.stats.decoded_frames)] = frame;
            result.stats.decoded_frames += 1;
          } else {
            result.output_overflows += 1;
          }
        }

        present_masks.fill(0);
      }

      byte_position = (byte_position + 1) % kMode2FrameBytesPerStream;
      lane_streams = 0;
    }
  }

  return result;
}

}  // namespace opena8djcpp
