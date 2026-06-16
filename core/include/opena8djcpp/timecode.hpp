#pragma once

#include "opena8djcpp/audio_model.hpp"

#include <cstdint>
#include <string_view>

namespace opena8djcpp {

struct TimecodeProfileSpec {
  TimecodeProfile profile;
  std::uint8_t caiaq_input_mode;
  bool vinyl_ground_lift_applicable;
  bool cd_line_ground_lift_applicable;
  bool phono_ground_lift_applicable;
  std::string_view name;
};

[[nodiscard]] constexpr TimecodeProfileSpec timecode_profile_spec(TimecodeProfile profile) {
  switch (profile) {
    case TimecodeProfile::Vinyl:
      return {profile, 0, true, false, false, "timecode-vinyl"};
    case TimecodeProfile::CdLine:
      return {profile, 1, false, true, false, "timecode-cd-line"};
    case TimecodeProfile::Phono:
      return {profile, 2, false, false, true, "phono"};
    case TimecodeProfile::Disabled:
      return {profile, 1, false, false, false, "disabled"};
  }
  return {TimecodeProfile::Disabled, 1, false, false, false, "unknown"};
}

struct DeckTimecodeAssignment {
  StereoPair deck;
  std::uint32_t left_input_channel;
  std::uint32_t right_input_channel;
};

[[nodiscard]] constexpr DeckTimecodeAssignment deck_timecode_assignment(StereoPair deck) {
  return {deck, channel_index(deck, PairSide::Left), channel_index(deck, PairSide::Right)};
}

}  // namespace opena8djcpp
