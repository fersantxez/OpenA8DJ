#pragma once

#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/prepared_transport.hpp"

#include <array>
#include <cstdint>

namespace opena8djcpp {

inline constexpr std::uint32_t kPreparedUsbSubmitMaxDescriptors = 1024;

enum class UsbSlotDirection : std::uint8_t {
  Capture = 0,
  Playback = 1,
};

struct PreparedUsbSubmitPlannerConfig {
  std::uint32_t slots_per_submit = 8;
  std::uint32_t frames_per_slot = 8;
  std::uint32_t bytes_per_slot = kMode2DefaultTransferBytes;
};

struct UsbSubmitDescriptor {
  UsbSlotDirection direction = UsbSlotDirection::Capture;
  std::uint64_t first_sequence = 0;
  std::uint64_t slot_count = 0;
  std::uint64_t first_sample_timestamp = 0;
  std::uint64_t frame_count = 0;
  std::uint64_t byte_count = 0;
};

struct PreparedUsbSubmitPlannerCounters {
  std::uint64_t logical_slots = 0;
  std::uint64_t capture_logical_slots = 0;
  std::uint64_t playback_logical_slots = 0;
  std::uint64_t usb_submit_calls = 0;
  std::uint64_t capture_usb_submit_calls = 0;
  std::uint64_t playback_usb_submit_calls = 0;
  std::uint64_t partial_submit_calls = 0;
  std::uint64_t descriptor_overflows = 0;
  std::uint64_t slot_order_errors = 0;
  std::uint64_t timestamp_regressions = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t total_bytes = 0;
  double usb_submit_reduction_ratio = 0.0;
};

struct PreparedUsbSubmitPlannerSafety {
  bool descriptors_preallocated = false;
  bool no_partial_submits = false;
  bool ordering_safe = false;
  bool timestamp_safe = false;
  bool batching_safe = false;
  bool product_safe = false;
};

class PreparedUsbSubmitPlanner {
 public:
  [[nodiscard]] bool start(const PreparedUsbSubmitPlannerConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool queue_slot(UsbSlotDirection direction, std::uint64_t sample_timestamp);
  [[nodiscard]] bool queue_slot_with_sequence(UsbSlotDirection direction,
                                              std::uint64_t sequence,
                                              std::uint64_t sample_timestamp);
  void finish();

  [[nodiscard]] const PreparedUsbSubmitPlannerConfig& config() const {
    return config_;
  }

  [[nodiscard]] const PreparedUsbSubmitPlannerCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PreparedUsbSubmitPlannerSafety safety() const;

 private:
  struct DirectionState {
    std::uint64_t next_sequence = 0;
    std::uint64_t pending_first_sequence = 0;
    std::uint64_t pending_first_timestamp = 0;
    std::uint32_t pending_slots = 0;
    std::uint64_t last_timestamp = 0;
    bool have_timestamp = false;
  };

  [[nodiscard]] DirectionState& state_for(UsbSlotDirection direction);
  [[nodiscard]] const DirectionState& state_for(UsbSlotDirection direction) const;
  void flush_direction(UsbSlotDirection direction, bool partial);
  void update_reduction_ratio();

  PreparedUsbSubmitPlannerConfig config_{};
  PreparedUsbSubmitPlannerCounters counters_{};
  std::array<UsbSubmitDescriptor, kPreparedUsbSubmitMaxDescriptors> descriptors_{};
  std::uint32_t descriptor_count_ = 0;
  DirectionState capture_{};
  DirectionState playback_{};
  bool started_ = false;
};

}  // namespace opena8djcpp
