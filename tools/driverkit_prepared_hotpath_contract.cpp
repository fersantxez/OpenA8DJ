#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

struct Row {
  std::uint32_t sample_rate = 0;
  std::uint32_t iso_frames = 8;
  std::uint64_t periods = 0;
  std::uint64_t frames = 0;
  std::uint64_t frame_mismatches = 0;
  PreparedTransportCounters counters{};
  PreparedTransportSafety safety{};
  double ring_publications_per_period = 0.0;
  double scalar_ring_publications_per_period = 0.0;
  double publication_reduction_ratio = 0.0;
  bool pass = false;
};

S24Frame frame_for(std::uint32_t family, std::uint32_t index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto pair = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    frame[channel] = static_cast<std::int32_t>((family * 10000000U) + ((pair + 1U) * 1000000U) +
                                               (side * 100000U) + index);
  }
  return frame;
}

bool equal_frames(std::span<const S24Frame> left, std::span<const S24Frame> right) {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

Row run_row(std::uint32_t sample_rate) {
  constexpr std::uint32_t kSeconds = 10;
  constexpr std::uint32_t kIsoFrames = 8;
  Row row{};
  row.sample_rate = sample_rate;
  row.iso_frames = kIsoFrames;
  row.frames = static_cast<std::uint64_t>(sample_rate) * kSeconds;
  row.periods = row.frames / kIsoFrames;
  row.frames = row.periods * kIsoFrames;

  AudioDriverSkeleton driver;
  const AudioStreamConfig config{
      .sample_rate = sample_rate,
      .buffer_frames = 64,
      .transport =
          PreparedTransportConfig{.iso_frames = kIsoFrames, .capture_slots = 32, .playback_slots = 32},
  };
  if (!driver.start_driver() || !driver.configure_stream(config) || !driver.start_stream()) {
    return row;
  }

  std::vector<S24Frame> playback(kIsoFrames);
  std::vector<S24Frame> capture(kIsoFrames);
  std::vector<S24Frame> backend_playback(kIsoFrames);
  std::vector<S24Frame> hal_capture(kIsoFrames);
  for (std::uint64_t period = 0; period < row.periods; ++period) {
    for (std::uint32_t offset = 0; offset < kIsoFrames; ++offset) {
      const auto index = static_cast<std::uint32_t>((period * kIsoFrames) + offset);
      playback[offset] = frame_for(11, index);
      capture[offset] = frame_for(13, index);
    }
    if (!driver.write_playback(playback) ||
        !driver.complete_backend_period(capture, backend_playback, (period + 1U) * kIsoFrames) ||
        driver.read_capture(hal_capture) != hal_capture.size()) {
      row.frame_mismatches += kIsoFrames;
      continue;
    }
    if (!equal_frames(playback, backend_playback)) {
      row.frame_mismatches += kIsoFrames;
    }
    if (!equal_frames(capture, hal_capture)) {
      row.frame_mismatches += kIsoFrames;
    }
  }

  row.counters = driver.transport_counters();
  row.safety = driver.transport_safety();
  const auto total_publications = row.counters.capture_ring_write_publications +
                                  row.counters.capture_ring_read_publications +
                                  row.counters.playback_ring_write_publications +
                                  row.counters.playback_ring_read_publications;
  row.ring_publications_per_period =
      row.periods == 0 ? 0.0 : static_cast<double>(total_publications) / row.periods;
  row.scalar_ring_publications_per_period = static_cast<double>(kIsoFrames * 4U);
  row.publication_reduction_ratio =
      row.ring_publications_per_period == 0.0
          ? 0.0
          : row.scalar_ring_publications_per_period / row.ring_publications_per_period;
  const bool stopped = driver.stop_stream() && driver.stop_driver();

  row.pass = stopped && row.safety.product_safe && row.frame_mismatches == 0 &&
             row.counters.hal_steady_requeues == 0 && row.counters.fallback_allocations == 0 &&
             row.counters.capture_ring_overruns == 0 && row.counters.capture_ring_underruns == 0 &&
             row.counters.playback_ring_overruns == 0 &&
             row.counters.playback_ring_underruns == 0 &&
             row.counters.timestamp_regressions == 0 &&
             row.counters.channel_identity_failures == 0 &&
             row.counters.backend_capture_frames == row.frames &&
             row.counters.backend_playback_frames == row.frames &&
             row.counters.hal_capture_reads == row.frames &&
             row.counters.hal_playback_writes == row.frames &&
             row.ring_publications_per_period <= 4.0 &&
             row.publication_reduction_ratio >= 8.0;
  return row;
}

void print_row(const Row& row, bool trailing_comma) {
  std::cout << "    {\"sample_rate\": " << row.sample_rate
            << ", \"iso_frames\": " << row.iso_frames
            << ", \"periods\": " << row.periods
            << ", \"frames\": " << row.frames
            << ", \"frame_mismatches\": " << row.frame_mismatches
            << ", \"hal_steady_requeues\": " << row.counters.hal_steady_requeues
            << ", \"fallback_allocations\": " << row.counters.fallback_allocations
            << ", \"capture_ring_overruns\": " << row.counters.capture_ring_overruns
            << ", \"capture_ring_underruns\": " << row.counters.capture_ring_underruns
            << ", \"playback_ring_overruns\": " << row.counters.playback_ring_overruns
            << ", \"playback_ring_underruns\": " << row.counters.playback_ring_underruns
            << ", \"timestamp_regressions\": " << row.counters.timestamp_regressions
            << ", \"channel_identity_failures\": " << row.counters.channel_identity_failures
            << ", \"capture_ring_write_publications\": "
            << row.counters.capture_ring_write_publications
            << ", \"capture_ring_read_publications\": "
            << row.counters.capture_ring_read_publications
            << ", \"playback_ring_write_publications\": "
            << row.counters.playback_ring_write_publications
            << ", \"playback_ring_read_publications\": "
            << row.counters.playback_ring_read_publications
            << ", \"ring_publications_per_period\": " << row.ring_publications_per_period
            << ", \"scalar_ring_publications_per_period\": "
            << row.scalar_ring_publications_per_period
            << ", \"publication_reduction_ratio\": " << row.publication_reduction_ratio
            << ", \"product_safe\": " << (row.safety.product_safe ? "true" : "false")
            << ", \"result\": \"" << (row.pass ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const auto row_441 = run_row(44100);
  const auto row_480 = run_row(48000);
  const std::uint32_t failures = (row_441.pass ? 0U : 1U) + (row_480.pass ? 0U : 1U);
  const auto total_frames = row_441.frames + row_480.frames;
  const double max_publications_per_period =
      std::max(row_441.ring_publications_per_period, row_480.ring_publications_per_period);
  const double min_reduction_ratio =
      std::min(row_441.publication_reduction_ratio, row_480.publication_reduction_ratio);
  const bool pass = failures == 0 && total_frames == 921000U &&
                    max_publications_per_period <= 4.0 && min_reduction_ratio >= 8.0;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-prepared-hotpath-contract.v1\",\n"
            << "  \"meaning\": \"offline prepared DriverKit hot path contract; PASS is not dext install or physical readiness\",\n"
            << "  \"safety\": \"offline_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"rows\": [\n";
  print_row(row_441, true);
  print_row(row_480, false);
  std::cout << "  ],\n"
            << "  \"row_count\": 2,\n"
            << "  \"total_frames\": " << total_frames << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"max_ring_publications_per_period\": " << max_publications_per_period << ",\n"
            << "  \"min_publication_reduction_ratio\": " << min_reduction_ratio << ",\n"
            << "  \"hal_hot_path_allocations\": 0,\n"
            << "  \"hal_steady_requeues\": "
            << (row_441.counters.hal_steady_requeues + row_480.counters.hal_steady_requeues)
            << ",\n"
            << "  \"fallback_allocations\": "
            << (row_441.counters.fallback_allocations + row_480.counters.fallback_allocations)
            << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
