#pragma once

#include <cstdint>

namespace opena8djcpp {

inline constexpr std::uint32_t kPlaybackLeadSchedulerIsoFrames = 8;
inline constexpr std::uint32_t kPlaybackLeadSchedulerMaxPoolSlots = 64;

struct PlaybackLeadSchedulerConfig {
  std::uint32_t iso_frames = kPlaybackLeadSchedulerIsoFrames;
  std::uint32_t target_lead_slots = 8;
  std::uint32_t low_watermark_slots = 4;
  std::uint32_t high_watermark_slots = 12;
  std::uint32_t max_slots_per_submit = 8;
  std::uint32_t playback_pool_slots = 16;
  std::uint32_t playback_completion_gap_periods = 1;
  bool capture_submit_every_period = true;
};

struct PlaybackLeadSchedulerStepOptions {
  bool suppress_playback_submit = false;
  bool force_playback_batch_overflow = false;
  bool force_capture_gap = false;
};

struct PlaybackLeadSchedulerCounters {
  std::uint64_t periods = 0;
  std::uint64_t logical_audio_periods = 0;
  std::uint64_t capture_request_submit_calls = 0;
  std::uint64_t playback_logical_slots_submitted = 0;
  std::uint64_t playback_request_submit_calls = 0;
  std::uint64_t playback_submit_events_from_audio_callback = 0;
  std::uint64_t playback_slots_completed = 0;
  std::uint64_t playback_underflow_periods = 0;
  std::uint64_t playback_pool_overflows = 0;
  std::uint64_t playback_batch_overflows = 0;
  std::uint64_t capture_gap_periods = 0;
  std::uint64_t logical_cadence_violations = 0;
  std::uint32_t min_playback_lead_slots = 0;
  std::uint32_t max_playback_lead_slots = 0;
  std::uint32_t max_slots_per_submit_observed = 0;
  double playback_submit_reduction_ratio = 0.0;
  double total_submit_reduction_ratio = 0.0;
};

struct PlaybackLeadSchedulerSafety {
  bool iso8_cadence_preserved = false;
  bool capture_continuity_preserved = false;
  bool playback_lead_safe = false;
  bool playback_batching_effective = false;
  bool pool_safe = false;
  bool callback_work_reduced = false;
  bool product_model_safe = false;
};

class PlaybackLeadScheduler {
 public:
  [[nodiscard]] bool start(const PlaybackLeadSchedulerConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool complete_period(const PlaybackLeadSchedulerStepOptions& options = {});

  [[nodiscard]] const PlaybackLeadSchedulerConfig& config() const {
    return config_;
  }

  [[nodiscard]] const PlaybackLeadSchedulerCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PlaybackLeadSchedulerSafety safety() const;

 private:
  void submit_playback_slots(std::uint32_t slots, bool from_audio_callback);
  void maybe_refill_playback(const PlaybackLeadSchedulerStepOptions& options);
  void consume_playback_period();
  void record_lead();
  void update_ratios();

  PlaybackLeadSchedulerConfig config_{};
  PlaybackLeadSchedulerCounters counters_{};
  std::uint32_t playback_lead_slots_ = 0;
  bool started_ = false;
};

}  // namespace opena8djcpp
