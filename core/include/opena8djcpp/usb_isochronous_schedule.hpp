#pragma once

#include "opena8djcpp/usb_submit_plan.hpp"

#include <cstdint>

namespace opena8djcpp {

struct UsbIsochronousScheduleConfig {
  std::uint32_t frames_per_slot = 8;
  std::uint32_t slots_per_submit = 8;
  std::uint32_t bytes_per_slot = kMode2DefaultTransferBytes;
  std::uint32_t min_submit_lead_frames = 8;
};

struct UsbIsochronousScheduleCounters {
  std::uint64_t scheduled_descriptors = 0;
  std::uint64_t capture_descriptors = 0;
  std::uint64_t playback_descriptors = 0;
  std::uint64_t late_submits = 0;
  std::uint64_t descriptor_shape_errors = 0;
  std::uint64_t timestamp_regressions = 0;
  std::uint64_t sequence_regressions = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t min_submit_lead_frames = 0;
  std::uint64_t max_submit_lead_frames = 0;
};

struct UsbIsochronousScheduleSafety {
  bool descriptor_shape_safe = false;
  bool deadline_safe = false;
  bool monotonic_safe = false;
  bool product_safe = false;
};

class UsbIsochronousSchedule {
 public:
  [[nodiscard]] bool start(const UsbIsochronousScheduleConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool schedule(const UsbSubmitDescriptor& descriptor,
                              std::uint64_t submit_sample_time);

  [[nodiscard]] const UsbIsochronousScheduleConfig& config() const {
    return config_;
  }

  [[nodiscard]] const UsbIsochronousScheduleCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] UsbIsochronousScheduleSafety safety() const;

 private:
  struct DirectionState {
    std::uint64_t last_timestamp = 0;
    std::uint64_t last_sequence = 0;
    bool have_descriptor = false;
  };

  [[nodiscard]] DirectionState& state_for(UsbSlotDirection direction);
  [[nodiscard]] bool descriptor_shape_ok(const UsbSubmitDescriptor& descriptor) const;

  UsbIsochronousScheduleConfig config_{};
  UsbIsochronousScheduleCounters counters_{};
  DirectionState capture_{};
  DirectionState playback_{};
  bool started_ = false;
};

}  // namespace opena8djcpp
