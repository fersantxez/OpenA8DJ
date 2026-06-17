#include "opena8djcpp/prepared_transport.hpp"

#include <algorithm>

namespace opena8djcpp {

bool PreparedTransportBackend::start(const PreparedTransportConfig& config) {
  if (config.iso_frames == 0 || config.capture_slots == 0 || config.playback_slots == 0 ||
      config.capture_slots > kPreparedTransportMaxSlots ||
      config.playback_slots > kPreparedTransportMaxSlots) {
    return false;
  }

  config_ = config;
  counters_ = {};
  capture_ring_.clear();
  playback_ring_.clear();
  last_timestamp_ = 0;
  have_timestamp_ = false;
  started_ = true;
  counters_.backend_prepare_enqueues =
      static_cast<std::uint64_t>(config_.capture_slots) + config_.playback_slots;
  validate_channel_identity();
  return true;
}

void PreparedTransportBackend::stop() {
  started_ = false;
  capture_ring_.clear();
  playback_ring_.clear();
}

bool PreparedTransportBackend::backend_complete_period(
    const S24Frame& capture_frame,
    std::uint64_t sample_timestamp,
    const PreparedTransportStepOptions& options) {
  S24Frame playback_frame{};
  return backend_complete_period(std::span<const S24Frame>(&capture_frame, 1),
                                 std::span<S24Frame>(&playback_frame, 1), sample_timestamp,
                                 options);
}

bool PreparedTransportBackend::backend_complete_period(
    std::span<const S24Frame> capture_frames,
    std::span<S24Frame> playback_frames,
    std::uint64_t sample_timestamp,
    const PreparedTransportStepOptions& options) {
  if (!started_) {
    return false;
  }

  const auto gap = static_cast<double>(std::max(1U, options.completion_gap_periods));
  counters_.max_completion_gap_ratio = std::max(counters_.max_completion_gap_ratio, gap);
  counters_.backend_steady_requeues += 2;
  if (options.hal_direct_requeue_attempt) {
    counters_.hal_steady_requeues += 2;
  }
  if (options.fallback_allocation_attempt) {
    counters_.fallback_allocations += 2;
  }

  if (options.force_timestamp_regression && have_timestamp_) {
    sample_timestamp = last_timestamp_;
  }
  record_timestamp(sample_timestamp);

  const auto captured = capture_ring_.push_many(capture_frames);
  counters_.backend_capture_frames += captured;
  if (captured != capture_frames.size()) {
    counters_.capture_ring_overruns += capture_frames.size() - captured;
  }

  const auto popped = playback_ring_.pop_many(playback_frames);
  counters_.backend_playback_frames += popped;
  if (popped != playback_frames.size()) {
    counters_.playback_ring_underruns += playback_frames.size() - popped;
    for (std::size_t index = popped; index < playback_frames.size(); ++index) {
      playback_frames[index] = {};
    }
  }

  return true;
}

bool PreparedTransportBackend::hal_write_playback(const S24Frame& frame) {
  if (!started_) {
    return false;
  }
  if (!playback_ring_.push(frame)) {
    counters_.playback_ring_overruns += 1;
    return false;
  }
  counters_.hal_playback_writes += 1;
  return true;
}

std::uint32_t PreparedTransportBackend::hal_write_playback(std::span<const S24Frame> frames) {
  if (!started_) {
    return 0;
  }
  const auto pushed = playback_ring_.push_many(frames);
  counters_.hal_playback_writes += pushed;
  if (pushed != frames.size()) {
    counters_.playback_ring_overruns += frames.size() - pushed;
  }
  return static_cast<std::uint32_t>(pushed);
}

bool PreparedTransportBackend::hal_read_capture(S24Frame& frame) {
  if (!started_) {
    return false;
  }
  if (!capture_ring_.pop(frame)) {
    counters_.capture_ring_underruns += 1;
    return false;
  }
  counters_.hal_capture_reads += 1;
  return true;
}

std::uint32_t PreparedTransportBackend::hal_read_capture(std::span<S24Frame> frames) {
  if (!started_) {
    return 0;
  }
  const auto popped = capture_ring_.pop_many(frames);
  counters_.hal_capture_reads += popped;
  if (popped != frames.size()) {
    counters_.capture_ring_underruns += frames.size() - popped;
  }
  return static_cast<std::uint32_t>(popped);
}

PreparedTransportSafety PreparedTransportBackend::safety() const {
  PreparedTransportSafety out{};
  if (!started_) {
    return out;
  }
  out.prepared_slots_only = counters_.fallback_allocations == 0;
  out.cadence_safe =
      counters_.max_completion_gap_ratio <= kPreparedTransportMaxCompletionGapRatio &&
      counters_.timestamp_regressions == 0 && counters_.capture_ring_overruns == 0 &&
      counters_.capture_ring_underruns == 0 && counters_.playback_ring_overruns == 0 &&
      counters_.playback_ring_underruns == 0;
  out.routing_safe = counters_.channel_identity_failures == 0;
  out.timecode_safe = counters_.timecode_profile_failures == 0 && out.routing_safe &&
                      counters_.timestamp_regressions == 0;
  out.hal_hot_path_safe = counters_.hal_steady_requeues == 0 && out.prepared_slots_only;
  out.product_safe =
      out.hal_hot_path_safe && out.cadence_safe && out.routing_safe && out.timecode_safe;
  return out;
}

void PreparedTransportBackend::validate_channel_identity() {
  const auto surface = make_audio8dj_surface();
  if (surface.input_channels != kInputChannels || surface.output_channels != kOutputChannels) {
    counters_.channel_identity_failures += 1;
    return;
  }

  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto input = surface.input_map[channel];
    const auto output = surface.output_map[channel];
    if (input.pair != output.pair || input.side != output.side) {
      counters_.channel_identity_failures += 1;
    }
  }
}

void PreparedTransportBackend::record_timestamp(std::uint64_t sample_timestamp) {
  if (have_timestamp_ && sample_timestamp <= last_timestamp_) {
    counters_.timestamp_regressions += 1;
  }
  last_timestamp_ = sample_timestamp;
  have_timestamp_ = true;
}

}  // namespace opena8djcpp
