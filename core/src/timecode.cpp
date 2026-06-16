#include "opena8djcpp/timecode.hpp"

namespace opena8djcpp {

static_assert(timecode_profile_spec(TimecodeProfile::Vinyl).caiaq_input_mode == 0);
static_assert(timecode_profile_spec(TimecodeProfile::CdLine).caiaq_input_mode == 1);
static_assert(timecode_profile_spec(TimecodeProfile::Phono).caiaq_input_mode == 2);
static_assert(deck_timecode_assignment(StereoPair::A).left_input_channel == 0);
static_assert(deck_timecode_assignment(StereoPair::D).right_input_channel == 7);

}  // namespace opena8djcpp
