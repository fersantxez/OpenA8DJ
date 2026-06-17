#include "opena8djcpp/prepared_transport.hpp"
#include "opena8djcpp/usb_submit_plan.hpp"

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
  PreparedSlotSchedulerConfig scheduler{};
  bool force_unbatched = false;
  bool force_order_error = false;
  bool force_timestamp_regression = false;
  bool force_partial_flush = false;
  bool expect_safe = true;
};

struct ScenarioResult {
  PreparedSlotSchedulerCounters scheduler_counters{};
  PreparedSlotSchedulerSafety scheduler_safety{};
  PreparedUsbSubmitPlannerCounters planner_counters{};
  PreparedUsbSubmitPlannerSafety planner_safety{};
  bool counters_match = false;
  bool pass = false;
};

std::uint64_t timestamp_for(std::uint64_t sequence, std::uint32_t frames_per_slot) {
  return (sequence + 1U) * static_cast<std::uint64_t>(frames_per_slot);
}

void queue_initial_slots(PreparedUsbSubmitPlanner& planner,
                         const PreparedSlotSchedulerConfig& config,
                         std::uint32_t frames_per_slot) {
  for (std::uint32_t index = 0; index < config.capture_target_slots; ++index) {
    (void)planner.queue_slot(UsbSlotDirection::Capture, timestamp_for(index, frames_per_slot));
  }
  for (std::uint32_t index = 0; index < config.playback_target_slots; ++index) {
    (void)planner.queue_slot(UsbSlotDirection::Playback, timestamp_for(index, frames_per_slot));
  }
}

void queue_steady_slots(PreparedUsbSubmitPlanner& planner,
                        const PreparedSlotSchedulerConfig& config,
                        std::uint64_t capture_sequence,
                        std::uint64_t playback_sequence,
                        std::uint32_t frames_per_slot,
                        const Scenario& scenario) {
  if (scenario.force_order_error && capture_sequence == config.capture_target_slots + 4U) {
    (void)planner.queue_slot_with_sequence(
        UsbSlotDirection::Capture, capture_sequence + 1U,
        timestamp_for(capture_sequence, frames_per_slot));
  } else if (scenario.force_timestamp_regression &&
             capture_sequence == config.capture_target_slots + 4U) {
    (void)planner.queue_slot_with_sequence(UsbSlotDirection::Capture, capture_sequence,
                                           timestamp_for(capture_sequence - 1U, frames_per_slot));
  } else {
    (void)planner.queue_slot(UsbSlotDirection::Capture,
                             timestamp_for(capture_sequence, frames_per_slot));
  }

  if ((capture_sequence - config.capture_target_slots + 1U) %
          config.playback_completion_gap_periods ==
      0U) {
    (void)planner.queue_slot(UsbSlotDirection::Playback,
                             timestamp_for(playback_sequence, frames_per_slot));
  }
}

ScenarioResult run(const Scenario& scenario) {
  ScenarioResult result{};
  auto scheduler_config = scenario.scheduler;
  if (scenario.force_unbatched) {
    scheduler_config.usb_slots_per_submit = 1;
  }

  PreparedSlotScheduler scheduler;
  PreparedUsbSubmitPlanner planner;
  if (!scheduler.start(scheduler_config)) {
    return result;
  }
  if (!planner.start(PreparedUsbSubmitPlannerConfig{
          .slots_per_submit = scheduler_config.usb_slots_per_submit,
          .frames_per_slot = 8,
          .bytes_per_slot = kMode2DefaultTransferBytes,
      })) {
    return result;
  }

  queue_initial_slots(planner, scheduler_config, planner.config().frames_per_slot);
  std::uint64_t capture_sequence = scheduler_config.capture_target_slots;
  std::uint64_t playback_sequence = scheduler_config.playback_target_slots;
  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    (void)scheduler.complete_period();
    if (!scenario.force_partial_flush || period + 1U < scenario.periods) {
      queue_steady_slots(planner, scheduler_config, capture_sequence, playback_sequence,
                         planner.config().frames_per_slot, scenario);
      capture_sequence += 1;
      if (((period + 1U) % scheduler_config.playback_completion_gap_periods) == 0U) {
        playback_sequence += 1;
      }
    }
  }
  planner.finish();

  result.scheduler_counters = scheduler.counters();
  result.scheduler_safety = scheduler.safety();
  result.planner_counters = planner.counters();
  result.planner_safety = planner.safety();
  const auto scheduler_logical_enqueues =
      result.scheduler_counters.backend_prepare_enqueues +
      result.scheduler_counters.backend_steady_requeues;
  result.counters_match =
      scheduler_logical_enqueues == result.planner_counters.logical_slots &&
      result.scheduler_counters.usb_submit_calls == result.planner_counters.usb_submit_calls;
  if (scenario.expect_safe) {
    result.pass =
        result.counters_match && result.scheduler_safety.product_safe &&
        result.planner_safety.product_safe;
  } else {
    result.pass = !result.counters_match || !result.scheduler_safety.product_safe ||
                  !result.planner_safety.product_safe;
  }
  return result;
}

void print_row(const Scenario& scenario,
               const ScenarioResult& result,
               bool trailing_comma) {
  const auto& scheduler = result.scheduler_counters;
  const auto& planner = result.planner_counters;
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"periods\": " << scenario.periods
            << ", \"scheduler_logical_audio_periods\": " << scheduler.logical_audio_periods
            << ", \"scheduler_backend_slot_completions\": "
            << scheduler.backend_slot_completions
            << ", \"scheduler_usb_submit_calls\": " << scheduler.usb_submit_calls
            << ", \"planner_logical_slots\": " << planner.logical_slots
            << ", \"planner_capture_logical_slots\": " << planner.capture_logical_slots
            << ", \"planner_playback_logical_slots\": " << planner.playback_logical_slots
            << ", \"planner_usb_submit_calls\": " << planner.usb_submit_calls
            << ", \"planner_capture_usb_submit_calls\": "
            << planner.capture_usb_submit_calls
            << ", \"planner_playback_usb_submit_calls\": "
            << planner.playback_usb_submit_calls
            << ", \"planner_partial_submit_calls\": " << planner.partial_submit_calls
            << ", \"planner_descriptor_overflows\": " << planner.descriptor_overflows
            << ", \"planner_slot_order_errors\": " << planner.slot_order_errors
            << ", \"planner_timestamp_regressions\": " << planner.timestamp_regressions
            << ", \"planner_total_frames\": " << planner.total_frames
            << ", \"planner_total_bytes\": " << planner.total_bytes
            << ", \"planner_usb_submit_reduction_ratio\": "
            << planner.usb_submit_reduction_ratio
            << ", \"scheduler_product_safe\": "
            << (result.scheduler_safety.product_safe ? "true" : "false")
            << ", \"planner_product_safe\": "
            << (result.planner_safety.product_safe ? "true" : "false")
            << ", \"counters_match\": " << (result.counters_match ? "true" : "false")
            << ", \"expect_safe\": " << (scenario.expect_safe ? "true" : "false")
            << ", \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const PreparedSlotSchedulerConfig batch8{
      .capture_target_slots = 8,
      .playback_target_slots = 8,
      .capture_pool_slots = 16,
      .playback_pool_slots = 16,
      .usb_slots_per_submit = 8,
  };
  const std::vector<Scenario> scenarios = {
      {.name = "usb_submit_batch8_stable", .scheduler = batch8},
      {.name = "usb_submit_unbatched_rejected",
       .scheduler = batch8,
       .force_unbatched = true,
       .expect_safe = false},
      {.name = "usb_submit_order_error_rejected",
       .scheduler = batch8,
       .force_order_error = true,
       .expect_safe = false},
      {.name = "usb_submit_timestamp_regression_rejected",
       .scheduler = batch8,
       .force_timestamp_regression = true,
       .expect_safe = false},
      {.name = "usb_submit_partial_flush_rejected",
       .periods = 255,
       .scheduler = batch8,
       .force_partial_flush = true,
       .expect_safe = false},
  };

  std::uint32_t failures = 0;
  std::uint32_t safe_rows = 0;
  std::uint64_t stable_logical_slots = 0;
  std::uint64_t stable_submit_calls = 0;
  std::uint64_t stable_total_bytes = 0;
  double stable_reduction = 0.0;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.usb-submit-plan-contract.v1\",\n"
            << "  \"meaning\": \"offline prepared USB submit descriptor plan; PASS means logical ISO8 slot ordering maps to batched submit descriptors without touching USB hardware\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const auto result = run(scenarios[index]);
    failures += result.pass ? 0U : 1U;
    if (result.planner_safety.product_safe) {
      safe_rows += 1;
    }
    if (std::string(scenarios[index].name) == "usb_submit_batch8_stable") {
      stable_logical_slots = result.planner_counters.logical_slots;
      stable_submit_calls = result.planner_counters.usb_submit_calls;
      stable_total_bytes = result.planner_counters.total_bytes;
      stable_reduction = result.planner_counters.usb_submit_reduction_ratio;
    }
    print_row(scenarios[index], result, index + 1U < scenarios.size());
  }

  const bool pass = failures == 0 && safe_rows == 1 && stable_logical_slots == 528 &&
                    stable_submit_calls == 66 && stable_total_bytes == 185856 &&
                    stable_reduction >= 8.0;
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"safe_rows\": " << safe_rows << ",\n"
            << "  \"stable_logical_slots\": " << stable_logical_slots << ",\n"
            << "  \"stable_usb_submit_calls\": " << stable_submit_calls << ",\n"
            << "  \"stable_total_bytes\": " << stable_total_bytes << ",\n"
            << "  \"stable_usb_submit_reduction_ratio\": " << stable_reduction << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
