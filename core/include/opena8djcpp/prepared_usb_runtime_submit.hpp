#pragma once

#include "opena8djcpp/usb_request_pool.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace opena8djcpp {

struct PreparedUsbRuntimeSubmitterConfig {
  PreparedUsbSubmitPlannerConfig planner{};
  PreparedUsbRequestPoolConfig request_pool{};
  std::uint32_t max_live_requests = 4;
  bool retain_submitted_descriptors = false;
};

struct PreparedUsbRuntimeSubmitterCounters {
  std::uint64_t logical_slots = 0;
  std::uint64_t capture_logical_slots = 0;
  std::uint64_t playback_logical_slots = 0;
  std::uint64_t usb_submit_calls = 0;
  std::uint64_t capture_usb_submit_calls = 0;
  std::uint64_t playback_usb_submit_calls = 0;
  std::uint64_t partial_submit_calls = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t total_bytes = 0;
  double usb_submit_reduction_ratio = 0.0;

  std::uint64_t descriptors_submitted = 0;
  std::uint64_t capture_descriptors_submitted = 0;
  std::uint64_t playback_descriptors_submitted = 0;
  std::uint64_t request_submit_calls = 0;
  std::uint64_t request_completion_calls = 0;
  std::uint64_t request_recycle_calls = 0;
  std::uint64_t max_live_requests = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t submit_failures = 0;
  std::uint64_t retained_descriptor_overflows = 0;
  std::uint64_t drain_calls = 0;
};

struct PreparedUsbRuntimeSubmitterSafety {
  bool planner_safe = false;
  bool request_pool_safe = false;
  bool bounded_live_requests = false;
  bool descriptors_retained_if_requested = false;
  bool no_submit_failures = false;
  bool product_safe = false;
};

class PreparedUsbRuntimeSubmitter {
 public:
  [[nodiscard]] bool start(const PreparedUsbRuntimeSubmitterConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] bool queue_slot(UsbSlotDirection direction,
                                std::uint64_t sample_timestamp);
  [[nodiscard]] bool queue_slot_with_sequence(UsbSlotDirection direction,
                                              std::uint64_t sequence,
                                              std::uint64_t sample_timestamp);
  void finish();
  void drain_pending_submits();
  void complete_all();

  [[nodiscard]] const PreparedUsbRuntimeSubmitterConfig& config() const {
    return config_;
  }

  [[nodiscard]] PreparedUsbRuntimeSubmitterCounters counters() const;

  [[nodiscard]] const PreparedUsbSubmitPlannerCounters& planner_counters() const {
    return planner_.counters();
  }

  [[nodiscard]] const PreparedUsbRequestPoolCounters& request_pool_counters() const {
    return request_pool_.counters();
  }

  [[nodiscard]] std::span<const UsbSubmitDescriptor> submitted_descriptors() const;
  [[nodiscard]] PreparedUsbRuntimeSubmitterSafety safety() const;

 private:
  void complete_oldest();
  [[nodiscard]] bool remember_descriptor(const UsbSubmitDescriptor& descriptor);
  [[nodiscard]] bool submit_descriptor(const UsbSubmitDescriptor& descriptor);
  [[nodiscard]] PreparedUsbRuntimeSubmitterCounters snapshot_counters() const;

  PreparedUsbRuntimeSubmitterConfig config_{};
  PreparedUsbSubmitPlanner planner_{};
  PreparedUsbRequestPool request_pool_{};
  std::array<PreparedUsbRequestHandle, kPreparedUsbRequestMaxSlots> live_requests_{};
  std::uint32_t live_head_ = 0;
  std::uint32_t live_count_ = 0;
  std::array<UsbSubmitDescriptor, kPreparedUsbSubmitMaxDescriptors> retained_descriptors_{};
  std::uint32_t retained_descriptor_count_ = 0;
  PreparedUsbRuntimeSubmitterCounters counters_{};
  bool started_ = false;
};

}  // namespace opena8djcpp
