#include "opena8djcpp/prepared_transport.hpp"

#include <algorithm>

namespace opena8djcpp {

bool PreparedSlotScheduler::start(const PreparedSlotSchedulerConfig& config) {
  if (config.capture_target_slots == 0 || config.playback_target_slots == 0 ||
      config.capture_pool_slots == 0 || config.playback_pool_slots == 0 ||
      config.capture_pool_slots > kPreparedTransportMaxSlots ||
      config.playback_pool_slots > kPreparedTransportMaxSlots ||
      config.capture_target_slots > kPreparedTransportMaxSlots ||
      config.playback_target_slots > kPreparedTransportMaxSlots ||
      config.playback_completion_gap_periods == 0 ||
      config.max_backend_requeues_per_period == 0 ||
      config.unavailable_capture_slots > config.capture_pool_slots ||
      config.unavailable_playback_slots > config.playback_pool_slots) {
    return false;
  }

  config_ = config;
  counters_ = {};
  started_ = true;
  for (std::uint32_t index = 0; index < config_.capture_target_slots; ++index) {
    queue_capture_slot(true);
  }
  for (std::uint32_t index = 0; index < config_.playback_target_slots; ++index) {
    queue_playback_slot(true);
  }
  counters_.min_capture_in_flight = counters_.capture_in_flight;
  counters_.min_playback_in_flight = counters_.playback_in_flight;
  record_lead();
  return true;
}

void PreparedSlotScheduler::stop() {
  started_ = false;
  counters_.capture_in_flight = 0;
  counters_.playback_in_flight = 0;
}

bool PreparedSlotScheduler::complete_period(const PreparedSlotSchedulerStepOptions& options) {
  if (!started_) {
    return false;
  }

  counters_.periods += 1;
  if (options.hal_direct_requeue_attempt) {
    counters_.hal_steady_requeues += 2;
  }
  if (options.fallback_allocation_attempt) {
    counters_.fallback_allocations += 2;
  }
  const double completion_gap_ratio =
      static_cast<double>(config_.playback_completion_gap_periods);
  counters_.max_completion_gap_ratio =
      std::max(counters_.max_completion_gap_ratio, completion_gap_ratio);
  if (completion_gap_ratio > kPreparedTransportMaxCompletionGapRatio) {
    counters_.completion_gap_violations += 1;
  }

  if (counters_.capture_in_flight == 0) {
    counters_.capture_starved_periods += 1;
  } else {
    counters_.capture_in_flight -= 1;
  }

  if ((counters_.periods % config_.playback_completion_gap_periods) == 0) {
    if (counters_.playback_in_flight == 0) {
      counters_.playback_starved_periods += 1;
    } else {
      counters_.playback_in_flight -= 1;
    }
  }

  const auto before_requeues = counters_.backend_steady_requeues;
  if (!options.suppress_backend_requeue) {
    refill_to_targets();
  }
  const auto period_requeues = counters_.backend_steady_requeues - before_requeues;
  if (period_requeues > config_.max_backend_requeues_per_period) {
    counters_.backend_requeue_budget_violations += 1;
  }

  record_lead();
  return true;
}

PreparedSlotSchedulerSafety PreparedSlotScheduler::safety() const {
  PreparedSlotSchedulerSafety out{};
  if (!started_) {
    return out;
  }
  out.prepared_slots_only = counters_.fallback_allocations == 0;
  out.lead_safe = counters_.capture_starved_periods == 0 &&
                  counters_.playback_starved_periods == 0 &&
                  counters_.min_capture_in_flight >= config_.capture_target_slots &&
                  counters_.min_playback_in_flight >= config_.playback_target_slots;
  out.cadence_safe = counters_.completion_gap_violations == 0;
  out.hal_hot_path_safe = counters_.hal_steady_requeues == 0;
  out.backend_budget_safe = counters_.backend_requeue_budget_violations == 0;
  out.product_safe = out.prepared_slots_only && out.lead_safe && out.cadence_safe &&
                     out.hal_hot_path_safe && out.backend_budget_safe;
  return out;
}

void PreparedSlotScheduler::queue_capture_slot(bool prepare) {
  if (config_.unavailable_capture_slots + counters_.capture_in_flight >=
      config_.capture_pool_slots) {
    counters_.fallback_allocations += 1;
  }
  counters_.capture_in_flight += 1;
  if (prepare) {
    counters_.backend_prepare_enqueues += 1;
  } else {
    counters_.backend_steady_requeues += 1;
  }
}

void PreparedSlotScheduler::queue_playback_slot(bool prepare) {
  if (config_.unavailable_playback_slots + counters_.playback_in_flight >=
      config_.playback_pool_slots) {
    counters_.fallback_allocations += 1;
  }
  counters_.playback_in_flight += 1;
  if (prepare) {
    counters_.backend_prepare_enqueues += 1;
  } else {
    counters_.backend_steady_requeues += 1;
  }
}

void PreparedSlotScheduler::refill_to_targets() {
  while (counters_.capture_in_flight < config_.capture_target_slots) {
    queue_capture_slot(false);
  }
  while (counters_.playback_in_flight < config_.playback_target_slots) {
    queue_playback_slot(false);
  }
}

void PreparedSlotScheduler::record_lead() {
  counters_.min_capture_in_flight =
      std::min(counters_.min_capture_in_flight, counters_.capture_in_flight);
  counters_.min_playback_in_flight =
      std::min(counters_.min_playback_in_flight, counters_.playback_in_flight);
  counters_.max_capture_in_flight =
      std::max(counters_.max_capture_in_flight, counters_.capture_in_flight);
  counters_.max_playback_in_flight =
      std::max(counters_.max_playback_in_flight, counters_.playback_in_flight);
}

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
  capture_ring_.reset_publication_counters();
  playback_ring_.reset_publication_counters();
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
  snapshot_ring_publications();

  return true;
}

bool PreparedTransportBackend::hal_write_playback(const S24Frame& frame) {
  if (!started_) {
    return false;
  }
  if (!playback_ring_.push(frame)) {
    counters_.playback_ring_overruns += 1;
    snapshot_ring_publications();
    return false;
  }
  counters_.hal_playback_writes += 1;
  snapshot_ring_publications();
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
  snapshot_ring_publications();
  return static_cast<std::uint32_t>(pushed);
}

bool PreparedTransportBackend::hal_read_capture(S24Frame& frame) {
  if (!started_) {
    return false;
  }
  if (!capture_ring_.pop(frame)) {
    counters_.capture_ring_underruns += 1;
    snapshot_ring_publications();
    return false;
  }
  counters_.hal_capture_reads += 1;
  snapshot_ring_publications();
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
  snapshot_ring_publications();
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

void PreparedTransportBackend::snapshot_ring_publications() {
  counters_.capture_ring_write_publications = capture_ring_.write_publications();
  counters_.capture_ring_read_publications = capture_ring_.read_publications();
  counters_.playback_ring_write_publications = playback_ring_.write_publications();
  counters_.playback_ring_read_publications = playback_ring_.read_publications();
}

}  // namespace opena8djcpp
