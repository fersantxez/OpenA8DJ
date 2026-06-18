#include "opena8djcpp/playback_scheduler_runtime.hpp"

#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

using namespace opena8djcpp;

namespace {

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 256;
  PlaybackSchedulerRuntimeConfig config{};
  PlaybackLeadSchedulerStepOptions options{};
  bool expect_safe = true;
};

struct ScenarioResult {
  bool started = false;
  PlaybackSchedulerRuntimeCounters counters{};
  PlaybackSchedulerRuntimeSafety safety{};
};

ScenarioResult run(const Scenario& scenario) {
  PlaybackSchedulerRuntimeBinding runtime;
  const bool started = runtime.start(scenario.config);
  for (std::uint32_t period = 0; started && period < scenario.periods; ++period) {
    (void)runtime.complete_period(scenario.options);
  }
  return ScenarioResult{started, runtime.counters(), runtime.safety()};
}

bool passes(const Scenario& scenario, const ScenarioResult& result) {
  if (!result.started) {
    return !scenario.expect_safe;
  }
  return result.safety.product_model_safe == scenario.expect_safe;
}

void print_row(const Scenario& scenario,
               const ScenarioResult& result,
               bool ok,
               bool trailing_comma) {
  const auto& counters = result.counters;
  const auto& safety = result.safety;
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"started\": " << (result.started ? "true" : "false")
            << ", \"expect_safe\": " << (scenario.expect_safe ? "true" : "false")
            << ", \"periods\": " << scenario.periods
            << ", \"scheduler_capture_submit_calls\": "
            << counters.scheduler.capture_request_submit_calls
            << ", \"runtime_capture_submit_calls\": "
            << counters.capture_runtime_submit_calls
            << ", \"scheduler_playback_submit_calls\": "
            << counters.scheduler.playback_request_submit_calls
            << ", \"runtime_playback_submit_calls\": "
            << counters.playback_runtime_submit_calls
            << ", \"playback_logical_slots_submitted\": "
            << counters.scheduler.playback_logical_slots_submitted
            << ", \"runtime_total_submit_reduction_ratio\": "
            << counters.runtime_total_submit_reduction_ratio
            << ", \"playback_submit_reduction_ratio\": "
            << counters.scheduler.playback_submit_reduction_ratio
            << ", \"runtime_submit_failures\": " << counters.runtime_submit_failures
            << ", \"runtime_completion_failures\": "
            << counters.runtime_completion_failures
            << ", \"fallback_allocations\": "
            << counters.request_pool.fallback_allocations
            << ", \"invalid_completions\": "
            << counters.request_pool.invalid_completions
            << ", \"stale_completions\": " << counters.request_pool.stale_completions
            << ", \"live_requests\": " << counters.request_pool.live_requests
            << ", \"submitted_frames\": " << counters.request_pool.submitted_frames
            << ", \"completed_frames\": " << counters.request_pool.completed_frames
            << ", \"submitted_bytes\": " << counters.request_pool.submitted_bytes
            << ", \"completed_bytes\": " << counters.request_pool.completed_bytes
            << ", \"scheduler_safe\": " << (safety.scheduler_safe ? "true" : "false")
            << ", \"request_pool_safe\": "
            << (safety.request_pool_safe ? "true" : "false")
            << ", \"capture_continuity_preserved\": "
            << (safety.capture_continuity_preserved ? "true" : "false")
            << ", \"playback_batching_bound\": "
            << (safety.playback_batching_bound ? "true" : "false")
            << ", \"runtime_accounting_matches_scheduler\": "
            << (safety.runtime_accounting_matches_scheduler ? "true" : "false")
            << ", \"no_runtime_failures\": "
            << (safety.no_runtime_failures ? "true" : "false")
            << ", \"product_model_safe\": "
            << (safety.product_model_safe ? "true" : "false")
            << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const PlaybackSchedulerRuntimeConfig stable_config{
      .scheduler =
          PlaybackLeadSchedulerConfig{
              .target_lead_slots = 8,
              .low_watermark_slots = 4,
              .high_watermark_slots = 12,
              .max_slots_per_submit = 8,
              .playback_pool_slots = 16,
          },
      .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 8},
  };
  const std::vector<Scenario> scenarios = {
      {
          .name = "iso8_runtime_binding_capture_single_playback_batch8_stable",
          .config = stable_config,
      },
      {
          .name = "suppressed_refill_runtime_underflow_rejected",
          .config = stable_config,
          .options = PlaybackLeadSchedulerStepOptions{.suppress_playback_submit = true},
          .expect_safe = false,
      },
      {
          .name = "capture_gap_runtime_rejected",
          .config = stable_config,
          .options = PlaybackLeadSchedulerStepOptions{.force_capture_gap = true},
          .expect_safe = false,
      },
      {
          .name = "non_iso8_runtime_start_rejected",
          .config =
              PlaybackSchedulerRuntimeConfig{
                  .scheduler = PlaybackLeadSchedulerConfig{.iso_frames = 10},
                  .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 8},
              },
          .expect_safe = false,
      },
      {
          .name = "zero_request_pool_runtime_start_rejected",
          .config =
              PlaybackSchedulerRuntimeConfig{
                  .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 0},
              },
          .expect_safe = false,
      },
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_scenarios = 0;
  std::uint64_t stable_capture_submits = 0;
  std::uint64_t stable_playback_submits = 0;
  std::uint64_t stable_playback_slots = 0;
  std::uint64_t stable_submitted_frames = 0;
  std::uint64_t stable_completed_frames = 0;
  std::uint64_t stable_submitted_bytes = 0;
  std::uint64_t stable_completed_bytes = 0;
  double stable_playback_reduction = 0.0;
  double stable_total_reduction = 0.0;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.playback-scheduler-runtime-contract.v1\",\n"
            << "  \"meaning\": \"offline runtime binding model; PASS is not hardware or product readiness\",\n"
            << "  \"safety\": \"offline_core_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const auto result = run(scenarios[index]);
    const bool ok = passes(scenarios[index], result);
    if (!ok) {
      failures += 1;
    }
    if (result.safety.product_model_safe) {
      safe_scenarios += 1;
    }
    if (std::string_view(scenarios[index].name) ==
        "iso8_runtime_binding_capture_single_playback_batch8_stable") {
      stable_capture_submits = result.counters.capture_runtime_submit_calls;
      stable_playback_submits = result.counters.playback_runtime_submit_calls;
      stable_playback_slots = result.counters.scheduler.playback_logical_slots_submitted;
      stable_submitted_frames = result.counters.request_pool.submitted_frames;
      stable_completed_frames = result.counters.request_pool.completed_frames;
      stable_submitted_bytes = result.counters.request_pool.submitted_bytes;
      stable_completed_bytes = result.counters.request_pool.completed_bytes;
      stable_playback_reduction =
          result.counters.scheduler.playback_submit_reduction_ratio;
      stable_total_reduction = result.counters.runtime_total_submit_reduction_ratio;
    }
    print_row(scenarios[index], result, ok, index + 1U < scenarios.size());
  }

  const bool pass = failures == 0 && safe_scenarios == 1 &&
                    stable_capture_submits == 256 && stable_playback_submits == 33 &&
                    stable_playback_slots == 264 && stable_playback_reduction >= 8.0 &&
                    stable_total_reduction > 1.5 &&
                    stable_submitted_frames == stable_completed_frames &&
                    stable_submitted_bytes == stable_completed_bytes;

  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_scenarios\": " << safe_scenarios << ",\n"
            << "  \"stable_capture_runtime_submit_calls\": " << stable_capture_submits
            << ",\n"
            << "  \"stable_playback_runtime_submit_calls\": " << stable_playback_submits
            << ",\n"
            << "  \"stable_playback_logical_slots_submitted\": " << stable_playback_slots
            << ",\n"
            << "  \"stable_playback_submit_reduction_ratio\": "
            << stable_playback_reduction << ",\n"
            << "  \"stable_total_submit_reduction_ratio\": " << stable_total_reduction
            << ",\n"
            << "  \"stable_submitted_frames\": " << stable_submitted_frames << ",\n"
            << "  \"stable_completed_frames\": " << stable_completed_frames << ",\n"
            << "  \"stable_submitted_bytes\": " << stable_submitted_bytes << ",\n"
            << "  \"stable_completed_bytes\": " << stable_completed_bytes << ",\n"
            << "  \"physical_evidence_present\": false,\n"
            << "  \"product_claim_allowed\": false,\n"
            << "  \"next_required_action\": "
               "\"IMPLEMENT_OPT_IN_HAL_PLAYBACK_SCHEDULER_BINDING_THEN_LOCK_GATED_SOURCE_REFERENCE_AB\",\n"
            << "  \"blocked_claim\": "
               "\"NO_CPU_AUDIOPHILE_OR_TIMECODE_CLAIM_UNTIL_RUNTIME_BINDING_IS_EXPOSED_AS_OPT_IN_HAL_CANDIDATE_AND_PASSES_LOCK_GATED_PHYSICAL_AB\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
