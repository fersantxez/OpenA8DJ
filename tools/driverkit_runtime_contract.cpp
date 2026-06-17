#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <cstdint>
#include <iostream>
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

bool vectors_equal(const std::vector<S24Frame>& left, const std::vector<S24Frame>& right) {
  return left.size() == right.size() && left == right;
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
  if (!vectors_equal(backend_playback, playback)) {
    frame_mismatches += 1;
  }
  if (!vectors_equal(hal_capture, capture)) {
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
      running_counters.hal_playback_writes == kFrames;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-runtime-contract.v1\",\n"
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
            << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
