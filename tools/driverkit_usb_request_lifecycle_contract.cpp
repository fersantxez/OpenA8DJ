#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"
#include "opena8djcpp/usb_request_pool.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

S24Frame frame_for(std::uint32_t family, std::uint64_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto side = channel % kChannelsPerPair;
    const auto motion =
        static_cast<std::int32_t>((frame_index * 89U) + (channel * 557U));
    const auto base = static_cast<std::int32_t>((family + 1U) * 350000U);
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

std::vector<UsbSubmitDescriptor> driverkit_descriptors() {
  constexpr std::uint32_t kIsoFrames = 8;
  constexpr std::uint32_t kPeriods = 256;
  std::vector<UsbSubmitDescriptor> out;
  AudioDriverSkeleton driver;
  const AudioStreamConfig config{
      .sample_rate = 48000,
      .buffer_frames = 64,
      .transport =
          PreparedTransportConfig{.iso_frames = kIsoFrames, .capture_slots = 32, .playback_slots = 32},
      .usb_slots_per_submit = 8,
      .usb_bytes_per_slot = kMode2DefaultTransferBytes,
      .usb_initial_capture_slots = 8,
      .usb_initial_playback_slots = 8,
  };
  if (!driver.start_driver() || !driver.configure_stream(config) || !driver.start_stream()) {
    return out;
  }
  std::vector<S24Frame> playback(kIsoFrames);
  std::vector<S24Frame> capture(kIsoFrames);
  std::vector<S24Frame> backend_playback(kIsoFrames);
  for (std::uint32_t period = 0; period < kPeriods; ++period) {
    for (std::uint32_t offset = 0; offset < kIsoFrames; ++offset) {
      const auto index = static_cast<std::uint64_t>(period * kIsoFrames) + offset;
      playback[offset] = frame_for(31, index);
      capture[offset] = frame_for(37, index);
    }
    (void)driver.write_playback(playback);
    (void)driver.complete_backend_period(capture, backend_playback, (period + 1U) * kIsoFrames);
  }
  driver.finish_usb_submit_binding();
  const auto descriptors = driver.usb_submit_descriptors();
  out.assign(descriptors.begin(), descriptors.end());
  (void)driver.stop_stream();
  (void)driver.stop_driver();
  return out;
}

struct Scenario {
  const char* name = "";
  std::uint32_t request_slots = 8;
  std::uint32_t completion_depth = 4;
  bool inject_stale_completion = false;
  bool expect_safe = true;
};

struct ScenarioResult {
  PreparedUsbRequestPoolCounters counters{};
  PreparedUsbRequestPoolSafety safety{};
  bool descriptors_ok = false;
  bool pass = false;
};

ScenarioResult run_scenario(const Scenario& scenario,
                            const std::vector<UsbSubmitDescriptor>& descriptors) {
  ScenarioResult result{};
  PreparedUsbRequestPool pool;
  if (!pool.start(PreparedUsbRequestPoolConfig{.request_slots = scenario.request_slots})) {
    return result;
  }

  std::vector<PreparedUsbRequestHandle> inflight;
  inflight.reserve(scenario.completion_depth + 1U);
  for (const auto& descriptor : descriptors) {
    const auto handle = pool.submit(descriptor);
    if (handle.valid()) {
      inflight.push_back(handle);
    }
    while (inflight.size() >= scenario.completion_depth && !inflight.empty()) {
      (void)pool.complete(inflight.front());
      inflight.erase(inflight.begin());
    }
  }

  for (const auto handle : inflight) {
    (void)pool.complete(handle);
  }

  if (scenario.inject_stale_completion && !descriptors.empty()) {
    const auto first = pool.submit(descriptors[0]);
    (void)pool.complete(first);
    const auto second = pool.submit(descriptors[1]);
    (void)pool.complete(first);
    (void)pool.complete(second);
  }

  result.counters = pool.counters();
  result.safety = pool.safety();
  result.descriptors_ok = descriptors.size() == 66;
  result.pass = result.descriptors_ok && result.safety.product_safe == scenario.expect_safe;
  pool.stop();
  return result;
}

void print_row(const Scenario& scenario,
               const ScenarioResult& result,
               bool trailing_comma) {
  const auto& counters = result.counters;
  const auto& safety = result.safety;
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"request_slots\": " << scenario.request_slots
            << ", \"completion_depth\": " << scenario.completion_depth
            << ", \"submit_calls\": " << counters.submit_calls
            << ", \"capture_submit_calls\": " << counters.capture_submit_calls
            << ", \"playback_submit_calls\": " << counters.playback_submit_calls
            << ", \"completion_calls\": " << counters.completion_calls
            << ", \"recycle_calls\": " << counters.recycle_calls
            << ", \"fallback_allocations\": " << counters.fallback_allocations
            << ", \"invalid_completions\": " << counters.invalid_completions
            << ", \"stale_completions\": " << counters.stale_completions
            << ", \"live_requests\": " << counters.live_requests
            << ", \"max_live_requests\": " << counters.max_live_requests
            << ", \"submitted_bytes\": " << counters.submitted_bytes
            << ", \"completed_bytes\": " << counters.completed_bytes
            << ", \"submitted_frames\": " << counters.submitted_frames
            << ", \"completed_frames\": " << counters.completed_frames
            << ", \"preallocated_only\": " << (safety.preallocated_only ? "true" : "false")
            << ", \"lifecycle_safe\": " << (safety.lifecycle_safe ? "true" : "false")
            << ", \"accounting_safe\": " << (safety.accounting_safe ? "true" : "false")
            << ", \"drained\": " << (safety.drained ? "true" : "false")
            << ", \"product_safe\": " << (safety.product_safe ? "true" : "false")
            << ", \"expect_safe\": " << (scenario.expect_safe ? "true" : "false")
            << ", \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const auto descriptors = driverkit_descriptors();
  const std::vector<Scenario> scenarios = {
      {.name = "driverkit_usb_request_pool_stable"},
      {.name = "request_pool_pressure_rejected",
       .request_slots = 2,
       .completion_depth = 4,
       .expect_safe = false},
      {.name = "stale_completion_rejected",
       .request_slots = 8,
       .completion_depth = 4,
       .inject_stale_completion = true,
       .expect_safe = false},
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_rows = 0;
  ScenarioResult stable{};
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-usb-request-lifecycle-contract.v1\",\n"
            << "  \"meaning\": \"offline DriverKit USB request pool submit/complete/recycle lifecycle; PASS is not physical USB readiness\",\n"
            << "  \"safety\": \"offline_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"source_descriptors\": " << descriptors.size() << ",\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const auto result = run_scenario(scenarios[index], descriptors);
    if (!result.pass) {
      failures += 1;
    }
    if (result.safety.product_safe) {
      safe_rows += 1;
    }
    if (std::string(scenarios[index].name) == "driverkit_usb_request_pool_stable") {
      stable = result;
    }
    print_row(scenarios[index], result, index + 1U < scenarios.size());
  }

  const bool pass = failures == 0 && safe_rows == 1 && descriptors.size() == 66 &&
                    stable.counters.submit_calls == 66 &&
                    stable.counters.completion_calls == 66 &&
                    stable.counters.recycle_calls == 66 &&
                    stable.counters.capture_submit_calls == 33 &&
                    stable.counters.playback_submit_calls == 33 &&
                    stable.counters.fallback_allocations == 0 &&
                    stable.counters.invalid_completions == 0 &&
                    stable.counters.stale_completions == 0 &&
                    stable.counters.live_requests == 0 &&
                    stable.counters.max_live_requests == 4 &&
                    stable.counters.submitted_bytes == 185856 &&
                    stable.counters.completed_bytes == 185856 &&
                    stable.counters.submitted_frames == 5808 &&
                    stable.counters.completed_frames == 5808;
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_rows\": " << safe_rows << ",\n"
            << "  \"stable_submit_calls\": " << stable.counters.submit_calls << ",\n"
            << "  \"stable_completion_calls\": " << stable.counters.completion_calls << ",\n"
            << "  \"stable_recycle_calls\": " << stable.counters.recycle_calls << ",\n"
            << "  \"stable_max_live_requests\": " << stable.counters.max_live_requests << ",\n"
            << "  \"stable_submitted_bytes\": " << stable.counters.submitted_bytes << ",\n"
            << "  \"stable_completed_bytes\": " << stable.counters.completed_bytes << ",\n"
            << "  \"stable_submitted_frames\": " << stable.counters.submitted_frames << ",\n"
            << "  \"stable_completed_frames\": " << stable.counters.completed_frames << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
