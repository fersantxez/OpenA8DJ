#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <iostream>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

const char* state_name(AudioDriverState state) {
  switch (state) {
    case AudioDriverState::Created:
      return "created";
    case AudioDriverState::Started:
      return "started";
    case AudioDriverState::Stopped:
      return "stopped";
  }
  return "unknown";
}

}  // namespace

int main() {
  AudioDriverSkeleton driver;
  const bool initial_surface_ok = validate_driverkit_device_model(driver.device_model());
  const auto initial_state = driver.state();
  const bool first_start = driver.start_driver();
  const bool duplicate_start = driver.start_driver();
  const auto started_state = driver.state();
  const bool first_stop = driver.stop_driver();
  const bool duplicate_stop = driver.stop_driver();
  const auto stopped_state = driver.state();

  const bool pass = initial_surface_ok && initial_state == AudioDriverState::Created &&
                    first_start && !duplicate_start &&
                    started_state == AudioDriverState::Started && first_stop &&
                    !duplicate_stop && stopped_state == AudioDriverState::Stopped;

  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"scope\": \"offline_driverkit_shell_only\",\n"
            << "  \"driverkit_sdk_required\": false,\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"device_model_valid\": " << (initial_surface_ok ? "true" : "false")
            << ",\n"
            << "  \"initial_state\": \"" << state_name(initial_state) << "\",\n"
            << "  \"first_start\": " << (first_start ? "true" : "false") << ",\n"
            << "  \"duplicate_start\": " << (duplicate_start ? "true" : "false") << ",\n"
            << "  \"started_state\": \"" << state_name(started_state) << "\",\n"
            << "  \"first_stop\": " << (first_stop ? "true" : "false") << ",\n"
            << "  \"duplicate_stop\": " << (duplicate_stop ? "true" : "false") << ",\n"
            << "  \"stopped_state\": \"" << state_name(stopped_state) << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
