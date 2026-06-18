#pragma once

#include "opena8djcpp/playback_scheduler.hpp"
#include "opena8djcpp/usb_request_pool.hpp"

#include <cstdint>

namespace opena8djcpp {

inline constexpr std::uint32_t kPlaybackSchedulerRuntimeCaptureBytesPerSlot =
    512 * kPlaybackLeadSchedulerIsoFrames;

struct PlaybackSchedulerRuntimeConfig {
  PlaybackLeadSchedulerConfig scheduler{};
  PreparedUsbRequestPoolConfig request_pool{};
  std::uint32_t frames_per_slot = kPlaybackLeadSchedulerIsoFrames;
  std::uint32_t capture_bytes_per_slot = kPlaybackSchedulerRuntimeCaptureBytesPerSlot;
  std::uint32_t playback_bytes_per_slot = kMode2DefaultTransferBytes;
};

struct PlaybackSchedulerRuntimeCounters {
  PlaybackLeadSchedulerCounters scheduler{};
  PreparedUsbRequestPoolCounters request_pool{};
  std::uint64_t capture_runtime_submit_calls = 0;
  std::uint64_t playback_runtime_submit_calls = 0;
  std::uint64_t runtime_submit_failures = 0;
  std::uint64_t runtime_completion_failures = 0;
  std::uint64_t runtime_capture_gap_periods = 0;
  std::uint64_t playback_runtime_logical_slots_submitted = 0;
  double runtime_total_submit_reduction_ratio = 0.0;
};

struct PlaybackSchedulerRuntimeSafety {
  bool scheduler_safe = false;
  bool request_pool_safe = false;
  bool capture_continuity_preserved = false;
  bool playback_batching_bound = false;
  bool runtime_accounting_matches_scheduler = false;
  bool no_runtime_failures = false;
  bool product_model_safe = false;
};

class PlaybackSchedulerRuntimeBinding {
 public:
  [[nodiscard]] bool start(const PlaybackSchedulerRuntimeConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool complete_period(const PlaybackLeadSchedulerStepOptions& options = {});

  [[nodiscard]] const PlaybackSchedulerRuntimeConfig& config() const {
    return config_;
  }

  [[nodiscard]] PlaybackSchedulerRuntimeCounters counters() const;
  [[nodiscard]] PlaybackSchedulerRuntimeSafety safety() const;

 private:
  [[nodiscard]] bool submit_runtime_descriptor(UsbSlotDirection direction,
                                               std::uint64_t first_sequence,
                                               std::uint64_t first_sample_timestamp,
                                               std::uint64_t slot_count);
  [[nodiscard]] bool submit_playback_delta(std::uint64_t previous_logical_slots);
  void update_runtime_ratios(PlaybackSchedulerRuntimeCounters& counters) const;

  PlaybackSchedulerRuntimeConfig config_{};
  PlaybackLeadScheduler scheduler_{};
  PreparedUsbRequestPool request_pool_{};
  PlaybackSchedulerRuntimeCounters counters_{};
  std::uint64_t next_capture_sequence_ = 0;
  std::uint64_t next_playback_sequence_ = 0;
  bool started_ = false;
};

}  // namespace opena8djcpp
