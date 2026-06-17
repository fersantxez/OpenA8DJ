#pragma once

#include "opena8djcpp/usb_submit_plan.hpp"

#include <array>
#include <cstdint>

namespace opena8djcpp {

inline constexpr std::uint32_t kPreparedUsbRequestMaxSlots = 128;
inline constexpr std::uint32_t kInvalidUsbRequestSlot = 0xffffffffU;

struct PreparedUsbRequestPoolConfig {
  std::uint32_t request_slots = 16;
};

struct PreparedUsbRequestHandle {
  std::uint32_t slot = kInvalidUsbRequestSlot;
  std::uint32_t generation = 0;

  [[nodiscard]] constexpr bool valid() const {
    return slot != kInvalidUsbRequestSlot;
  }
};

struct PreparedUsbRequestPoolCounters {
  std::uint64_t submit_calls = 0;
  std::uint64_t capture_submit_calls = 0;
  std::uint64_t playback_submit_calls = 0;
  std::uint64_t completion_calls = 0;
  std::uint64_t capture_completion_calls = 0;
  std::uint64_t playback_completion_calls = 0;
  std::uint64_t recycle_calls = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t invalid_completions = 0;
  std::uint64_t stale_completions = 0;
  std::uint64_t live_requests = 0;
  std::uint64_t max_live_requests = 0;
  std::uint64_t submitted_bytes = 0;
  std::uint64_t completed_bytes = 0;
  std::uint64_t submitted_frames = 0;
  std::uint64_t completed_frames = 0;
  std::uint64_t submitted_capture_bytes = 0;
  std::uint64_t completed_capture_bytes = 0;
  std::uint64_t submitted_playback_bytes = 0;
  std::uint64_t completed_playback_bytes = 0;
};

struct PreparedUsbRequestPoolSafety {
  bool preallocated_only = false;
  bool lifecycle_safe = false;
  bool accounting_safe = false;
  bool drained = false;
  bool product_safe = false;
};

class PreparedUsbRequestPool {
 public:
  [[nodiscard]] bool start(const PreparedUsbRequestPoolConfig& config);
  void stop();

  [[nodiscard]] bool started() const {
    return started_;
  }

  [[nodiscard]] PreparedUsbRequestHandle submit(const UsbSubmitDescriptor& descriptor);
  [[nodiscard]] bool complete(PreparedUsbRequestHandle handle);

  [[nodiscard]] const PreparedUsbRequestPoolConfig& config() const {
    return config_;
  }

  [[nodiscard]] const PreparedUsbRequestPoolCounters& counters() const {
    return counters_;
  }

  [[nodiscard]] PreparedUsbRequestPoolSafety safety() const;

 private:
  struct RequestSlot {
    UsbSubmitDescriptor descriptor{};
    std::uint32_t generation = 1;
    bool in_use = false;
  };

  [[nodiscard]] std::uint32_t find_free_slot() const;
  void account_submit(const UsbSubmitDescriptor& descriptor);
  void account_completion(const UsbSubmitDescriptor& descriptor);

  PreparedUsbRequestPoolConfig config_{};
  PreparedUsbRequestPoolCounters counters_{};
  std::array<RequestSlot, kPreparedUsbRequestMaxSlots> requests_{};
  bool started_ = false;
};

}  // namespace opena8djcpp
