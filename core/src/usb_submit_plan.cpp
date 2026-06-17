#include "opena8djcpp/usb_submit_plan.hpp"

#include <cstddef>

namespace opena8djcpp {

bool PreparedUsbSubmitPlanner::start(const PreparedUsbSubmitPlannerConfig& config) {
  if (config.slots_per_submit == 0 ||
      config.slots_per_submit > kPreparedTransportMaxSlots ||
      config.frames_per_slot == 0 || config.bytes_per_slot == 0) {
    return false;
  }

  config_ = config;
  counters_ = {};
  descriptor_count_ = 0;
  capture_ = {};
  playback_ = {};
  started_ = true;
  return true;
}

void PreparedUsbSubmitPlanner::stop() {
  if (!started_) {
    return;
  }
  finish();
  started_ = false;
}

bool PreparedUsbSubmitPlanner::queue_slot(UsbSlotDirection direction,
                                          std::uint64_t sample_timestamp) {
  const auto sequence = state_for(direction).next_sequence;
  return queue_slot_with_sequence(direction, sequence, sample_timestamp);
}

bool PreparedUsbSubmitPlanner::queue_slot_with_sequence(UsbSlotDirection direction,
                                                        std::uint64_t sequence,
                                                        std::uint64_t sample_timestamp) {
  if (!started_) {
    return false;
  }

  auto& state = state_for(direction);
  const bool sequence_ok = sequence == state.next_sequence;
  if (!sequence_ok) {
    counters_.slot_order_errors += 1;
  }
  if (state.have_timestamp && sample_timestamp <= state.last_timestamp) {
    counters_.timestamp_regressions += 1;
  }
  if (state.pending_slots == 0) {
    state.pending_first_sequence = sequence;
    state.pending_first_timestamp = sample_timestamp;
  }

  state.pending_slots += 1;
  state.next_sequence = sequence + 1U;
  state.last_timestamp = sample_timestamp;
  state.have_timestamp = true;
  counters_.logical_slots += 1;
  if (direction == UsbSlotDirection::Capture) {
    counters_.capture_logical_slots += 1;
  } else {
    counters_.playback_logical_slots += 1;
  }

  if (state.pending_slots >= config_.slots_per_submit) {
    flush_direction(direction, false);
  }
  update_reduction_ratio();
  return sequence_ok;
}

void PreparedUsbSubmitPlanner::finish() {
  if (!started_) {
    return;
  }
  if (capture_.pending_slots > 0) {
    flush_direction(UsbSlotDirection::Capture, true);
  }
  if (playback_.pending_slots > 0) {
    flush_direction(UsbSlotDirection::Playback, true);
  }
  update_reduction_ratio();
}

void PreparedUsbSubmitPlanner::clear_descriptors() {
  descriptor_count_ = 0;
}

PreparedUsbSubmitPlannerSafety PreparedUsbSubmitPlanner::safety() const {
  PreparedUsbSubmitPlannerSafety out{};
  if (!started_) {
    return out;
  }
  out.descriptors_preallocated = counters_.descriptor_overflows == 0;
  out.no_partial_submits = counters_.partial_submit_calls == 0;
  out.ordering_safe = counters_.slot_order_errors == 0;
  out.timestamp_safe = counters_.timestamp_regressions == 0;
  out.batching_safe = config_.slots_per_submit > 1 &&
                      counters_.usb_submit_calls > 0 &&
                      counters_.usb_submit_reduction_ratio >=
                          static_cast<double>(config_.slots_per_submit);
  out.product_safe = out.descriptors_preallocated && out.no_partial_submits &&
                     out.ordering_safe && out.timestamp_safe && out.batching_safe;
  return out;
}

std::span<const UsbSubmitDescriptor> PreparedUsbSubmitPlanner::descriptors() const {
  return std::span<const UsbSubmitDescriptor>(descriptors_.data(), descriptor_count_);
}

PreparedUsbSubmitPlanner::DirectionState& PreparedUsbSubmitPlanner::state_for(
    UsbSlotDirection direction) {
  return direction == UsbSlotDirection::Capture ? capture_ : playback_;
}

const PreparedUsbSubmitPlanner::DirectionState& PreparedUsbSubmitPlanner::state_for(
    UsbSlotDirection direction) const {
  return direction == UsbSlotDirection::Capture ? capture_ : playback_;
}

void PreparedUsbSubmitPlanner::flush_direction(UsbSlotDirection direction, bool partial) {
  auto& state = state_for(direction);
  if (state.pending_slots == 0) {
    return;
  }

  if (descriptor_count_ >= descriptors_.size()) {
    counters_.descriptor_overflows += 1;
    state.pending_slots = 0;
    return;
  }

  const auto slots = static_cast<std::uint64_t>(state.pending_slots);
  const auto payload_frames =
      (slots * static_cast<std::uint64_t>(config_.bytes_per_slot)) / kMode2FullFrameBytes;
  descriptors_[static_cast<std::size_t>(descriptor_count_)] =
      UsbSubmitDescriptor{.direction = direction,
                          .first_sequence = state.pending_first_sequence,
                          .slot_count = slots,
                          .first_sample_timestamp = state.pending_first_timestamp,
                          .frame_count = payload_frames,
                          .byte_count = slots * config_.bytes_per_slot};
  descriptor_count_ += 1;

  counters_.usb_submit_calls += 1;
  if (direction == UsbSlotDirection::Capture) {
    counters_.capture_usb_submit_calls += 1;
  } else {
    counters_.playback_usb_submit_calls += 1;
  }
  if (partial) {
    counters_.partial_submit_calls += 1;
  }
  counters_.total_frames += payload_frames;
  counters_.total_bytes += slots * config_.bytes_per_slot;
  state.pending_slots = 0;
}

void PreparedUsbSubmitPlanner::update_reduction_ratio() {
  if (counters_.usb_submit_calls == 0) {
    counters_.usb_submit_reduction_ratio = 0.0;
    return;
  }
  counters_.usb_submit_reduction_ratio =
      static_cast<double>(counters_.logical_slots) /
      static_cast<double>(counters_.usb_submit_calls);
}

}  // namespace opena8djcpp
