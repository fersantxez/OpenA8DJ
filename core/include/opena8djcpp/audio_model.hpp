#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace opena8djcpp {

inline constexpr std::uint32_t kInputChannels = 8;
inline constexpr std::uint32_t kOutputChannels = 8;
inline constexpr std::uint32_t kStereoPairs = 4;
inline constexpr std::uint32_t kChannelsPerPair = 2;

enum class StereoPair : std::uint8_t {
  A = 0,
  B = 1,
  C = 2,
  D = 3,
};

enum class PairSide : std::uint8_t {
  Left = 0,
  Right = 1,
};

enum class SampleFormat : std::uint8_t {
  Float32Interleaved,
  Signed24PackedUsb,
};

enum class TimecodeProfile : std::uint8_t {
  Disabled,
  Vinyl,
  CdLine,
  Phono,
};

struct ChannelRef {
  StereoPair pair;
  PairSide side;
};

struct DeviceSurface {
  std::uint32_t input_channels;
  std::uint32_t output_channels;
  std::array<ChannelRef, kInputChannels> input_map;
  std::array<ChannelRef, kOutputChannels> output_map;
};

[[nodiscard]] constexpr std::uint32_t pair_index(StereoPair pair) {
  return static_cast<std::uint32_t>(pair);
}

[[nodiscard]] constexpr std::uint32_t side_index(PairSide side) {
  return static_cast<std::uint32_t>(side);
}

[[nodiscard]] constexpr std::uint32_t channel_index(StereoPair pair, PairSide side) {
  return (pair_index(pair) * kChannelsPerPair) + side_index(side);
}

[[nodiscard]] constexpr std::string_view pair_name(StereoPair pair) {
  switch (pair) {
    case StereoPair::A:
      return "A";
    case StereoPair::B:
      return "B";
    case StereoPair::C:
      return "C";
    case StereoPair::D:
      return "D";
  }
  return "unknown";
}

[[nodiscard]] constexpr std::string_view side_name(PairSide side) {
  switch (side) {
    case PairSide::Left:
      return "left";
    case PairSide::Right:
      return "right";
  }
  return "unknown";
}

[[nodiscard]] DeviceSurface make_audio8dj_surface();

}  // namespace opena8djcpp
