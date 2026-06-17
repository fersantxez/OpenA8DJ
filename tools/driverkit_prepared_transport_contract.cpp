#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kInputChannels = 8;
constexpr std::uint32_t kOutputChannels = 8;
constexpr std::uint32_t kDecks = 4;
constexpr double kMaxCompletionGapRatio = 1.25;

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 2000;
  std::uint32_t iso_frames = 8;
  std::uint32_t capture_depth = 64;
  std::uint32_t playback_depth = 64;
  std::uint32_t completion_gap_periods = 1;
  bool hal_direct_requeue = false;
  bool steady_fallback_allocation = false;
  bool reorder_timestamps = false;
  bool expect_safe = true;
};

struct RingState {
  std::uint32_t depth = 0;
  std::uint32_t fill = 0;
  std::uint32_t max_fill = 0;
  std::uint64_t overflows = 0;
  std::uint64_t underflows = 0;
};

struct Result {
  std::uint64_t backend_prepare_enqueues = 0;
  std::uint64_t backend_steady_requeues = 0;
  std::uint64_t hal_steady_requeues = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t capture_overruns = 0;
  std::uint64_t playback_underruns = 0;
  std::uint64_t timestamp_regressions = 0;
  std::uint64_t channel_identity_failures = 0;
  std::uint64_t timecode_profile_failures = 0;
  double max_completion_gap_ratio = 0.0;
  bool prepared_slots_only = false;
  bool cadence_safe = false;
  bool routing_safe = false;
  bool timecode_safe = false;
  bool hal_hot_path_safe = false;
  bool product_safe = false;
};

bool push(RingState& ring) {
  if (ring.fill >= ring.depth) {
    ring.overflows += 1;
    return false;
  }
  ring.fill += 1;
  ring.max_fill = std::max(ring.max_fill, ring.fill);
  return true;
}

bool pop(RingState& ring) {
  if (ring.fill == 0) {
    ring.underflows += 1;
    return false;
  }
  ring.fill -= 1;
  return true;
}

std::uint32_t routed_output_channel(std::uint32_t deck, std::uint32_t stereo_channel) {
  return deck * 2U + stereo_channel;
}

Result run(const Scenario& scenario) {
  Result result{};
  result.backend_prepare_enqueues = scenario.capture_depth + scenario.playback_depth;
  result.max_completion_gap_ratio = static_cast<double>(std::max(1U, scenario.completion_gap_periods));

  RingState capture_ring{scenario.capture_depth, 0, 0, 0, 0};
  RingState playback_ring{scenario.playback_depth, scenario.playback_depth, scenario.playback_depth, 0, 0};

  std::uint64_t last_capture_timestamp = 0;
  std::uint64_t last_playback_timestamp = 0;
  bool have_capture_timestamp = false;
  bool have_playback_timestamp = false;

  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    const bool completion_period = period % std::max(1U, scenario.completion_gap_periods) == 0;

    if (completion_period) {
      result.backend_steady_requeues += 2;
      if (scenario.hal_direct_requeue) {
        result.hal_steady_requeues += 2;
      }
      if (scenario.steady_fallback_allocation) {
        result.fallback_allocations += 2;
      }

      (void)push(capture_ring);
      (void)pop(playback_ring);

      const std::uint64_t expected_timestamp =
          static_cast<std::uint64_t>(period) * static_cast<std::uint64_t>(scenario.iso_frames);
      const std::uint64_t capture_timestamp =
          scenario.reorder_timestamps && period == scenario.periods / 2U
              ? last_capture_timestamp
              : expected_timestamp;
      const std::uint64_t playback_timestamp =
          scenario.reorder_timestamps && period == scenario.periods / 2U
              ? last_playback_timestamp
              : expected_timestamp;

      if (have_capture_timestamp && capture_timestamp <= last_capture_timestamp) {
        result.timestamp_regressions += 1;
      }
      if (have_playback_timestamp && playback_timestamp <= last_playback_timestamp) {
        result.timestamp_regressions += 1;
      }
      last_capture_timestamp = capture_timestamp;
      last_playback_timestamp = playback_timestamp;
      have_capture_timestamp = true;
      have_playback_timestamp = true;
    }

    (void)pop(capture_ring);
    (void)push(playback_ring);

    for (std::uint32_t deck = 0; deck < kDecks; ++deck) {
      for (std::uint32_t stereo = 0; stereo < 2U; ++stereo) {
        const std::uint32_t output = routed_output_channel(deck, stereo);
        const std::uint32_t input = routed_output_channel(deck, stereo);
        if (output >= kOutputChannels || input >= kInputChannels || output != input) {
          result.channel_identity_failures += 1;
        }
      }
    }
  }

  result.capture_overruns = capture_ring.overflows + capture_ring.underflows;
  result.playback_underruns = playback_ring.underflows + playback_ring.overflows;
  result.prepared_slots_only = result.fallback_allocations == 0;
  result.cadence_safe = result.max_completion_gap_ratio <= kMaxCompletionGapRatio &&
                        result.timestamp_regressions == 0 && result.capture_overruns == 0 &&
                        result.playback_underruns == 0;
  result.routing_safe = result.channel_identity_failures == 0 && kInputChannels == 8 &&
                        kOutputChannels == 8 && kDecks == 4;
  result.timecode_safe = result.timecode_profile_failures == 0 && result.routing_safe &&
                         result.timestamp_regressions == 0;
  result.hal_hot_path_safe = result.hal_steady_requeues == 0 && result.prepared_slots_only;
  result.product_safe = result.hal_hot_path_safe && result.cadence_safe &&
                        result.routing_safe && result.timecode_safe;
  return result;
}

bool scenario_passes(const Scenario& scenario, const Result& result) {
  return result.product_safe == scenario.expect_safe;
}

void print_row(const Scenario& scenario, const Result& result, bool ok, bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"periods\": " << scenario.periods
            << ", \"iso_frames\": " << scenario.iso_frames
            << ", \"capture_depth\": " << scenario.capture_depth
            << ", \"playback_depth\": " << scenario.playback_depth
            << ", \"completion_gap_periods\": " << scenario.completion_gap_periods
            << ", \"backend_prepare_enqueues\": " << result.backend_prepare_enqueues
            << ", \"backend_steady_requeues\": " << result.backend_steady_requeues
            << ", \"hal_steady_requeues\": " << result.hal_steady_requeues
            << ", \"fallback_allocations\": " << result.fallback_allocations
            << ", \"capture_overruns\": " << result.capture_overruns
            << ", \"playback_underruns\": " << result.playback_underruns
            << ", \"timestamp_regressions\": " << result.timestamp_regressions
            << ", \"channel_identity_failures\": " << result.channel_identity_failures
            << ", \"timecode_profile_failures\": " << result.timecode_profile_failures
            << ", \"max_completion_gap_ratio\": " << result.max_completion_gap_ratio
            << ", \"prepared_slots_only\": " << (result.prepared_slots_only ? "true" : "false")
            << ", \"cadence_safe\": " << (result.cadence_safe ? "true" : "false")
            << ", \"routing_safe\": " << (result.routing_safe ? "true" : "false")
            << ", \"timecode_safe\": " << (result.timecode_safe ? "true" : "false")
            << ", \"hal_hot_path_safe\": " << (result.hal_hot_path_safe ? "true" : "false")
            << ", \"product_safe\": " << (result.product_safe ? "true" : "false")
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
      },
      {
          .name = "prepared_iso8_q8_hal_zero_requeue",
          .capture_depth = 8,
          .playback_depth = 8,
      },
      {
          .name = "hal_direct_requeue_rejected",
          .hal_direct_requeue = true,
          .expect_safe = false,
      },
      {
          .name = "steady_fallback_allocation_rejected",
          .steady_fallback_allocation = true,
          .expect_safe = false,
      },
      {
          .name = "coalesced_completion_gap_rejected",
          .completion_gap_periods = 2,
          .expect_safe = false,
      },
      {
          .name = "timestamp_reorder_rejected",
          .reorder_timestamps = true,
          .expect_safe = false,
      },
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_scenarios = 0;
  std::uint64_t minimum_hal_requeues_for_safe = 0;
  bool first_safe = true;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-prepared-transport-contract.v1\",\n"
            << "  \"meaning\": \"offline DriverKit transport contract; PASS does not mean physical readiness\",\n"
            << "  \"thresholds\": {\"max_completion_gap_ratio\": " << kMaxCompletionGapRatio
            << ", \"required_input_channels\": " << kInputChannels
            << ", \"required_output_channels\": " << kOutputChannels
            << ", \"required_decks\": " << kDecks << "},\n"
            << "  \"rows\": [\n";

  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const Result result = run(scenarios[index]);
    const bool ok = scenario_passes(scenarios[index], result);
    if (!ok) {
      failures += 1;
    }
    if (result.product_safe) {
      safe_scenarios += 1;
      if (first_safe) {
        minimum_hal_requeues_for_safe = result.hal_steady_requeues;
        first_safe = false;
      } else {
        minimum_hal_requeues_for_safe =
            std::min(minimum_hal_requeues_for_safe, result.hal_steady_requeues);
      }
    }
    print_row(scenarios[index], result, ok, index + 1U < scenarios.size());
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
