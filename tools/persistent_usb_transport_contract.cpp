#include "opena8djcpp/persistent_usb_transport.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace opena8djcpp;

namespace {

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
  const PersistentUsbTransportConfig config{
      .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 8},
      .slots_per_submit = 8,
      .frames_per_slot = 8,
      .capture_bytes_per_slot = 512 * 8,
      .playback_bytes_per_slot = kMode2DefaultTransferBytes,
      .capture_queue_depth = 4,
      .playback_queue_depth = 4,
  };
  constexpr std::uint32_t kSteadyCompletionsPerDirection = 128;

  PersistentUsbTransport transport;
  bool ok = transport.start(config);
  ok = transport.prime() && ok;
  for (std::uint32_t period = 0; period < kSteadyCompletionsPerDirection; ++period) {
    ok = transport.complete_next(UsbSlotDirection::Capture) && ok;
    ok = transport.complete_next(UsbSlotDirection::Playback) && ok;
  }
  const auto steady_counters = transport.counters();
  const auto cancelled = transport.drain();
  const auto counters = transport.counters();
  const auto safety = transport.safety();

  PersistentUsbTransport bad_depth_transport;
  const bool bad_depth_rejected =
      !bad_depth_transport.start(PersistentUsbTransportConfig{
          .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 7},
          .slots_per_submit = 8,
          .frames_per_slot = 8,
          .capture_bytes_per_slot = 512 * 8,
          .playback_bytes_per_slot = kMode2DefaultTransferBytes,
          .capture_queue_depth = 4,
          .playback_queue_depth = 4,
      });

  PersistentUsbTransport unprimed_transport;
  const bool unprimed_started = unprimed_transport.start(config);
  const bool unprimed_completion_rejected =
      !unprimed_transport.complete_next(UsbSlotDirection::Capture);
  unprimed_transport.stop();

  const auto expected_live = static_cast<std::uint64_t>(config.capture_queue_depth +
                                                       config.playback_queue_depth);
  const auto expected_prime_submits = expected_live;
  const auto expected_completion_calls =
      static_cast<std::uint64_t>(kSteadyCompletionsPerDirection) * 2U;
  const auto expected_steady_submits = expected_completion_calls;
  const auto expected_max_lead =
      static_cast<std::uint64_t>(config.capture_queue_depth * config.slots_per_submit *
                                 config.frames_per_slot);
  const auto expected_completed_frames =
      expected_completion_calls * config.slots_per_submit * config.frames_per_slot;

  std::vector<const char*> blockers;
  if (!ok) blockers.push_back("steady_runtime_operation_failed");
  if (!bad_depth_rejected) blockers.push_back("oversubscribed_depth_not_rejected");
  if (!unprimed_started) blockers.push_back("negative_runtime_start_failed");
  if (!unprimed_completion_rejected) blockers.push_back("unprimed_completion_not_rejected");
  if (counters.prime_submit_calls != expected_prime_submits) {
    blockers.push_back("unexpected_prime_submit_count");
  }
  if (counters.steady_submit_calls != expected_steady_submits) {
    blockers.push_back("unexpected_steady_submit_count");
  }
  if (counters.completion_calls != expected_completion_calls) {
    blockers.push_back("unexpected_completion_count");
  }
  if (steady_counters.live_requests != expected_live) {
    blockers.push_back("steady_live_depth_not_preserved");
  }
  if (cancelled != expected_live || counters.live_requests != 0) {
    blockers.push_back("drain_did_not_cancel_live_window");
  }
  if (counters.max_capture_live_requests != config.capture_queue_depth ||
      counters.max_playback_live_requests != config.playback_queue_depth) {
    blockers.push_back("direction_depth_not_observed");
  }
  if (counters.max_capture_lead_frames != expected_max_lead ||
      counters.max_playback_lead_frames != expected_max_lead) {
    blockers.push_back("submit_lead_window_unexpected");
  }
  if (counters.slot_identity_mismatches != 0) {
    blockers.push_back("slot_identity_mismatch");
  }
  if (counters.persistent_slot_reuses != counters.steady_submit_calls) {
    blockers.push_back("persistent_slot_reuse_count_unexpected");
  }
  if (counters.completed_frames != expected_completed_frames) {
    blockers.push_back("completed_frame_accounting_unexpected");
  }
  if (!safety.product_safe) blockers.push_back("persistent_transport_safety_failed");

  const bool pass = blockers.empty();
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.persistent-usb-transport-contract.v1\",\n"
            << "  \"meaning\": \"offline persistent USB request window contract; PASS means stable preallocated request lifecycle only, not HAL install, physical USB, DriverKit runtime, audio quality, CPU superiority, or product readiness\",\n"
            << "  \"safety\": \"offline_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"status\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  print_number("slots_per_submit", config.slots_per_submit);
  print_number("frames_per_slot", config.frames_per_slot);
  print_number("capture_queue_depth", config.capture_queue_depth);
  print_number("playback_queue_depth", config.playback_queue_depth);
  print_number("request_slots", config.request_pool.request_slots);
  print_number("steady_completions_per_direction", kSteadyCompletionsPerDirection);
  print_number("prime_submit_calls", counters.prime_submit_calls);
  print_number("steady_submit_calls", counters.steady_submit_calls);
  print_number("completion_calls", counters.completion_calls);
  print_number("capture_submit_calls", counters.capture_submit_calls);
  print_number("playback_submit_calls", counters.playback_submit_calls);
  print_number("capture_completion_calls", counters.capture_completion_calls);
  print_number("playback_completion_calls", counters.playback_completion_calls);
  print_number("steady_live_requests_before_drain", steady_counters.live_requests);
  print_number("max_live_requests", counters.max_live_requests);
  print_number("max_capture_live_requests", counters.max_capture_live_requests);
  print_number("max_playback_live_requests", counters.max_playback_live_requests);
  print_number("max_capture_lead_frames", counters.max_capture_lead_frames);
  print_number("max_playback_lead_frames", counters.max_playback_lead_frames);
  print_number("sequence_gap_errors", counters.sequence_gap_errors);
  print_number("timestamp_gap_errors", counters.timestamp_gap_errors);
  print_number("depth_drift_errors", counters.depth_drift_errors);
  print_number("live_limit_failures", counters.live_limit_failures);
  print_number("descriptor_shape_errors", counters.descriptor_shape_errors);
  print_number("slot_identity_mismatches", counters.slot_identity_mismatches);
  print_number("persistent_slot_reuses", counters.persistent_slot_reuses);
  print_number("fallback_allocations", counters.fallback_allocations);
  print_number("completed_frames", counters.completed_frames);
  print_number("submitted_frames", counters.submitted_frames);
  print_number("cancelled_requests", counters.cancelled_requests);
  print_bool("oversubscribed_depth_rejected", bad_depth_rejected);
  print_bool("unprimed_completion_rejected", unprimed_completion_rejected);
  print_bool("preallocated_only", safety.preallocated_only);
  print_bool("bounded_live_requests", safety.bounded_live_requests);
  print_bool("stable_queue_depth", safety.stable_queue_depth);
  print_bool("continuous_sequences", safety.continuous_sequences);
  print_bool("timestamp_continuity", safety.timestamp_continuity);
  print_bool("descriptor_shape_safe", safety.descriptor_shape_safe);
  print_bool("persistent_slot_identity", safety.persistent_slot_identity);
  print_bool("completion_owned_lifecycle", safety.completion_owned_lifecycle);
  print_bool("drained", safety.drained);
  print_bool("product_safe", safety.product_safe);
  print_bool("physical_evidence_present", false);
  print_bool("hal_binding_present", false);
  print_bool("product_claim_allowed", false);
  print_string_array("blockers", blockers);
  std::cout
      << "  \"next_required_action\": \"BIND_PERSISTENT_USB_TRANSPORT_TO_HAL_CANDIDATE_WITH_SOURCE_REFERENCE_AB_BEFORE_ANY_PRODUCT_CLAIM\",\n"
      << "  \"blocked_claim\": \"NO_CPU_AUDIOPHILE_TIMECODE_OR_MAINLINE_SUPERIORITY_CLAIM_FROM_OFFLINE_PERSISTENT_TRANSPORT_MODEL\"\n"
      << "}\n";
  return pass ? 0 : 1;
}
