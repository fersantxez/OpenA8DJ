#include "opena8djcpp/playback_scheduler.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

using namespace opena8djcpp;

namespace {

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 256;
  PlaybackLeadSchedulerConfig config{};
  PlaybackLeadSchedulerStepOptions options{};
  bool expect_safe = true;
};

struct ScenarioResult {
  bool started = false;
  PlaybackLeadSchedulerCounters counters{};
  PlaybackLeadSchedulerSafety safety{};
};

ScenarioResult run(const Scenario& scenario) {
  PlaybackLeadScheduler scheduler;
  const bool started = scheduler.start(scenario.config);
  for (std::uint32_t period = 0; started && period < scenario.periods; ++period) {
    (void)scheduler.complete_period(scenario.options);
  }
  return ScenarioResult{started, scheduler.counters(), scheduler.safety()};
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
  const auto& config = scenario.config;
  const auto& counters = result.counters;
  const auto& safety = result.safety;
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"started\": " << (result.started ? "true" : "false")
            << ", \"periods\": " << scenario.periods
            << ", \"iso_frames\": " << config.iso_frames
            << ", \"target_lead_slots\": " << config.target_lead_slots
            << ", \"low_watermark_slots\": " << config.low_watermark_slots
            << ", \"high_watermark_slots\": " << config.high_watermark_slots
            << ", \"max_slots_per_submit\": " << config.max_slots_per_submit
            << ", \"playback_pool_slots\": " << config.playback_pool_slots
            << ", \"playback_completion_gap_periods\": "
            << config.playback_completion_gap_periods
            << ", \"logical_audio_periods\": " << counters.logical_audio_periods
            << ", \"capture_request_submit_calls\": " << counters.capture_request_submit_calls
            << ", \"playback_logical_slots_submitted\": "
            << counters.playback_logical_slots_submitted
            << ", \"playback_request_submit_calls\": "
            << counters.playback_request_submit_calls
            << ", \"playback_submit_events_from_audio_callback\": "
            << counters.playback_submit_events_from_audio_callback
            << ", \"playback_slots_completed\": " << counters.playback_slots_completed
            << ", \"playback_underflow_periods\": " << counters.playback_underflow_periods
            << ", \"playback_pool_overflows\": " << counters.playback_pool_overflows
            << ", \"playback_batch_overflows\": " << counters.playback_batch_overflows
            << ", \"capture_gap_periods\": " << counters.capture_gap_periods
            << ", \"logical_cadence_violations\": " << counters.logical_cadence_violations
            << ", \"min_playback_lead_slots\": " << counters.min_playback_lead_slots
            << ", \"max_playback_lead_slots\": " << counters.max_playback_lead_slots
            << ", \"max_slots_per_submit_observed\": "
            << counters.max_slots_per_submit_observed
            << ", \"playback_submit_reduction_ratio\": "
            << counters.playback_submit_reduction_ratio
            << ", \"total_submit_reduction_ratio\": " << counters.total_submit_reduction_ratio
            << ", \"iso8_cadence_preserved\": "
            << (safety.iso8_cadence_preserved ? "true" : "false")
            << ", \"capture_continuity_preserved\": "
            << (safety.capture_continuity_preserved ? "true" : "false")
            << ", \"playback_lead_safe\": " << (safety.playback_lead_safe ? "true" : "false")
            << ", \"playback_batching_effective\": "
            << (safety.playback_batching_effective ? "true" : "false")
            << ", \"pool_safe\": " << (safety.pool_safe ? "true" : "false")
            << ", \"callback_work_reduced\": "
            << (safety.callback_work_reduced ? "true" : "false")
            << ", \"product_model_safe\": "
            << (safety.product_model_safe ? "true" : "false")
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
          .name = "iso8_playback_lead_batch8_stable",
          .config =
              PlaybackLeadSchedulerConfig{
                  .target_lead_slots = 8,
                  .low_watermark_slots = 4,
                  .high_watermark_slots = 12,
                  .max_slots_per_submit = 8,
                  .playback_pool_slots = 16,
              },
      },
      {
          .name = "iso8_playback_lead_batch4_safe_but_weaker",
          .config =
              PlaybackLeadSchedulerConfig{
                  .target_lead_slots = 4,
                  .low_watermark_slots = 2,
                  .high_watermark_slots = 6,
                  .max_slots_per_submit = 4,
                  .playback_pool_slots = 8,
              },
      },
      {
          .name = "unbatched_playback_rejected",
          .config =
              PlaybackLeadSchedulerConfig{
                  .target_lead_slots = 8,
                  .low_watermark_slots = 4,
                  .high_watermark_slots = 8,
                  .max_slots_per_submit = 1,
                  .playback_pool_slots = 12,
              },
          .expect_safe = false,
      },
      {
          .name = "suppressed_refill_underflow_rejected",
          .options =
              PlaybackLeadSchedulerStepOptions{
                  .suppress_playback_submit = true,
              },
          .expect_safe = false,
      },
      {
          .name = "non_iso8_cadence_rejected",
          .config =
              PlaybackLeadSchedulerConfig{
                  .iso_frames = 10,
              },
          .expect_safe = false,
      },
      {
          .name = "playback_completion_gap_rejected",
          .config =
              PlaybackLeadSchedulerConfig{
                  .playback_completion_gap_periods = 2,
              },
          .expect_safe = false,
      },
      {
          .name = "capture_gap_rejected",
          .options =
              PlaybackLeadSchedulerStepOptions{
                  .force_capture_gap = true,
              },
          .expect_safe = false,
      },
      {
          .name = "batch_overflow_rejected",
          .options =
              PlaybackLeadSchedulerStepOptions{
                  .force_playback_batch_overflow = true,
              },
          .expect_safe = false,
      },
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_scenarios = 0;
  double max_safe_playback_submit_reduction = 0.0;
  double max_safe_total_submit_reduction = 0.0;
  std::uint64_t safe_underflows = 0;
  std::uint64_t safe_capture_gaps = 0;
  std::uint64_t safe_cadence_violations = 0;
  std::uint32_t stable_min_lead = 0;
  std::uint32_t stable_max_lead = 0;
  std::uint64_t stable_playback_submit_calls = 0;
  std::uint64_t stable_capture_submit_calls = 0;
  double stable_playback_reduction = 0.0;
  double stable_total_reduction = 0.0;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.playback-scheduler-contract.v1\",\n"
            << "  \"meaning\": \"offline playback lead scheduler model; PASS is not hardware or product readiness\",\n"
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
      safe_underflows += result.counters.playback_underflow_periods;
      safe_capture_gaps += result.counters.capture_gap_periods;
      safe_cadence_violations += result.counters.logical_cadence_violations;
      max_safe_playback_submit_reduction =
          std::max(max_safe_playback_submit_reduction,
                   result.counters.playback_submit_reduction_ratio);
      max_safe_total_submit_reduction =
          std::max(max_safe_total_submit_reduction,
                   result.counters.total_submit_reduction_ratio);
    }
    if (std::string_view(scenarios[index].name) == "iso8_playback_lead_batch8_stable") {
      stable_min_lead = result.counters.min_playback_lead_slots;
      stable_max_lead = result.counters.max_playback_lead_slots;
      stable_playback_submit_calls = result.counters.playback_request_submit_calls;
      stable_capture_submit_calls = result.counters.capture_request_submit_calls;
      stable_playback_reduction = result.counters.playback_submit_reduction_ratio;
      stable_total_reduction = result.counters.total_submit_reduction_ratio;
    }
    print_row(scenarios[index], result, ok, index + 1U < scenarios.size());
  }

  const bool pass = failures == 0 && safe_scenarios >= 2 && safe_underflows == 0 &&
                    safe_capture_gaps == 0 && safe_cadence_violations == 0 &&
                    max_safe_playback_submit_reduction >= 8.0 &&
                    max_safe_total_submit_reduction > 1.5 && stable_min_lead >= 4 &&
                    stable_max_lead <= 12 && stable_playback_submit_calls <= 33 &&
                    stable_capture_submit_calls == 256;

  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_scenarios\": " << safe_scenarios << ",\n"
            << "  \"stable_min_playback_lead_slots\": " << stable_min_lead << ",\n"
            << "  \"stable_max_playback_lead_slots\": " << stable_max_lead << ",\n"
            << "  \"stable_capture_request_submit_calls\": " << stable_capture_submit_calls
            << ",\n"
            << "  \"stable_playback_request_submit_calls\": " << stable_playback_submit_calls
            << ",\n"
            << "  \"stable_playback_submit_reduction_ratio\": "
            << stable_playback_reduction << ",\n"
            << "  \"stable_total_submit_reduction_ratio\": " << stable_total_reduction
            << ",\n"
            << "  \"max_safe_playback_submit_reduction_ratio\": "
            << max_safe_playback_submit_reduction << ",\n"
            << "  \"max_safe_total_submit_reduction_ratio\": "
            << max_safe_total_submit_reduction << ",\n"
            << "  \"safe_playback_underflow_periods\": " << safe_underflows << ",\n"
            << "  \"safe_capture_gap_periods\": " << safe_capture_gaps << ",\n"
            << "  \"safe_logical_cadence_violations\": " << safe_cadence_violations << ",\n"
            << "  \"physical_evidence_present\": false,\n"
            << "  \"product_claim_allowed\": false,\n"
            << "  \"blocked_claim\": "
               "\"NO_CPU_AUDIOPHILE_OR_TIMECODE_CLAIM_UNTIL_PLAYBACK_SCHEDULER_IS_BOUND_TO_OPT_IN_RUNTIME_AND_PASSES_LOCK_GATED_PHYSICAL_AB\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
