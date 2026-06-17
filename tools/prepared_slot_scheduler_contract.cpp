#include "opena8djcpp/prepared_transport.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace opena8djcpp;

namespace {

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 256;
  PreparedSlotSchedulerConfig config{};
  PreparedSlotSchedulerStepOptions options{};
  bool expect_safe = true;
};

struct ScenarioResult {
  PreparedSlotSchedulerCounters counters{};
  PreparedSlotSchedulerSafety safety{};
  double usb_submit_reduction_ratio = 0.0;
};

ScenarioResult run(const Scenario& scenario) {
  PreparedSlotScheduler scheduler;
  (void)scheduler.start(scenario.config);
  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    (void)scheduler.complete_period(scenario.options);
  }
  auto counters = scheduler.counters();
  const auto logical_enqueues = counters.backend_prepare_enqueues + counters.backend_steady_requeues;
  const double reduction =
      counters.usb_submit_calls == 0
          ? 0.0
          : static_cast<double>(logical_enqueues) / static_cast<double>(counters.usb_submit_calls);
  return ScenarioResult{counters, scheduler.safety(), reduction};
}

bool passes(const Scenario& scenario, const ScenarioResult& result) {
  return result.safety.product_safe == scenario.expect_safe;
}

void print_row(const Scenario& scenario,
               const ScenarioResult& result,
               bool ok,
               bool trailing_comma) {
  const auto& config = scenario.config;
  const auto& counters = result.counters;
  const auto& safety = result.safety;
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"periods\": " << scenario.periods
            << ", \"capture_target_slots\": " << config.capture_target_slots
            << ", \"playback_target_slots\": " << config.playback_target_slots
            << ", \"capture_pool_slots\": " << config.capture_pool_slots
            << ", \"playback_pool_slots\": " << config.playback_pool_slots
            << ", \"usb_slots_per_submit\": " << config.usb_slots_per_submit
            << ", \"unavailable_capture_slots\": " << config.unavailable_capture_slots
            << ", \"unavailable_playback_slots\": " << config.unavailable_playback_slots
            << ", \"playback_completion_gap_periods\": "
            << config.playback_completion_gap_periods
            << ", \"max_backend_requeues_per_period\": "
            << config.max_backend_requeues_per_period
            << ", \"backend_prepare_enqueues\": " << counters.backend_prepare_enqueues
            << ", \"backend_steady_requeues\": " << counters.backend_steady_requeues
            << ", \"logical_audio_periods\": " << counters.logical_audio_periods
            << ", \"usb_submit_calls\": " << counters.usb_submit_calls
            << ", \"usb_submit_reduction_ratio\": " << result.usb_submit_reduction_ratio
            << ", \"backend_slot_completions\": " << counters.backend_slot_completions
            << ", \"hal_steady_requeues\": " << counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << counters.fallback_allocations
            << ", \"capture_starved_periods\": " << counters.capture_starved_periods
            << ", \"playback_starved_periods\": " << counters.playback_starved_periods
            << ", \"backend_requeue_budget_violations\": "
            << counters.backend_requeue_budget_violations
            << ", \"completion_gap_violations\": " << counters.completion_gap_violations
            << ", \"logical_audio_gap_violations\": "
            << counters.logical_audio_gap_violations
            << ", \"slot_order_errors\": " << counters.slot_order_errors
            << ", \"max_completion_gap_ratio\": " << counters.max_completion_gap_ratio
            << ", \"max_logical_audio_gap_ratio\": " << counters.max_logical_audio_gap_ratio
            << ", \"min_capture_in_flight\": " << counters.min_capture_in_flight
            << ", \"min_playback_in_flight\": " << counters.min_playback_in_flight
            << ", \"max_capture_in_flight\": " << counters.max_capture_in_flight
            << ", \"max_playback_in_flight\": " << counters.max_playback_in_flight
            << ", \"prepared_slots_only\": " << (safety.prepared_slots_only ? "true" : "false")
            << ", \"lead_safe\": " << (safety.lead_safe ? "true" : "false")
            << ", \"cadence_safe\": " << (safety.cadence_safe ? "true" : "false")
            << ", \"hal_hot_path_safe\": " << (safety.hal_hot_path_safe ? "true" : "false")
            << ", \"backend_budget_safe\": "
            << (safety.backend_budget_safe ? "true" : "false")
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
          .name = "prepared_iso8_q8_stable",
          .config =
              PreparedSlotSchedulerConfig{
                  .capture_target_slots = 8,
                  .playback_target_slots = 8,
                  .capture_pool_slots = 16,
                  .playback_pool_slots = 16,
              },
      },
      {
          .name = "prepared_iso8_q64_stable",
          .config =
              PreparedSlotSchedulerConfig{
                  .capture_target_slots = 64,
                  .playback_target_slots = 64,
                  .capture_pool_slots = 64,
                  .playback_pool_slots = 64,
              },
      },
      {
          .name = "prepared_iso8_usb_batch8_stable",
          .config =
              PreparedSlotSchedulerConfig{
                  .capture_target_slots = 8,
                  .playback_target_slots = 8,
                  .capture_pool_slots = 16,
                  .playback_pool_slots = 16,
                  .usb_slots_per_submit = 8,
              },
      },
      {
          .name = "hal_requeue_rejected",
          .options =
              PreparedSlotSchedulerStepOptions{
                  .hal_direct_requeue_attempt = true,
              },
          .expect_safe = false,
      },
      {
          .name = "fallback_allocation_rejected",
          .options =
              PreparedSlotSchedulerStepOptions{
                  .fallback_allocation_attempt = true,
              },
          .expect_safe = false,
      },
      {
          .name = "pool_leak_rejected",
          .config =
              PreparedSlotSchedulerConfig{
                  .capture_target_slots = 8,
                  .playback_target_slots = 8,
                  .capture_pool_slots = 8,
                  .playback_pool_slots = 8,
                  .unavailable_capture_slots = 1,
              },
          .expect_safe = false,
      },
      {
          .name = "coalesced_completion_gap_rejected",
          .config =
              PreparedSlotSchedulerConfig{
                  .playback_completion_gap_periods = 2,
              },
          .expect_safe = false,
      },
      {
          .name = "slot_order_error_rejected",
          .config =
              PreparedSlotSchedulerConfig{
                  .usb_slots_per_submit = 8,
              },
          .options =
              PreparedSlotSchedulerStepOptions{
                  .force_slot_order_error = true,
              },
          .expect_safe = false,
      },
      {
          .name = "backend_budget_violation_rejected",
          .config =
              PreparedSlotSchedulerConfig{
                  .max_backend_requeues_per_period = 1,
              },
          .expect_safe = false,
      },
      {
          .name = "lead_starvation_rejected",
          .config =
              PreparedSlotSchedulerConfig{
                  .capture_target_slots = 2,
                  .playback_target_slots = 2,
              },
          .options =
              PreparedSlotSchedulerStepOptions{
                  .suppress_backend_requeue = true,
              },
          .expect_safe = false,
      },
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_scenarios = 0;
  std::uint64_t min_hal_requeues_for_safe = 0;
  std::uint64_t safe_logical_audio_gap_violations = 0;
  std::uint64_t safe_slot_order_errors = 0;
  double max_safe_logical_audio_gap_ratio = 0.0;
  double max_safe_usb_submit_reduction_ratio = 0.0;
  bool saw_safe = false;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-slot-scheduler-contract.v1\",\n"
            << "  \"meaning\": \"offline prepared-slot lead/requeue contract; PASS is not physical readiness\",\n"
            << "  \"thresholds\": {\"max_completion_gap_ratio\": "
            << kPreparedTransportMaxCompletionGapRatio << "},\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const auto result = run(scenarios[index]);
    const bool ok = passes(scenarios[index], result);
    if (!ok) {
      failures += 1;
    }
    if (result.safety.product_safe) {
      safe_scenarios += 1;
      safe_logical_audio_gap_violations += result.counters.logical_audio_gap_violations;
      safe_slot_order_errors += result.counters.slot_order_errors;
      max_safe_logical_audio_gap_ratio =
          std::max(max_safe_logical_audio_gap_ratio, result.counters.max_logical_audio_gap_ratio);
      max_safe_usb_submit_reduction_ratio =
          std::max(max_safe_usb_submit_reduction_ratio, result.usb_submit_reduction_ratio);
      if (!saw_safe || result.counters.hal_steady_requeues < min_hal_requeues_for_safe) {
        min_hal_requeues_for_safe = result.counters.hal_steady_requeues;
        saw_safe = true;
      }
    }
    print_row(scenarios[index], result, ok, index + 1U < scenarios.size());
  }
  const bool pass = failures == 0 && safe_scenarios >= 3 && min_hal_requeues_for_safe == 0 &&
                    safe_logical_audio_gap_violations == 0 && safe_slot_order_errors == 0 &&
                    max_safe_logical_audio_gap_ratio <= 1.0 &&
                    max_safe_usb_submit_reduction_ratio >= 8.0;
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_scenarios\": " << safe_scenarios << ",\n"
            << "  \"minimum_hal_steady_requeues_for_safe\": " << min_hal_requeues_for_safe
            << ",\n"
            << "  \"safe_logical_audio_gap_violations\": "
            << safe_logical_audio_gap_violations << ",\n"
            << "  \"safe_slot_order_errors\": " << safe_slot_order_errors << ",\n"
            << "  \"max_safe_logical_audio_gap_ratio\": "
            << max_safe_logical_audio_gap_ratio << ",\n"
            << "  \"max_safe_usb_submit_reduction_ratio\": "
            << max_safe_usb_submit_reduction_ratio << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
