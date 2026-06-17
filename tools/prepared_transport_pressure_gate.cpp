#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/prepared_transport.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

constexpr std::uint32_t kPressureSeconds = 10;

struct Scenario {
  std::uint32_t sample_rate = 48000;
  StereoPair active_deck = StereoPair::A;
};

struct ScenarioResult {
  PreparedTransportCounters counters{};
  PreparedTransportSafety safety{};
  std::uint64_t periods = 0;
  std::uint64_t frames = 0;
  std::uint64_t capture_mismatches = 0;
  std::uint64_t playback_mismatches = 0;
  std::uint64_t packet_check_errors = 0;
  std::uint64_t packet_panic_flags = 0;
  std::uint64_t packet_output_overflows = 0;
  bool packet_prefix_ok = false;
  bool pass = false;
};

S24Frame frame_for(const Scenario& scenario, std::uint64_t frame_index, std::uint32_t salt) {
  S24Frame frame{};
  const auto active_left = channel_index(scenario.active_deck, PairSide::Left);
  const auto active_right = channel_index(scenario.active_deck, PairSide::Right);
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto deck = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    const auto base = static_cast<std::int32_t>((deck + 1U) * 400000);
    const auto motion = static_cast<std::int32_t>(
        ((frame_index + static_cast<std::uint64_t>(salt)) * 97U) +
        (static_cast<std::uint64_t>(channel) * 1009U));
    const auto active_boost = channel == active_left || channel == active_right ? 120000 : 0;
    frame[channel] = side == 0 ? base + active_boost + motion
                               : -base - active_boost - motion;
  }
  return frame;
}

bool packet_prefix_ok(const std::vector<S24Frame>& source,
                      std::uint64_t& check_errors,
                      std::uint64_t& panic_flags,
                      std::uint64_t& output_overflows) {
  std::vector<std::uint8_t> packed(source.size() * kMode2GroupBytes);
  Mode2OutputPacker packer(source, kMode2DefaultStartByte);
  packed.resize(packer.fill_into(packed));

  std::vector<S24Frame> decoded(source.size() + 16U);
  const auto decode = decode_mode2_usb_bytes_into(
      packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes, decoded);
  decoded.resize(static_cast<std::size_t>(decode.stats.decoded_frames));
  check_errors += decode.stats.check_errors;
  panic_flags += decode.stats.panic_flags;
  output_overflows += decode.output_overflows;

  const std::size_t source_offset = kMode2DefaultStartByte == 0 ? 0U : 1U;
  const auto comparable =
      std::min(decoded.size(), source.size() > source_offset ? source.size() - source_offset : 0U);
  for (std::size_t index = 0; index < comparable; ++index) {
    if (decoded[index] != source[index + source_offset]) {
      return false;
    }
  }
  return comparable > 0 && decode.stats.check_errors == 0 && decode.stats.panic_flags == 0 &&
         decode.output_overflows == 0;
}

ScenarioResult run_scenario(const Scenario& scenario) {
  PreparedTransportBackend transport;
  ScenarioResult result{};
  const PreparedTransportConfig config{
      .iso_frames = 8,
      .capture_slots = 64,
      .playback_slots = 64,
  };
  if (!transport.start(config)) {
    return result;
  }

  result.frames = static_cast<std::uint64_t>(scenario.sample_rate) * kPressureSeconds;
  result.periods = result.frames / config.iso_frames;
  result.frames = result.periods * config.iso_frames;

  std::vector<S24Frame> capture_batch(config.iso_frames);
  std::vector<S24Frame> playback_batch(config.iso_frames);
  std::vector<S24Frame> backend_playback(config.iso_frames);
  std::vector<S24Frame> hal_capture(config.iso_frames);
  std::vector<S24Frame> packet_probe;
  packet_probe.reserve(2048);

  for (std::uint64_t period = 0; period < result.periods; ++period) {
    for (std::uint32_t offset = 0; offset < config.iso_frames; ++offset) {
      const auto frame_index = (period * config.iso_frames) + offset;
      capture_batch[offset] = frame_for(scenario, frame_index, 0);
      playback_batch[offset] = frame_for(scenario, frame_index, 5000);
      if (packet_probe.size() < 2048U) {
        packet_probe.push_back(playback_batch[offset]);
      }
    }

    if (transport.hal_write_playback(playback_batch) != playback_batch.size()) {
      result.playback_mismatches += playback_batch.size();
      continue;
    }
    const auto timestamp = (period + 1U) * config.iso_frames;
    if (!transport.backend_complete_period(capture_batch, backend_playback, timestamp)) {
      result.playback_mismatches += playback_batch.size();
      continue;
    }
    if (transport.hal_read_capture(hal_capture) != hal_capture.size()) {
      result.capture_mismatches += capture_batch.size();
      continue;
    }
    for (std::size_t index = 0; index < capture_batch.size(); ++index) {
      if (hal_capture[index] != capture_batch[index]) {
        result.capture_mismatches += 1;
      }
      if (backend_playback[index] != playback_batch[index]) {
        result.playback_mismatches += 1;
      }
    }
  }

  result.packet_prefix_ok = packet_prefix_ok(packet_probe, result.packet_check_errors,
                                             result.packet_panic_flags,
                                             result.packet_output_overflows);
  result.counters = transport.counters();
  result.safety = transport.safety();
  result.pass = result.safety.product_safe && result.capture_mismatches == 0 &&
                result.playback_mismatches == 0 && result.packet_prefix_ok &&
                result.counters.backend_capture_frames == result.frames &&
                result.counters.backend_playback_frames == result.frames &&
                result.counters.hal_capture_reads == result.frames &&
                result.counters.hal_playback_writes == result.frames &&
                result.counters.backend_steady_requeues == result.periods * 2U &&
                result.counters.hal_steady_requeues == 0 &&
                result.counters.fallback_allocations == 0 &&
                result.counters.capture_ring_overruns == 0 &&
                result.counters.capture_ring_underruns == 0 &&
                result.counters.playback_ring_overruns == 0 &&
                result.counters.playback_ring_underruns == 0 &&
                result.counters.timestamp_regressions == 0 &&
                result.counters.channel_identity_failures == 0;
  return result;
}

void print_row(const Scenario& scenario, const ScenarioResult& result, bool trailing_comma) {
  const auto& counters = result.counters;
  std::cout << "    {\"sample_rate\": " << scenario.sample_rate << ", \"deck\": \""
            << pair_name(scenario.active_deck) << "\", \"seconds\": " << kPressureSeconds
            << ", \"periods\": " << result.periods << ", \"frames\": " << result.frames
            << ", \"backend_steady_requeues\": " << counters.backend_steady_requeues
            << ", \"hal_steady_requeues\": " << counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << counters.fallback_allocations
            << ", \"capture_ring_overruns\": " << counters.capture_ring_overruns
            << ", \"capture_ring_underruns\": " << counters.capture_ring_underruns
            << ", \"playback_ring_overruns\": " << counters.playback_ring_overruns
            << ", \"playback_ring_underruns\": " << counters.playback_ring_underruns
            << ", \"timestamp_regressions\": " << counters.timestamp_regressions
            << ", \"channel_identity_failures\": " << counters.channel_identity_failures
            << ", \"capture_mismatches\": " << result.capture_mismatches
            << ", \"playback_mismatches\": " << result.playback_mismatches
            << ", \"packet_check_errors\": " << result.packet_check_errors
            << ", \"packet_panic_flags\": " << result.packet_panic_flags
            << ", \"packet_output_overflows\": " << result.packet_output_overflows
            << ", \"packet_prefix_ok\": " << (result.packet_prefix_ok ? "true" : "false")
            << ", \"product_safe\": " << (result.safety.product_safe ? "true" : "false")
            << ", \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const Scenario scenarios[] = {
      {44100, StereoPair::A}, {44100, StereoPair::B}, {44100, StereoPair::C},
      {44100, StereoPair::D}, {48000, StereoPair::A}, {48000, StereoPair::B},
      {48000, StereoPair::C}, {48000, StereoPair::D},
  };

  std::uint32_t failures = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t total_hal_requeues = 0;
  std::uint64_t total_fallback_allocations = 0;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-transport-pressure-gate.v1\",\n"
            << "  \"meaning\": \"offline long-run prepared transport pressure gate; PASS is not physical readiness\",\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < std::size(scenarios); ++index) {
    const auto result = run_scenario(scenarios[index]);
    failures += result.pass ? 0U : 1U;
    total_frames += result.frames;
    total_hal_requeues += result.counters.hal_steady_requeues;
    total_fallback_allocations += result.counters.fallback_allocations;
    print_row(scenarios[index], result, index + 1U < std::size(scenarios));
  }
  const bool pass = failures == 0 && total_hal_requeues == 0 &&
                    total_fallback_allocations == 0 && total_frames > 0;
  std::cout << "  ],\n"
            << "  \"row_count\": " << std::size(scenarios) << ",\n"
            << "  \"total_frames\": " << total_frames << ",\n"
            << "  \"total_hal_steady_requeues\": " << total_hal_requeues << ",\n"
            << "  \"total_fallback_allocations\": " << total_fallback_allocations << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
