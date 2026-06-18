#include "opena8djcpp/persistent_usb_transport.hpp"

#include <algorithm>

namespace opena8djcpp {

bool PersistentUsbTransport::start(const PersistentUsbTransportConfig& config) {
  if (!config_valid(config)) {
    return false;
  }

  PreparedUsbRequestPool request_pool;
  if (!request_pool.start(config.request_pool)) {
    return false;
  }

  config_ = config;
  request_pool_ = request_pool;
  counters_ = {};
  capture_ = {};
  playback_ = {};
  capture_.pool_slot_base = 0;
  playback_.pool_slot_base = config.capture_queue_depth;
  started_ = true;
  primed_ = false;
  return true;
}

void PersistentUsbTransport::stop() {
  if (!started_) {
    return;
  }
  (void)drain();
  request_pool_.stop();
  started_ = false;
  primed_ = false;
}

bool PersistentUsbTransport::prime() {
  if (!started_ || primed_) {
    return false;
  }

  for (std::uint32_t index = 0; index < config_.capture_queue_depth; ++index) {
    if (!submit_next(UsbSlotDirection::Capture, true)) {
      return false;
    }
  }
  for (std::uint32_t index = 0; index < config_.playback_queue_depth; ++index) {
    if (!submit_next(UsbSlotDirection::Playback, true)) {
      return false;
    }
  }
  primed_ = true;
  account_depth(UsbSlotDirection::Capture);
  account_depth(UsbSlotDirection::Playback);
  return true;
}

bool PersistentUsbTransport::complete_next(UsbSlotDirection direction) {
  if (!started_ || !primed_) {
    counters_.completion_failures += 1;
    return false;
  }

  auto& state = state_for(direction);
  if (state.live == 0) {
    counters_.completion_failures += 1;
    counters_.depth_drift_errors += 1;
    return false;
  }

  const auto request = pop_live(state);
  bool ok = request_pool_.complete(request.handle);
  if (!ok) {
    counters_.completion_failures += 1;
  }

  if (request.sequence != state.expected_completion_sequence) {
    counters_.sequence_gap_errors += 1;
    ok = false;
  }
  if (request.sample_timestamp != state.expected_completion_timestamp) {
    counters_.timestamp_gap_errors += 1;
    ok = false;
  }

  counters_.completion_calls += 1;
  counters_.completed_frames += config_.slots_per_submit * config_.frames_per_slot;
  counters_.completed_bytes += config_.slots_per_submit * bytes_per_slot_for(direction);
  if (direction == UsbSlotDirection::Capture) {
    counters_.capture_completion_calls += 1;
  } else {
    counters_.playback_completion_calls += 1;
  }

  state.expected_completion_sequence += config_.slots_per_submit;
  state.expected_completion_timestamp += config_.slots_per_submit * config_.frames_per_slot;

  ok = submit_next(direction, false) && ok;
  account_depth(direction);
  return ok;
}

std::uint64_t PersistentUsbTransport::drain() {
  if (!started_) {
    return 0;
  }
  const auto cancelled = request_pool_.cancel_all();
  counters_.cancelled_requests += cancelled;
  capture_.live = 0;
  capture_.head = 0;
  playback_.live = 0;
  playback_.head = 0;
  return cancelled;
}

PersistentUsbTransportCounters PersistentUsbTransport::counters() const {
  auto out = counters_;
  const auto pool = request_pool_.counters();
  out.fallback_allocations = pool.fallback_allocations;
  out.live_requests = pool.live_requests;
  out.max_live_requests = std::max(out.max_live_requests, pool.max_live_requests);
  out.submitted_frames = pool.submitted_frames;
  out.completed_frames = pool.completed_frames;
  out.submitted_bytes = pool.submitted_bytes;
  out.completed_bytes = pool.completed_bytes;
  return out;
}

PersistentUsbTransportSafety PersistentUsbTransport::safety() const {
  const auto counters = this->counters();
  const auto pool = request_pool_.safety();
  const auto max_configured_live =
      static_cast<std::uint64_t>(config_.capture_queue_depth + config_.playback_queue_depth);
  PersistentUsbTransportSafety out{};
  out.preallocated_only = pool.preallocated_only && counters.fallback_allocations == 0;
  out.bounded_live_requests = counters.max_live_requests <= max_configured_live &&
                              counters.live_limit_failures == 0;
  out.stable_queue_depth = counters.depth_drift_errors == 0 &&
                           counters.max_capture_live_requests == config_.capture_queue_depth &&
                           counters.max_playback_live_requests == config_.playback_queue_depth;
  out.continuous_sequences = counters.sequence_gap_errors == 0;
  out.timestamp_continuity = counters.timestamp_gap_errors == 0;
  out.descriptor_shape_safe = counters.descriptor_shape_errors == 0;
  out.persistent_slot_identity =
      counters.slot_identity_mismatches == 0 &&
      counters.persistent_slot_reuses == counters.steady_submit_calls;
  out.completion_owned_lifecycle =
      counters.completion_failures == 0 &&
      counters.prime_submit_calls + counters.steady_submit_calls ==
          counters.completion_calls + counters.cancelled_requests;
  out.drained = counters.live_requests == 0;
  out.product_safe = out.preallocated_only && out.bounded_live_requests &&
                     out.stable_queue_depth && out.continuous_sequences &&
                     out.timestamp_continuity && out.descriptor_shape_safe &&
                     out.persistent_slot_identity && out.completion_owned_lifecycle &&
                     out.drained;
  return out;
}

bool PersistentUsbTransport::config_valid(const PersistentUsbTransportConfig& config) const {
  const auto configured_live = config.capture_queue_depth + config.playback_queue_depth;
  return config.slots_per_submit > 0 && config.frames_per_slot > 0 &&
         config.capture_bytes_per_slot > 0 && config.playback_bytes_per_slot > 0 &&
         config.capture_queue_depth > 0 && config.playback_queue_depth > 0 &&
         configured_live <= config.request_pool.request_slots &&
         configured_live <= kPreparedUsbRequestMaxSlots &&
         config.request_pool.request_slots <= kPreparedUsbRequestMaxSlots;
}

std::uint32_t PersistentUsbTransport::depth_for(UsbSlotDirection direction) const {
  return direction == UsbSlotDirection::Capture ? config_.capture_queue_depth
                                                : config_.playback_queue_depth;
}

std::uint32_t PersistentUsbTransport::bytes_per_slot_for(UsbSlotDirection direction) const {
  return direction == UsbSlotDirection::Capture ? config_.capture_bytes_per_slot
                                                : config_.playback_bytes_per_slot;
}

PersistentUsbTransport::DirectionState& PersistentUsbTransport::state_for(
    UsbSlotDirection direction) {
  return direction == UsbSlotDirection::Capture ? capture_ : playback_;
}

const PersistentUsbTransport::DirectionState& PersistentUsbTransport::state_for(
    UsbSlotDirection direction) const {
  return direction == UsbSlotDirection::Capture ? capture_ : playback_;
}

bool PersistentUsbTransport::submit_next(UsbSlotDirection direction, bool prime_submit) {
  auto& state = state_for(direction);
  if (state.live >= depth_for(direction)) {
    counters_.live_limit_failures += 1;
    counters_.submit_failures += 1;
    return false;
  }

  const auto slot_count = static_cast<std::uint64_t>(config_.slots_per_submit);
  const auto frame_count = slot_count * config_.frames_per_slot;
  const auto byte_count = slot_count * bytes_per_slot_for(direction);
  if (slot_count == 0 || frame_count == 0 || byte_count == 0) {
    counters_.descriptor_shape_errors += 1;
    counters_.submit_failures += 1;
    return false;
  }

  const UsbSubmitDescriptor descriptor{
      .direction = direction,
      .first_sequence = state.next_sequence,
      .slot_count = slot_count,
      .first_sample_timestamp = state.next_sample_timestamp,
      .frame_count = frame_count,
      .byte_count = byte_count,
  };
  const auto expected_slot = expected_pool_slot(state, direction);
  const auto handle = request_pool_.submit(descriptor);
  if (!handle.valid()) {
    counters_.submit_failures += 1;
    return false;
  }
  if (handle.slot != expected_slot) {
    counters_.slot_identity_mismatches += 1;
    counters_.submit_failures += 1;
    return false;
  }
  if (!prime_submit) {
    counters_.persistent_slot_reuses += 1;
  }

  if (!push_live(state, LiveRequest{.handle = handle,
                                    .sequence = descriptor.first_sequence,
                                    .sample_timestamp = descriptor.first_sample_timestamp})) {
    counters_.submit_failures += 1;
    return false;
  }

  if (prime_submit) {
    counters_.prime_submit_calls += 1;
  } else {
    counters_.steady_submit_calls += 1;
  }
  if (direction == UsbSlotDirection::Capture) {
    counters_.capture_submit_calls += 1;
  } else {
    counters_.playback_submit_calls += 1;
  }

  state.next_sequence += slot_count;
  state.next_sample_timestamp += frame_count;
  account_lead(direction);
  return true;
}

std::uint32_t PersistentUsbTransport::expected_pool_slot(
    const DirectionState& state,
    UsbSlotDirection direction) const {
  const auto depth = depth_for(direction);
  if (depth == 0) {
    return kInvalidUsbRequestSlot;
  }
  return state.pool_slot_base + ((state.head + state.live) % depth);
}

bool PersistentUsbTransport::push_live(DirectionState& state, const LiveRequest& request) {
  if (state.live >= kPreparedUsbRequestMaxSlots) {
    return false;
  }
  const auto tail = (state.head + state.live) % kPreparedUsbRequestMaxSlots;
  state.queue[tail] = request;
  state.live += 1;
  return true;
}

PersistentUsbTransport::LiveRequest PersistentUsbTransport::pop_live(DirectionState& state) {
  const auto request = state.queue[state.head];
  state.queue[state.head] = {};
  state.head = (state.head + 1U) % kPreparedUsbRequestMaxSlots;
  state.live -= 1;
  return request;
}

void PersistentUsbTransport::account_lead(UsbSlotDirection direction) {
  const auto& state = state_for(direction);
  const auto lead = state.next_sample_timestamp - state.expected_completion_timestamp;
  if (direction == UsbSlotDirection::Capture) {
    counters_.max_capture_lead_frames = std::max(counters_.max_capture_lead_frames, lead);
  } else {
    counters_.max_playback_lead_frames = std::max(counters_.max_playback_lead_frames, lead);
  }
}

void PersistentUsbTransport::account_depth(UsbSlotDirection direction) {
  const auto& state = state_for(direction);
  if (state.live != depth_for(direction)) {
    counters_.depth_drift_errors += 1;
  }
  if (direction == UsbSlotDirection::Capture) {
    counters_.max_capture_live_requests =
        std::max<std::uint64_t>(counters_.max_capture_live_requests, state.live);
  } else {
    counters_.max_playback_live_requests =
        std::max<std::uint64_t>(counters_.max_playback_live_requests, state.live);
  }
}

}  // namespace opena8djcpp
