#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

constexpr double kTargetLatencyFrames = 192.0;
constexpr double kLatencyMarginFrames = 16.0;
constexpr double kCallbackJitterToleranceFrames = 1.0;
constexpr double kTimelineResetThresholdFrames = 16.0;

struct GapEvent {
  std::uint32_t period = 0;
  double frame_delta = 0.0;
};

struct Scenario {
  const char* name = "";
  double callback_jitter_frames = 0.0;
  double callback_jitter_rate = 0.0;
  double callback_drift_ppm = 0.0;
  double device_drift_ppm = 0.0;
  GapEvent first_gap{};
  GapEvent second_gap{};
  std::uint64_t min_timeline_resets = 0;
  std::uint64_t max_timeline_resets = 0;
  std::uint64_t max_lag_jumps_gt_2_frames = 0;
  double max_abs_error_limit_frames = 64.0;
};

struct JitterResult {
  std::uint64_t samples = 0;
  std::uint64_t lag_jumps_gt_2_frames = 0;
  std::uint64_t timeline_resets = 0;
  std::uint64_t elastic_drop_frames = 0;
  std::uint64_t elastic_replay_frames = 0;
  std::uint64_t phase_discontinuities = 0;
  double max_abs_error_frames = 0.0;
  double mean_abs_error_frames = 0.0;
  double max_lag_error_frames = 0.0;
  double mean_lag_error_frames = 0.0;
  double max_phase_step_error_frames = 0.0;
  std::uint64_t regressions = 0;
};

struct BurstScenario {
  const char* name = "";
  std::uint32_t capture_period_frames = 64;
  std::uint32_t playback_coalesce = 1;
  double max_safe_gap_ratio = 1.25;
  bool expected_safe = true;
};

struct BurstResult {
  std::uint32_t playback_completion_interval_frames = 0;
  std::uint32_t frames_per_playback_completion = 0;
  double completion_gap_ratio = 0.0;
  double cpu_completion_reduction_ratio = 1.0;
  bool model_safe = true;
};

struct RateShapeScenario {
  const char* name = "";
  std::uint32_t sample_rate = 48000;
  double playback_transactions_per_ms = 0.0;
  std::uint32_t request_bytes = 352;
  std::uint32_t output_usb_bytes_per_frame = 32;
  double max_rate_error_ppm = 1000.0;
  bool expected_rate_safe = true;
  bool physically_rejected = false;
};

struct RateShapeResult {
  double frames_per_transaction = 0.0;
  double output_frames_per_second = 0.0;
  double rate_error_ppm = 0.0;
  bool rate_safe = false;
};

bool has_gap(const GapEvent& gap, std::uint32_t period) {
  return gap.frame_delta != 0.0 && gap.period == period;
}

std::uint64_t frames_to_counter(double frames) {
  return static_cast<std::uint64_t>(std::llround(std::max(0.0, frames)));
}

JitterResult model(const Scenario& scenario,
                   std::uint32_t sample_rate,
                   std::uint32_t frames_per_period,
                   std::uint32_t periods) {
  JitterResult result{};
  double callback_gap_offset = 0.0;
  double last_callback_time = 0.0;
  double last_write_start = 0.0;
  double read_frame = 0.0;
  double previous_lag = kTargetLatencyFrames;
  double sum_abs_error = 0.0;
  double sum_lag_error = 0.0;
  bool initialized = false;

  for (std::uint32_t period = 0; period < periods; ++period) {
    if (has_gap(scenario.first_gap, period)) {
      callback_gap_offset += scenario.first_gap.frame_delta;
    }
    if (has_gap(scenario.second_gap, period)) {
      callback_gap_offset += scenario.second_gap.frame_delta;
    }

    const double expected_sample_time = static_cast<double>(period) * frames_per_period;
    const double deterministic_callback_jitter =
        std::sin(static_cast<double>(period) * scenario.callback_jitter_rate) *
        scenario.callback_jitter_frames;
    const double callback_drift =
        expected_sample_time * (scenario.callback_drift_ppm / 1'000'000.0);
    const double callback_time =
        expected_sample_time + callback_drift + deterministic_callback_jitter + callback_gap_offset;

    double write_start = callback_time;
    bool timeline_reset = false;
    if (initialized) {
      const double callback_delta = callback_time - last_callback_time;
      const double delta_error = callback_delta - static_cast<double>(frames_per_period);
      if (std::fabs(delta_error) <= kCallbackJitterToleranceFrames) {
        write_start = last_write_start + static_cast<double>(frames_per_period);
      } else if (std::fabs(delta_error) >= kTimelineResetThresholdFrames) {
        timeline_reset = true;
        result.timeline_resets += 1;
      } else {
        write_start = last_write_start + static_cast<double>(frames_per_period) + delta_error;
      }

      if (!timeline_reset && write_start <= last_write_start) {
        result.regressions += 1;
      }

      const double expected_phase_step = static_cast<double>(frames_per_period);
      const double actual_phase_step = static_cast<double>(frames_per_period);
      const double phase_step_error = std::fabs(actual_phase_step - expected_phase_step);
      result.max_phase_step_error_frames =
          std::max(result.max_phase_step_error_frames, phase_step_error);
      if (phase_step_error > 0.001) {
        result.phase_discontinuities += 1;
      }
    }

    const double write_end = write_start + static_cast<double>(frames_per_period);
    if (!initialized) {
      read_frame = write_end - kTargetLatencyFrames;
      initialized = true;
    } else {
      read_frame += static_cast<double>(frames_per_period) *
                    (1.0 + scenario.device_drift_ppm / 1'000'000.0);
    }

    const double raw_lag = write_end - read_frame;
    if (std::fabs(raw_lag - previous_lag) > 2.0) {
      result.lag_jumps_gt_2_frames += 1;
    }

    if (raw_lag > kTargetLatencyFrames + kLatencyMarginFrames) {
      const double drop = raw_lag - kTargetLatencyFrames;
      read_frame += drop;
      result.elastic_drop_frames += frames_to_counter(drop);
    } else if (raw_lag < kTargetLatencyFrames - kLatencyMarginFrames) {
      const double replay = kTargetLatencyFrames - raw_lag;
      read_frame -= replay;
      result.elastic_replay_frames += frames_to_counter(replay);
    }

    const double corrected_lag = write_end - read_frame;
    const double lag_error = std::fabs(corrected_lag - kTargetLatencyFrames);
    result.max_lag_error_frames = std::max(result.max_lag_error_frames, lag_error);
    sum_lag_error += lag_error;

    const double error = std::fabs(callback_time - expected_sample_time);
    result.max_abs_error_frames = std::max(result.max_abs_error_frames, error);
    sum_abs_error += error;
    last_callback_time = callback_time;
    last_write_start = write_start;
    previous_lag = corrected_lag;
    result.samples += 1;
  }

  (void)sample_rate;
  result.mean_abs_error_frames = sum_abs_error / static_cast<double>(periods);
  result.mean_lag_error_frames = sum_lag_error / static_cast<double>(periods);
  return result;
}

bool passes(const Scenario& scenario, const JitterResult& result) {
  return result.regressions == 0 && result.phase_discontinuities == 0 &&
         result.timeline_resets >= scenario.min_timeline_resets &&
         result.timeline_resets <= scenario.max_timeline_resets &&
         result.lag_jumps_gt_2_frames <= scenario.max_lag_jumps_gt_2_frames &&
         result.max_abs_error_frames <= scenario.max_abs_error_limit_frames &&
         result.max_lag_error_frames <= kLatencyMarginFrames;
}

BurstResult burst_model(const BurstScenario& scenario) {
  BurstResult result{};
  const std::uint32_t coalesce = std::max(1U, scenario.playback_coalesce);
  result.playback_completion_interval_frames = scenario.capture_period_frames * coalesce;
  result.frames_per_playback_completion = scenario.capture_period_frames * coalesce;
  result.completion_gap_ratio =
      static_cast<double>(result.playback_completion_interval_frames) /
      static_cast<double>(scenario.capture_period_frames);
  result.cpu_completion_reduction_ratio = 1.0 / static_cast<double>(coalesce);
  result.model_safe = result.completion_gap_ratio <= scenario.max_safe_gap_ratio;
  return result;
}

bool burst_passes(const BurstScenario& scenario, const BurstResult& result) {
  return result.model_safe == scenario.expected_safe;
}

RateShapeResult rate_shape_model(const RateShapeScenario& scenario) {
  RateShapeResult result{};
  result.frames_per_transaction =
      static_cast<double>(scenario.request_bytes) /
      static_cast<double>(scenario.output_usb_bytes_per_frame);
  result.output_frames_per_second =
      scenario.playback_transactions_per_ms * result.frames_per_transaction * 1000.0;
  result.rate_error_ppm =
      ((result.output_frames_per_second - static_cast<double>(scenario.sample_rate)) /
       static_cast<double>(scenario.sample_rate)) *
      1'000'000.0;
  result.rate_safe = std::fabs(result.rate_error_ppm) <= scenario.max_rate_error_ppm;
  return result;
}

bool rate_shape_passes(const RateShapeScenario& scenario, const RateShapeResult& result) {
  return result.rate_safe == scenario.expected_rate_safe;
}

void print_row(const Scenario& scenario,
               std::uint32_t sample_rate,
               std::uint32_t frames_per_period,
               const JitterResult& result,
               bool ok,
               bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"sample_rate\": " << sample_rate
            << ", \"period_frames\": " << frames_per_period
            << ", \"samples\": " << result.samples
            << ", \"target_latency_frames\": " << kTargetLatencyFrames
            << ", \"max_abs_error_frames\": " << result.max_abs_error_frames
            << ", \"mean_abs_error_frames\": " << result.mean_abs_error_frames
            << ", \"max_lag_error_frames\": " << result.max_lag_error_frames
            << ", \"mean_lag_error_frames\": " << result.mean_lag_error_frames
            << ", \"lag_jumps_gt_2_frames\": " << result.lag_jumps_gt_2_frames
            << ", \"timeline_resets\": " << result.timeline_resets
            << ", \"elastic_drop_frames\": " << result.elastic_drop_frames
            << ", \"elastic_replay_frames\": " << result.elastic_replay_frames
            << ", \"phase_discontinuities\": " << result.phase_discontinuities
            << ", \"max_phase_step_error_frames\": " << result.max_phase_step_error_frames
            << ", \"regressions\": " << result.regressions << ", \"result\": \""
            << (ok ? "PASS" : "FAIL") << "\"}" << (trailing_comma ? ",\n" : "\n");
}

void print_burst_row(const BurstScenario& scenario,
                     const BurstResult& result,
                     bool ok,
                     bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"capture_period_frames\": " << scenario.capture_period_frames
            << ", \"playback_coalesce\": " << scenario.playback_coalesce
            << ", \"playback_completion_interval_frames\": "
            << result.playback_completion_interval_frames
            << ", \"frames_per_playback_completion\": "
            << result.frames_per_playback_completion
            << ", \"completion_gap_ratio\": " << result.completion_gap_ratio
            << ", \"cpu_completion_reduction_ratio\": "
            << result.cpu_completion_reduction_ratio
            << ", \"max_safe_gap_ratio\": " << scenario.max_safe_gap_ratio
            << ", \"model_safe\": " << (result.model_safe ? "true" : "false")
            << ", \"expected_safe\": " << (scenario.expected_safe ? "true" : "false")
            << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}"
            << (trailing_comma ? ",\n" : "\n");
}

void print_rate_shape_row(const RateShapeScenario& scenario,
                          const RateShapeResult& result,
                          bool ok,
                          bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"sample_rate\": " << scenario.sample_rate
            << ", \"playback_transactions_per_ms\": "
            << scenario.playback_transactions_per_ms
            << ", \"request_bytes\": " << scenario.request_bytes
            << ", \"output_usb_bytes_per_frame\": "
            << scenario.output_usb_bytes_per_frame
            << ", \"frames_per_transaction\": " << result.frames_per_transaction
            << ", \"output_frames_per_second\": "
            << result.output_frames_per_second
            << ", \"rate_error_ppm\": " << result.rate_error_ppm
            << ", \"max_rate_error_ppm\": " << scenario.max_rate_error_ppm
            << ", \"rate_safe\": " << (result.rate_safe ? "true" : "false")
            << ", \"expected_rate_safe\": "
            << (scenario.expected_rate_safe ? "true" : "false")
            << ", \"physically_rejected\": "
            << (scenario.physically_rejected ? "true" : "false")
            << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}"
            << (trailing_comma ? ",\n" : "\n");
}

}  // namespace

int main() {
  const std::uint32_t sample_rates[] = {44100, 48000};
  const Scenario scenarios[] = {
      {"callback_jitter_phase_lock",
       0.35,
       0.173,
       0.0,
       0.0,
       {},
       {},
       0,
       0,
       0,
       1.0},
      {"gradual_drift_elastic_drop",
       0.45,
       0.131,
       18.0,
       -45.0,
       {},
       {},
       0,
       0,
       4,
       32.0},
      {"future_gap_drop_recovery",
       0.40,
       0.149,
       8.0,
       0.0,
       {5000, 36.0},
       {},
       1,
       1,
       2,
       48.0},
      {"stale_gap_replay_recovery",
       0.40,
       0.157,
       -8.0,
       0.0,
       {9000, -36.0},
       {},
       1,
       1,
       2,
       48.0},
  };
  const BurstScenario burst_scenarios[] = {
      {"capture_paced_periodic_playback", 64, 1, 1.25, true},
      {"capture_paced_coalesce2_rejected_by_physical_gate", 64, 2, 1.25, false},
      {"capture_paced_coalesce4_rejected_by_model", 64, 4, 1.25, false},
  };
  const RateShapeScenario rate_shape_scenarios[] = {
      {
          "iso8_observed_partial_layout_rate_safe",
          48000,
          4.3607214428857715,
          352,
          32,
          1000.0,
          true,
          false,
      },
      {
          "iso8_forced_full8_layout_overreads",
          48000,
          8.0,
          352,
          32,
          1000.0,
          false,
          false,
      },
      {
          "mainline_like_iso64_q8_rate_shape_not_sufficient",
          48000,
          48000.0 / (352.0 / 32.0) / 1000.0,
          352,
          32,
          1000.0,
          true,
          true,
      },
  };
  const std::uint32_t periods = 20000;
  const std::uint32_t frames_per_period = 64;
  std::uint32_t failures = 0;
  std::uint32_t burst_failures = 0;
  std::uint32_t rate_shape_failures = 0;
  std::uint32_t rows = 0;
  std::uint32_t burst_rows = 0;
  std::uint32_t rate_shape_rows = 0;

  std::cout << "{\n  \"rows\": [\n";
  for (std::uint32_t sample_index = 0; sample_index < 2; ++sample_index) {
    for (std::uint32_t scenario_index = 0; scenario_index < 4; ++scenario_index) {
      const auto result =
          model(scenarios[scenario_index], sample_rates[sample_index], frames_per_period, periods);
      const bool ok = passes(scenarios[scenario_index], result);
      failures += ok ? 0U : 1U;
      rows += 1;
      const bool trailing_comma = !(sample_index == 1 && scenario_index == 3);
      print_row(scenarios[scenario_index],
                sample_rates[sample_index],
                frames_per_period,
                result,
                ok,
                trailing_comma);
    }
  }
  std::cout << "  ],\n  \"burst_rows\": [\n";
  for (std::uint32_t scenario_index = 0; scenario_index < 3; ++scenario_index) {
    const auto result = burst_model(burst_scenarios[scenario_index]);
    const bool ok = burst_passes(burst_scenarios[scenario_index], result);
    burst_failures += ok ? 0U : 1U;
    burst_rows += 1;
    const bool trailing_comma = scenario_index != 2;
    print_burst_row(burst_scenarios[scenario_index], result, ok, trailing_comma);
  }
  failures += burst_failures;
  std::cout << "  ],\n  \"rate_shape_rows\": [\n";
  for (std::uint32_t scenario_index = 0; scenario_index < 3; ++scenario_index) {
    const auto result = rate_shape_model(rate_shape_scenarios[scenario_index]);
    const bool ok = rate_shape_passes(rate_shape_scenarios[scenario_index], result);
    rate_shape_failures += ok ? 0U : 1U;
    rate_shape_rows += 1;
    const bool trailing_comma = scenario_index != 2;
    print_rate_shape_row(rate_shape_scenarios[scenario_index], result, ok, trailing_comma);
  }
  failures += rate_shape_failures;
  std::cout << "  ],\n  \"row_count\": " << rows
            << ",\n  \"burst_row_count\": " << burst_rows
            << ",\n  \"rate_shape_row_count\": " << rate_shape_rows
            << ",\n  \"burst_failures\": " << burst_failures
            << ",\n  \"rate_shape_failures\": " << rate_shape_failures
            << ",\n  \"failures\": " << failures
            << ",\n  \"result\": \""
            << (failures == 0 ? "PASS" : "FAIL") << "\"\n}\n";

  return failures == 0 ? 0 : 1;
}
