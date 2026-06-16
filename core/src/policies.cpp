#include "opena8djcpp/policies.hpp"

namespace opena8djcpp {

static_assert(SampleRatePolicy::is_supported(44100));
static_assert(SampleRatePolicy::is_supported(48000));
static_assert(!SampleRatePolicy::is_supported(96000));
static_assert(BufferPolicy::offline_default().accepts(512));

}  // namespace opena8djcpp
