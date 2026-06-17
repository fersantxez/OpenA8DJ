#include "opena8djcpp/input_decode.hpp"
#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/prepared_transport.hpp"
#include "opena8djcpp/routing.hpp"
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

struct TimecodeCase {
  InputProfile profile;
  double frequency_hz = 0.0;
};

S24Frame synthetic_playback_frame(std::uint32_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    frame[channel] = static_cast<std::int32_t>((channel + 1U) * 100000 +
                                               (frame_index * 101U));
  }
  return frame;
}

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

InputDecodeIntoResult profile_decode_frames(std::span<const S24Frame> frames,
                                            const InputProfile& profile,
                                            std::span<float> output_interleaved_f32) {
  InputDecodeIntoResult result{};
  result.profile_valid = profile.valid();
  result.input_decode_enabled = profile.input_decode_enabled;
  result.stats.decoded_frames = frames.size();
  if (!result.profile_valid || !result.input_decode_enabled) {
    return result;
  }

  const auto output_capacity_frames =
      static_cast<std::uint64_t>(output_interleaved_f32.size() / kInputChannels);
  const auto frames_to_write =
      std::min<std::uint64_t>(static_cast<std::uint64_t>(frames.size()), output_capacity_frames);
  result.output_frame_overflows = frames.size() - frames_to_write;
  for (std::uint64_t frame_index = 0; frame_index < frames_to_write; ++frame_index) {
    const auto& input_frame = frames[static_cast<std::size_t>(frame_index)];
    const auto output_base = static_cast<std::size_t>(frame_index) * kInputChannels;
    for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
      output_interleaved_f32[output_base + channel] =
          s24_to_float(input_frame[profile.source_map[channel]]);
    }
  }
  result.frames_written = frames_to_write;
  return result;
}

bool playback_routing_ok(std::uint32_t& mismatches,
                         std::uint64_t& hal_requeues,
                         std::uint64_t& fallback_allocations) {
  constexpr std::uint32_t kFrames = 64;
  std::vector<S24Frame> source(kFrames);
  std::vector<S24Frame> routed(kFrames);
  std::vector<S24Frame> backend_playback(kFrames);
  std::vector<S24Frame> capture_silence(kFrames);
  for (std::uint32_t frame = 0; frame < kFrames; ++frame) {
    source[frame] = synthetic_playback_frame(frame);
  }

  const RoutingPlan route(RoutingMatrix(RoutingMatrix::Mapping{6, 7, 4, 5, 2, 3, 0, 1}));
  if (!route_s24_frames(source, routed, route)) {
    return false;
  }

  PreparedTransportBackend transport;
  if (!transport.start(PreparedTransportConfig{})) {
    return false;
  }
  if (transport.hal_write_playback(routed) != routed.size()) {
    return false;
  }
  if (!transport.backend_complete_period(capture_silence, backend_playback, 8)) {
    return false;
  }

  for (std::size_t frame = 0; frame < routed.size(); ++frame) {
    if (backend_playback[frame] != routed[frame]) {
      mismatches += 1;
    }
  }
  const auto counters = transport.counters();
  hal_requeues += counters.hal_steady_requeues;
  fallback_allocations += counters.fallback_allocations;
  return transport.safety().product_safe && mismatches == 0;
}

bool timecode_case_ok(const TimecodeCase& item,
                      std::uint32_t sample_rate,
                      StereoPair deck,
                      std::uint64_t& frames_written,
                      double& frequency_error_ppm,
                      double& jitter_p95_frames,
                      double& leak,
                      std::uint64_t& hal_requeues,
                      std::uint64_t& fallback_allocations) {
  const std::size_t source_frames = 2048;
  std::vector<S24Frame> source(source_frames);
  for (std::uint32_t frame = 0; frame < source_frames; ++frame) {
    source[frame] = make_timecode_frame(deck, sample_rate, item.frequency_hz, frame);
  }

  Mode2OutputPacker packer(source, kMode2DefaultStartByte);
  std::vector<std::uint8_t> packed(source.size() * kMode2GroupBytes);
  packed.resize(packer.fill_into(packed));

  std::vector<S24Frame> scratch(source.size() + 16U);
  const auto decoded = decode_mode2_usb_bytes_into(
      packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes, scratch);
  scratch.resize(static_cast<std::size_t>(decoded.stats.decoded_frames));

  PreparedTransportBackend transport;
  if (!transport.start(PreparedTransportConfig{})) {
    return false;
  }
  std::vector<S24Frame> playback_silence(scratch.size());
  if (transport.hal_write_playback(playback_silence) != playback_silence.size()) {
    return false;
  }
  std::vector<S24Frame> backend_playback(scratch.size());
  if (!transport.backend_complete_period(scratch, backend_playback, 8)) {
    return false;
  }
  std::vector<S24Frame> capture_from_backend(scratch.size());
  if (transport.hal_read_capture(capture_from_backend) != capture_from_backend.size()) {
    return false;
  }

  std::vector<float> decoded_f32(capture_from_backend.size() * kInputChannels, 0.0F);
  const auto profile_decode =
      profile_decode_frames(capture_from_backend, item.profile, decoded_f32);
  frames_written = profile_decode.frames_written;

  std::vector<float> left(static_cast<std::size_t>(frames_written));
  std::vector<float> right(static_cast<std::size_t>(frames_written));
  const auto left_channel = channel_index(deck, PairSide::Left);
  const auto right_channel = channel_index(deck, PairSide::Right);
  for (std::size_t frame = 0; frame < left.size(); ++frame) {
    const auto base = frame * kInputChannels;
    left[frame] = decoded_f32[base + left_channel];
    right[frame] = decoded_f32[base + right_channel];
  }

  TimecodeAnalysisConfig config{};
  config.sample_rate = sample_rate;
  config.expected_frequency_hz = item.frequency_hz;
  const auto analysis = analyze_timecode_stereo(left, right, config);
  leak = leakage_rms(decoded_f32, frames_written, deck);
  frequency_error_ppm = analysis.frequency_error_ppm;
  jitter_p95_frames = analysis.jitter_p95_frames;
  const auto counters = transport.counters();
  hal_requeues += counters.hal_steady_requeues;
  fallback_allocations += counters.fallback_allocations;

  return decoded.stats.check_errors == 0 && decoded.stats.panic_flags == 0 &&
         decoded.output_overflows == 0 && profile_decode.profile_valid &&
         profile_decode.input_decode_enabled && profile_decode.output_frame_overflows == 0 &&
         profile_decode.frames_written > 0 && analysis.passed && leak <= 0.0001 &&
         transport.safety().product_safe;
}

}  // namespace

int main() {
  const TimecodeCase cases[] = {
      {timecode_vinyl_input_profile(), 1000.0},
      {timecode_cd_line_input_profile(), 1500.0},
      {phono_input_profile(), 800.0},
  };
  const StereoPair decks[] = {StereoPair::A, StereoPair::B, StereoPair::C, StereoPair::D};

  std::uint32_t playback_mismatches = 0;
  std::uint64_t total_hal_requeues = 0;
  std::uint64_t total_fallback_allocations = 0;
  const bool playback_ok =
      playback_routing_ok(playback_mismatches, total_hal_requeues, total_fallback_allocations);

  std::uint32_t rows = 0;
  std::uint32_t failures = playback_ok ? 0U : 1U;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-transport-routing-timecode-contract.v1\",\n"
            << "  \"playback_routing\": {\"result\": \""
            << (playback_ok ? "PASS" : "FAIL")
            << "\", \"mismatches\": " << playback_mismatches << "},\n"
            << "  \"rows\": [\n";

  bool first = true;
  for (const auto& item : cases) {
    for (const auto deck : decks) {
      std::uint64_t frames_written = 0;
      double frequency_error_ppm = 0.0;
      double jitter_p95_frames = 0.0;
      double leak = 0.0;
      const bool ok = timecode_case_ok(item, 48000, deck, frames_written, frequency_error_ppm,
                                       jitter_p95_frames, leak, total_hal_requeues,
                                       total_fallback_allocations);
      rows += 1;
      failures += ok ? 0U : 1U;
      if (!first) {
        std::cout << ",\n";
      }
      first = false;
      std::cout << "    {\"profile\": \"" << item.profile.name << "\", \"deck\": \""
                << pair_name(deck) << "\", \"sample_rate\": 48000"
                << ", \"frames_written\": " << frames_written
                << ", \"frequency_error_ppm\": " << frequency_error_ppm
                << ", \"jitter_p95_frames\": " << jitter_p95_frames
                << ", \"leakage_rms\": " << leak << ", \"result\": \""
                << (ok ? "PASS" : "FAIL") << "\"}";
    }
  }

  const bool pass = failures == 0 && total_hal_requeues == 0 && total_fallback_allocations == 0;
  std::cout << "\n  ],\n"
            << "  \"row_count\": " << rows << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"hal_steady_requeues\": " << total_hal_requeues << ",\n"
            << "  \"fallback_allocations\": " << total_fallback_allocations << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
