#include "opena8djcpp/input_decode.hpp"

#include <algorithm>

namespace opena8djcpp {

float s24_to_float(std::int32_t sample) {
  constexpr float kScale = 1.0F / 8388608.0F;
  return std::clamp(static_cast<float>(sample) * kScale, -1.0F, 1.0F);
}

InputDecodeIntoResult decode_input_profile_mode2_into(
    std::span<const std::uint8_t> data,
    std::uint32_t start_byte,
    std::uint32_t transfer_bytes,
    const InputProfile& profile,
    std::span<S24Frame> decode_scratch,
    std::span<float> output_interleaved_f32) {
  InputDecodeIntoResult result{};
  result.profile_valid = profile.valid();
  result.input_decode_enabled = profile.input_decode_enabled;

  const auto decode = decode_mode2_usb_bytes_into(data, start_byte, transfer_bytes, decode_scratch);
  result.stats = decode.stats;
  result.decoded_frame_overflows = decode.output_overflows;

  if (!result.profile_valid || !result.input_decode_enabled) {
    return result;
  }

  const auto output_capacity_frames =
      static_cast<std::uint64_t>(output_interleaved_f32.size() / kInputChannels);
  const auto decoded_frames = std::min<std::uint64_t>(result.stats.decoded_frames,
                                                     decode_scratch.size());
  const auto frames_to_write = std::min(decoded_frames, output_capacity_frames);
  result.output_frame_overflows = decoded_frames - frames_to_write;

  for (std::uint64_t frame_index = 0; frame_index < frames_to_write; ++frame_index) {
    const auto& input_frame = decode_scratch[static_cast<std::size_t>(frame_index)];
    const auto output_base = static_cast<std::size_t>(frame_index) * kInputChannels;
    for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
      const auto source = profile.source_map[channel];
      output_interleaved_f32[output_base + channel] = s24_to_float(input_frame[source]);
    }
  }

  result.frames_written = frames_to_write;
  return result;
}

}  // namespace opena8djcpp
