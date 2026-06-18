#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

S24Frame frame_for(std::uint32_t family, std::uint64_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto side = channel % kChannelsPerPair;
    const auto motion =
        static_cast<std::int32_t>((frame_index * 79U) + (channel * 613U));
    const auto base = static_cast<std::int32_t>((family + 1U) * 250000U);
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
}

void print_number(const char* key, std::uint64_t value) {
  std::cout << "  \"" << key << "\": " << value << ",\n";
}

void print_string_array(const char* key, const std::vector<const char*>& values) {
  std::cout << "  \"" << key << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << values[index] << "\"";
  }
  std::cout << "],\n";
}

}  // namespace

int main() {
  constexpr std::uint32_t kIsoFrames = 8;
  constexpr std::uint32_t kSlotsPerSubmit = 8;
  constexpr std::uint32_t kFramesPerPeriod = kIsoFrames * kSlotsPerSubmit;
  constexpr std::uint32_t kPeriods = 128;

  const AudioStreamConfig config{
      .sample_rate = 48000,
      .buffer_frames = kFramesPerPeriod,
      .transport =
          PreparedTransportConfig{.iso_frames = kIsoFrames, .capture_slots = 32, .playback_slots = 32},
      .usb_slots_per_submit = kSlotsPerSubmit,
      .usb_bytes_per_slot = kMode2DefaultTransferBytes,
      .usb_request_slots = 8,
      .use_persistent_usb_transport = true,
      .usb_capture_bytes_per_slot = 512 * 8,
      .persistent_capture_queue_depth = 4,
      .persistent_playback_queue_depth = 4,
  };

  AudioDriverSkeleton driver;
  bool ok = driver.start_driver() && driver.configure_stream(config) && driver.start_stream();
  std::uint64_t transport_frame_mismatches = 0;
  std::uint64_t capture_read_mismatches = 0;
  std::vector<S24Frame> playback(kFramesPerPeriod);
  std::vector<S24Frame> capture(kFramesPerPeriod);
  std::vector<S24Frame> backend_playback(kFramesPerPeriod);
  std::vector<S24Frame> hal_capture(kFramesPerPeriod);

  for (std::uint32_t period = 0; period < kPeriods && ok; ++period) {
    for (std::uint32_t offset = 0; offset < kFramesPerPeriod; ++offset) {
      const auto frame_index =
          static_cast<std::uint64_t>(period) * kFramesPerPeriod + offset;
      playback[offset] = frame_for(41, frame_index);
      capture[offset] = frame_for(43, frame_index);
    }

    ok = driver.write_playback(playback) && ok;
    ok = driver.complete_backend_period(capture, backend_playback,
                                        static_cast<std::uint64_t>(period + 1U) *
                                            kFramesPerPeriod) &&
         ok;
    const auto read = driver.read_capture(hal_capture);
    if (read != hal_capture.size()) {
      capture_read_mismatches += 1;
      ok = false;
    }
    if (backend_playback != playback || hal_capture != capture) {
      transport_frame_mismatches += 1;
      ok = false;
    }
  }

  const auto steady_counters = driver.persistent_usb_transport_counters();
  const auto transport_safety = driver.transport_safety();
  const bool stopped = driver.stop_stream() && driver.stop_driver();
  const auto counters = driver.persistent_usb_transport_counters();
  const auto persistent_safety = driver.persistent_usb_transport_safety();

  AudioDriverSkeleton bad_depth_driver;
  const bool bad_depth_rejected =
      bad_depth_driver.start_driver() &&
      !bad_depth_driver.configure_stream(AudioStreamConfig{
          .sample_rate = 48000,
          .buffer_frames = kFramesPerPeriod,
          .transport = PreparedTransportConfig{.iso_frames = kIsoFrames,
                                               .capture_slots = 32,
                                               .playback_slots = 32},
          .usb_slots_per_submit = kSlotsPerSubmit,
          .usb_bytes_per_slot = kMode2DefaultTransferBytes,
          .usb_request_slots = 7,
          .use_persistent_usb_transport = true,
          .usb_capture_bytes_per_slot = 512 * 8,
          .persistent_capture_queue_depth = 4,
          .persistent_playback_queue_depth = 4,
      });
  (void)bad_depth_driver.stop_driver();

  const auto expected_live = static_cast<std::uint64_t>(
      config.persistent_capture_queue_depth + config.persistent_playback_queue_depth);
  const auto expected_completion_calls = static_cast<std::uint64_t>(kPeriods) * 2U;
  const auto expected_steady_submits = expected_completion_calls;
  const auto expected_max_lead =
      static_cast<std::uint64_t>(config.persistent_capture_queue_depth *
                                 config.usb_slots_per_submit * config.transport.iso_frames);
  const auto expected_completed_frames =
      expected_completion_calls * config.usb_slots_per_submit * config.transport.iso_frames;
  const auto expected_cancelled = expected_live;

  std::vector<const char*> blockers;
  if (!ok) blockers.push_back("driverkit_persistent_runtime_operation_failed");
  if (!stopped) blockers.push_back("driverkit_persistent_shutdown_failed");
  if (!bad_depth_rejected) blockers.push_back("oversubscribed_depth_not_rejected");
  if (transport_frame_mismatches != 0) blockers.push_back("transport_frame_mismatch");
  if (capture_read_mismatches != 0) blockers.push_back("capture_read_mismatch");
  if (steady_counters.live_requests != expected_live) {
    blockers.push_back("steady_live_depth_not_preserved");
  }
  if (counters.prime_submit_calls != expected_live) {
    blockers.push_back("unexpected_prime_submit_count");
  }
  if (counters.steady_submit_calls != expected_steady_submits) {
    blockers.push_back("unexpected_steady_submit_count");
  }
  if (counters.completion_calls != expected_completion_calls) {
    blockers.push_back("unexpected_completion_count");
  }
  if (counters.cancelled_requests != expected_cancelled || counters.live_requests != 0) {
    blockers.push_back("shutdown_did_not_drain_live_window");
  }
  if (counters.max_live_requests != expected_live ||
      counters.max_capture_live_requests != config.persistent_capture_queue_depth ||
      counters.max_playback_live_requests != config.persistent_playback_queue_depth) {
    blockers.push_back("bounded_live_window_unexpected");
  }
  if (counters.max_capture_lead_frames != expected_max_lead ||
      counters.max_playback_lead_frames != expected_max_lead) {
    blockers.push_back("submit_lead_window_unexpected");
  }
  if (counters.slot_identity_mismatches != 0 ||
      counters.persistent_slot_reuses != counters.steady_submit_calls) {
    blockers.push_back("persistent_slot_identity_failed");
  }
  if (counters.sequence_gap_errors != 0 || counters.timestamp_gap_errors != 0) {
    blockers.push_back("sequence_or_timestamp_gap");
  }
  if (counters.depth_drift_errors != 0 || counters.fallback_allocations != 0) {
    blockers.push_back("allocation_or_depth_drift");
  }
  if (counters.completed_frames != expected_completed_frames) {
    blockers.push_back("completed_frame_accounting_unexpected");
  }
  if (!transport_safety.product_safe) blockers.push_back("prepared_transport_safety_failed");
  if (!persistent_safety.product_safe) {
    blockers.push_back("persistent_driverkit_transport_safety_failed");
  }

  const bool pass = blockers.empty();
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-persistent-usb-transport-contract.v1\",\n"
            << "  \"meaning\": \"offline DriverKit skeleton binding to persistent USB request window; PASS is not dext install, physical USB, audio quality, CPU superiority, or product readiness\",\n"
            << "  \"safety\": \"offline_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"status\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  print_number("periods", kPeriods);
  print_number("frames_per_period", kFramesPerPeriod);
  print_number("slots_per_submit", config.usb_slots_per_submit);
  print_number("frames_per_slot", config.transport.iso_frames);
  print_number("capture_queue_depth", config.persistent_capture_queue_depth);
  print_number("playback_queue_depth", config.persistent_playback_queue_depth);
  print_number("request_slots", config.usb_request_slots);
  print_number("transport_frame_mismatches", transport_frame_mismatches);
  print_number("capture_read_mismatches", capture_read_mismatches);
  print_number("prime_submit_calls", counters.prime_submit_calls);
  print_number("steady_submit_calls", counters.steady_submit_calls);
  print_number("completion_calls", counters.completion_calls);
  print_number("capture_submit_calls", counters.capture_submit_calls);
  print_number("playback_submit_calls", counters.playback_submit_calls);
  print_number("capture_completion_calls", counters.capture_completion_calls);
  print_number("playback_completion_calls", counters.playback_completion_calls);
  print_number("steady_live_requests_before_stop", steady_counters.live_requests);
  print_number("max_live_requests", counters.max_live_requests);
  print_number("max_capture_live_requests", counters.max_capture_live_requests);
  print_number("max_playback_live_requests", counters.max_playback_live_requests);
  print_number("max_capture_lead_frames", counters.max_capture_lead_frames);
  print_number("max_playback_lead_frames", counters.max_playback_lead_frames);
  print_number("sequence_gap_errors", counters.sequence_gap_errors);
  print_number("timestamp_gap_errors", counters.timestamp_gap_errors);
  print_number("depth_drift_errors", counters.depth_drift_errors);
  print_number("slot_identity_mismatches", counters.slot_identity_mismatches);
  print_number("persistent_slot_reuses", counters.persistent_slot_reuses);
  print_number("fallback_allocations", counters.fallback_allocations);
  print_number("completed_frames", counters.completed_frames);
  print_number("submitted_frames", counters.submitted_frames);
  print_number("cancelled_requests", counters.cancelled_requests);
  print_bool("prepared_transport_product_safe", transport_safety.product_safe);
  print_bool("preallocated_only", persistent_safety.preallocated_only);
  print_bool("bounded_live_requests", persistent_safety.bounded_live_requests);
  print_bool("stable_queue_depth", persistent_safety.stable_queue_depth);
  print_bool("continuous_sequences", persistent_safety.continuous_sequences);
  print_bool("timestamp_continuity", persistent_safety.timestamp_continuity);
  print_bool("descriptor_shape_safe", persistent_safety.descriptor_shape_safe);
  print_bool("persistent_slot_identity", persistent_safety.persistent_slot_identity);
  print_bool("completion_owned_lifecycle", persistent_safety.completion_owned_lifecycle);
  print_bool("drained", persistent_safety.drained);
  print_bool("persistent_transport_product_safe", persistent_safety.product_safe);
  print_bool("oversubscribed_depth_rejected", bad_depth_rejected);
  print_bool("driverkit_shell_binding_present", true);
  print_bool("hal_binding_present", false);
  print_bool("physical_evidence_present", false);
  print_bool("product_claim_allowed", false);
  print_string_array("blockers", blockers);
  std::cout
      << "  \"next_required_action\": \"LOCK_GATED_SOURCE_REFERENCE_MAINLINE_CPP_AB_AUDIO8_TO_IRIG_BEFORE_ANY_PRODUCT_OR_HUMAN_TEST_CLAIM\",\n"
      << "  \"blocked_claim\": \"NO_AUDIOPHILE_TIMECODE_CPU_MAINLINE_SUPERIORITY_OR_BRANCH_PROMOTION_CLAIM_FROM_OFFLINE_DRIVERKIT_PERSISTENT_BINDING\"\n"
      << "}\n";
  return pass ? 0 : 1;
}
