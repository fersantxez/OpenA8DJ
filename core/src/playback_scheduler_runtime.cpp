#include "opena8djcpp/playback_scheduler_runtime.hpp"

namespace opena8djcpp {

bool PlaybackSchedulerRuntimeBinding::start(const PlaybackSchedulerRuntimeConfig& config) {
  if (config.frames_per_slot != kPlaybackLeadSchedulerIsoFrames ||
      config.capture_bytes_per_slot == 0 || config.playback_bytes_per_slot == 0 ||
      config.scheduler.iso_frames != kPlaybackLeadSchedulerIsoFrames) {
    return false;
  }

  PreparedUsbRequestPool request_pool;
  if (!request_pool.start(config.request_pool)) {
    return false;
  }

  PlaybackLeadScheduler scheduler;
  if (!scheduler.start(config.scheduler)) {
    request_pool.stop();
    return false;
  }

  config_ = config;
  request_pool_ = request_pool;
  scheduler_ = scheduler;
  counters_ = {};
  next_capture_sequence_ = 0;
  next_playback_sequence_ = 0;
  started_ = true;

  const bool primed = submit_playback_delta(0);
  counters_.scheduler = scheduler_.counters();
  update_runtime_ratios(counters_);
  if (!primed) {
    stop();
    return false;
  }
  return true;
}

void PlaybackSchedulerRuntimeBinding::stop() {
  if (!started_) {
    return;
  }
  (void)request_pool_.cancel_all();
  request_pool_.stop();
  scheduler_.stop();
  started_ = false;
}

bool PlaybackSchedulerRuntimeBinding::complete_period(
    const PlaybackLeadSchedulerStepOptions& options) {
  if (!started_) {
    counters_.runtime_submit_failures += 1;
    return false;
  }

  bool ok = true;
  if (!options.force_capture_gap) {
    ok = submit_runtime_descriptor(UsbSlotDirection::Capture,
                                   next_capture_sequence_,
                                   next_capture_sequence_ * config_.frames_per_slot,
                                   1) &&
         ok;
    next_capture_sequence_ += 1;
  } else {
    counters_.runtime_capture_gap_periods += 1;
  }

  const auto previous_playback_slots =
      scheduler_.counters().playback_logical_slots_submitted;
  ok = scheduler_.complete_period(options) && ok;
  ok = submit_playback_delta(previous_playback_slots) && ok;

  counters_.scheduler = scheduler_.counters();
  update_runtime_ratios(counters_);
  return ok;
}

PlaybackSchedulerRuntimeCounters PlaybackSchedulerRuntimeBinding::counters() const {
  auto out = counters_;
  out.scheduler = scheduler_.counters();
  out.request_pool = request_pool_.counters();
  const auto pool = request_pool_.counters();
  out.capture_runtime_submit_calls = pool.capture_submit_calls;
  out.playback_runtime_submit_calls = pool.playback_submit_calls;
  update_runtime_ratios(out);
  return out;
}

PlaybackSchedulerRuntimeSafety PlaybackSchedulerRuntimeBinding::safety() const {
  const auto counters = this->counters();
  const auto scheduler_safety = scheduler_.safety();
  const auto pool_safety = request_pool_.safety();
  PlaybackSchedulerRuntimeSafety out{};
  out.scheduler_safe = scheduler_safety.product_model_safe;
  out.request_pool_safe = pool_safety.product_safe;
  out.capture_continuity_preserved =
      counters.scheduler.capture_request_submit_calls == counters.capture_runtime_submit_calls &&
      counters.scheduler.capture_gap_periods == counters.runtime_capture_gap_periods &&
      counters.runtime_capture_gap_periods == 0;
  out.playback_batching_bound =
      counters.playback_runtime_submit_calls == counters.scheduler.playback_request_submit_calls &&
      counters.scheduler.playback_submit_reduction_ratio >=
          static_cast<double>(config_.scheduler.max_slots_per_submit);
  out.runtime_accounting_matches_scheduler =
      counters.playback_runtime_logical_slots_submitted ==
          counters.scheduler.playback_logical_slots_submitted &&
      counters.request_pool.submit_calls ==
          counters.capture_runtime_submit_calls + counters.playback_runtime_submit_calls &&
      counters.request_pool.completion_calls == counters.request_pool.submit_calls;
  out.no_runtime_failures =
      counters.runtime_submit_failures == 0 && counters.runtime_completion_failures == 0 &&
      counters.request_pool.fallback_allocations == 0 &&
      counters.request_pool.invalid_completions == 0 &&
      counters.request_pool.stale_completions == 0;
  out.product_model_safe = out.scheduler_safe && out.request_pool_safe &&
                           out.capture_continuity_preserved &&
                           out.playback_batching_bound &&
                           out.runtime_accounting_matches_scheduler &&
                           out.no_runtime_failures;
  return out;
}

bool PlaybackSchedulerRuntimeBinding::submit_runtime_descriptor(
    UsbSlotDirection direction,
    std::uint64_t first_sequence,
    std::uint64_t first_sample_timestamp,
    std::uint64_t slot_count) {
  const auto bytes_per_slot = direction == UsbSlotDirection::Capture
                                  ? config_.capture_bytes_per_slot
                                  : config_.playback_bytes_per_slot;
  const UsbSubmitDescriptor descriptor{
      .direction = direction,
      .first_sequence = first_sequence,
      .slot_count = slot_count,
      .first_sample_timestamp = first_sample_timestamp,
      .frame_count = slot_count * config_.frames_per_slot,
      .byte_count = slot_count * bytes_per_slot,
  };
  const auto handle = request_pool_.submit(descriptor);
  if (!handle.valid()) {
    counters_.runtime_submit_failures += 1;
    return false;
  }
  if (!request_pool_.complete(handle)) {
    counters_.runtime_completion_failures += 1;
    return false;
  }
  return true;
}

bool PlaybackSchedulerRuntimeBinding::submit_playback_delta(
    std::uint64_t previous_logical_slots) {
  const auto current = scheduler_.counters().playback_logical_slots_submitted;
  if (current == previous_logical_slots) {
    return true;
  }
  if (current < previous_logical_slots) {
    counters_.runtime_submit_failures += 1;
    return false;
  }
  const auto delta = current - previous_logical_slots;
  const bool ok = submit_runtime_descriptor(UsbSlotDirection::Playback,
                                            next_playback_sequence_,
                                            next_playback_sequence_ * config_.frames_per_slot,
                                            delta);
  if (ok) {
    next_playback_sequence_ += delta;
    counters_.playback_runtime_logical_slots_submitted += delta;
  }
  return ok;
}

void PlaybackSchedulerRuntimeBinding::update_runtime_ratios(
    PlaybackSchedulerRuntimeCounters& counters) const {
  const auto logical_submit_work =
      counters.scheduler.capture_request_submit_calls +
      counters.scheduler.playback_logical_slots_submitted;
  const auto runtime_submit_work =
      counters.capture_runtime_submit_calls + counters.playback_runtime_submit_calls;
  if (runtime_submit_work > 0) {
    counters.runtime_total_submit_reduction_ratio =
        static_cast<double>(logical_submit_work) / static_cast<double>(runtime_submit_work);
  }
}

}  // namespace opena8djcpp
