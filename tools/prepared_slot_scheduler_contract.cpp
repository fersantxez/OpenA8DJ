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
};

ScenarioResult run(const Scenario& scenario) {
  PreparedSlotScheduler scheduler;
  (void)scheduler.start(scenario.config);
  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    (void)scheduler.complete_period(scenario.options);
  }
  return ScenarioResult{scheduler.counters(), scheduler.safety()};
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
            << ", \"unavailable_capture_slots\": " << config.unavailable_capture_slots
            << ", \"unavailable_playback_slots\": " << config.unavailable_playback_slots
            << ", \"playback_completion_gap_periods\": "
            << config.playback_completion_gap_periods
            << ", \"max_backend_requeues_per_period\": "
            << config.max_backend_requeues_per_period
            << ", \"backend_prepare_enqueues\": " << counters.backend_prepare_enqueues
            << ", \"backend_steady_requeues\": " << counters.backend_steady_requeues
            << ", \"hal_steady_requeues\": " << counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << counters.fallback_allocations
            << ", \"capture_starved_periods\": " << counters.capture_starved_periods
            << ", \"playback_starved_periods\": " << counters.playback_starved_periods
            << ", \"backend_requeue_budget_violations\": "
            << counters.backend_requeue_budget_violations
            << ", \"completion_gap_violations\": " << counters.completion_gap_violations
            << ", \"max_completion_gap_ratio\": " << counters.max_completion_gap_ratio
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
      if (!saw_safe || result.counters.hal_steady_requeues < min_hal_requeues_for_safe) {
        min_hal_requeues_for_safe = result.counters.hal_steady_requeues;
        saw_safe = true;
      }
    }
    print_row(scenarios[index], result, ok, index + 1U < scenarios.size());
  }
  const bool pass = failures == 0 && safe_scenarios >= 2 && min_hal_requeues_for_safe == 0;
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_scenarios\": " << safe_scenarios << ",\n"
            << "  \"minimum_hal_steady_requeues_for_safe\": " << min_hal_requeues_for_safe
            << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
