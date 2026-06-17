#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

S24Frame frame_for(std::uint32_t family, std::uint32_t index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto base = static_cast<std::int32_t>((family + 1U) * 300000U);
    const auto motion = static_cast<std::int32_t>((index * 307U) + (channel * 601U));
    frame[channel] = channel % 2U == 0 ? base + motion : -base - motion;
  }
  return frame;
}

std::vector<S24Frame> frames_for(std::uint32_t count, std::uint32_t family) {
  std::vector<S24Frame> frames;
  frames.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    frames.push_back(frame_for(family, index));
  }
  return frames;
}

struct PressureResult {
  std::uint32_t sample_rate = 0;
  std::uint64_t periods = 0;
  std::uint64_t frames = 0;
  std::uint64_t capture_mismatches = 0;
  std::uint64_t playback_mismatches = 0;
  PreparedTransportCounters counters{};
  PreparedTransportSafety safety{};
  bool pass = false;
};

bool frames_equal(std::span<const S24Frame> left, std::span<const S24Frame> right) {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

PressureResult run_pressure(std::uint32_t sample_rate) {
  constexpr std::uint32_t kSeconds = 2;
  constexpr std::uint32_t kIsoFrames = 8;
  PressureResult result{};
  result.sample_rate = sample_rate;

  AudioDriverSkeleton driver;
  const AudioStreamConfig config{
      .sample_rate = sample_rate,
      .buffer_frames = 64,
      .transport =
          PreparedTransportConfig{.iso_frames = kIsoFrames, .capture_slots = 32, .playback_slots = 32},
  };
  if (!driver.start_driver() || !driver.configure_stream(config) || !driver.start_stream()) {
    return result;
  }

  result.frames = static_cast<std::uint64_t>(sample_rate) * kSeconds;
  result.periods = result.frames / kIsoFrames;
  result.frames = result.periods * kIsoFrames;
  std::vector<S24Frame> playback(kIsoFrames);
  std::vector<S24Frame> capture(kIsoFrames);
  std::vector<S24Frame> backend_playback(kIsoFrames);
  std::vector<S24Frame> hal_capture(kIsoFrames);
  for (std::uint64_t period = 0; period < result.periods; ++period) {
    for (std::uint32_t offset = 0; offset < kIsoFrames; ++offset) {
      const auto frame_index = static_cast<std::uint32_t>((period * kIsoFrames) + offset);
      playback[offset] = frame_for(7, frame_index);
      capture[offset] = frame_for(9, frame_index);
    }
    if (!driver.write_playback(playback)) {
      result.playback_mismatches += kIsoFrames;
      continue;
    }
    if (!driver.complete_backend_period(capture, backend_playback, (period + 1U) * kIsoFrames)) {
      result.playback_mismatches += kIsoFrames;
      continue;
    }
    if (driver.read_capture(hal_capture) != hal_capture.size()) {
      result.capture_mismatches += kIsoFrames;
      continue;
    }
    if (!frames_equal(playback, backend_playback)) {
      result.playback_mismatches += kIsoFrames;
    }
    if (!frames_equal(capture, hal_capture)) {
      result.capture_mismatches += kIsoFrames;
    }
  }

  result.counters = driver.transport_counters();
  result.safety = driver.transport_safety();
  const bool stopped = driver.stop_stream() && driver.stop_driver();
  result.pass = stopped && result.safety.product_safe && result.capture_mismatches == 0 &&
                result.playback_mismatches == 0 &&
                result.counters.backend_prepare_enqueues == 64 &&
                result.counters.backend_steady_requeues == result.periods * 2U &&
                result.counters.hal_steady_requeues == 0 &&
                result.counters.fallback_allocations == 0 &&
                result.counters.capture_ring_overruns == 0 &&
                result.counters.capture_ring_underruns == 0 &&
                result.counters.playback_ring_overruns == 0 &&
                result.counters.playback_ring_underruns == 0 &&
                result.counters.timestamp_regressions == 0 &&
                result.counters.channel_identity_failures == 0 &&
                result.counters.backend_capture_frames == result.frames &&
                result.counters.backend_playback_frames == result.frames &&
                result.counters.hal_capture_reads == result.frames &&
                result.counters.hal_playback_writes == result.frames;
  return result;
}

}  // namespace

int main() {
  constexpr std::uint32_t kFrames = 32;
  AudioDriverSkeleton driver;

  std::uint32_t lifecycle_failures = 0;
  std::uint32_t config_failures = 0;
  std::uint32_t frame_mismatches = 0;
  std::uint32_t shutdown_failures = 0;

  const AudioStreamConfig valid_config{
      .sample_rate = 48000,
      .buffer_frames = 64,
      .transport = PreparedTransportConfig{.iso_frames = 8, .capture_slots = 16, .playback_slots = 16},
  };
  const AudioStreamConfig invalid_rate_config{
      .sample_rate = 96000,
      .buffer_frames = 64,
      .transport = PreparedTransportConfig{.iso_frames = 8, .capture_slots = 16, .playback_slots = 16},
  };
  const AudioStreamConfig invalid_transport_config{
      .sample_rate = 48000,
      .buffer_frames = 64,
      .transport = PreparedTransportConfig{.iso_frames = 8,
                                           .capture_slots = kPreparedTransportMaxSlots + 1U,
                                           .playback_slots = 16},
  };
  const AudioStreamConfig invalid_zero_buffer_config{
      .sample_rate = 48000,
      .buffer_frames = 0,
      .transport = PreparedTransportConfig{.iso_frames = 8, .capture_slots = 16, .playback_slots = 16},
  };
  const AudioStreamConfig invalid_large_buffer_config{
      .sample_rate = 48000,
      .buffer_frames = 4097,
      .transport = PreparedTransportConfig{.iso_frames = 8, .capture_slots = 16, .playback_slots = 16},
  };
  const AudioStreamConfig invalid_zero_iso_config{
      .sample_rate = 48000,
      .buffer_frames = 64,
      .transport = PreparedTransportConfig{.iso_frames = 0, .capture_slots = 16, .playback_slots = 16},
  };

  if (driver.configure_stream(valid_config)) {
    lifecycle_failures += 1;
  }
  if (driver.start_stream()) {
    lifecycle_failures += 1;
  }
  if (!driver.start_driver()) {
    lifecycle_failures += 1;
  }
  if (driver.configure_stream(invalid_rate_config)) {
    config_failures += 1;
  }
  if (driver.configure_stream(invalid_transport_config)) {
    config_failures += 1;
  }
  if (driver.configure_stream(invalid_zero_buffer_config)) {
    config_failures += 1;
  }
  if (driver.configure_stream(invalid_large_buffer_config)) {
    config_failures += 1;
  }
  if (driver.configure_stream(invalid_zero_iso_config)) {
    config_failures += 1;
  }
  if (!driver.configure_stream(valid_config)) {
    config_failures += 1;
  }
  if (!driver.start_stream()) {
    lifecycle_failures += 1;
  }
  if (driver.configure_stream(valid_config)) {
    config_failures += 1;
  }
  if (driver.start_stream()) {
    lifecycle_failures += 1;
  }

  const auto playback = frames_for(kFrames, 3);
  const auto capture = frames_for(kFrames, 4);
  std::vector<S24Frame> backend_playback(kFrames);
  std::vector<S24Frame> hal_capture(kFrames);

  if (!driver.write_playback(playback)) {
    frame_mismatches += 1;
  }
  if (!driver.complete_backend_period(capture, backend_playback, 8)) {
    lifecycle_failures += 1;
  }
  if (driver.read_capture(hal_capture) != hal_capture.size()) {
    frame_mismatches += 1;
  }
  if (!frames_equal(backend_playback, playback)) {
    frame_mismatches += 1;
  }
  if (!frames_equal(hal_capture, capture)) {
    frame_mismatches += 1;
  }

  const auto running_counters = driver.transport_counters();
  const auto running_safety = driver.transport_safety();
  if (!running_safety.product_safe) {
    lifecycle_failures += 1;
  }

  if (!driver.stop_driver()) {
    shutdown_failures += 1;
  }
  if (driver.stream_started()) {
    shutdown_failures += 1;
  }
  if (driver.transport_safety().product_safe) {
    shutdown_failures += 1;
  }
  if (driver.write_playback(playback)) {
    shutdown_failures += 1;
  }
  if (driver.read_capture(hal_capture) != 0) {
    shutdown_failures += 1;
  }
  if (driver.start_driver()) {
    if (driver.start_stream()) {
      shutdown_failures += 1;
    }
    if (!driver.configure_stream(valid_config)) {
      shutdown_failures += 1;
    }
    if (!driver.start_stream()) {
      shutdown_failures += 1;
    }
    if (!driver.stop_driver()) {
      shutdown_failures += 1;
    }
  } else {
    shutdown_failures += 1;
  }

  const auto pressure_441 = run_pressure(44100);
  const auto pressure_480 = run_pressure(48000);
  const std::uint32_t pressure_failures =
      (pressure_441.pass ? 0U : 1U) + (pressure_480.pass ? 0U : 1U);
  const auto pressure_total_frames = pressure_441.frames + pressure_480.frames;
  const auto pressure_total_hal_requeues =
      pressure_441.counters.hal_steady_requeues + pressure_480.counters.hal_steady_requeues;
  const auto pressure_total_fallbacks =
      pressure_441.counters.fallback_allocations + pressure_480.counters.fallback_allocations;

  const bool pass =
      lifecycle_failures == 0 && config_failures == 0 && frame_mismatches == 0 &&
      shutdown_failures == 0 && running_counters.backend_prepare_enqueues == 32 &&
      running_counters.backend_steady_requeues == 2 &&
      running_counters.hal_steady_requeues == 0 &&
      running_counters.fallback_allocations == 0 &&
      running_counters.capture_ring_overruns == 0 &&
      running_counters.capture_ring_underruns == 0 &&
      running_counters.playback_ring_overruns == 0 &&
      running_counters.playback_ring_underruns == 0 &&
      running_counters.timestamp_regressions == 0 &&
      running_counters.channel_identity_failures == 0 &&
      running_counters.backend_capture_frames == kFrames &&
      running_counters.backend_playback_frames == kFrames &&
      running_counters.hal_capture_reads == kFrames &&
      running_counters.hal_playback_writes == kFrames && pressure_failures == 0 &&
      pressure_total_hal_requeues == 0 && pressure_total_fallbacks == 0 &&
      pressure_total_frames == 184200U;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-runtime-contract.v2\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"offline DriverKit shell/runtime bridge contract; PASS is not dext install or physical readiness\",\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"sample_rate\": " << valid_config.sample_rate << ",\n"
            << "  \"buffer_frames\": " << valid_config.buffer_frames << ",\n"
            << "  \"lifecycle_failures\": " << lifecycle_failures << ",\n"
            << "  \"config_failures\": " << config_failures << ",\n"
            << "  \"frame_mismatches\": " << frame_mismatches << ",\n"
            << "  \"shutdown_failures\": " << shutdown_failures << ",\n"
            << "  \"backend_prepare_enqueues\": " << running_counters.backend_prepare_enqueues << ",\n"
            << "  \"backend_steady_requeues\": " << running_counters.backend_steady_requeues << ",\n"
            << "  \"hal_steady_requeues\": " << running_counters.hal_steady_requeues << ",\n"
            << "  \"fallback_allocations\": " << running_counters.fallback_allocations << ",\n"
            << "  \"capture_ring_overruns\": " << running_counters.capture_ring_overruns << ",\n"
            << "  \"capture_ring_underruns\": " << running_counters.capture_ring_underruns << ",\n"
            << "  \"playback_ring_overruns\": " << running_counters.playback_ring_overruns << ",\n"
            << "  \"playback_ring_underruns\": " << running_counters.playback_ring_underruns << ",\n"
            << "  \"timestamp_regressions\": " << running_counters.timestamp_regressions << ",\n"
            << "  \"channel_identity_failures\": " << running_counters.channel_identity_failures << ",\n"
            << "  \"backend_capture_frames\": " << running_counters.backend_capture_frames << ",\n"
            << "  \"backend_playback_frames\": " << running_counters.backend_playback_frames << ",\n"
            << "  \"hal_capture_reads\": " << running_counters.hal_capture_reads << ",\n"
            << "  \"hal_playback_writes\": " << running_counters.hal_playback_writes << ",\n"
            << "  \"running_product_safe\": " << (running_safety.product_safe ? "true" : "false")
            << ",\n"
            << "  \"pressure_rows\": [\n"
            << "    {\"sample_rate\": " << pressure_441.sample_rate
            << ", \"periods\": " << pressure_441.periods
            << ", \"frames\": " << pressure_441.frames
            << ", \"backend_steady_requeues\": "
            << pressure_441.counters.backend_steady_requeues
            << ", \"hal_steady_requeues\": " << pressure_441.counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << pressure_441.counters.fallback_allocations
            << ", \"capture_mismatches\": " << pressure_441.capture_mismatches
            << ", \"playback_mismatches\": " << pressure_441.playback_mismatches
            << ", \"product_safe\": " << (pressure_441.safety.product_safe ? "true" : "false")
            << ", \"result\": \"" << (pressure_441.pass ? "PASS" : "FAIL") << "\"},\n"
            << "    {\"sample_rate\": " << pressure_480.sample_rate
            << ", \"periods\": " << pressure_480.periods
            << ", \"frames\": " << pressure_480.frames
            << ", \"backend_steady_requeues\": "
            << pressure_480.counters.backend_steady_requeues
            << ", \"hal_steady_requeues\": " << pressure_480.counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << pressure_480.counters.fallback_allocations
            << ", \"capture_mismatches\": " << pressure_480.capture_mismatches
            << ", \"playback_mismatches\": " << pressure_480.playback_mismatches
            << ", \"product_safe\": " << (pressure_480.safety.product_safe ? "true" : "false")
            << ", \"result\": \"" << (pressure_480.pass ? "PASS" : "FAIL") << "\"}\n"
            << "  ],\n"
            << "  \"pressure_failures\": " << pressure_failures << ",\n"
            << "  \"pressure_total_frames\": " << pressure_total_frames << ",\n"
            << "  \"pressure_total_hal_steady_requeues\": " << pressure_total_hal_requeues
            << ",\n"
            << "  \"pressure_total_fallback_allocations\": " << pressure_total_fallbacks << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
