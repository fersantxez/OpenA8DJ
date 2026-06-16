#include "opena8djcpp/input_profile.hpp"

namespace opena8djcpp {

static_assert(playback_input_profile().caiaq_input_mode == 1);
static_assert(!playback_input_profile().input_decode_enabled);
static_assert(!playback_input_profile().software_lock_enabled);
static_assert(timecode_vinyl_input_profile().caiaq_input_mode == 0);
static_assert(timecode_vinyl_input_profile().input_decode_enabled);
static_assert(timecode_vinyl_input_profile().software_lock_enabled);
static_assert(timecode_cd_line_input_profile().caiaq_input_mode == 1);
static_assert(phono_input_profile().caiaq_input_mode == 2);

}  // namespace opena8djcpp
