#include "opena8djcpp/input_decode.hpp"
#include "opena8djcpp/timecode_analysis.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

using namespace opena8djcpp;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct ProfileCase {
  InputProfile profile;
  double frequency_hz;
};

S24Frame make_timecode_frame(StereoPair deck,
                             std::uint32_t sample_rate,
                             double frequency_hz,
                             std::uint32_t frame_index) {
  S24Frame frame{};
  const double phase = 2.0 * kPi * frequency_hz * static_cast<double>(frame_index) /
                       static_cast<double>(sample_rate);
  constexpr float kAmplitude = 0.70F;
  frame[channel_index(deck, PairSide::Left)] =
      float_to_s24(static_cast<float>(kAmplitude * std::sin(phase)), 1.0F);
  frame[channel_index(deck, PairSide::Right)] =
      float_to_s24(static_cast<float>(kAmplitude * std::cos(phase)), 1.0F);
  return frame;
}

double leakage_rms(std::span<const float> interleaved,
                   std::uint64_t frames,
                   StereoPair active_deck) {
  double sum = 0.0;
  std::uint64_t count = 0;
  const auto left = channel_index(active_deck, PairSide::Left);
  const auto right = channel_index(active_deck, PairSide::Right);
  for (std::uint64_t frame = 0; frame < frames; ++frame) {
    const auto base = static_cast<std::size_t>(frame) * kInputChannels;
    for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
      if (channel == left || channel == right) {
        continue;
      }
      const double value = static_cast<double>(interleaved[base + channel]);
      sum += value * value;
      count += 1;
    }
  }
  return count == 0 ? 0.0 : std::sqrt(sum / static_cast<double>(count));
}

}  // namespace

int main() {
  const ProfileCase profiles[] = {
      {timecode_vinyl_input_profile(), 1000.0},
      {timecode_cd_line_input_profile(), 1500.0},
      {phono_input_profile(), 800.0},
  };
  const std::uint32_t sample_rates[] = {44100, 48000};
  const StereoPair decks[] = {StereoPair::A, StereoPair::B, StereoPair::C, StereoPair::D};

  std::uint32_t rows = 0;
  std::uint32_t failures = 0;
  std::cout << "{\n  \"schema\": \"opena8djcpp.dvs-packet-input-decode.v1\",\n"
            << "  \"rows\": [\n";
  bool first = true;
  for (const auto& profile_case : profiles) {
    for (const auto rate : sample_rates) {
      const auto source_frames_count = static_cast<std::size_t>(rate);
      for (const auto deck : decks) {
        std::vector<S24Frame> source_frames(source_frames_count);
        for (std::uint32_t frame = 0; frame < source_frames_count; ++frame) {
          source_frames[frame] =
              make_timecode_frame(deck, rate, profile_case.frequency_hz, frame);
        }

        Mode2OutputPacker packer(source_frames, kMode2DefaultStartByte);
        std::vector<std::uint8_t> packed(source_frames.size() * kMode2GroupBytes);
        const auto written = packer.fill_into(packed);
        packed.resize(written);

        std::vector<S24Frame> scratch(source_frames.size() + 16U);
        std::vector<float> decoded_f32(scratch.size() * kInputChannels, 0.0F);
        const auto decoded = decode_input_profile_mode2_into(
            packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes,
            profile_case.profile, scratch, decoded_f32);

        const auto frames_written = static_cast<std::size_t>(decoded.frames_written);
        std::vector<float> left(frames_written);
        std::vector<float> right(frames_written);
        const auto left_channel = channel_index(deck, PairSide::Left);
        const auto right_channel = channel_index(deck, PairSide::Right);
        for (std::size_t frame = 0; frame < frames_written; ++frame) {
          const auto base = frame * kInputChannels;
          left[frame] = decoded_f32[base + left_channel];
          right[frame] = decoded_f32[base + right_channel];
        }

        TimecodeAnalysisConfig config{};
        config.sample_rate = rate;
        config.expected_frequency_hz = profile_case.frequency_hz;
        const auto analysis = analyze_timecode_stereo(left, right, config);
        const auto leak = leakage_rms(decoded_f32, decoded.frames_written, deck);
        const bool ok = decoded.profile_valid && decoded.input_decode_enabled &&
                        decoded.decoded_frame_overflows == 0 &&
                        decoded.output_frame_overflows == 0 &&
                        decoded.stats.check_errors == 0 && decoded.stats.panic_flags == 0 &&
                        decoded.frames_written > 0 && analysis.passed &&
                        leak <= 0.0001;
        rows += 1;
        failures += ok ? 0U : 1U;

        if (!first) {
          std::cout << ",\n";
        }
        first = false;
        std::cout << "    {\"profile\": \"" << profile_case.profile.name
                  << "\", \"sample_rate\": " << rate << ", \"deck\": \""
                  << pair_name(deck) << "\", \"frames_written\": "
                  << decoded.frames_written << ", \"check_errors\": "
                  << decoded.stats.check_errors << ", \"panic_flags\": "
                  << decoded.stats.panic_flags << ", \"decoded_frame_overflows\": "
                  << decoded.decoded_frame_overflows << ", \"output_frame_overflows\": "
                  << decoded.output_frame_overflows << ", \"left_rms\": "
                  << analysis.left_rms << ", \"right_rms\": " << analysis.right_rms
                  << ", \"frequency_error_ppm\": " << analysis.frequency_error_ppm
                  << ", \"jitter_p95_frames\": " << analysis.jitter_p95_frames
                  << ", \"abs_correlation\": " << analysis.abs_correlation
                  << ", \"leakage_rms\": " << leak << ", \"result\": \""
                  << (ok ? "PASS" : "FAIL") << "\"}";
      }
    }
  }

  const auto playback = playback_input_profile();
  std::vector<S24Frame> silence(128);
  Mode2OutputPacker playback_packer(silence, kMode2DefaultStartByte);
  std::vector<std::uint8_t> playback_bytes(4096);
  const auto playback_written = playback_packer.fill_into(playback_bytes);
  playback_bytes.resize(playback_written);
  std::vector<S24Frame> playback_scratch(256);
  std::vector<float> playback_output(playback_scratch.size() * kInputChannels, 1.0F);
  const auto playback_decode = decode_input_profile_mode2_into(
      playback_bytes, kMode2DefaultStartByte, kMode2DefaultTransferBytes,
      playback, playback_scratch, playback_output);
  const bool playback_ok = playback_decode.profile_valid &&
                           !playback_decode.input_decode_enabled &&
                           playback_decode.stats.decoded_frames > 0 &&
                           playback_decode.frames_written == 0;

  std::cout << "\n  ],\n  \"row_count\": " << rows << ",\n  \"failures\": "
            << failures << ",\n  \"playback_decode_off\": \""
            << (playback_ok ? "PASS" : "FAIL") << "\",\n  \"result\": \""
            << (failures == 0 && playback_ok ? "PASS" : "FAIL") << "\"\n}\n";
  return failures == 0 && playback_ok ? 0 : 1;
}
