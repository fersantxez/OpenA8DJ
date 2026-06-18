#include "opena8djcpp/input_decode.hpp"
#include "opena8djcpp/timecode_analysis.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct StressCase {
  InputProfile profile;
  double frequency_hz = 1000.0;
  double drift_ppm = 0.0;
  double noise_amp = 0.0;
  double crosstalk_dbfs = -120.0;
  double right_imbalance_db = 0.0;
  std::uint32_t dropout_period_frames = 0;
};

struct DeckView {
  std::vector<float> left;
  std::vector<float> right;
};

double db_to_linear(double db) {
  return std::pow(10.0, db / 20.0);
}

float deterministic_noise(std::uint32_t frame, std::uint32_t channel) {
  std::uint32_t value = frame * 1664525U + channel * 1013904223U + 0x9e3779b9U;
  value ^= value >> 16U;
  value *= 2246822519U;
  value ^= value >> 13U;
  const double normalized =
      (static_cast<double>(value & 0xffffU) / 32767.5) - 1.0;
  return static_cast<float>(normalized);
}

S24Frame make_stress_frame(StereoPair deck,
                           std::uint32_t sample_rate,
                           const StressCase& item,
                           std::uint32_t frame_index) {
  S24Frame frame{};
  const double frequency = item.frequency_hz * (1.0 + item.drift_ppm / 1.0e6);
  const double phase = 2.0 * kPi * frequency * static_cast<double>(frame_index) /
                       static_cast<double>(sample_rate);
  constexpr double kAmplitude = 0.70;
  const double right_gain = db_to_linear(item.right_imbalance_db);
  const bool dropout = item.dropout_period_frames > 0 &&
                       frame_index > 0 &&
                       (frame_index % item.dropout_period_frames) == 0;
  const auto active_left = channel_index(deck, PairSide::Left);
  const auto active_right = channel_index(deck, PairSide::Right);
  const double crosstalk = db_to_linear(item.crosstalk_dbfs);

  for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
    double value = item.noise_amp * deterministic_noise(frame_index, channel);
    if (channel == active_left && !dropout) {
      value += kAmplitude * std::sin(phase);
    } else if (channel == active_right && !dropout) {
      value += kAmplitude * right_gain * std::cos(phase);
    } else {
      value += crosstalk * std::sin(phase + 0.17 * static_cast<double>(channel + 1U));
    }
    frame[channel] = float_to_s24(static_cast<float>(value), 1.0F);
  }
  return frame;
}

DeckView extract_deck(std::span<const float> interleaved,
                      std::uint64_t frames,
                      StereoPair deck) {
  DeckView out{};
  out.left.reserve(static_cast<std::size_t>(frames));
  out.right.reserve(static_cast<std::size_t>(frames));
  const auto left = channel_index(deck, PairSide::Left);
  const auto right = channel_index(deck, PairSide::Right);
  for (std::uint64_t frame = 0; frame < frames; ++frame) {
    const auto base = static_cast<std::size_t>(frame) * kInputChannels;
    out.left.push_back(interleaved[base + left]);
    out.right.push_back(interleaved[base + right]);
  }
  return out;
}

double channel_tone_level_dbfs(std::span<const float> interleaved,
                               std::uint64_t frames,
                               std::uint32_t channel,
                               std::uint32_t sample_rate,
                               double frequency_hz) {
  double sin_dot = 0.0;
  double cos_dot = 0.0;
  double sin_norm = 0.0;
  double cos_norm = 0.0;
  for (std::uint64_t frame = 0; frame < frames; ++frame) {
    const auto base = static_cast<std::size_t>(frame) * kInputChannels;
    const double phase = 2.0 * kPi * frequency_hz * static_cast<double>(frame) /
                         static_cast<double>(sample_rate);
    const double sin_ref = std::sin(phase);
    const double cos_ref = std::cos(phase);
    const double value = interleaved[base + channel];
    sin_dot += value * sin_ref;
    cos_dot += value * cos_ref;
    sin_norm += sin_ref * sin_ref;
    cos_norm += cos_ref * cos_ref;
  }
  if (sin_norm <= 0.0 || cos_norm <= 0.0) {
    return -240.0;
  }
  const double sin_peak = sin_dot / sin_norm;
  const double cos_peak = cos_dot / cos_norm;
  const double peak = std::sqrt((sin_peak * sin_peak) + (cos_peak * cos_peak));
  const double rms = peak / std::sqrt(2.0);
  return rms <= 0.0 ? -240.0 : 20.0 * std::log10(rms);
}

double deck_tone_level_dbfs(std::span<const float> interleaved,
                            std::uint64_t frames,
                            StereoPair deck,
                            std::uint32_t sample_rate,
                            double frequency_hz) {
  const auto left = channel_index(deck, PairSide::Left);
  const auto right = channel_index(deck, PairSide::Right);
  return std::max(channel_tone_level_dbfs(interleaved, frames, left, sample_rate, frequency_hz),
                  channel_tone_level_dbfs(interleaved, frames, right, sample_rate, frequency_hz));
}

double inactive_tone_leakage_dbfs(std::span<const float> interleaved,
                                  std::uint64_t frames,
                                  StereoPair active_deck,
                                  std::uint32_t sample_rate,
                                  double frequency_hz) {
  const std::array<StereoPair, 4> decks = {StereoPair::A, StereoPair::B,
                                           StereoPair::C, StereoPair::D};
  double max_dbfs = -240.0;
  for (const auto deck : decks) {
    if (deck == active_deck) {
      continue;
    }
    max_dbfs = std::max(
        max_dbfs, deck_tone_level_dbfs(interleaved, frames, deck, sample_rate, frequency_hz));
  }
  return max_dbfs;
}

bool deck_swap_detected(std::span<const float> interleaved,
                        std::uint64_t frames,
                        StereoPair active_deck,
                        std::uint32_t sample_rate,
                        double frequency_hz) {
  const double active_dbfs =
      deck_tone_level_dbfs(interleaved, frames, active_deck, sample_rate, frequency_hz);
  const std::array<StereoPair, 4> decks = {StereoPair::A, StereoPair::B,
                                           StereoPair::C, StereoPair::D};
  for (const auto deck : decks) {
    if (deck == active_deck) {
      continue;
    }
    const double inactive_dbfs =
        deck_tone_level_dbfs(interleaved, frames, deck, sample_rate, frequency_hz);
    if (inactive_dbfs >= active_dbfs - 6.0) {
      return true;
    }
  }
  return false;
}

double min_inactive_to_active_tone_gap_db(std::span<const float> interleaved,
                                          std::uint64_t frames,
                                          StereoPair active_deck,
                                          std::uint32_t sample_rate,
                                          double frequency_hz) {
  const double active_dbfs =
      deck_tone_level_dbfs(interleaved, frames, active_deck, sample_rate, frequency_hz);
  const std::array<StereoPair, 4> decks = {StereoPair::A, StereoPair::B,
                                           StereoPair::C, StereoPair::D};
  double min_gap = 240.0;
  for (const auto deck : decks) {
    if (deck == active_deck) {
      continue;
    }
    const double inactive_dbfs =
        deck_tone_level_dbfs(interleaved, frames, deck, sample_rate, frequency_hz);
    min_gap = std::min(min_gap, active_dbfs - inactive_dbfs);
  }
  return min_gap;
}

bool inactive_false_accept(std::span<const float> interleaved,
                           std::uint64_t frames,
                           StereoPair active_deck,
                           const TimecodeAnalysisConfig& config) {
  const std::array<StereoPair, 4> decks = {StereoPair::A, StereoPair::B,
                                           StereoPair::C, StereoPair::D};
  for (const auto deck : decks) {
    if (deck == active_deck) {
      continue;
    }
    const auto view = extract_deck(interleaved, frames, deck);
    const auto analysis = analyze_timecode_stereo(view.left, view.right, config);
    if (analysis.passed) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  const std::array<StressCase, 6> cases = {
      StressCase{timecode_vinyl_input_profile(), 1000.0, 20.0, 0.00035, -90.0, -0.4, 8192},
      StressCase{timecode_vinyl_input_profile(), 1000.0, -22.0, 0.00045, -86.0, 0.45, 4096},
      StressCase{timecode_cd_line_input_profile(), 1500.0, -18.0, 0.00030, -92.0, 0.35, 12000},
      StressCase{timecode_cd_line_input_profile(), 1500.0, 22.0, 0.00040, -88.0, -0.45, 6000},
      StressCase{phono_input_profile(), 800.0, 15.0, 0.00025, -94.0, 0.25, 0},
      StressCase{phono_input_profile(), 800.0, -20.0, 0.00035, -90.0, -0.35, 10000},
  };
  const std::array<std::uint32_t, 2> sample_rates = {44100, 48000};
  const std::array<StereoPair, 4> decks = {StereoPair::A, StereoPair::B,
                                           StereoPair::C, StereoPair::D};

  std::uint32_t rows = 0;
  std::uint32_t failures = 0;
  std::uint32_t false_accepts = 0;
  std::uint32_t deck_swaps = 0;
  double min_abs_correlation = 1.0;
  double max_frequency_error_ppm = 0.0;
  double max_jitter_p95_frames = 0.0;
  double max_balance_db = 0.0;
  double max_inactive_leakage_dbfs = -240.0;
  double min_inactive_to_active_gap_db = 240.0;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.dvs-timecode-stress-margin.v1\",\n"
            << "  \"safety\": \"offline_synthetic_dvs_only_no_audio_coreaudio_usb_traktor_or_hardware_touch\",\n"
            << "  \"rows\": [\n";
  bool first = true;
  for (const auto& item : cases) {
    for (const auto rate : sample_rates) {
      const auto source_frames_count = static_cast<std::size_t>(rate);
      for (const auto deck : decks) {
        std::vector<S24Frame> source(source_frames_count);
        for (std::uint32_t frame = 0; frame < source_frames_count; ++frame) {
          source[frame] = make_stress_frame(deck, rate, item, frame);
        }

        Mode2OutputPacker packer(source, kMode2DefaultStartByte);
        std::vector<std::uint8_t> packed(source.size() * kMode2GroupBytes);
        const auto written = packer.fill_into(packed);
        packed.resize(written);

        std::vector<S24Frame> scratch(source.size() + 16U);
        std::vector<float> decoded_f32(scratch.size() * kInputChannels, 0.0F);
        const auto decoded = decode_input_profile_mode2_into(
            packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes,
            item.profile, scratch, decoded_f32);

        const auto frames = static_cast<std::uint64_t>(decoded.frames_written);
        const auto active = extract_deck(decoded_f32, frames, deck);
        TimecodeAnalysisConfig config{};
        config.sample_rate = rate;
        config.expected_frequency_hz = item.frequency_hz;
        config.max_frequency_error_ppm = 30.0;
        config.max_jitter_p95_frames = 1.0;
        config.min_abs_correlation = 0.97;
        config.max_balance_db = 0.5;
        const auto analysis = analyze_timecode_stereo(active.left, active.right, config);
        const double leakage =
            inactive_tone_leakage_dbfs(decoded_f32, frames, deck, rate, item.frequency_hz);
        const bool false_accept = inactive_false_accept(decoded_f32, frames, deck, config);
        const bool deck_swap =
            deck_swap_detected(decoded_f32, frames, deck, rate, item.frequency_hz);
        const double inactive_to_active_gap =
            min_inactive_to_active_tone_gap_db(decoded_f32, frames, deck, rate,
                                               item.frequency_hz);
        const bool ok = decoded.profile_valid && decoded.input_decode_enabled &&
                        decoded.decoded_frame_overflows == 0 &&
                        decoded.output_frame_overflows == 0 &&
                        decoded.stats.check_errors == 0 && decoded.stats.panic_flags == 0 &&
                        analysis.passed && leakage <= -80.0 && !false_accept && !deck_swap;

        rows += 1;
        failures += ok ? 0U : 1U;
        false_accepts += false_accept ? 1U : 0U;
        deck_swaps += deck_swap ? 1U : 0U;
        min_abs_correlation = std::min(min_abs_correlation, analysis.abs_correlation);
        max_frequency_error_ppm =
            std::max(max_frequency_error_ppm, analysis.frequency_error_ppm);
        max_jitter_p95_frames =
            std::max(max_jitter_p95_frames, analysis.jitter_p95_frames);
        max_balance_db = std::max(max_balance_db, analysis.balance_db);
        max_inactive_leakage_dbfs = std::max(max_inactive_leakage_dbfs, leakage);
        min_inactive_to_active_gap_db =
            std::min(min_inactive_to_active_gap_db, inactive_to_active_gap);

        if (!first) {
          std::cout << ",\n";
        }
        first = false;
        std::cout << "    {\"profile\": \"" << item.profile.name
                  << "\", \"sample_rate\": " << rate << ", \"deck\": \""
                  << pair_name(deck) << "\", \"drift_ppm\": " << item.drift_ppm
                  << ", \"noise_amp\": " << item.noise_amp
                  << ", \"crosstalk_dbfs\": " << item.crosstalk_dbfs
                  << ", \"right_imbalance_db\": " << item.right_imbalance_db
                  << ", \"dropout_period_frames\": " << item.dropout_period_frames
                  << ", \"frames_written\": " << decoded.frames_written
                  << ", \"frequency_error_ppm\": " << analysis.frequency_error_ppm
                  << ", \"jitter_p95_frames\": " << analysis.jitter_p95_frames
                  << ", \"abs_correlation\": " << analysis.abs_correlation
                  << ", \"balance_db\": " << analysis.balance_db
                  << ", \"inactive_leakage_dbfs\": " << leakage
                  << ", \"inactive_to_active_tone_gap_db\": " << inactive_to_active_gap
                  << ", \"false_accept\": " << (false_accept ? "true" : "false")
                  << ", \"deck_swap\": " << (deck_swap ? "true" : "false")
                  << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}";
      }
    }
  }

  std::cout << "\n  ],\n"
            << "  \"row_count\": " << rows << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"false_accepts\": " << false_accepts << ",\n"
            << "  \"deck_swaps\": " << deck_swaps << ",\n"
            << "  \"min_abs_correlation\": " << min_abs_correlation << ",\n"
            << "  \"max_frequency_error_ppm\": " << max_frequency_error_ppm << ",\n"
            << "  \"max_jitter_p95_frames\": " << max_jitter_p95_frames << ",\n"
            << "  \"max_balance_db\": " << max_balance_db << ",\n"
            << "  \"max_inactive_leakage_dbfs\": " << max_inactive_leakage_dbfs << ",\n"
            << "  \"min_inactive_to_active_tone_gap_db\": "
            << min_inactive_to_active_gap_db << ",\n"
            << "  \"thresholds\": {\"min_abs_correlation\": 0.97, "
               "\"max_frequency_error_ppm\": 30.0, \"max_jitter_p95_frames\": 1.0, "
               "\"max_balance_db\": 0.5, \"max_inactive_leakage_dbfs\": -80.0, "
               "\"min_inactive_to_active_tone_gap_db\": 6.0, "
               "\"false_accepts\": 0, \"deck_swaps\": 0},\n"
            << "  \"result\": \"" << (failures == 0 ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return failures == 0 ? 0 : 1;
}
