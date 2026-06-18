#include "opena8djcpp/prepared_usb_async_runtime.hpp"

#include <algorithm>

namespace opena8djcpp {

bool PreparedUsbAsyncRuntime::start(const PreparedUsbAsyncRuntimeConfig& config) {
  if (config.slots_per_submit == 0 || config.frames_per_slot == 0 ||
      config.capture_bytes_per_slot == 0 || config.playback_bytes_per_slot == 0 ||
      config.max_live_requests == 0 ||
      config.max_live_requests > config.request_pool.request_slots ||
      config.max_live_requests > kPreparedUsbRequestMaxSlots) {
    return false;
  }

  PreparedUsbRequestPool request_pool;
  if (!request_pool.start(config.request_pool)) {
    return false;
  }

  config_ = config;
  request_pool_ = request_pool;
  counters_ = {};
  started_ = true;
  return true;
}

void PreparedUsbAsyncRuntime::stop() {
  if (!started_) {
    return;
  }
  (void)cancel_all();
  request_pool_.stop();
  started_ = false;
}

PreparedUsbAsyncSubmit PreparedUsbAsyncRuntime::submit(
    UsbSlotDirection direction,
    std::uint64_t first_sequence,
    std::uint64_t first_sample_timestamp,
    std::uint64_t slot_count,
    std::uint64_t frame_count,
    std::uint64_t byte_count) {
  PreparedUsbAsyncSubmit out{};
  if (!started_) {
    counters_.submit_failures += 1;
    return out;
  }

  out.descriptor = UsbSubmitDescriptor{
      .direction = direction,
      .first_sequence = first_sequence,
      .slot_count = slot_count,
      .first_sample_timestamp = first_sample_timestamp,
      .frame_count = frame_count,
      .byte_count = byte_count,
  };

  if (!descriptor_matches_config(out.descriptor)) {
    counters_.descriptor_mismatches += 1;
    counters_.submit_failures += 1;
    return out;
  }

  const auto pool_counters = request_pool_.counters();
  if (pool_counters.live_requests >= config_.max_live_requests) {
    counters_.live_limit_failures += 1;
    counters_.submit_failures += 1;
    return out;
  }

  out.handle = request_pool_.submit(out.descriptor);
  if (!out.handle.valid()) {
    counters_.submit_failures += 1;
    return out;
  }

  counters_.submit_calls += 1;
  if (direction == UsbSlotDirection::Capture) {
    counters_.capture_submit_calls += 1;
  } else {
    counters_.playback_submit_calls += 1;
  }
  return out;
}

bool PreparedUsbAsyncRuntime::complete(PreparedUsbRequestHandle handle) {
  if (!started_) {
    counters_.invalid_completions += 1;
    return false;
  }
  const bool completed = request_pool_.complete(handle);
  if (completed) {
    counters_.completion_calls += 1;
  }
  return completed;
}

std::uint64_t PreparedUsbAsyncRuntime::cancel_all() {
  if (!started_) {
    return 0;
  }
  const auto cancelled = request_pool_.cancel_all();
  counters_.cancel_calls += cancelled;
  return cancelled;
}

PreparedUsbAsyncRuntimeCounters PreparedUsbAsyncRuntime::counters() const {
  return snapshot_counters();
}

PreparedUsbAsyncRuntimeSafety PreparedUsbAsyncRuntime::safety() const {
  const auto pool_safety = request_pool_.safety();
  const auto counters = snapshot_counters();
  PreparedUsbAsyncRuntimeSafety out{};
  out.preallocated_only = pool_safety.preallocated_only;
  out.bounded_live_requests = counters.max_live_requests <= config_.max_live_requests;
  out.completion_owned_lifecycle =
      counters.completion_calls + counters.cancel_calls == counters.submit_calls;
  out.descriptor_shape_safe = counters.descriptor_mismatches == 0;
  out.no_fallback_allocations =
      counters.fallback_allocations == 0 && counters.submit_failures == 0;
  out.drained = counters.live_requests == 0;
  out.product_safe = out.preallocated_only && out.bounded_live_requests &&
                     out.completion_owned_lifecycle && out.descriptor_shape_safe &&
                     out.no_fallback_allocations && out.drained;
  return out;
}

bool PreparedUsbAsyncRuntime::descriptor_matches_config(
    const UsbSubmitDescriptor& descriptor) const {
  return descriptor.slot_count == config_.slots_per_submit &&
         descriptor.frame_count == config_.slots_per_submit * config_.frames_per_slot &&
         descriptor.byte_count ==
             config_.slots_per_submit * bytes_per_slot_for(descriptor.direction);
}

std::uint32_t PreparedUsbAsyncRuntime::bytes_per_slot_for(
    UsbSlotDirection direction) const {
  return direction == UsbSlotDirection::Capture ? config_.capture_bytes_per_slot
                                                : config_.playback_bytes_per_slot;
}

PreparedUsbAsyncRuntimeCounters PreparedUsbAsyncRuntime::snapshot_counters() const {
  auto counters = counters_;
  const auto pool = request_pool_.counters();
  counters.fallback_allocations = pool.fallback_allocations;
  counters.invalid_completions = std::max(counters.invalid_completions,
                                          pool.invalid_completions);
  counters.stale_completions = pool.stale_completions;
  counters.late_completions_after_cancel = pool.late_completions_after_cancel;
  counters.live_requests = pool.live_requests;
  counters.max_live_requests = std::max(counters.max_live_requests,
                                        pool.max_live_requests);
  counters.submitted_frames = pool.submitted_frames;
  counters.completed_frames = pool.completed_frames;
  counters.cancelled_frames = pool.cancelled_frames;
  counters.submitted_bytes = pool.submitted_bytes;
  counters.completed_bytes = pool.completed_bytes;
  counters.cancelled_bytes = pool.cancelled_bytes;
  counters.capture_completion_calls = pool.capture_completion_calls;
  counters.playback_completion_calls = pool.playback_completion_calls;
  return counters;
}

}  // namespace opena8djcpp
