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
