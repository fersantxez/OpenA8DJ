#pragma once

#include "opena8djcpp/usb_request_pool.hpp"

#include <cstdint>

namespace opena8djcpp {

struct PreparedUsbAsyncRuntimeConfig {
  PreparedUsbRequestPoolConfig request_pool{};
  std::uint32_t slots_per_submit = 8;
  std::uint32_t frames_per_slot = 8;
  std::uint32_t bytes_per_slot = kMode2DefaultTransferBytes;
  std::uint32_t max_live_requests = 4;
};

struct PreparedUsbAsyncSubmit {
  PreparedUsbRequestHandle handle{};
  UsbSubmitDescriptor descriptor{};
};

struct PreparedUsbAsyncRuntimeCounters {
  std::uint64_t submit_calls = 0;
  std::uint64_t capture_submit_calls = 0;
  std::uint64_t playback_submit_calls = 0;
  std::uint64_t completion_calls = 0;
  std::uint64_t capture_completion_calls = 0;
  std::uint64_t playback_completion_calls = 0;
  std::uint64_t cancel_calls = 0;
  std::uint64_t submit_failures = 0;
  std::uint64_t live_limit_failures = 0;
  std::uint64_t descriptor_mismatches = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t invalid_completions = 0;
  std::uint64_t stale_completions = 0;
  std::uint64_t late_completions_after_cancel = 0;
  std::uint64_t live_requests = 0;
  std::uint64_t max_live_requests = 0;
  std::uint64_t submitted_frames = 0;
  std::uint64_t completed_frames = 0;
  std::uint64_t cancelled_frames = 0;
  std::uint64_t submitted_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t cancelled_bytes = 0;
};

struct PreparedUsbAsyncRuntimeSafety {
  bool preallocated_only = false;
  bool bounded_live_requests = false;
  bool completion_owned_lifecycle = false;
  bool descriptor_shape_safe = false;
  bool no_fallback_allocations = false;
  bool drained = false;
  bool product_safe = false;
};

class PreparedUsbAsyncRuntime {
 public:
  [[nodiscard]] bool start(const PreparedUsbAsyncRuntimeConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] PreparedUsbAsyncSubmit submit(UsbSlotDirection direction,
                                              std::uint64_t first_sequence,
                                              std::uint64_t first_sample_timestamp,
                                              std::uint64_t slot_count,
                                              std::uint64_t frame_count,
                                              std::uint64_t byte_count);
  [[nodiscard]] bool complete(PreparedUsbRequestHandle handle);
  [[nodiscard]] std::uint64_t cancel_all();

  [[nodiscard]] const PreparedUsbAsyncRuntimeConfig& config() const {
    return config_;
  }

  [[nodiscard]] const PreparedUsbAsyncRuntimeCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PreparedUsbAsyncRuntimeSafety safety() const;

 private:
  [[nodiscard]] bool descriptor_matches_config(const UsbSubmitDescriptor& descriptor) const;
  void refresh_counters();

  PreparedUsbAsyncRuntimeConfig config_{};
  PreparedUsbRequestPool request_pool_{};
  PreparedUsbAsyncRuntimeCounters counters_{};
  bool started_ = false;
};

}  // namespace opena8djcpp
