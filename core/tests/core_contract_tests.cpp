#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/audio_ring.hpp"
#include "opena8djcpp/input_decode.hpp"
#include "opena8djcpp/input_profile.hpp"
#include "opena8djcpp/metrics.hpp"
#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/policies.hpp"
#include "opena8djcpp/prepared_transport.hpp"
#include "opena8djcpp/protocol.hpp"
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

void test_protocol_constants() {
  assert(kNativeInstrumentsVendorId == 0x17cc);
  assert(kAudio8DjProductId == 0x1978);
  assert(kUsbInterfaceNumber == 0);
  assert(kUsbConfigurationValue == 1);
  assert(kUsbAlternateSetting == 1);
  assert(kEndpointControlOut == 0x01);
  assert(kEndpointControlIn == 0x81);
  assert(kEndpointIsoCapture == 0x82);
  assert(kEndpointIsoPlayback == 0x06);
  assert(kCommandGetDeviceInfo == 0x01);
  assert(kCommandReadIo == 0x04);
  assert(kCommandWriteIo == 0x05);
  assert(kCommandAudioParams == 0x09);
  assert(kCommandMidiRead == 0x06);
  assert(kCommandMidiWrite == 0x07);
  assert(kCommandAutoMsg == 0x0b);
  assert(caiaq_rate_code(44100) == 0);
  assert(caiaq_rate_code(48000) == 1);
  assert(caiaq_rate_code(96000) == 2);
  assert(caiaq_rate_code(88200) == 4);
  assert(caiaq_rate_code(32000) == 0xff);
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

  const RoutingMatrix reversed(RoutingMatrix::Mapping{7, 6, 5, 4, 3, 2, 1, 0});
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

  const RoutingMatrix invalid(RoutingMatrix::Mapping{0, 1, 2, 3, 4, 5, 6, kInputChannels});
  const RoutingPlan invalid_plan(invalid);
  assert(!invalid_plan.valid());
  assert(!route_interleaved_f32(input, output, frames, invalid));
  assert(!route_interleaved_f32(input, output, frames, invalid_plan));
}

void test_advanced_routing_matrix() {
  constexpr std::uint32_t frames = 1;
  const std::array<float, frames * kInputChannels> input{1.0F, 2.0F, 3.0F, 4.0F,
                                                         5.0F, 6.0F, 7.0F, 8.0F};
  std::array<float, frames * kOutputChannels> output{};

  const RoutingMatrix advanced(RoutingMatrix::Routes{
      RouteEntry::passthrough(channel_index(StereoPair::D, PairSide::Left)),
      RouteEntry::passthrough(channel_index(StereoPair::D, PairSide::Right)),
      RouteEntry::muted(),
      RouteEntry::muted(),
      RouteEntry::passthrough(channel_index(StereoPair::C, PairSide::Right)),
      RouteEntry::passthrough(channel_index(StereoPair::C, PairSide::Left)),
      RouteEntry::passthrough(channel_index(StereoPair::D, PairSide::Left)),
      RouteEntry::inverted(channel_index(StereoPair::D, PairSide::Right)),
  });

  const RoutingPlan plan(advanced);
  assert(plan.valid());
  assert(!plan.is_identity());
  assert(!advanced.route_for_output(2).is_inverted());
  assert(advanced.route_for_output(2).is_muted());
  assert(advanced.route_for_output(7).is_inverted());
  assert(RoutingMatrix::dvs_default().is_identity());

  assert(route_interleaved_f32(input, output, frames, plan));
  const std::array<float, frames * kOutputChannels> expected{7.0F, 8.0F, 0.0F, 0.0F,
                                                             6.0F, 5.0F, 7.0F, -8.0F};
  assert(output == expected);

  const RoutingPlan muted_plan(RoutingMatrix::muted());
  assert(muted_plan.valid());
  assert(!muted_plan.is_identity());
  output.fill(123.0F);
  assert(route_interleaved_f32(input, output, frames, muted_plan));
  for (const auto value : output) {
    assert(value == 0.0F);
  }

  const RoutingMatrix invalid(RoutingMatrix::Routes{
      RouteEntry::passthrough(0),
      RouteEntry::passthrough(1),
      RouteEntry::passthrough(2),
      RouteEntry::passthrough(3),
      RouteEntry::passthrough(4),
      RouteEntry::passthrough(5),
      RouteEntry::passthrough(6),
      RouteEntry::inverted(kInputChannels),
  });
  assert(!RoutingPlan(invalid).valid());
  assert(!route_interleaved_f32(input, output, frames, invalid));
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

void test_input_profile_decode_contract() {
  std::vector<S24Frame> frames;
  for (std::uint32_t frame = 0; frame < 64; ++frame) {
    frames.push_back(synthetic_s24_frame(frame));
  }

  Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
  std::vector<std::uint8_t> packed(kMode2DefaultTransferBytes * 8U);
  assert(packer.fill_into(packed) == packed.size());

  std::vector<S24Frame> scratch((packed.size() / kMode2GroupBytes) + 4);
  std::vector<float> output(scratch.size() * kInputChannels, -123.0F);

  const auto playback = decode_input_profile_mode2_into(
      packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes, playback_input_profile(),
      scratch, output);
  assert(playback.profile_valid);
  assert(!playback.input_decode_enabled);
  assert(playback.stats.decoded_frames > 0);
  assert(playback.frames_written == 0);
  assert(output[0] == -123.0F);

  const auto vinyl = decode_input_profile_mode2_into(
      packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes,
      timecode_vinyl_input_profile(), scratch, output);
  assert(vinyl.profile_valid);
  assert(vinyl.input_decode_enabled);
  assert(vinyl.stats.decoded_frames > 0);
  assert(vinyl.frames_written == vinyl.stats.decoded_frames);
  assert(vinyl.output_frame_overflows == 0);
  assert(vinyl.decoded_frame_overflows == 0);
  assert(vinyl.stats.check_errors == 0);
  assert(vinyl.stats.panic_flags == 0);
  assert(std::abs(output[0] - s24_to_float(scratch[0][0])) < 1.0e-7F);
  assert(std::abs(output[7] - s24_to_float(scratch[0][7])) < 1.0e-7F);
}

void test_prepared_transport_backend_contract() {
  PreparedTransportBackend transport;
  assert(transport.start(PreparedTransportConfig{.iso_frames = 8,
                                                 .capture_slots = 8,
                                                 .playback_slots = 8}));

  for (std::uint32_t period = 0; period < 32; ++period) {
    const auto frame = synthetic_s24_frame(period);
    assert(transport.hal_write_playback(frame));
    assert(transport.backend_complete_period(
        frame, static_cast<std::uint64_t>(period + 1U) * 8U));
    S24Frame captured{};
    assert(transport.hal_read_capture(captured));
    assert(captured == frame);
  }

  const auto counters = transport.counters();
  assert(counters.backend_prepare_enqueues == 16);
  assert(counters.backend_steady_requeues == 64);
  assert(counters.hal_steady_requeues == 0);
  assert(counters.fallback_allocations == 0);
  assert(counters.capture_ring_overruns == 0);
  assert(counters.capture_ring_underruns == 0);
  assert(counters.playback_ring_overruns == 0);
  assert(counters.playback_ring_underruns == 0);
  assert(counters.timestamp_regressions == 0);
  assert(counters.channel_identity_failures == 0);
  assert(counters.hal_capture_reads == 32);
  assert(counters.hal_playback_writes == 32);
  const auto safety = transport.safety();
  assert(safety.prepared_slots_only);
  assert(safety.cadence_safe);
  assert(safety.routing_safe);
  assert(safety.timecode_safe);
  assert(safety.hal_hot_path_safe);
  assert(safety.product_safe);

  PreparedTransportBackend bad_transport;
  assert(bad_transport.start(PreparedTransportConfig{}));
  const auto frame = synthetic_s24_frame(0);
  assert(bad_transport.hal_write_playback(frame));
  assert(bad_transport.backend_complete_period(
      frame, 8, PreparedTransportStepOptions{.completion_gap_periods = 2,
                                             .hal_direct_requeue_attempt = true,
                                             .fallback_allocation_attempt = true}));
  S24Frame captured{};
  assert(bad_transport.hal_read_capture(captured));
  const auto bad = bad_transport.safety();
  assert(!bad.prepared_slots_only);
  assert(!bad.cadence_safe);
  assert(!bad.hal_hot_path_safe);
  assert(!bad.product_safe);
  bad_transport.stop();
  assert(!bad_transport.started());
}

void test_prepared_transport_batch_contract() {
  PreparedTransportBackend transport;
  assert(transport.start(PreparedTransportConfig{}));

  std::array<S24Frame, 8> playback{};
  std::array<S24Frame, 8> capture{};
  for (std::uint32_t index = 0; index < playback.size(); ++index) {
    playback[index] = synthetic_s24_frame(index);
    capture[index] = synthetic_s24_frame(index + 100U);
  }

  assert(transport.hal_write_playback(playback) == playback.size());
  std::array<S24Frame, 8> backend_playback{};
  assert(transport.backend_complete_period(capture, backend_playback, 8));
  assert(backend_playback == playback);

  std::array<S24Frame, 8> hal_capture{};
  assert(transport.hal_read_capture(hal_capture) == hal_capture.size());
  assert(hal_capture == capture);

  const auto counters = transport.counters();
  assert(counters.backend_capture_frames == capture.size());
  assert(counters.backend_playback_frames == playback.size());
  assert(counters.hal_capture_reads == capture.size());
  assert(counters.hal_playback_writes == playback.size());
  assert(transport.safety().product_safe);
}

}  // namespace

int main() {
  test_device_surface();
  test_sample_format_placeholder();
  test_protocol_constants();
  test_identity_routing();
  test_custom_and_invalid_routing();
  test_advanced_routing_matrix();
  test_s24_big_endian_conversion();
  test_mode2_roundtrip_all_start_bytes();
  test_mode2_decode_into_matches_allocating_decoder();
  test_no_deck_leakage_for_pair_a_only();
  test_metrics_gate();
  test_timecode_profile_policy();
  test_input_profiles();
  test_timecode_signal_analysis();
  test_spsc_frame_ring_contract();
  test_input_profile_decode_contract();
  test_prepared_transport_backend_contract();
  test_prepared_transport_batch_contract();

  std::cout << "opena8djcpp_core_contract PASS\n";
  return 0;
}
