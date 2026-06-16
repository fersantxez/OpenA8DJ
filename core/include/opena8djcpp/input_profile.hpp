#pragma once

#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/timecode.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace opena8djcpp {

enum class InputProfileKind : std::uint8_t {
  Playback,
  TimecodeVinyl,
  TimecodeCdLine,
  Phono,
};

struct InputProfile {
  InputProfileKind kind;
  TimecodeProfile timecode_profile;
  std::uint8_t caiaq_input_mode;
  bool input_decode_enabled;
  bool software_lock_enabled;
  bool vinyl_ground_lift_enabled;
  bool cd_line_ground_lift_enabled;
  bool phono_ground_lift_enabled;
  std::array<std::uint32_t, kInputChannels> source_map;
  std::string_view name;

  [[nodiscard]] constexpr bool source_map_is_identity() const {
    for (std::uint32_t index = 0; index < kInputChannels; ++index) {
      if (source_map[index] != index) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] constexpr bool valid() const {
    for (const auto source : source_map) {
      if (source >= kInputChannels) {
        return false;
      }
    }
    return true;
  }
};

[[nodiscard]] constexpr std::array<std::uint32_t, kInputChannels> identity_input_source_map() {
  return {0, 1, 2, 3, 4, 5, 6, 7};
}

[[nodiscard]] constexpr InputProfile playback_input_profile() {
  const auto spec = timecode_profile_spec(TimecodeProfile::Disabled);
  return {InputProfileKind::Playback,
          TimecodeProfile::Disabled,
          spec.caiaq_input_mode,
          false,
          false,
          false,
          false,
          false,
          identity_input_source_map(),
          "playback"};
}

[[nodiscard]] constexpr InputProfile timecode_vinyl_input_profile() {
  const auto spec = timecode_profile_spec(TimecodeProfile::Vinyl);
  return {InputProfileKind::TimecodeVinyl,
          TimecodeProfile::Vinyl,
          spec.caiaq_input_mode,
          true,
          true,
          spec.vinyl_ground_lift_applicable,
          false,
          false,
          identity_input_source_map(),
          "timecode-vinyl"};
}

[[nodiscard]] constexpr InputProfile timecode_cd_line_input_profile() {
  const auto spec = timecode_profile_spec(TimecodeProfile::CdLine);
  return {InputProfileKind::TimecodeCdLine,
          TimecodeProfile::CdLine,
          spec.caiaq_input_mode,
          true,
          true,
          false,
          spec.cd_line_ground_lift_applicable,
          false,
          identity_input_source_map(),
          "timecode-cd-line"};
}

[[nodiscard]] constexpr InputProfile phono_input_profile() {
  const auto spec = timecode_profile_spec(TimecodeProfile::Phono);
  return {InputProfileKind::Phono,
          TimecodeProfile::Phono,
          spec.caiaq_input_mode,
          true,
          true,
          false,
          false,
          spec.phono_ground_lift_applicable,
          identity_input_source_map(),
          "phono"};
}

}  // namespace opena8djcpp
