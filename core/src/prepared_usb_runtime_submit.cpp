#include "opena8djcpp/prepared_usb_runtime_submit.hpp"

#include <algorithm>
#include <cstddef>

namespace opena8djcpp {

bool PreparedUsbRuntimeSubmitter::start(const PreparedUsbRuntimeSubmitterConfig& config) {
  if (config.max_live_requests == 0 ||
      config.max_live_requests > config.request_pool.request_slots ||
      config.max_live_requests > kPreparedUsbRequestMaxSlots) {
    return false;
  }

  PreparedUsbSubmitPlanner planner;
  PreparedUsbRequestPool request_pool;
  if (!planner.start(config.planner) || !request_pool.start(config.request_pool)) {
    return false;
  }

  config_ = config;
  planner_ = planner;
  request_pool_ = request_pool;
  live_head_ = 0;
  live_count_ = 0;
  retained_descriptor_count_ = 0;
  counters_ = {};
  started_ = true;
  return true;
}

void PreparedUsbRuntimeSubmitter::stop() {
  if (!started_) {
    return;
  }
  finish();
  request_pool_.stop();
  planner_.stop();
  started_ = false;
}

bool PreparedUsbRuntimeSubmitter::queue_slot(UsbSlotDirection direction,
                                             std::uint64_t sample_timestamp) {
  if (!started_) {
    return false;
  }
  const bool queued = planner_.queue_slot(direction, sample_timestamp);
  drain_pending_submits();
  return queued;
}

bool PreparedUsbRuntimeSubmitter::queue_slot_with_sequence(
    UsbSlotDirection direction,
    std::uint64_t sequence,
    std::uint64_t sample_timestamp) {
  if (!started_) {
    return false;
  }
  const bool queued = planner_.queue_slot_with_sequence(direction, sequence, sample_timestamp);
  drain_pending_submits();
  return queued;
}

void PreparedUsbRuntimeSubmitter::finish() {
  if (!started_) {
    return;
  }
  planner_.finish();
  drain_pending_submits();
  complete_all();
}

void PreparedUsbRuntimeSubmitter::drain_pending_submits() {
  if (!started_) {
    return;
  }

  const auto descriptors = planner_.descriptors();
  if (descriptors.empty()) {
    return;
  }

  counters_.drain_calls += 1;
  for (const auto& descriptor : descriptors) {
    (void)remember_descriptor(descriptor);
    (void)submit_descriptor(descriptor);
  }
  planner_.clear_descriptors();
}

void PreparedUsbRuntimeSubmitter::complete_all() {
  while (live_count_ > 0) {
    complete_oldest();
  }
}

PreparedUsbRuntimeSubmitterCounters PreparedUsbRuntimeSubmitter::counters() const {
  return snapshot_counters();
}

std::span<const UsbSubmitDescriptor> PreparedUsbRuntimeSubmitter::submitted_descriptors() const {
  return std::span<const UsbSubmitDescriptor>(retained_descriptors_.data(),
                                              retained_descriptor_count_);
}

PreparedUsbRuntimeSubmitterSafety PreparedUsbRuntimeSubmitter::safety() const {
  const auto planner_safety = planner_.safety();
  const auto request_safety = request_pool_.safety();
  const auto counters = snapshot_counters();
  PreparedUsbRuntimeSubmitterSafety out{};
  out.planner_safe = planner_safety.product_safe;
  out.request_pool_safe = request_safety.product_safe;
  out.bounded_live_requests = counters.max_live_requests <= config_.max_live_requests;
  out.descriptors_retained_if_requested =
      !config_.retain_submitted_descriptors ||
      (counters.retained_descriptor_overflows == 0 &&
       retained_descriptor_count_ == counters.descriptors_submitted);
  out.no_submit_failures =
      counters.submit_failures == 0 && counters.fallback_allocations == 0;
  out.product_safe = out.planner_safe && out.request_pool_safe &&
                     out.bounded_live_requests &&
                     out.descriptors_retained_if_requested && out.no_submit_failures;
  return out;
}

void PreparedUsbRuntimeSubmitter::complete_oldest() {
  if (live_count_ == 0) {
    return;
  }

  const auto handle = live_requests_[live_head_];
  (void)request_pool_.complete(handle);
  live_requests_[live_head_] = {};
  live_head_ = (live_head_ + 1U) % static_cast<std::uint32_t>(live_requests_.size());
  live_count_ -= 1;
}

bool PreparedUsbRuntimeSubmitter::remember_descriptor(const UsbSubmitDescriptor& descriptor) {
  if (!config_.retain_submitted_descriptors) {
    return true;
  }
  if (retained_descriptor_count_ >= retained_descriptors_.size()) {
    counters_.retained_descriptor_overflows += 1;
    return false;
  }
  retained_descriptors_[retained_descriptor_count_] = descriptor;
  retained_descriptor_count_ += 1;
  return true;
}

bool PreparedUsbRuntimeSubmitter::submit_descriptor(const UsbSubmitDescriptor& descriptor) {
  while (live_count_ >= config_.max_live_requests) {
    complete_oldest();
  }

  const auto handle = request_pool_.submit(descriptor);
  if (!handle.valid()) {
    counters_.submit_failures += 1;
    return false;
  }

  const auto tail =
      (live_head_ + live_count_) % static_cast<std::uint32_t>(live_requests_.size());
  live_requests_[tail] = handle;
  live_count_ += 1;
  counters_.descriptors_submitted += 1;
  if (descriptor.direction == UsbSlotDirection::Capture) {
    counters_.capture_descriptors_submitted += 1;
  } else {
    counters_.playback_descriptors_submitted += 1;
  }
  return true;
}

PreparedUsbRuntimeSubmitterCounters PreparedUsbRuntimeSubmitter::snapshot_counters() const {
  auto counters = counters_;
  const auto planner_counters = planner_.counters();
  const auto request_counters = request_pool_.counters();
  counters.logical_slots = planner_counters.logical_slots;
  counters.capture_logical_slots = planner_counters.capture_logical_slots;
  counters.playback_logical_slots = planner_counters.playback_logical_slots;
  counters.usb_submit_calls = planner_counters.usb_submit_calls;
  counters.capture_usb_submit_calls = planner_counters.capture_usb_submit_calls;
  counters.playback_usb_submit_calls = planner_counters.playback_usb_submit_calls;
  counters.partial_submit_calls = planner_counters.partial_submit_calls;
  counters.total_frames = planner_counters.total_frames;
  counters.total_bytes = planner_counters.total_bytes;
  counters.usb_submit_reduction_ratio = planner_counters.usb_submit_reduction_ratio;
  counters.request_submit_calls = request_counters.submit_calls;
  counters.request_completion_calls = request_counters.completion_calls;
  counters.request_recycle_calls = request_counters.recycle_calls;
  counters.max_live_requests =
      std::max(counters.max_live_requests, request_counters.max_live_requests);
  counters.fallback_allocations = request_counters.fallback_allocations;
  return counters;
}

}  // namespace opena8djcpp
