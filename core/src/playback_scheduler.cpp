#include "opena8djcpp/playback_scheduler.hpp"

#include <algorithm>

namespace opena8djcpp {

bool PlaybackLeadScheduler::start(const PlaybackLeadSchedulerConfig& config) {
  if (config.iso_frames != kPlaybackLeadSchedulerIsoFrames ||
      config.target_lead_slots == 0 || config.low_watermark_slots == 0 ||
      config.high_watermark_slots < config.target_lead_slots ||
      config.low_watermark_slots >= config.high_watermark_slots ||
      config.max_slots_per_submit == 0 ||
      config.max_slots_per_submit > kPlaybackLeadSchedulerMaxPoolSlots ||
      config.playback_pool_slots == 0 ||
      config.playback_pool_slots > kPlaybackLeadSchedulerMaxPoolSlots ||
      config.high_watermark_slots > config.playback_pool_slots ||
      config.playback_completion_gap_periods == 0) {
    return false;
  }

  config_ = config;
  counters_ = {};
  playback_lead_slots_ = 0;
  started_ = true;
  submit_playback_slots(config_.target_lead_slots, false);
  counters_.min_playback_lead_slots = playback_lead_slots_;
  counters_.max_playback_lead_slots = playback_lead_slots_;
  update_ratios();
  return true;
}

void PlaybackLeadScheduler::stop() {
  started_ = false;
  playback_lead_slots_ = 0;
}

bool PlaybackLeadScheduler::complete_period(const PlaybackLeadSchedulerStepOptions& options) {
  if (!started_) {
    return false;
  }

  counters_.periods += 1;
  counters_.logical_audio_periods += 1;

  if (config_.capture_submit_every_period && !options.force_capture_gap) {
    counters_.capture_request_submit_calls += 1;
  } else {
    counters_.capture_gap_periods += 1;
  }

  if ((counters_.periods % config_.playback_completion_gap_periods) == 0) {
    consume_playback_period();
  } else {
    counters_.logical_cadence_violations += 1;
  }

  maybe_refill_playback(options);
  record_lead();
  update_ratios();
  return true;
}

PlaybackLeadSchedulerSafety PlaybackLeadScheduler::safety() const {
  PlaybackLeadSchedulerSafety out{};
  if (!started_) {
    return out;
  }
  out.iso8_cadence_preserved =
      config_.iso_frames == kPlaybackLeadSchedulerIsoFrames &&
      config_.playback_completion_gap_periods == 1 &&
      counters_.logical_cadence_violations == 0;
  out.capture_continuity_preserved =
      config_.capture_submit_every_period && counters_.capture_gap_periods == 0;
  out.playback_lead_safe =
      counters_.playback_underflow_periods == 0 &&
      counters_.min_playback_lead_slots >= config_.low_watermark_slots &&
      counters_.max_playback_lead_slots <= config_.playback_pool_slots;
  out.playback_batching_effective =
      config_.max_slots_per_submit > 1 &&
      counters_.playback_request_submit_calls > 0 &&
      counters_.playback_submit_reduction_ratio >=
          static_cast<double>(config_.max_slots_per_submit);
  out.pool_safe = counters_.playback_pool_overflows == 0 &&
                  counters_.playback_batch_overflows == 0;
  out.callback_work_reduced =
      counters_.playback_submit_events_from_audio_callback < counters_.logical_audio_periods &&
      counters_.total_submit_reduction_ratio > 1.0;
  out.product_model_safe = out.iso8_cadence_preserved && out.capture_continuity_preserved &&
                           out.playback_lead_safe && out.playback_batching_effective &&
                           out.pool_safe && out.callback_work_reduced;
  return out;
}

void PlaybackLeadScheduler::submit_playback_slots(std::uint32_t slots, bool from_audio_callback) {
  if (slots == 0) {
    return;
  }
  if (slots > config_.max_slots_per_submit) {
    counters_.playback_batch_overflows += 1;
    slots = config_.max_slots_per_submit;
  }
  if (playback_lead_slots_ + slots > config_.playback_pool_slots) {
    counters_.playback_pool_overflows += 1;
    slots = config_.playback_pool_slots - playback_lead_slots_;
  }
  if (slots == 0) {
    return;
  }
  playback_lead_slots_ += slots;
  counters_.playback_logical_slots_submitted += slots;
  counters_.playback_request_submit_calls += 1;
  counters_.max_slots_per_submit_observed =
      std::max(counters_.max_slots_per_submit_observed, slots);
  if (from_audio_callback) {
    counters_.playback_submit_events_from_audio_callback += 1;
  }
}

void PlaybackLeadScheduler::maybe_refill_playback(
    const PlaybackLeadSchedulerStepOptions& options) {
  if (options.force_playback_batch_overflow) {
    submit_playback_slots(config_.max_slots_per_submit + 1U, true);
    return;
  }
  if (options.suppress_playback_submit || playback_lead_slots_ > config_.low_watermark_slots) {
    return;
  }
  const auto desired = config_.high_watermark_slots - playback_lead_slots_;
  submit_playback_slots(std::min(desired, config_.max_slots_per_submit), true);
}

void PlaybackLeadScheduler::consume_playback_period() {
  if (playback_lead_slots_ == 0) {
    counters_.playback_underflow_periods += 1;
    return;
  }
  playback_lead_slots_ -= 1;
  counters_.playback_slots_completed += 1;
}

void PlaybackLeadScheduler::record_lead() {
  counters_.min_playback_lead_slots =
      std::min(counters_.min_playback_lead_slots, playback_lead_slots_);
  counters_.max_playback_lead_slots =
      std::max(counters_.max_playback_lead_slots, playback_lead_slots_);
}

void PlaybackLeadScheduler::update_ratios() {
  if (counters_.playback_request_submit_calls > 0) {
    counters_.playback_submit_reduction_ratio =
        static_cast<double>(counters_.playback_logical_slots_submitted) /
        static_cast<double>(counters_.playback_request_submit_calls);
  }
  const auto logical_submit_work =
      counters_.capture_request_submit_calls + counters_.playback_logical_slots_submitted;
  const auto request_submit_work =
      counters_.capture_request_submit_calls + counters_.playback_request_submit_calls;
  if (request_submit_work > 0) {
    counters_.total_submit_reduction_ratio =
        static_cast<double>(logical_submit_work) / static_cast<double>(request_submit_work);
  }
}

}  // namespace opena8djcpp
