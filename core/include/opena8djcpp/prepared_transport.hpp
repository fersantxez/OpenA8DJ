#pragma once

#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/audio_ring.hpp"
#include "opena8djcpp/mode2_packet.hpp"

#include <cstdint>
#include <span>

namespace opena8djcpp {

inline constexpr std::uint32_t kPreparedTransportMaxSlots = 64;
inline constexpr std::uint32_t kPreparedTransportRingFrames = 4096;
inline constexpr double kPreparedTransportMaxCompletionGapRatio = 1.25;

struct PreparedTransportConfig {
  std::uint32_t iso_frames = 8;
  std::uint32_t capture_slots = kPreparedTransportMaxSlots;
  std::uint32_t playback_slots = kPreparedTransportMaxSlots;
};

struct PreparedTransportStepOptions {
  std::uint32_t completion_gap_periods = 1;
  bool hal_direct_requeue_attempt = false;
  bool fallback_allocation_attempt = false;
  bool force_timestamp_regression = false;
};

struct PreparedTransportCounters {
  std::uint64_t backend_prepare_enqueues = 0;
  std::uint64_t backend_steady_requeues = 0;
  std::uint64_t hal_steady_requeues = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t capture_ring_overruns = 0;
  std::uint64_t capture_ring_underruns = 0;
  std::uint64_t playback_ring_overruns = 0;
  std::uint64_t playback_ring_underruns = 0;
  std::uint64_t timestamp_regressions = 0;
  std::uint64_t channel_identity_failures = 0;
  std::uint64_t timecode_profile_failures = 0;
  std::uint64_t backend_capture_frames = 0;
  std::uint64_t backend_playback_frames = 0;
  std::uint64_t hal_capture_reads = 0;
  std::uint64_t hal_playback_writes = 0;
  double max_completion_gap_ratio = 0.0;
};

struct PreparedTransportSafety {
  bool prepared_slots_only = false;
  bool cadence_safe = false;
  bool routing_safe = false;
  bool timecode_safe = false;
  bool hal_hot_path_safe = false;
  bool product_safe = false;
};

struct PreparedSlotSchedulerConfig {
  std::uint32_t capture_target_slots = 8;
  std::uint32_t playback_target_slots = 8;
  std::uint32_t capture_pool_slots = kPreparedTransportMaxSlots;
  std::uint32_t playback_pool_slots = kPreparedTransportMaxSlots;
  std::uint32_t unavailable_capture_slots = 0;
  std::uint32_t unavailable_playback_slots = 0;
  std::uint32_t playback_completion_gap_periods = 1;
  std::uint32_t max_backend_requeues_per_period = 2;
};

struct PreparedSlotSchedulerStepOptions {
  bool hal_direct_requeue_attempt = false;
  bool fallback_allocation_attempt = false;
  bool suppress_backend_requeue = false;
};

struct PreparedSlotSchedulerCounters {
  std::uint64_t periods = 0;
  std::uint64_t backend_prepare_enqueues = 0;
  std::uint64_t backend_steady_requeues = 0;
  std::uint64_t hal_steady_requeues = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t capture_starved_periods = 0;
  std::uint64_t playback_starved_periods = 0;
  std::uint64_t backend_requeue_budget_violations = 0;
  std::uint64_t completion_gap_violations = 0;
  std::uint32_t capture_in_flight = 0;
  std::uint32_t playback_in_flight = 0;
  std::uint32_t min_capture_in_flight = 0;
  std::uint32_t min_playback_in_flight = 0;
  std::uint32_t max_capture_in_flight = 0;
  std::uint32_t max_playback_in_flight = 0;
};

struct PreparedSlotSchedulerSafety {
  bool prepared_slots_only = false;
  bool lead_safe = false;
  bool cadence_safe = false;
  bool hal_hot_path_safe = false;
  bool backend_budget_safe = false;
  bool product_safe = false;
};

class PreparedSlotScheduler {
 public:
  [[nodiscard]] bool start(const PreparedSlotSchedulerConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool complete_period(const PreparedSlotSchedulerStepOptions& options = {});

  [[nodiscard]] const PreparedSlotSchedulerConfig& config() const {
    return config_;
  }

  [[nodiscard]] const PreparedSlotSchedulerCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PreparedSlotSchedulerSafety safety() const;

 private:
  void queue_capture_slot(bool prepare);
  void queue_playback_slot(bool prepare);
  void refill_to_targets();
  void record_lead();

  PreparedSlotSchedulerConfig config_{};
  PreparedSlotSchedulerCounters counters_{};
  bool started_ = false;
};

class PreparedTransportBackend {
 public:
  [[nodiscard]] bool start(const PreparedTransportConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool backend_complete_period(const S24Frame& capture_frame,
                                             std::uint64_t sample_timestamp,
                                             const PreparedTransportStepOptions& options = {});
  [[nodiscard]] bool backend_complete_period(std::span<const S24Frame> capture_frames,
                                             std::span<S24Frame> playback_frames,
                                             std::uint64_t sample_timestamp,
                                             const PreparedTransportStepOptions& options = {});
  [[nodiscard]] bool hal_write_playback(const S24Frame& frame);
  [[nodiscard]] std::uint32_t hal_write_playback(std::span<const S24Frame> frames);
  [[nodiscard]] bool hal_read_capture(S24Frame& frame);
  [[nodiscard]] std::uint32_t hal_read_capture(std::span<S24Frame> frames);

  [[nodiscard]] const PreparedTransportConfig& config() const {
    return config_;
  }

  [[nodiscard]] const PreparedTransportCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PreparedTransportSafety safety() const;

 private:
  void validate_channel_identity();
  void record_timestamp(std::uint64_t sample_timestamp);

  PreparedTransportConfig config_{};
  PreparedTransportCounters counters_{};
  SpscFrameRing<S24Frame, kPreparedTransportRingFrames> capture_ring_{};
  SpscFrameRing<S24Frame, kPreparedTransportRingFrames> playback_ring_{};
  std::uint64_t last_timestamp_ = 0;
  bool have_timestamp_ = false;
  bool started_ = false;
};

}  // namespace opena8djcpp
