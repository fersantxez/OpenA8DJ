#include "opena8djcpp/timecode_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace opena8djcpp {

namespace {

constexpr double kPi = 3.14159265358979323846;

double percentile_95(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(
      std::ceil(0.95 * static_cast<double>(values.size())) - 1.0);
  return values[std::min(index, values.size() - 1)];
}

double projection_correlation(std::span<const float> samples,
                              std::uint32_t sample_rate,
                              double frequency_hz) {
  if (samples.empty() || sample_rate == 0 || frequency_hz <= 0.0) {
    return 0.0;
  }

  double sin_sum = 0.0;
  double cos_sum = 0.0;
  double energy = 0.0;
  const double omega = 2.0 * kPi * frequency_hz / static_cast<double>(sample_rate);
  for (std::size_t index = 0; index < samples.size(); ++index) {
    const double sample = static_cast<double>(samples[index]);
    const double phase = omega * static_cast<double>(index);
    sin_sum += sample * std::sin(phase);
    cos_sum += sample * std::cos(phase);
    energy += sample * sample;
  }

  if (energy <= 0.0) {
    return 0.0;
  }
  const double basis_energy = 0.5 * static_cast<double>(samples.size());
  return std::sqrt((sin_sum * sin_sum) + (cos_sum * cos_sum)) /
         std::sqrt(energy * basis_energy);
}

}  // namespace

TimecodeAnalysisResult analyze_timecode_stereo(std::span<const float> left,
                                               std::span<const float> right,
                                               const TimecodeAnalysisConfig& config) {
  TimecodeAnalysisResult result{};
  result.frames = std::min(left.size(), right.size());
  if (result.frames == 0 || config.sample_rate == 0 || config.expected_frequency_hz <= 0.0) {
    return result;
  }

  double left_sum = 0.0;
  double right_sum = 0.0;
  double peak = 0.0;
  std::vector<double> rising_crossings;
  rising_crossings.reserve(static_cast<std::size_t>(
      (static_cast<double>(result.frames) * config.expected_frequency_hz /
       static_cast<double>(config.sample_rate)) +
      4.0));

  for (std::size_t index = 0; index < result.frames; ++index) {
    const double left_sample = static_cast<double>(left[index]);
    const double right_sample = static_cast<double>(right[index]);
    left_sum += left_sample * left_sample;
    right_sum += right_sample * right_sample;
    peak = std::max(peak, std::fabs(left_sample));
    peak = std::max(peak, std::fabs(right_sample));
    if (std::fabs(left_sample) >= config.clip_threshold) {
      result.clipped_samples += 1;
    }
    if (std::fabs(right_sample) >= config.clip_threshold) {
      result.clipped_samples += 1;
    }
    if (index > 0) {
      const double previous = static_cast<double>(left[index - 1]);
      if (previous < 0.0 && left_sample >= 0.0) {
        const double denom = previous - left_sample;
        const double fraction = denom == 0.0 ? 0.0 : previous / denom;
        rising_crossings.push_back(static_cast<double>(index - 1) + fraction);
      }
    }
  }

  result.left_rms = std::sqrt(left_sum / static_cast<double>(result.frames));
  result.right_rms = std::sqrt(right_sum / static_cast<double>(result.frames));
  result.peak = peak;
  const double rms_floor = 1.0e-12;
  result.balance_db =
      20.0 * std::log10(std::max(result.left_rms, rms_floor) /
                        std::max(result.right_rms, rms_floor));
  result.balance_db = std::fabs(result.balance_db);

  std::vector<double> period_errors;
  if (rising_crossings.size() >= 2) {
    std::vector<double> periods;
    periods.reserve(rising_crossings.size() - 1);
    for (std::size_t index = 1; index < rising_crossings.size(); ++index) {
      periods.push_back(rising_crossings[index] - rising_crossings[index - 1]);
    }
    std::vector<double> sorted_periods = periods;
    std::sort(sorted_periods.begin(), sorted_periods.end());
    const double median_period = sorted_periods[sorted_periods.size() / 2];
    if (median_period > 0.0) {
      result.frequency_hz = static_cast<double>(config.sample_rate) / median_period;
    }
    const double expected_period =
        static_cast<double>(config.sample_rate) / config.expected_frequency_hz;
    for (const auto period : periods) {
      period_errors.push_back(std::fabs(period - expected_period));
    }
    result.jitter_p95_frames = percentile_95(period_errors);
  }

  result.frequency_error_ppm =
      std::fabs(result.frequency_hz - config.expected_frequency_hz) /
      config.expected_frequency_hz * 1.0e6;
  const double left_corr =
      projection_correlation(left.first(result.frames), config.sample_rate,
                             config.expected_frequency_hz);
  const double right_corr =
      projection_correlation(right.first(result.frames), config.sample_rate,
                             config.expected_frequency_hz);
  result.abs_correlation = std::min(left_corr, right_corr);

  result.rms_ok = result.left_rms >= config.min_rms && result.right_rms >= config.min_rms;
  result.balance_ok = result.balance_db <= config.max_balance_db;
  result.frequency_ok = result.frequency_error_ppm <= config.max_frequency_error_ppm;
  result.jitter_ok = result.jitter_p95_frames <= config.max_jitter_p95_frames;
  result.correlation_ok = result.abs_correlation >= config.min_abs_correlation;
  result.clipping_ok = result.clipped_samples <= config.max_clipped_samples;
  result.passed = result.rms_ok && result.balance_ok && result.frequency_ok &&
                  result.jitter_ok && result.correlation_ok && result.clipping_ok;
  return result;
}

}  // namespace opena8djcpp
