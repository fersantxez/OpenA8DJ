#pragma once

#include "opena8djcpp/audio_model.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace opena8djcpp {

enum class StreamDirection : std::uint8_t {
  Input,
  Output,
};

struct DriverKitStreamModel {
  StreamDirection direction;
  std::string_view name;
  std::uint32_t starting_channel;
  std::uint32_t channel_count;
};

struct DriverKitDeviceModel {
  std::string_view device_name;
  std::string_view uid;
  std::array<std::uint32_t, 2> sample_rates;
  std::array<DriverKitStreamModel, 5> streams;
};

[[nodiscard]] constexpr DriverKitDeviceModel make_driverkit_device_model() {
  return {
      "Open Audio 8 DJ C++",
      "org.opena8dj.Audio8DJ.cpp",
      {44100, 48000},
      {{
          {StreamDirection::Input, "Input A/B/C/D", 0, 8},
          {StreamDirection::Output, "Output A", 0, 2},
          {StreamDirection::Output, "Output B", 2, 2},
          {StreamDirection::Output, "Output C", 4, 2},
          {StreamDirection::Output, "Output D", 6, 2},
      }},
  };
}

[[nodiscard]] constexpr bool validate_driverkit_device_model(const DriverKitDeviceModel& model) {
  return model.sample_rates[0] == 44100 && model.sample_rates[1] == 48000 &&
         model.streams[0].direction == StreamDirection::Input &&
         model.streams[0].starting_channel == 0 && model.streams[0].channel_count == 8 &&
         model.streams[1].direction == StreamDirection::Output &&
         model.streams[1].starting_channel == channel_index(StereoPair::A, PairSide::Left) &&
         model.streams[2].starting_channel == channel_index(StereoPair::B, PairSide::Left) &&
         model.streams[3].starting_channel == channel_index(StereoPair::C, PairSide::Left) &&
         model.streams[4].starting_channel == channel_index(StereoPair::D, PairSide::Left);
}

}  // namespace opena8djcpp
