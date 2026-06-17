#include "opena8djcpp/prepared_transport.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

S24Frame synthetic_frame(std::uint32_t family, std::uint32_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto signed_family = static_cast<std::int32_t>(family * 700000U);
    const auto motion = static_cast<std::int32_t>((frame_index * 211U) + (channel * 977U));
    frame[channel] = channel % 2U == 0 ? signed_family + motion : -signed_family - motion;
  }
  return frame;
}

std::vector<S24Frame> make_frames(std::uint32_t count,
                                  std::uint32_t family,
                                  std::uint32_t offset = 0) {
  std::vector<S24Frame> frames;
  frames.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    frames.push_back(synthetic_frame(family, index + offset));
  }
  return frames;
}

bool counters_are_clean(const PreparedTransportCounters& counters,
                        std::uint64_t expected_prepare_enqueues,
                        std::uint64_t expected_backend_frames,
                        std::uint64_t expected_hal_frames) {
  return counters.backend_prepare_enqueues == expected_prepare_enqueues &&
         counters.backend_steady_requeues == 2 &&
         counters.hal_steady_requeues == 0 && counters.fallback_allocations == 0 &&
         counters.capture_ring_overruns == 0 && counters.capture_ring_underruns == 0 &&
         counters.playback_ring_overruns == 0 && counters.playback_ring_underruns == 0 &&
         counters.timestamp_regressions == 0 && counters.channel_identity_failures == 0 &&
         counters.timecode_profile_failures == 0 &&
         counters.backend_capture_frames == expected_backend_frames &&
         counters.backend_playback_frames == expected_backend_frames &&
         counters.hal_capture_reads == expected_hal_frames &&
         counters.hal_playback_writes == expected_hal_frames;
}

bool invalid_configs_rejected(std::uint32_t& failures) {
  const std::array<PreparedTransportConfig, 5> invalid_configs = {
      PreparedTransportConfig{.iso_frames = 0, .capture_slots = 8, .playback_slots = 8},
      PreparedTransportConfig{.iso_frames = 8, .capture_slots = 0, .playback_slots = 8},
      PreparedTransportConfig{.iso_frames = 8, .capture_slots = 8, .playback_slots = 0},
      PreparedTransportConfig{
          .iso_frames = 8, .capture_slots = kPreparedTransportMaxSlots + 1U, .playback_slots = 8},
      PreparedTransportConfig{
          .iso_frames = 8, .capture_slots = 8, .playback_slots = kPreparedTransportMaxSlots + 1U},
  };

  for (const auto& config : invalid_configs) {
    PreparedTransportBackend transport;
    if (transport.start(config)) {
      failures += 1;
    }
    if (transport.started()) {
      failures += 1;
    }
    if (transport.counters().backend_prepare_enqueues != 0) {
      failures += 1;
    }
  }
  return failures == 0;
}

bool unstarted_safety_rejected(std::uint32_t& false_safe_failures) {
  PreparedTransportBackend transport;
  if (transport.safety().product_safe) {
    false_safe_failures += 1;
  }
  return false_safe_failures == 0;
}

bool stopped_operations_blocked(std::uint32_t& failures) {
  PreparedTransportBackend transport;
  const auto frame = synthetic_frame(11, 0);
  const auto frames = make_frames(4, 12);
  std::vector<S24Frame> scratch(4, synthetic_frame(99, 99));
  const auto sentinel = scratch;

  if (!transport.start(PreparedTransportConfig{})) {
    failures += 1;
    return false;
  }
  transport.stop();

  if (transport.started()) {
    failures += 1;
  }
  if (transport.hal_write_playback(frame)) {
    failures += 1;
  }
  if (transport.hal_write_playback(frames) != 0) {
    failures += 1;
  }
  if (transport.backend_complete_period(frame, 8)) {
    failures += 1;
  }
  if (transport.backend_complete_period(std::span<const S24Frame>(frames), std::span<S24Frame>(scratch),
                                        8)) {
    failures += 1;
  }
  if (scratch != sentinel) {
    failures += 1;
  }
  S24Frame out{};
  if (transport.hal_read_capture(out)) {
    failures += 1;
  }
  if (transport.hal_read_capture(std::span<S24Frame>(scratch)) != 0) {
    failures += 1;
  }
  return failures == 0;
}

bool restart_clears_state(std::uint32_t& stale_frame_mismatches,
                          std::uint32_t& counter_reset_failures,
                          std::uint32_t& timestamp_reset_failures,
                          PreparedTransportCounters& final_counters,
                          PreparedTransportSafety& final_safety) {
  constexpr std::uint32_t kFrames = 16;
  constexpr PreparedTransportConfig kInitialConfig{
      .iso_frames = 8, .capture_slots = 8, .playback_slots = 8};
  constexpr PreparedTransportConfig kRestartConfig{
      .iso_frames = 8, .capture_slots = 16, .playback_slots = 16};

  PreparedTransportBackend transport;
  if (!transport.start(kInitialConfig)) {
    counter_reset_failures += 1;
    return false;
  }

  const auto stale_capture = make_frames(kFrames, 21);
  const auto stale_playback = make_frames(kFrames, 22);
  std::vector<S24Frame> stale_backend_playback(kFrames);
  if (transport.hal_write_playback(stale_playback) != stale_playback.size()) {
    counter_reset_failures += 1;
  }
  if (!transport.backend_complete_period(stale_capture, stale_backend_playback, 8000)) {
    counter_reset_failures += 1;
  }

  transport.stop();
  if (!transport.start(kRestartConfig)) {
    counter_reset_failures += 1;
    return false;
  }

  const auto fresh_capture = make_frames(kFrames, 31);
  const auto fresh_playback = make_frames(kFrames, 32);
  std::vector<S24Frame> fresh_backend_playback(kFrames);
  std::vector<S24Frame> fresh_hal_capture(kFrames);

  if (transport.hal_write_playback(fresh_playback) != fresh_playback.size()) {
    counter_reset_failures += 1;
  }
  if (!transport.backend_complete_period(fresh_capture, fresh_backend_playback, 8)) {
    counter_reset_failures += 1;
  }
  if (transport.hal_read_capture(fresh_hal_capture) != fresh_hal_capture.size()) {
    counter_reset_failures += 1;
  }

  for (std::size_t index = 0; index < fresh_capture.size(); ++index) {
    if (fresh_hal_capture[index] != fresh_capture[index]) {
      stale_frame_mismatches += 1;
    }
    if (fresh_backend_playback[index] != fresh_playback[index]) {
      stale_frame_mismatches += 1;
    }
    if (fresh_hal_capture[index] == stale_capture[index]) {
      stale_frame_mismatches += 1;
    }
    if (fresh_backend_playback[index] == stale_playback[index]) {
      stale_frame_mismatches += 1;
    }
  }

  final_counters = transport.counters();
  final_safety = transport.safety();
  if (final_counters.timestamp_regressions != 0) {
    timestamp_reset_failures += 1;
  }
  const auto expected_prepare =
      static_cast<std::uint64_t>(kRestartConfig.capture_slots) + kRestartConfig.playback_slots;
  if (!counters_are_clean(final_counters, expected_prepare, kFrames, kFrames)) {
    counter_reset_failures += 1;
  }

  return stale_frame_mismatches == 0 && counter_reset_failures == 0 &&
         timestamp_reset_failures == 0 && final_safety.product_safe;
}

}  // namespace

int main() {
  std::uint32_t invalid_config_failures = 0;
  std::uint32_t false_unstarted_safe_failures = 0;
  std::uint32_t stopped_operation_failures = 0;
  std::uint32_t stale_frame_mismatches = 0;
  std::uint32_t counter_reset_failures = 0;
  std::uint32_t timestamp_reset_failures = 0;
  PreparedTransportCounters final_counters{};
  PreparedTransportSafety final_safety{};

  const bool invalid_ok = invalid_configs_rejected(invalid_config_failures);
  const bool unstarted_ok = unstarted_safety_rejected(false_unstarted_safe_failures);
  const bool stopped_ok = stopped_operations_blocked(stopped_operation_failures);
  const bool restart_ok = restart_clears_state(stale_frame_mismatches, counter_reset_failures,
                                               timestamp_reset_failures, final_counters,
                                               final_safety);

  const bool pass = invalid_ok && unstarted_ok && stopped_ok && restart_ok;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-transport-recovery-contract.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"offline lifecycle/recovery contract; PASS means restart clears prepared rings and counters, not physical readiness\",\n"
            << "  \"invalid_config_failures\": " << invalid_config_failures << ",\n"
            << "  \"false_unstarted_safe_failures\": " << false_unstarted_safe_failures << ",\n"
            << "  \"stopped_operation_failures\": " << stopped_operation_failures << ",\n"
            << "  \"stale_frame_mismatches\": " << stale_frame_mismatches << ",\n"
            << "  \"counter_reset_failures\": " << counter_reset_failures << ",\n"
            << "  \"timestamp_reset_failures\": " << timestamp_reset_failures << ",\n"
            << "  \"backend_prepare_enqueues\": " << final_counters.backend_prepare_enqueues << ",\n"
            << "  \"backend_steady_requeues\": " << final_counters.backend_steady_requeues << ",\n"
            << "  \"hal_steady_requeues\": " << final_counters.hal_steady_requeues << ",\n"
            << "  \"fallback_allocations\": " << final_counters.fallback_allocations << ",\n"
            << "  \"capture_ring_overruns\": " << final_counters.capture_ring_overruns << ",\n"
            << "  \"capture_ring_underruns\": " << final_counters.capture_ring_underruns << ",\n"
            << "  \"playback_ring_overruns\": " << final_counters.playback_ring_overruns << ",\n"
            << "  \"playback_ring_underruns\": " << final_counters.playback_ring_underruns << ",\n"
            << "  \"timestamp_regressions\": " << final_counters.timestamp_regressions << ",\n"
            << "  \"channel_identity_failures\": " << final_counters.channel_identity_failures << ",\n"
            << "  \"backend_capture_frames\": " << final_counters.backend_capture_frames << ",\n"
            << "  \"backend_playback_frames\": " << final_counters.backend_playback_frames << ",\n"
            << "  \"hal_capture_reads\": " << final_counters.hal_capture_reads << ",\n"
            << "  \"hal_playback_writes\": " << final_counters.hal_playback_writes << ",\n"
            << "  \"product_safe\": " << (final_safety.product_safe ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
