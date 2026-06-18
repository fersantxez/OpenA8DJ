#include "opena8djcpp/usb_isochronous_schedule.hpp"

#include <algorithm>
#include <limits>

namespace opena8djcpp {

bool UsbIsochronousSchedule::start(const UsbIsochronousScheduleConfig& config) {
  if (config.frames_per_slot == 0 || config.slots_per_submit == 0 ||
      config.bytes_per_slot == 0 || config.min_submit_lead_frames == 0) {
    return false;
  }
  config_ = config;
  counters_ = {};
  counters_.min_submit_lead_frames = std::numeric_limits<std::uint64_t>::max();
  capture_ = {};
  playback_ = {};
  started_ = true;
  return true;
}

void UsbIsochronousSchedule::stop() {
  started_ = false;
}

bool UsbIsochronousSchedule::schedule(const UsbSubmitDescriptor& descriptor,
                                      std::uint64_t submit_sample_time) {
  if (!started_) {
    return false;
  }

  bool ok = true;
  if (!descriptor_shape_ok(descriptor)) {
    counters_.descriptor_shape_errors += 1;
    ok = false;
  }

  auto& state = state_for(descriptor.direction);
  if (state.have_descriptor) {
    if (descriptor.first_sample_timestamp <= state.last_timestamp) {
      counters_.timestamp_regressions += 1;
      ok = false;
    }
    if (descriptor.first_sequence <= state.last_sequence) {
      counters_.sequence_regressions += 1;
      ok = false;
    }
  }

  std::uint64_t lead = 0;
  if (descriptor.first_sample_timestamp > submit_sample_time) {
    lead = descriptor.first_sample_timestamp - submit_sample_time;
  }
  if (lead < config_.min_submit_lead_frames) {
    counters_.late_submits += 1;
    ok = false;
  }

  counters_.scheduled_descriptors += 1;
  if (descriptor.direction == UsbSlotDirection::Capture) {
    counters_.capture_descriptors += 1;
  } else {
    counters_.playback_descriptors += 1;
  }
  counters_.total_frames += descriptor.frame_count;
  counters_.total_bytes += descriptor.byte_count;
  counters_.min_submit_lead_frames = std::min(counters_.min_submit_lead_frames, lead);
  counters_.max_submit_lead_frames = std::max(counters_.max_submit_lead_frames, lead);

  state.last_timestamp = descriptor.first_sample_timestamp;
  state.last_sequence = descriptor.first_sequence;
  state.have_descriptor = true;
  return ok;
}

UsbIsochronousScheduleSafety UsbIsochronousSchedule::safety() const {
  UsbIsochronousScheduleSafety out{};
  out.descriptor_shape_safe = counters_.descriptor_shape_errors == 0;
  out.deadline_safe = counters_.scheduled_descriptors > 0 && counters_.late_submits == 0 &&
                      counters_.min_submit_lead_frames >= config_.min_submit_lead_frames;
  out.monotonic_safe =
      counters_.timestamp_regressions == 0 && counters_.sequence_regressions == 0;
  out.product_safe = out.descriptor_shape_safe && out.deadline_safe && out.monotonic_safe;
  return out;
}

UsbIsochronousSchedule::DirectionState& UsbIsochronousSchedule::state_for(
    UsbSlotDirection direction) {
  return direction == UsbSlotDirection::Capture ? capture_ : playback_;
}

bool UsbIsochronousSchedule::descriptor_shape_ok(const UsbSubmitDescriptor& descriptor) const {
  const auto expected_byte_count =
      static_cast<std::uint64_t>(config_.slots_per_submit) * config_.bytes_per_slot;
  const auto expected_frame_count = expected_byte_count / kMode2FullFrameBytes;
  return descriptor.slot_count == config_.slots_per_submit &&
         descriptor.byte_count == expected_byte_count &&
         descriptor.frame_count == expected_frame_count &&
         descriptor.first_sample_timestamp > 0;
}

}  // namespace opena8djcpp
