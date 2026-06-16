#pragma once

#include "opena8djcpp/audio_model.hpp"

#include <cstdint>

namespace opena8djcpp {

enum class SampleRate : std::uint32_t {
  Rate44100 = 44100,
  Rate48000 = 48000,
};

struct SampleRatePolicy {
  [[nodiscard]] static constexpr bool is_supported(std::uint32_t sample_rate) {
    return sample_rate == static_cast<std::uint32_t>(SampleRate::Rate44100) ||
           sample_rate == static_cast<std::uint32_t>(SampleRate::Rate48000);
  }
};

struct BufferPolicy {
  std::uint32_t nominal_frames;
  std::uint32_t min_frames;
  std::uint32_t max_frames;

  [[nodiscard]] static constexpr BufferPolicy offline_default() {
    return BufferPolicy{512, 64, 2048};
  }

  [[nodiscard]] constexpr bool accepts(std::uint32_t frames) const {
    return frames >= min_frames && frames <= max_frames;
  }
};

struct TimecodePolicy {
  TimecodeProfile profile;

  [[nodiscard]] static constexpr TimecodePolicy vinyl_default() {
    return TimecodePolicy{TimecodeProfile::Vinyl};
  }
};

}  // namespace opena8djcpp
