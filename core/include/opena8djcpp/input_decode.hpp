#pragma once

#include "opena8djcpp/input_profile.hpp"
#include "opena8djcpp/mode2_packet.hpp"

#include <cstdint>
#include <span>

namespace opena8djcpp {

struct InputDecodeIntoResult {
  Mode2DecodeStats stats;
  std::uint64_t decoded_frame_overflows = 0;
  std::uint64_t output_frame_overflows = 0;
  std::uint64_t frames_written = 0;
  bool input_decode_enabled = false;
  bool profile_valid = false;
};

[[nodiscard]] float s24_to_float(std::int32_t sample);

[[nodiscard]] InputDecodeIntoResult decode_input_profile_mode2_into(
    std::span<const std::uint8_t> data,
    std::uint32_t start_byte,
    std::uint32_t transfer_bytes,
    const InputProfile& profile,
    std::span<S24Frame> decode_scratch,
    std::span<float> output_interleaved_f32);

}  // namespace opena8djcpp
