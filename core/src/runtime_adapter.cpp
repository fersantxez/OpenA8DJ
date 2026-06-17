#include "opena8djcpp/runtime_adapter.hpp"

namespace opena8djcpp {

bool FakeRuntimeAdapter::start(const FakeRuntimeAdapterConfig& config) {
  if (started_) {
    return false;
  }
  if (!scheduler_.start(config.scheduler)) {
    return false;
  }

  config_ = config;
  counters_ = {};
  counters_.runtime_start_calls = 1;
  started_ = true;
  refresh_counters();
  return true;
}

void FakeRuntimeAdapter::stop() {
  if (!started_) {
    return;
  }
  scheduler_.stop();
  started_ = false;
  counters_.runtime_stop_calls += 1;
  refresh_counters();
}

bool FakeRuntimeAdapter::tick_period(const FakeRuntimeAdapterStepOptions& options) {
  if (!started_) {
    counters_.failed_ticks += 1;
    return false;
  }
  if (!scheduler_.complete_period(options.scheduler)) {
    counters_.failed_ticks += 1;
    refresh_counters();
    return false;
  }
  counters_.runtime_period_ticks += 1;
  refresh_counters();
  return true;
}

FakeRuntimeAdapterSafety FakeRuntimeAdapter::safety() const {
  FakeRuntimeAdapterSafety out{};
  if (!started_) {
    return out;
  }

  const auto scheduler_safety = scheduler_.safety();
  const auto logical_enqueues =
      counters_.backend_prepare_enqueues + counters_.backend_steady_requeues;
  out.counters_exposed = counters_.logical_audio_periods == counters_.runtime_period_ticks &&
                         counters_.backend_slot_completions > 0 &&
                         counters_.usb_submit_calls > 0;
  out.logical_iso8_cadence_exposed =
      counters_.logical_audio_periods == counters_.runtime_period_ticks &&
      counters_.logical_audio_gap_errors == 0;
  out.usb_submit_batching_exposed =
      logical_enqueues > counters_.usb_submit_calls &&
      counters_.usb_submit_reduction_ratio >=
          static_cast<double>(config_.scheduler.usb_slots_per_submit) &&
      config_.scheduler.usb_slots_per_submit > 1;
  out.scheduler_product_safe = scheduler_safety.product_safe;
  out.product_safe = out.counters_exposed && out.logical_iso8_cadence_exposed &&
                     out.usb_submit_batching_exposed && out.scheduler_product_safe;
  return out;
}

void FakeRuntimeAdapter::refresh_counters() {
  const auto scheduler_counters = scheduler_.counters();
  const auto start_calls = counters_.runtime_start_calls;
  const auto stop_calls = counters_.runtime_stop_calls;
  const auto period_ticks = counters_.runtime_period_ticks;
  const auto failed_ticks = counters_.failed_ticks;

  counters_ = {};
  counters_.runtime_start_calls = start_calls;
  counters_.runtime_stop_calls = stop_calls;
  counters_.runtime_period_ticks = period_ticks;
  counters_.failed_ticks = failed_ticks;
  counters_.logical_audio_periods = scheduler_counters.logical_audio_periods;
  counters_.backend_prepare_enqueues = scheduler_counters.backend_prepare_enqueues;
  counters_.backend_steady_requeues = scheduler_counters.backend_steady_requeues;
  counters_.usb_submit_calls = scheduler_counters.usb_submit_calls;
  counters_.backend_slot_completions = scheduler_counters.backend_slot_completions;
  counters_.hal_steady_requeues = scheduler_counters.hal_steady_requeues;
  counters_.fallback_allocations = scheduler_counters.fallback_allocations;
  counters_.capture_starved_periods = scheduler_counters.capture_starved_periods;
  counters_.playback_starved_periods = scheduler_counters.playback_starved_periods;
  counters_.backend_requeue_budget_violations =
      scheduler_counters.backend_requeue_budget_violations;
  counters_.logical_audio_gap_errors = scheduler_counters.logical_audio_gap_violations;
  counters_.slot_order_errors = scheduler_counters.slot_order_errors;
  counters_.max_logical_audio_gap_ratio = scheduler_counters.max_logical_audio_gap_ratio;

  const auto logical_enqueues =
      counters_.backend_prepare_enqueues + counters_.backend_steady_requeues;
  if (counters_.usb_submit_calls > 0) {
    counters_.usb_submit_reduction_ratio =
        static_cast<double>(logical_enqueues) /
        static_cast<double>(counters_.usb_submit_calls);
  }
}

}  // namespace opena8djcpp
