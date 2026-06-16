#pragma once

#include "opena8djcpp/audio_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opena8djcpp {

inline constexpr std::uint32_t kMode2Streams = kStereoPairs;
inline constexpr std::uint32_t kMode2BytesPerSample = 3;
inline constexpr std::uint32_t kMode2UsbBytesPerSample = 4;
inline constexpr std::uint32_t kMode2FrameBytesPerStream =
    kChannelsPerPair * kMode2BytesPerSample;
inline constexpr std::uint32_t kMode2GroupBytes = kMode2Streams * kMode2UsbBytesPerSample;
inline constexpr std::uint32_t kMode2FullFrameBytes =
    kMode2Streams * (kMode2FrameBytesPerStream + kChannelsPerPair);
inline constexpr std::uint32_t kMode2CheckOffset = kMode2Streams * kChannelsPerPair;
inline constexpr std::uint32_t kMode2DefaultStartByte = kMode2BytesPerSample + 1;
inline constexpr std::uint32_t kMode2DefaultTransferBytes = 352;

inline constexpr std::int32_t kS24Min = -8388608;
inline constexpr std::int32_t kS24Max = 8388607;

using S24Frame = std::array<std::int32_t, kOutputChannels>;

struct Mode2DecodeStats {
  std::uint64_t decoded_frames = 0;
  std::uint64_t checks = 0;
  std::uint64_t check_errors = 0;
  std::uint64_t panic_flags = 0;
  std::uint64_t sample_bytes = 0;
};

struct Mode2DecodeResult {
  std::vector<S24Frame> frames;
  Mode2DecodeStats stats;
};

struct Mode2DecodeIntoResult {
  Mode2DecodeStats stats;
  std::uint64_t output_overflows = 0;
};

[[nodiscard]] std::uint8_t mode2_check_byte(std::uint32_t stream, std::uint32_t byte_index);
[[nodiscard]] std::array<std::uint8_t, 3> encode_s24_big_endian(std::int32_t sample);
[[nodiscard]] std::int32_t decode_s24_big_endian(std::span<const std::uint8_t, 3> bytes);
[[nodiscard]] std::int32_t float_to_s24(float sample, float gain);

class Mode2OutputPacker {
 public:
  Mode2OutputPacker(std::span<const S24Frame> frames, std::uint32_t start_byte);

  [[nodiscard]] std::vector<std::uint8_t> fill(std::uint32_t length);
  [[nodiscard]] std::uint32_t fill_into(std::span<std::uint8_t> output);

 private:
  void load_next_frame_if_needed();

  std::span<const S24Frame> frames_;
  std::uint32_t frame_index_ = 0;
  std::uint32_t output_byte_in_frame_ = 0;
  bool frame_loaded_ = false;
  std::array<std::array<std::uint8_t, kMode2FrameBytesPerStream>, kMode2Streams>
      output_frame_bytes_{};
};

[[nodiscard]] Mode2DecodeResult decode_mode2_usb_bytes(std::span<const std::uint8_t> data,
                                                       std::uint32_t start_byte,
                                                       std::uint32_t transfer_bytes);

[[nodiscard]] Mode2DecodeIntoResult decode_mode2_usb_bytes_into(
    std::span<const std::uint8_t> data,
    std::uint32_t start_byte,
    std::uint32_t transfer_bytes,
    std::span<S24Frame> output_frames);

}  // namespace opena8djcpp
