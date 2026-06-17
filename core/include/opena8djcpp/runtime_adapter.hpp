#pragma once

#include "opena8djcpp/prepared_transport.hpp"

#include <cstdint>

namespace opena8djcpp {

struct FakeRuntimeAdapterConfig {
  PreparedSlotSchedulerConfig scheduler{};
};

struct FakeRuntimeAdapterStepOptions {
  PreparedSlotSchedulerStepOptions scheduler{};
};

struct FakeRuntimeAdapterCounters {
  std::uint64_t runtime_start_calls = 0;
  std::uint64_t runtime_stop_calls = 0;
  std::uint64_t runtime_period_ticks = 0;
  std::uint64_t failed_ticks = 0;
  std::uint64_t logical_audio_periods = 0;
  std::uint64_t backend_prepare_enqueues = 0;
  std::uint64_t backend_steady_requeues = 0;
  std::uint64_t usb_submit_calls = 0;
  std::uint64_t backend_slot_completions = 0;
  std::uint64_t hal_steady_requeues = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t capture_starved_periods = 0;
  std::uint64_t playback_starved_periods = 0;
  std::uint64_t backend_requeue_budget_violations = 0;
  std::uint64_t logical_audio_gap_errors = 0;
  std::uint64_t slot_order_errors = 0;
  double max_logical_audio_gap_ratio = 0.0;
  double usb_submit_reduction_ratio = 0.0;
};

struct FakeRuntimeAdapterSafety {
  bool counters_exposed = false;
  bool logical_iso8_cadence_exposed = false;
  bool usb_submit_batching_exposed = false;
  bool scheduler_product_safe = false;
  bool product_safe = false;
};

class FakeRuntimeAdapter {
 public:
  [[nodiscard]] bool start(const FakeRuntimeAdapterConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool tick_period(const FakeRuntimeAdapterStepOptions& options = {});

  [[nodiscard]] const FakeRuntimeAdapterConfig& config() const {
    return config_;
  }

  [[nodiscard]] const FakeRuntimeAdapterCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PreparedSlotSchedulerCounters scheduler_counters() const {
    return scheduler_.counters();
  }

  [[nodiscard]] FakeRuntimeAdapterSafety safety() const;

 private:
  void refresh_counters();

  FakeRuntimeAdapterConfig config_{};
  PreparedSlotScheduler scheduler_{};
  FakeRuntimeAdapterCounters counters_{};
  bool started_ = false;
};

}  // namespace opena8djcpp
