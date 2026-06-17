#include "opena8djcpp/runtime_adapter.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace opena8djcpp;

namespace {

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 256;
  FakeRuntimeAdapterConfig config{};
  FakeRuntimeAdapterStepOptions options{};
  bool expect_safe = true;
};

struct ScenarioResult {
  FakeRuntimeAdapterCounters counters{};
  FakeRuntimeAdapterSafety safety{};
  PreparedSlotSchedulerCounters scheduler_counters{};
  bool scheduler_match = false;
  bool pass = false;
};

ScenarioResult run(const Scenario& scenario) {
  FakeRuntimeAdapter adapter;
  ScenarioResult result{};
  if (!adapter.start(scenario.config)) {
    return result;
  }
  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    (void)adapter.tick_period(scenario.options);
  }

  result.counters = adapter.counters();
  result.safety = adapter.safety();
  result.scheduler_counters = adapter.scheduler_counters();
  result.scheduler_match =
      result.counters.logical_audio_periods == result.scheduler_counters.logical_audio_periods &&
      result.counters.backend_prepare_enqueues ==
          result.scheduler_counters.backend_prepare_enqueues &&
      result.counters.backend_steady_requeues ==
          result.scheduler_counters.backend_steady_requeues &&
      result.counters.usb_submit_calls == result.scheduler_counters.usb_submit_calls &&
      result.counters.backend_slot_completions ==
          result.scheduler_counters.backend_slot_completions &&
      result.counters.logical_audio_gap_errors ==
          result.scheduler_counters.logical_audio_gap_violations &&
      result.counters.slot_order_errors == result.scheduler_counters.slot_order_errors;
  result.pass = result.scheduler_match && result.safety.product_safe == scenario.expect_safe;
  return result;
}

void print_row(const Scenario& scenario,
               const ScenarioResult& result,
               bool trailing_comma) {
  const auto& counters = result.counters;
  const auto& safety = result.safety;
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"periods\": " << scenario.periods
            << ", \"runtime_period_ticks\": " << counters.runtime_period_ticks
            << ", \"failed_ticks\": " << counters.failed_ticks
            << ", \"logical_audio_periods\": " << counters.logical_audio_periods
            << ", \"backend_prepare_enqueues\": " << counters.backend_prepare_enqueues
            << ", \"backend_steady_requeues\": " << counters.backend_steady_requeues
            << ", \"usb_submit_calls\": " << counters.usb_submit_calls
            << ", \"usb_submit_reduction_ratio\": "
            << counters.usb_submit_reduction_ratio
            << ", \"backend_slot_completions\": " << counters.backend_slot_completions
            << ", \"hal_steady_requeues\": " << counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << counters.fallback_allocations
            << ", \"capture_starved_periods\": " << counters.capture_starved_periods
            << ", \"playback_starved_periods\": " << counters.playback_starved_periods
            << ", \"backend_requeue_budget_violations\": "
            << counters.backend_requeue_budget_violations
            << ", \"logical_audio_gap_errors\": " << counters.logical_audio_gap_errors
            << ", \"slot_order_errors\": " << counters.slot_order_errors
            << ", \"max_logical_audio_gap_ratio\": "
            << counters.max_logical_audio_gap_ratio
            << ", \"counters_exposed\": " << (safety.counters_exposed ? "true" : "false")
            << ", \"logical_iso8_cadence_exposed\": "
            << (safety.logical_iso8_cadence_exposed ? "true" : "false")
            << ", \"usb_submit_batching_exposed\": "
            << (safety.usb_submit_batching_exposed ? "true" : "false")
            << ", \"scheduler_product_safe\": "
            << (safety.scheduler_product_safe ? "true" : "false")
            << ", \"adapter_product_safe\": " << (safety.product_safe ? "true" : "false")
            << ", \"scheduler_counter_match\": "
            << (result.scheduler_match ? "true" : "false")
            << ", \"expect_safe\": " << (scenario.expect_safe ? "true" : "false")
            << ", \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const std::vector<Scenario> scenarios = {
      {
          .name = "runtime_iso8_usb_batch8_stable",
          .config =
              FakeRuntimeAdapterConfig{
                  .scheduler =
                      PreparedSlotSchedulerConfig{
                          .capture_target_slots = 8,
                          .playback_target_slots = 8,
                          .capture_pool_slots = 16,
                          .playback_pool_slots = 16,
                          .usb_slots_per_submit = 8,
                      },
              },
      },
      {
          .name = "runtime_unbatched_submit_rejected",
          .config =
              FakeRuntimeAdapterConfig{
                  .scheduler =
                      PreparedSlotSchedulerConfig{
                          .capture_target_slots = 8,
                          .playback_target_slots = 8,
                          .capture_pool_slots = 16,
                          .playback_pool_slots = 16,
                          .usb_slots_per_submit = 1,
                      },
              },
          .expect_safe = false,
      },
      {
          .name = "runtime_logical_gap_rejected",
          .config =
              FakeRuntimeAdapterConfig{
                  .scheduler =
                      PreparedSlotSchedulerConfig{
                          .capture_target_slots = 8,
                          .playback_target_slots = 8,
                          .capture_pool_slots = 16,
                          .playback_pool_slots = 16,
                          .usb_slots_per_submit = 8,
                          .playback_completion_gap_periods = 2,
                      },
              },
          .expect_safe = false,
      },
      {
          .name = "runtime_slot_order_error_rejected",
          .config =
              FakeRuntimeAdapterConfig{
                  .scheduler =
                      PreparedSlotSchedulerConfig{
                          .capture_target_slots = 8,
                          .playback_target_slots = 8,
                          .capture_pool_slots = 16,
                          .playback_pool_slots = 16,
                          .usb_slots_per_submit = 8,
                      },
              },
          .options =
              FakeRuntimeAdapterStepOptions{
                  .scheduler =
                      PreparedSlotSchedulerStepOptions{
                          .force_slot_order_error = true,
                      },
              },
          .expect_safe = false,
      },
      {
          .name = "runtime_hal_requeue_rejected",
          .config =
              FakeRuntimeAdapterConfig{
                  .scheduler =
                      PreparedSlotSchedulerConfig{
                          .capture_target_slots = 8,
                          .playback_target_slots = 8,
                          .capture_pool_slots = 16,
                          .playback_pool_slots = 16,
                          .usb_slots_per_submit = 8,
                      },
              },
          .options =
              FakeRuntimeAdapterStepOptions{
                  .scheduler =
                      PreparedSlotSchedulerStepOptions{
                          .hal_direct_requeue_attempt = true,
                      },
              },
          .expect_safe = false,
      },
      {
          .name = "runtime_fallback_allocation_rejected",
          .config =
              FakeRuntimeAdapterConfig{
                  .scheduler =
                      PreparedSlotSchedulerConfig{
                          .capture_target_slots = 8,
                          .playback_target_slots = 8,
                          .capture_pool_slots = 16,
                          .playback_pool_slots = 16,
                          .usb_slots_per_submit = 8,
                      },
              },
          .options =
              FakeRuntimeAdapterStepOptions{
                  .scheduler =
                      PreparedSlotSchedulerStepOptions{
                          .fallback_allocation_attempt = true,
                      },
              },
          .expect_safe = false,
      },
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_rows = 0;
  std::uint64_t max_stable_usb_submit_calls = 0;
  std::uint64_t stable_logical_audio_periods = 0;
  std::uint64_t stable_backend_slot_completions = 0;
  double max_stable_usb_submit_reduction_ratio = 0.0;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.runtime-adapter-contract.v1\",\n"
            << "  \"meaning\": \"offline fake runtime adapter exposing logical ISO8 to USB submit batching counters; PASS is not physical readiness\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const auto result = run(scenarios[index]);
    failures += result.pass ? 0U : 1U;
    if (result.safety.product_safe) {
      safe_rows += 1;
    }
    if (std::string(scenarios[index].name) == "runtime_iso8_usb_batch8_stable") {
      max_stable_usb_submit_calls =
          std::max(max_stable_usb_submit_calls, result.counters.usb_submit_calls);
      stable_logical_audio_periods = result.counters.logical_audio_periods;
      stable_backend_slot_completions = result.counters.backend_slot_completions;
      max_stable_usb_submit_reduction_ratio =
          std::max(max_stable_usb_submit_reduction_ratio,
                   result.counters.usb_submit_reduction_ratio);
    }
    print_row(scenarios[index], result, index + 1U < scenarios.size());
  }

  const bool pass = failures == 0 && safe_rows == 1 &&
                    stable_logical_audio_periods == 256 &&
                    stable_backend_slot_completions == 512 &&
                    max_stable_usb_submit_calls == 66 &&
                    max_stable_usb_submit_reduction_ratio >= 8.0;
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_rows\": " << safe_rows << ",\n"
            << "  \"stable_logical_audio_periods\": " << stable_logical_audio_periods
            << ",\n"
            << "  \"stable_backend_slot_completions\": "
            << stable_backend_slot_completions << ",\n"
            << "  \"stable_usb_submit_calls\": " << max_stable_usb_submit_calls
            << ",\n"
            << "  \"stable_usb_submit_reduction_ratio\": "
            << max_stable_usb_submit_reduction_ratio << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
