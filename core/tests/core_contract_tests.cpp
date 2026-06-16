#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/audio_ring.hpp"
#include "opena8djcpp/input_profile.hpp"
#include "opena8djcpp/metrics.hpp"
#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/policies.hpp"
#include "opena8djcpp/routing.hpp"
#include "opena8djcpp/timecode.hpp"
#include "opena8djcpp/timecode_analysis.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using namespace opena8djcpp;

namespace {

void test_device_surface() {
  const auto surface = make_audio8dj_surface();
  assert(surface.input_channels == 8);
  assert(surface.output_channels == 8);

  assert(channel_index(StereoPair::A, PairSide::Left) == 0);
  assert(channel_index(StereoPair::A, PairSide::Right) == 1);
  assert(channel_index(StereoPair::B, PairSide::Left) == 2);
  assert(channel_index(StereoPair::B, PairSide::Right) == 3);
  assert(channel_index(StereoPair::C, PairSide::Left) == 4);
  assert(channel_index(StereoPair::C, PairSide::Right) == 5);
  assert(channel_index(StereoPair::D, PairSide::Left) == 6);
  assert(channel_index(StereoPair::D, PairSide::Right) == 7);
}

void test_sample_format_placeholder() {
  const SampleFormat host_format = SampleFormat::Float32Interleaved;
  const SampleFormat usb_format = SampleFormat::Signed24PackedUsb;
  assert(host_format != usb_format);
  assert(SampleRatePolicy::is_supported(44100));
  assert(SampleRatePolicy::is_supported(48000));
  assert(!SampleRatePolicy::is_supported(32000));
}

void test_identity_routing() {
  constexpr std::uint32_t frames = 4;
  std::array<float, frames * kInputChannels> input{};
  std::array<float, frames * kOutputChannels> output{};

  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
      input[(frame * kInputChannels) + channel] =
          static_cast<float>((frame * 100U) + channel);
    }
  }

  const bool ok = route_interleaved_f32(input, output, frames, RoutingMatrix::identity());
  assert(ok);
  assert(output == input);

  std::array<float, frames * kOutputChannels> plan_output{};
  const RoutingPlan identity_plan(RoutingMatrix::identity());
  assert(identity_plan.valid());
  assert(identity_plan.is_identity());
  assert(route_interleaved_f32(input, plan_output, frames, identity_plan));
  assert(plan_output == input);
}

void test_custom_and_invalid_routing() {
  constexpr std::uint32_t frames = 2;
  std::array<float, frames * kInputChannels> input{};
  std::array<float, frames * kOutputChannels> output{};

  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    for (std::uint32_t channel = 0; channel < kInputChannels; ++channel) {
      input[(frame * kInputChannels) + channel] =
          static_cast<float>((frame * 10U) + channel);
    }
  }

  const RoutingMatrix reversed({7, 6, 5, 4, 3, 2, 1, 0});
  assert(!reversed.is_identity());
  assert(route_interleaved_f32(input, output, frames, reversed));
  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    const auto base = frame * kOutputChannels;
    assert(output[base + 0] == input[base + 7]);
    assert(output[base + 7] == input[base + 0]);
  }

  const RoutingPlan reversed_plan(reversed);
  assert(reversed_plan.valid());
  assert(!reversed_plan.is_identity());
  assert(route_interleaved_f32(input, output, frames, reversed_plan));
  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    const auto base = frame * kOutputChannels;
    assert(output[base + 0] == input[base + 7]);
    assert(output[base + 7] == input[base + 0]);
  }

  const RoutingMatrix invalid({0, 1, 2, 3, 4, 5, 6, kInputChannels});
  const RoutingPlan invalid_plan(invalid);
  assert(!invalid_plan.valid());
  assert(!route_interleaved_f32(input, output, frames, invalid));
  assert(!route_interleaved_f32(input, output, frames, invalid_plan));
}

S24Frame synthetic_s24_frame(std::uint32_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto stream = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    auto magnitude = static_cast<std::int32_t>(((stream + 1U) * 1000000U) +
                                              (side * 250000U) +
                                              ((frame_index % 8192U) * 257U));
    frame[channel] = side == 0 ? magnitude : -magnitude;
  }
  return frame;
}

void test_s24_big_endian_conversion() {
  const auto zero = encode_s24_big_endian(0);
  assert((zero == std::array<std::uint8_t, 3>{0x00, 0x00, 0x00}));
  assert(decode_s24_big_endian(zero) == 0);

  const auto max = encode_s24_big_endian(kS24Max);
  assert((max == std::array<std::uint8_t, 3>{0x7F, 0xFF, 0xFF}));
  assert(decode_s24_big_endian(max) == kS24Max);

  const auto min = encode_s24_big_endian(kS24Min);
  assert((min == std::array<std::uint8_t, 3>{0x80, 0x00, 0x00}));
  assert(decode_s24_big_endian(min) == kS24Min);

  assert(float_to_s24(1.0F, 1.0F) == kS24Max);
  assert(float_to_s24(-1.0F, 1.0F) == kS24Min);
  assert(float_to_s24(std::numeric_limits<float>::infinity(), 1.0F) == 0);
  assert(float_to_s24(-std::numeric_limits<float>::infinity(), 1.0F) == 0);
  assert(float_to_s24(std::numeric_limits<float>::quiet_NaN(), 1.0F) == 0);
  assert(float_to_s24(0.5F, 0.0F) == 0);
  assert(std::abs(float_to_s24(0.5F, 1.0F) - 4194303) <= 1);
}

void test_mode2_roundtrip_all_start_bytes() {
  std::vector<S24Frame> frames;
  for (std::uint32_t frame = 0; frame < 96; ++frame) {
    frames.push_back(synthetic_s24_frame(frame));
  }

  for (std::uint32_t start_byte = 0; start_byte < kMode2FrameBytesPerStream; ++start_byte) {
    Mode2OutputPacker packer(frames, start_byte);
    std::vector<std::uint8_t> packed;
    while (packed.size() < 4096) {
      const auto chunk = packer.fill(kMode2DefaultTransferBytes);
      packed.insert(packed.end(), chunk.begin(), chunk.end());
    }

    const auto decoded =
        decode_mode2_usb_bytes(packed, start_byte, kMode2DefaultTransferBytes);
    assert(decoded.stats.check_errors == 0);
    assert(decoded.stats.panic_flags == 0);

    const std::uint32_t source_start_frame = start_byte == 0 ? 0 : 1;
    const auto expected_count = frames.size() - source_start_frame;
    assert(decoded.frames.size() >= expected_count);
    for (std::size_t index = 0; index < expected_count; ++index) {
      assert(decoded.frames[index] == frames[index + source_start_frame]);
    }
  }
}

void test_mode2_decode_into_matches_allocating_decoder() {
  std::vector<S24Frame> frames;
  for (std::uint32_t frame = 0; frame < 128; ++frame) {
    frames.push_back(synthetic_s24_frame(frame));
  }

  for (const auto transfer_bytes : {48U, 80U, kMode2DefaultTransferBytes}) {
    for (std::uint32_t start_byte = 0; start_byte < kMode2FrameBytesPerStream; ++start_byte) {
      Mode2OutputPacker packer(frames, start_byte);
      std::vector<std::uint8_t> packed(transfer_bytes * 16U);
      const auto written = packer.fill_into(packed);
      assert(written == packed.size());

      const auto allocating = decode_mode2_usb_bytes(packed, start_byte, transfer_bytes);
      std::vector<S24Frame> output(allocating.frames.size() + 4);
      const auto into = decode_mode2_usb_bytes_into(packed, start_byte, transfer_bytes, output);

      assert(into.output_overflows == 0);
      assert(into.stats.decoded_frames == allocating.stats.decoded_frames);
      assert(into.stats.checks == allocating.stats.checks);
      assert(into.stats.check_errors == allocating.stats.check_errors);
      assert(into.stats.panic_flags == allocating.stats.panic_flags);
      assert(into.stats.sample_bytes == allocating.stats.sample_bytes);
      for (std::size_t index = 0; index < allocating.frames.size(); ++index) {
        assert(output[index] == allocating.frames[index]);
      }
    }
  }
}

void test_no_deck_leakage_for_pair_a_only() {
  std::vector<S24Frame> frames;
  for (std::uint32_t frame = 0; frame < 48; ++frame) {
    S24Frame item{};
    item[channel_index(StereoPair::A, PairSide::Left)] = 1000000 + static_cast<std::int32_t>(frame);
    item[channel_index(StereoPair::A, PairSide::Right)] =
        -1100000 - static_cast<std::int32_t>(frame);
    frames.push_back(item);
  }

  Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
  std::vector<std::uint8_t> packed;
  while (packed.size() < 3072) {
    const auto chunk = packer.fill(kMode2DefaultTransferBytes);
    packed.insert(packed.end(), chunk.begin(), chunk.end());
  }

  const auto decoded =
      decode_mode2_usb_bytes(packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes);
  assert(!decoded.frames.empty());
  const std::size_t expected_count = frames.size() - 1;
  assert(decoded.frames.size() >= expected_count);
  for (std::size_t index = 0; index < expected_count; ++index) {
    const auto& frame = decoded.frames[index];
    assert(frame[channel_index(StereoPair::A, PairSide::Left)] != 0);
    assert(frame[channel_index(StereoPair::A, PairSide::Right)] != 0);
    for (std::uint32_t channel = channel_index(StereoPair::B, PairSide::Left);
         channel < kOutputChannels; ++channel) {
      assert(frame[channel] == 0);
    }
  }
}

void test_metrics_gate() {
  MetricsSnapshot clean{};
  assert(offline_gate_passes(clean));

  MetricsSnapshot dirty{};
  dirty.output.underruns = 1;
  assert(!offline_gate_passes(dirty));
}

void test_timecode_profile_policy() {
  const auto vinyl = timecode_profile_spec(TimecodeProfile::Vinyl);
  assert(vinyl.caiaq_input_mode == 0);
  assert(vinyl.vinyl_ground_lift_applicable);
  assert(!vinyl.cd_line_ground_lift_applicable);

  const auto cd_line = timecode_profile_spec(TimecodeProfile::CdLine);
  assert(cd_line.caiaq_input_mode == 1);
  assert(cd_line.cd_line_ground_lift_applicable);

  const auto phono = timecode_profile_spec(TimecodeProfile::Phono);
  assert(phono.caiaq_input_mode == 2);
  assert(phono.phono_ground_lift_applicable);

  assert(deck_timecode_assignment(StereoPair::A).left_input_channel == 0);
  assert(deck_timecode_assignment(StereoPair::B).left_input_channel == 2);
  assert(deck_timecode_assignment(StereoPair::C).left_input_channel == 4);
  assert(deck_timecode_assignment(StereoPair::D).right_input_channel == 7);
}

void test_input_profiles() {
  const auto playback = playback_input_profile();
  assert(playback.valid());
  assert(playback.source_map_is_identity());
  assert(playback.timecode_profile == TimecodeProfile::Disabled);
  assert(playback.caiaq_input_mode == 1);
  assert(!playback.input_decode_enabled);
  assert(!playback.software_lock_enabled);

  const auto vinyl = timecode_vinyl_input_profile();
  assert(vinyl.valid());
  assert(vinyl.source_map_is_identity());
  assert(vinyl.timecode_profile == TimecodeProfile::Vinyl);
  assert(vinyl.caiaq_input_mode == 0);
  assert(vinyl.input_decode_enabled);
  assert(vinyl.software_lock_enabled);
  assert(vinyl.vinyl_ground_lift_enabled);

  const auto cd_line = timecode_cd_line_input_profile();
  assert(cd_line.valid());
  assert(cd_line.timecode_profile == TimecodeProfile::CdLine);
  assert(cd_line.caiaq_input_mode == 1);
  assert(cd_line.input_decode_enabled);
  assert(cd_line.software_lock_enabled);
  assert(cd_line.cd_line_ground_lift_enabled);

  const auto phono = phono_input_profile();
  assert(phono.valid());
  assert(phono.timecode_profile == TimecodeProfile::Phono);
  assert(phono.caiaq_input_mode == 2);
  assert(phono.input_decode_enabled);
  assert(phono.software_lock_enabled);
  assert(phono.phono_ground_lift_enabled);
}

void test_timecode_signal_analysis() {
  constexpr std::uint32_t sample_rate = 48000;
  constexpr std::size_t frames = sample_rate;
  constexpr double frequency = 1000.0;
  constexpr double amplitude = 0.7;
  constexpr double pi = 3.14159265358979323846;
  std::vector<float> left(frames);
  std::vector<float> right(frames);
  for (std::size_t index = 0; index < frames; ++index) {
    const auto phase = 2.0 * pi * frequency * static_cast<double>(index) /
                       static_cast<double>(sample_rate);
    left[index] = static_cast<float>(amplitude * std::sin(phase));
    right[index] = static_cast<float>(amplitude * std::cos(phase));
  }

  TimecodeAnalysisConfig config{};
  config.sample_rate = sample_rate;
  config.expected_frequency_hz = frequency;
  const auto ok = analyze_timecode_stereo(left, right, config);
  assert(ok.passed);
  assert(ok.left_rms >= config.min_rms);
  assert(ok.right_rms >= config.min_rms);
  assert(ok.balance_db <= config.max_balance_db);
  assert(ok.frequency_error_ppm <= config.max_frequency_error_ppm);
  assert(ok.jitter_p95_frames <= config.max_jitter_p95_frames);
  assert(ok.abs_correlation >= config.min_abs_correlation);
  assert(ok.clipped_samples == 0);

  auto wrong_frequency = left;
  for (std::size_t index = 0; index < frames; ++index) {
    const auto phase = 2.0 * pi * 1002.0 * static_cast<double>(index) /
                       static_cast<double>(sample_rate);
    wrong_frequency[index] = static_cast<float>(amplitude * std::sin(phase));
    right[index] = static_cast<float>(amplitude * std::cos(phase));
  }
  const auto bad_frequency = analyze_timecode_stereo(wrong_frequency, right, config);
  assert(!bad_frequency.passed);
  assert(!bad_frequency.frequency_ok);
}

void test_spsc_frame_ring_contract() {
  SpscFrameRing<S24Frame, 4> ring;
  assert(ring.capacity() == 4);
  assert(ring.readable() == 0);
  assert(ring.writable() == 4);

  const auto frame0 = synthetic_s24_frame(0);
  const auto frame1 = synthetic_s24_frame(1);
  const auto frame2 = synthetic_s24_frame(2);
  const auto frame3 = synthetic_s24_frame(3);
  const auto frame4 = synthetic_s24_frame(4);
  assert(ring.push(frame0));
  assert(ring.push(frame1));
  assert(ring.push(frame2));
  assert(ring.push(frame3));
  assert(!ring.push(frame4));
  assert(ring.readable() == 4);
  assert(ring.writable() == 0);

  S24Frame out{};
  assert(ring.pop(out));
  assert(out == frame0);
  assert(ring.push(frame4));

  std::array<S24Frame, 4> popped{};
  assert(ring.pop_many(popped) == 4);
  assert(popped[0] == frame1);
  assert(popped[1] == frame2);
  assert(popped[2] == frame3);
  assert(popped[3] == frame4);
  assert(!ring.pop(out));

  std::array<S24Frame, 3> batch{frame0, frame1, frame2};
  assert(ring.push_many(batch) == batch.size());
  ring.clear();
  assert(ring.readable() == 0);
  assert(ring.writable() == 4);
}

}  // namespace

int main() {
  test_device_surface();
  test_sample_format_placeholder();
  test_identity_routing();
  test_custom_and_invalid_routing();
  test_s24_big_endian_conversion();
  test_mode2_roundtrip_all_start_bytes();
  test_mode2_decode_into_matches_allocating_decoder();
  test_no_deck_leakage_for_pair_a_only();
  test_metrics_gate();
  test_timecode_profile_policy();
  test_input_profiles();
  test_timecode_signal_analysis();
  test_spsc_frame_ring_contract();

  std::cout << "opena8djcpp_core_contract PASS\n";
  return 0;
}
