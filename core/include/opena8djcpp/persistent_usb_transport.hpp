#pragma once

#include "opena8djcpp/usb_request_pool.hpp"

#include <array>
#include <cstdint>

namespace opena8djcpp {

struct PersistentUsbTransportConfig {
  PreparedUsbRequestPoolConfig request_pool{};
  std::uint32_t slots_per_submit = 8;
  std::uint32_t frames_per_slot = 8;
  std::uint32_t capture_bytes_per_slot = 512 * 8;
  std::uint32_t playback_bytes_per_slot = kMode2DefaultTransferBytes;
  std::uint32_t capture_queue_depth = 4;
  std::uint32_t playback_queue_depth = 4;
};

struct PersistentUsbTransportCounters {
  std::uint64_t prime_submit_calls = 0;
  std::uint64_t steady_submit_calls = 0;
  std::uint64_t submit_failures = 0;
  std::uint64_t completion_calls = 0;
  std::uint64_t completion_failures = 0;
  std::uint64_t capture_submit_calls = 0;
  std::uint64_t playback_submit_calls = 0;
  std::uint64_t capture_completion_calls = 0;
  std::uint64_t playback_completion_calls = 0;
  std::uint64_t sequence_gap_errors = 0;
  std::uint64_t timestamp_gap_errors = 0;
  std::uint64_t depth_drift_errors = 0;
  std::uint64_t live_limit_failures = 0;
  std::uint64_t descriptor_shape_errors = 0;
  std::uint64_t slot_identity_mismatches = 0;
  std::uint64_t persistent_slot_reuses = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t live_requests = 0;
  std::uint64_t max_live_requests = 0;
  std::uint64_t max_capture_live_requests = 0;
  std::uint64_t max_playback_live_requests = 0;
  std::uint64_t max_capture_lead_frames = 0;
  std::uint64_t max_playback_lead_frames = 0;
  std::uint64_t submitted_frames = 0;
  std::uint64_t completed_frames = 0;
  std::uint64_t submitted_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t cancelled_requests = 0;
};

struct PersistentUsbTransportSafety {
  bool preallocated_only = false;
  bool bounded_live_requests = false;
  bool stable_queue_depth = false;
  bool continuous_sequences = false;
  bool timestamp_continuity = false;
  bool descriptor_shape_safe = false;
  bool persistent_slot_identity = false;
  bool completion_owned_lifecycle = false;
  bool drained = false;
  bool product_safe = false;
};

class PersistentUsbTransport {
 public:
  [[nodiscard]] bool start(const PersistentUsbTransportConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool prime();
  [[nodiscard]] bool complete_next(UsbSlotDirection direction);
  [[nodiscard]] std::uint64_t drain();

  [[nodiscard]] const PersistentUsbTransportConfig& config() const {
    return config_;
  }

  [[nodiscard]] PersistentUsbTransportCounters counters() const;
  [[nodiscard]] PersistentUsbTransportSafety safety() const;

 private:
  struct LiveRequest {
    PreparedUsbRequestHandle handle{};
    std::uint64_t sequence = 0;
    std::uint64_t sample_timestamp = 0;
  };

  struct DirectionState {
    std::array<LiveRequest, kPreparedUsbRequestMaxSlots> queue{};
    std::uint32_t head = 0;
    std::uint32_t live = 0;
    std::uint32_t pool_slot_base = 0;
    std::uint64_t next_sequence = 0;
    std::uint64_t next_sample_timestamp = 0;
    std::uint64_t expected_completion_sequence = 0;
    std::uint64_t expected_completion_timestamp = 0;
  };

  [[nodiscard]] bool config_valid(const PersistentUsbTransportConfig& config) const;
  [[nodiscard]] std::uint32_t depth_for(UsbSlotDirection direction) const;
  [[nodiscard]] std::uint32_t bytes_per_slot_for(UsbSlotDirection direction) const;
  [[nodiscard]] DirectionState& state_for(UsbSlotDirection direction);
  [[nodiscard]] const DirectionState& state_for(UsbSlotDirection direction) const;
  [[nodiscard]] bool submit_next(UsbSlotDirection direction, bool prime_submit);
  [[nodiscard]] std::uint32_t expected_pool_slot(const DirectionState& state,
                                                 UsbSlotDirection direction) const;
  [[nodiscard]] bool push_live(DirectionState& state, const LiveRequest& request);
  [[nodiscard]] LiveRequest pop_live(DirectionState& state);
  void account_lead(UsbSlotDirection direction);
  void account_depth(UsbSlotDirection direction);

  PersistentUsbTransportConfig config_{};
  PreparedUsbRequestPool request_pool_{};
  PersistentUsbTransportCounters counters_{};
  DirectionState capture_{};
  DirectionState playback_{};
  bool started_ = false;
  bool primed_ = false;
};

}  // namespace opena8djcpp
