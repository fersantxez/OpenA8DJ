#include "opena8djcpp/audio_model.hpp"

namespace opena8djcpp {

DeviceSurface make_audio8dj_surface() {
  return DeviceSurface{
      kInputChannels,
      kOutputChannels,
      {{
          {StereoPair::A, PairSide::Left},
          {StereoPair::A, PairSide::Right},
          {StereoPair::B, PairSide::Left},
          {StereoPair::B, PairSide::Right},
          {StereoPair::C, PairSide::Left},
          {StereoPair::C, PairSide::Right},
          {StereoPair::D, PairSide::Left},
          {StereoPair::D, PairSide::Right},
      }},
      {{
          {StereoPair::A, PairSide::Left},
          {StereoPair::A, PairSide::Right},
          {StereoPair::B, PairSide::Left},
          {StereoPair::B, PairSide::Right},
          {StereoPair::C, PairSide::Left},
          {StereoPair::C, PairSide::Right},
          {StereoPair::D, PairSide::Left},
          {StereoPair::D, PairSide::Right},
      }},
  };
}

}  // namespace opena8djcpp
