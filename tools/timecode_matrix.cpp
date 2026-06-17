#include "opena8djcpp/timecode.hpp"
#include "opena8djcpp/input_profile.hpp"

#include <cstdint>
#include <iostream>

using namespace opena8djcpp;

namespace {

bool validate_profile(TimecodeProfile profile) {
  const auto spec = timecode_profile_spec(profile);
  switch (profile) {
    case TimecodeProfile::Vinyl:
      return spec.caiaq_input_mode == 0 && spec.vinyl_ground_lift_applicable &&
             !spec.cd_line_ground_lift_applicable && !spec.phono_ground_lift_applicable;
    case TimecodeProfile::CdLine:
      return spec.caiaq_input_mode == 1 && !spec.vinyl_ground_lift_applicable &&
             spec.cd_line_ground_lift_applicable && !spec.phono_ground_lift_applicable;
    case TimecodeProfile::Phono:
      return spec.caiaq_input_mode == 2 && !spec.vinyl_ground_lift_applicable &&
             !spec.cd_line_ground_lift_applicable && spec.phono_ground_lift_applicable;
    case TimecodeProfile::Disabled:
      return spec.caiaq_input_mode == 1 && !spec.vinyl_ground_lift_applicable &&
             !spec.cd_line_ground_lift_applicable && !spec.phono_ground_lift_applicable;
  }
  return false;
}

bool validate_deck(StereoPair deck) {
  const auto assignment = deck_timecode_assignment(deck);
  return assignment.left_input_channel == channel_index(deck, PairSide::Left) &&
         assignment.right_input_channel == channel_index(deck, PairSide::Right) &&
         assignment.right_input_channel == assignment.left_input_channel + 1;
}

InputProfile profile_for_matrix(TimecodeProfile profile) {
  switch (profile) {
    case TimecodeProfile::Vinyl:
      return timecode_vinyl_input_profile();
    case TimecodeProfile::CdLine:
      return timecode_cd_line_input_profile();
    case TimecodeProfile::Phono:
      return phono_input_profile();
    case TimecodeProfile::Disabled:
      return playback_input_profile();
  }
  return playback_input_profile();
}

bool validate_input_profile(TimecodeProfile profile) {
  const auto input = profile_for_matrix(profile);
  if (!input.valid() || !input.source_map_is_identity()) {
    return false;
  }
  switch (profile) {
    case TimecodeProfile::Vinyl:
      return input.caiaq_input_mode == 0 && input.input_decode_enabled &&
             input.software_lock_enabled && input.vinyl_ground_lift_enabled;
    case TimecodeProfile::CdLine:
      return input.caiaq_input_mode == 1 && input.input_decode_enabled &&
             input.software_lock_enabled && input.cd_line_ground_lift_enabled;
    case TimecodeProfile::Phono:
      return input.caiaq_input_mode == 2 && input.input_decode_enabled &&
             input.software_lock_enabled && input.phono_ground_lift_enabled;
    case TimecodeProfile::Disabled:
      return input.caiaq_input_mode == 1 && !input.input_decode_enabled &&
             !input.software_lock_enabled;
  }
  return false;
}

}  // namespace

int main() {
  const TimecodeProfile profiles[] = {TimecodeProfile::Vinyl, TimecodeProfile::CdLine,
                                      TimecodeProfile::Phono, TimecodeProfile::Disabled};
  const StereoPair decks[] = {StereoPair::A, StereoPair::B, StereoPair::C, StereoPair::D};

  constexpr std::uint32_t profile_count = 4;
  constexpr std::uint32_t deck_count = 4;
  constexpr std::uint32_t row_count = profile_count + deck_count;
  std::uint32_t failures = 0;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.timecode-matrix.v1\",\n"
            << "  \"profiles\": [\n";
  for (std::uint32_t index = 0; index < profile_count; ++index) {
    const auto spec = timecode_profile_spec(profiles[index]);
    const auto input = profile_for_matrix(profiles[index]);
    const bool ok = validate_profile(profiles[index]) && validate_input_profile(profiles[index]);
    failures += ok ? 0U : 1U;
    std::cout << "    {\"name\": \"" << spec.name << "\", \"caiaq_input_mode\": "
              << static_cast<std::uint32_t>(spec.caiaq_input_mode)
              << ", \"input_decode_enabled\": "
              << (input.input_decode_enabled ? "true" : "false")
              << ", \"software_lock_enabled\": "
              << (input.software_lock_enabled ? "true" : "false")
              << ", \"source_map_identity\": "
              << (input.source_map_is_identity() ? "true" : "false")
              << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}"
              << (index + 1U == profile_count ? "\n" : ",\n");
  }

  std::cout << "  ],\n  \"deck_assignments\": [\n";
  for (std::uint32_t index = 0; index < deck_count; ++index) {
    const auto assignment = deck_timecode_assignment(decks[index]);
    const bool ok = validate_deck(decks[index]);
    failures += ok ? 0U : 1U;
    std::cout << "    {\"deck\": \"" << pair_name(decks[index]) << "\", \"left\": "
              << assignment.left_input_channel << ", \"right\": "
              << assignment.right_input_channel << ", \"result\": \""
              << (ok ? "PASS" : "FAIL") << "\"}"
              << (index + 1U == deck_count ? "\n" : ",\n");
  }

  std::cout << "  ],\n  \"row_count\": " << row_count << ",\n  \"failures\": " << failures
            << ",\n  \"result\": \""
            << (failures == 0 ? "PASS" : "FAIL") << "\"\n}\n";
  return failures == 0 ? 0 : 1;
}
