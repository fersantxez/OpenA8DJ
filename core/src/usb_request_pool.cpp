#include "opena8djcpp/usb_request_pool.hpp"

#include <algorithm>

namespace opena8djcpp {

bool PreparedUsbRequestPool::start(const PreparedUsbRequestPoolConfig& config) {
  if (config.request_slots == 0 || config.request_slots > kPreparedUsbRequestMaxSlots) {
    return false;
  }
  config_ = config;
  counters_ = {};
  for (auto& request : requests_) {
    request.descriptor = {};
    request.in_use = false;
    request.last_cancelled_generation = 0;
  }
  started_ = true;
  return true;
}

void PreparedUsbRequestPool::stop() {
  started_ = false;
}

PreparedUsbRequestHandle PreparedUsbRequestPool::submit(const UsbSubmitDescriptor& descriptor) {
  if (!started_) {
    counters_.fallback_allocations += 1;
    return {};
  }

  const auto slot = find_free_slot();
  if (slot == kInvalidUsbRequestSlot) {
    counters_.fallback_allocations += 1;
    return {};
  }

  auto& request = requests_[slot];
  request.descriptor = descriptor;
  request.in_use = true;
  counters_.live_requests += 1;
  counters_.max_live_requests = std::max(counters_.max_live_requests, counters_.live_requests);
  account_submit(descriptor);
  return PreparedUsbRequestHandle{.slot = slot, .generation = request.generation};
}

bool PreparedUsbRequestPool::complete(PreparedUsbRequestHandle handle) {
  if (!started_ || !handle.valid() || handle.slot >= config_.request_slots) {
    counters_.invalid_completions += 1;
    return false;
  }

  auto& request = requests_[handle.slot];
  if (request.generation != handle.generation) {
    counters_.stale_completions += 1;
    if (request.last_cancelled_generation == handle.generation) {
      counters_.late_completions_after_cancel += 1;
    }
    return false;
  }
  if (!request.in_use) {
    counters_.invalid_completions += 1;
    return false;
  }

  account_completion(request.descriptor);
  request.in_use = false;
  request.generation += 1;
  request.descriptor = {};
  counters_.live_requests -= 1;
  counters_.recycle_calls += 1;
  return true;
}

std::uint64_t PreparedUsbRequestPool::cancel_all() {
  if (!started_) {
    return 0;
  }
  std::uint64_t cancelled = 0;
  for (std::uint32_t index = 0; index < config_.request_slots; ++index) {
    auto& request = requests_[index];
    if (!request.in_use) {
      continue;
    }
    account_cancel(request.descriptor);
    request.in_use = false;
    request.last_cancelled_generation = request.generation;
    request.generation += 1;
    request.descriptor = {};
    counters_.live_requests -= 1;
    counters_.recycle_calls += 1;
    cancelled += 1;
  }
  return cancelled;
}

PreparedUsbRequestPoolSafety PreparedUsbRequestPool::safety() const {
  PreparedUsbRequestPoolSafety out{};
  out.preallocated_only = counters_.fallback_allocations == 0;
  out.lifecycle_safe =
      counters_.invalid_completions == 0 && counters_.stale_completions == 0 &&
      counters_.submit_calls == counters_.completion_calls + counters_.cancel_calls &&
      counters_.recycle_calls == counters_.completion_calls + counters_.cancel_calls;
  out.accounting_safe =
      counters_.submitted_bytes == counters_.completed_bytes + counters_.cancelled_bytes &&
      counters_.submitted_frames == counters_.completed_frames + counters_.cancelled_frames &&
      counters_.submitted_capture_bytes ==
          counters_.completed_capture_bytes + counters_.cancelled_capture_bytes &&
      counters_.submitted_playback_bytes ==
          counters_.completed_playback_bytes + counters_.cancelled_playback_bytes;
  out.drained = counters_.live_requests == 0;
  out.product_safe =
      out.preallocated_only && out.lifecycle_safe && out.accounting_safe && out.drained;
  return out;
}

std::uint32_t PreparedUsbRequestPool::find_free_slot() const {
  for (std::uint32_t index = 0; index < config_.request_slots; ++index) {
    if (!requests_[index].in_use) {
      return index;
    }
  }
  return kInvalidUsbRequestSlot;
}

void PreparedUsbRequestPool::account_submit(const UsbSubmitDescriptor& descriptor) {
  counters_.submit_calls += 1;
  counters_.submitted_bytes += descriptor.byte_count;
  counters_.submitted_frames += descriptor.frame_count;
  if (descriptor.direction == UsbSlotDirection::Capture) {
    counters_.capture_submit_calls += 1;
    counters_.submitted_capture_bytes += descriptor.byte_count;
  } else {
    counters_.playback_submit_calls += 1;
    counters_.submitted_playback_bytes += descriptor.byte_count;
  }
}

void PreparedUsbRequestPool::account_completion(const UsbSubmitDescriptor& descriptor) {
  counters_.completion_calls += 1;
  counters_.completed_bytes += descriptor.byte_count;
  counters_.completed_frames += descriptor.frame_count;
  if (descriptor.direction == UsbSlotDirection::Capture) {
    counters_.capture_completion_calls += 1;
    counters_.completed_capture_bytes += descriptor.byte_count;
  } else {
    counters_.playback_completion_calls += 1;
    counters_.completed_playback_bytes += descriptor.byte_count;
  }
}

void PreparedUsbRequestPool::account_cancel(const UsbSubmitDescriptor& descriptor) {
  counters_.cancel_calls += 1;
  counters_.cancelled_bytes += descriptor.byte_count;
  counters_.cancelled_frames += descriptor.frame_count;
  if (descriptor.direction == UsbSlotDirection::Capture) {
    counters_.capture_cancel_calls += 1;
    counters_.cancelled_capture_bytes += descriptor.byte_count;
  } else {
    counters_.playback_cancel_calls += 1;
    counters_.cancelled_playback_bytes += descriptor.byte_count;
  }
}

}  // namespace opena8djcpp
