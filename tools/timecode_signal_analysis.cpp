#include "opena8djcpp/timecode_analysis.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

using namespace opena8djcpp;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct CaseDef {
  std::string_view name;
  double frequency_hz;
  double right_gain;
  double amplitude;
  bool expect_pass;
};

std::vector<float> sine_channel(std::uint32_t sample_rate,
                                double frequency_hz,
                                double amplitude,
                                double phase,
                                std::size_t frames) {
  std::vector<float> out(frames);
  const double omega = 2.0 * kPi * frequency_hz / static_cast<double>(sample_rate);
  for (std::size_t index = 0; index < frames; ++index) {
    out[index] = static_cast<float>(amplitude *
                                    std::sin((omega * static_cast<double>(index)) + phase));
  }
  return out;
}

}  // namespace

int main() {
  const CaseDef cases[] = {
      {"balanced_timecode", 1000.0, 1.0, 0.70, true},
      {"wrong_frequency", 1002.0, 1.0, 0.70, false},
      {"imbalanced_channel", 1000.0, 0.50, 0.70, false},
      {"clipped_signal", 1000.0, 1.0, 1.00, false},
  };
  const std::uint32_t rates[] = {44100, 48000};
  std::uint32_t failures = 0;
  std::uint32_t rows = 0;

  std::cout << "{\n  \"schema\": \"opena8djcpp.timecode-signal-analysis.v1\",\n"
            << "  \"rows\": [\n";
  bool first = true;
  for (const auto rate : rates) {
    for (const auto& test_case : cases) {
      const auto frames = static_cast<std::size_t>(rate * 6U);
      auto left = sine_channel(rate, test_case.frequency_hz, test_case.amplitude, 0.0, frames);
      auto right = sine_channel(rate, test_case.frequency_hz,
                                test_case.amplitude * test_case.right_gain, kPi / 2.0, frames);
      TimecodeAnalysisConfig config{};
      config.sample_rate = rate;
      config.expected_frequency_hz = 1000.0;
      const auto result = analyze_timecode_stereo(left, right, config);
      const bool ok = result.passed == test_case.expect_pass;
      failures += ok ? 0U : 1U;
      rows += 1;
      if (!first) {
        std::cout << ",\n";
      }
      first = false;
      std::cout << "    {\"case\": \"" << test_case.name << "\", \"sample_rate\": "
                << rate << ", \"expected_pass\": "
                << (test_case.expect_pass ? "true" : "false") << ", \"passed\": "
                << (result.passed ? "true" : "false") << ", \"left_rms\": "
                << result.left_rms << ", \"right_rms\": " << result.right_rms
                << ", \"balance_db\": " << result.balance_db
                << ", \"frequency_hz\": " << result.frequency_hz
                << ", \"frequency_error_ppm\": " << result.frequency_error_ppm
                << ", \"jitter_p95_frames\": " << result.jitter_p95_frames
                << ", \"abs_correlation\": " << result.abs_correlation
                << ", \"clipped_samples\": " << result.clipped_samples
                << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}";
    }
  }

  std::cout << "\n  ],\n  \"row_count\": " << rows << ",\n  \"failures\": "
            << failures << ",\n  \"result\": \"" << (failures == 0 ? "PASS" : "FAIL")
            << "\"\n}\n";
  return failures == 0 ? 0 : 1;
}
