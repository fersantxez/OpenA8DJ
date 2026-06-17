#include "opena8djcpp/prepared_transport.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace opena8djcpp;

namespace {

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 2000;
  PreparedTransportConfig config{};
  std::uint32_t completion_gap_periods = 1;
  bool hal_direct_requeue = false;
  bool steady_fallback_allocation = false;
  bool reorder_timestamps = false;
  bool expect_safe = true;
};

struct ScenarioResult {
  PreparedTransportCounters counters{};
  PreparedTransportSafety safety{};
};

S24Frame synthetic_frame(std::uint32_t period) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto pair = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    frame[channel] = static_cast<std::int32_t>(((pair + 1U) * 1000000U) +
                                               (side * 250000U) + period);
  }
  return frame;
}

PreparedTransportStepOptions options_for(const Scenario& scenario, std::uint32_t period) {
  return PreparedTransportStepOptions{
      .completion_gap_periods = scenario.completion_gap_periods,
      .hal_direct_requeue_attempt = scenario.hal_direct_requeue,
      .fallback_allocation_attempt = scenario.steady_fallback_allocation,
      .force_timestamp_regression =
          scenario.reorder_timestamps && period == scenario.periods / 2U,
  };
}

ScenarioResult run(const Scenario& scenario) {
  PreparedTransportBackend transport;
  (void)transport.start(scenario.config);
  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    const auto frame = synthetic_frame(period);
    (void)transport.hal_write_playback(frame);
    const auto timestamp =
        static_cast<std::uint64_t>(period + 1U) * static_cast<std::uint64_t>(scenario.config.iso_frames);
    (void)transport.backend_complete_period(frame, timestamp, options_for(scenario, period));
    S24Frame captured{};
    (void)transport.hal_read_capture(captured);
  }
  return ScenarioResult{transport.counters(), transport.safety()};
}

bool scenario_passes(const Scenario& scenario, const PreparedTransportSafety& safety) {
  return safety.product_safe == scenario.expect_safe;
}

void print_row(const Scenario& scenario,
               const PreparedTransportCounters& counters,
               const PreparedTransportSafety& safety,
               bool ok,
               bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"periods\": " << scenario.periods
            << ", \"iso_frames\": " << scenario.config.iso_frames
            << ", \"capture_depth\": " << scenario.config.capture_slots
            << ", \"playback_depth\": " << scenario.config.playback_slots
            << ", \"completion_gap_periods\": " << scenario.completion_gap_periods
            << ", \"backend_prepare_enqueues\": " << counters.backend_prepare_enqueues
            << ", \"backend_steady_requeues\": " << counters.backend_steady_requeues
            << ", \"hal_steady_requeues\": " << counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << counters.fallback_allocations
            << ", \"capture_overruns\": " << counters.capture_ring_overruns
            << ", \"capture_underruns\": " << counters.capture_ring_underruns
            << ", \"playback_overruns\": " << counters.playback_ring_overruns
            << ", \"playback_underruns\": " << counters.playback_ring_underruns
            << ", \"timestamp_regressions\": " << counters.timestamp_regressions
            << ", \"channel_identity_failures\": " << counters.channel_identity_failures
            << ", \"timecode_profile_failures\": " << counters.timecode_profile_failures
            << ", \"hal_capture_reads\": " << counters.hal_capture_reads
            << ", \"hal_playback_writes\": " << counters.hal_playback_writes
            << ", \"max_completion_gap_ratio\": " << counters.max_completion_gap_ratio
            << ", \"prepared_slots_only\": " << (safety.prepared_slots_only ? "true" : "false")
            << ", \"cadence_safe\": " << (safety.cadence_safe ? "true" : "false")
            << ", \"routing_safe\": " << (safety.routing_safe ? "true" : "false")
            << ", \"timecode_safe\": " << (safety.timecode_safe ? "true" : "false")
            << ", \"hal_hot_path_safe\": " << (safety.hal_hot_path_safe ? "true" : "false")
            << ", \"product_safe\": " << (safety.product_safe ? "true" : "false")
            << ", \"expect_safe\": " << (scenario.expect_safe ? "true" : "false")
            << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const std::vector<Scenario> scenarios = {
      {
          .name = "prepared_iso8_q64_hal_zero_requeue",
          .config =
              PreparedTransportConfig{
                  .iso_frames = 8,
                  .capture_slots = 64,
                  .playback_slots = 64,
              },
      },
      {
          .name = "prepared_iso8_q8_hal_zero_requeue",
          .config =
              PreparedTransportConfig{
                  .iso_frames = 8,
                  .capture_slots = 8,
                  .playback_slots = 8,
              },
      },
      {
          .name = "hal_direct_requeue_rejected",
          .config = PreparedTransportConfig{},
          .hal_direct_requeue = true,
          .expect_safe = false,
      },
      {
          .name = "steady_fallback_allocation_rejected",
          .config = PreparedTransportConfig{},
          .steady_fallback_allocation = true,
          .expect_safe = false,
      },
      {
          .name = "coalesced_completion_gap_rejected",
          .config = PreparedTransportConfig{},
          .completion_gap_periods = 2,
          .expect_safe = false,
      },
      {
          .name = "timestamp_reorder_rejected",
          .config = PreparedTransportConfig{},
          .reorder_timestamps = true,
          .expect_safe = false,
      },
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_scenarios = 0;
  std::uint64_t minimum_hal_requeues_for_safe = 0;
  bool first_safe = true;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-prepared-transport-contract.v2\",\n"
            << "  \"meaning\": \"offline DriverKit transport contract backed by core PreparedTransportBackend; PASS does not mean physical readiness\",\n"
            << "  \"thresholds\": {\"max_completion_gap_ratio\": "
            << kPreparedTransportMaxCompletionGapRatio
            << ", \"required_input_channels\": " << kInputChannels
            << ", \"required_output_channels\": " << kOutputChannels
            << ", \"required_decks\": " << kStereoPairs << "},\n"
            << "  \"rows\": [\n";

  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const auto result = run(scenarios[index]);
    const bool ok = scenario_passes(scenarios[index], result.safety);
    if (!ok) {
      failures += 1;
    }
    if (result.safety.product_safe) {
      safe_scenarios += 1;
      if (first_safe) {
        minimum_hal_requeues_for_safe = result.counters.hal_steady_requeues;
        first_safe = false;
      } else {
        minimum_hal_requeues_for_safe =
            std::min(minimum_hal_requeues_for_safe, result.counters.hal_steady_requeues);
      }
    }
    print_row(scenarios[index], result.counters, result.safety, ok,
              index + 1U < scenarios.size());
  }

  const bool pass = failures == 0 && safe_scenarios >= 2 && minimum_hal_requeues_for_safe == 0;
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_scenarios\": " << safe_scenarios << ",\n"
            << "  \"minimum_hal_steady_requeues_for_safe\": " << minimum_hal_requeues_for_safe
            << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
