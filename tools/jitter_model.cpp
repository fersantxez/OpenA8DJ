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
  const std::uint32_t periods = 20000;
  const std::uint32_t frames_per_period = 64;
  std::uint32_t failures = 0;
  std::uint32_t rows = 0;

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
  std::cout << "  ],\n  \"row_count\": " << rows << ",\n  \"failures\": " << failures
            << ",\n  \"result\": \""
            << (failures == 0 ? "PASS" : "FAIL") << "\"\n}\n";

  return failures == 0 ? 0 : 1;
}
