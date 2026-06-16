#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace opena8djcpp {

struct TimecodeAnalysisConfig {
  std::uint32_t sample_rate = 48000;
  double expected_frequency_hz = 1000.0;
  double min_rms = 0.05;
  double max_balance_db = 1.0;
  double max_frequency_error_ppm = 50.0;
  double max_jitter_p95_frames = 2.0;
  double min_abs_correlation = 0.95;
  double clip_threshold = 0.999;
  std::uint64_t max_clipped_samples = 0;
};

struct TimecodeAnalysisResult {
  std::size_t frames = 0;
  double left_rms = 0.0;
  double right_rms = 0.0;
  double peak = 0.0;
  double balance_db = 0.0;
  double frequency_hz = 0.0;
  double frequency_error_ppm = 0.0;
  double jitter_p95_frames = 0.0;
  double abs_correlation = 0.0;
  std::uint64_t clipped_samples = 0;
  bool rms_ok = false;
  bool balance_ok = false;
  bool frequency_ok = false;
  bool jitter_ok = false;
  bool correlation_ok = false;
  bool clipping_ok = false;
  bool passed = false;
};

[[nodiscard]] TimecodeAnalysisResult analyze_timecode_stereo(
    std::span<const float> left,
    std::span<const float> right,
    const TimecodeAnalysisConfig& config);

}  // namespace opena8djcpp
