#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/timecode.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace opena8djcpp;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct SignalMetrics {
  double left_rms = 0.0;
  double right_rms = 0.0;
  double leakage_rms = 0.0;
};

SignalMetrics analyze_deck_signal(StereoPair deck,
                                  std::uint32_t sample_rate,
                                  TimecodeProfile profile) {
  constexpr std::uint32_t frames = 4096;
  const double frequency = profile == TimecodeProfile::Vinyl     ? 1000.0
                           : profile == TimecodeProfile::CdLine ? 1500.0
                           : profile == TimecodeProfile::Phono  ? 800.0
                                                                 : 500.0;
  std::array<double, kInputChannels> sums{};

  const auto assignment = deck_timecode_assignment(deck);
  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    const double phase = 2.0 * kPi * frequency * static_cast<double>(frame) /
                         static_cast<double>(sample_rate);
    std::array<double, kInputChannels> sample{};
    sample[assignment.left_input_channel] = std::sin(phase);
    sample[assignment.right_input_channel] = std::cos(phase);

    for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
      sums[channel] += sample[channel] * sample[channel];
    }
  }

  SignalMetrics metrics{};
  metrics.left_rms = std::sqrt(sums[assignment.left_input_channel] / frames);
  metrics.right_rms = std::sqrt(sums[assignment.right_input_channel] / frames);
  double leakage_sum = 0.0;
  for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
    if (channel != assignment.left_input_channel && channel != assignment.right_input_channel) {
      leakage_sum += sums[channel];
    }
  }
  metrics.leakage_rms = std::sqrt(leakage_sum / (frames * (kInputChannels - 2)));
  return metrics;
}

bool pass_metrics(const SignalMetrics& metrics) {
  const double balance = std::fabs(metrics.left_rms - metrics.right_rms);
  return metrics.left_rms >= 0.70 && metrics.right_rms >= 0.70 && balance <= 0.02 &&
         metrics.leakage_rms == 0.0;
}

}  // namespace

int main() {
  const std::uint32_t sample_rates[] = {44100, 48000};
  const StereoPair decks[] = {StereoPair::A, StereoPair::B, StereoPair::C, StereoPair::D};
  const TimecodeProfile profiles[] = {TimecodeProfile::Vinyl, TimecodeProfile::CdLine,
                                      TimecodeProfile::Phono};
  std::uint32_t rows = 0;
  std::uint32_t failures = 0;

  std::cout << "{\n  \"rows\": [\n";
  bool first = true;
  for (const auto profile : profiles) {
    const auto spec = timecode_profile_spec(profile);
    for (const auto rate : sample_rates) {
      for (const auto deck : decks) {
        const auto metrics = analyze_deck_signal(deck, rate, profile);
        const bool ok = pass_metrics(metrics);
        rows += 1;
        failures += ok ? 0U : 1U;
        if (!first) {
          std::cout << ",\n";
        }
        first = false;
        std::cout << "    {\"profile\": \"" << spec.name << "\", \"sample_rate\": " << rate
                  << ", \"deck\": \"" << pair_name(deck) << "\", \"left_rms\": "
                  << metrics.left_rms << ", \"right_rms\": " << metrics.right_rms
                  << ", \"leakage_rms\": " << metrics.leakage_rms << ", \"result\": \""
                  << (ok ? "PASS" : "FAIL") << "\"}";
      }
    }
  }
  std::cout << "\n  ],\n  \"row_count\": " << rows << ",\n  \"failures\": " << failures
            << ",\n  \"result\": \"" << (failures == 0 ? "PASS" : "FAIL") << "\"\n}\n";
  return failures == 0 ? 0 : 1;
}
