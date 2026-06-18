#include "opena8djcpp/prepared_usb_async_runtime.hpp"

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
  const PreparedUsbAsyncRuntimeConfig config{
      .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 4},
      .slots_per_submit = 8,
      .frames_per_slot = 8,
      .bytes_per_slot = kMode2DefaultTransferBytes,
      .max_live_requests = 4,
  };

  PreparedUsbAsyncRuntime runtime;
  const bool started = runtime.start(config);
  std::vector<PreparedUsbRequestHandle> handles;
  bool submissions_ok = started;
  for (std::uint32_t index = 0; index < config.max_live_requests; ++index) {
    const auto direction = (index % 2U) == 0U ? UsbSlotDirection::Capture
                                              : UsbSlotDirection::Playback;
    const auto submit = runtime.submit(direction,
                                       index * config.slots_per_submit,
                                       index * config.slots_per_submit * config.frames_per_slot,
                                       config.slots_per_submit,
                                       config.slots_per_submit * config.frames_per_slot,
                                       config.slots_per_submit * config.bytes_per_slot);
    submissions_ok = submissions_ok && submit.handle.valid();
    handles.push_back(submit.handle);
  }

  bool completions_ok = true;
  for (const auto& handle : handles) {
    completions_ok = runtime.complete(handle) && completions_ok;
  }

  const auto counters = runtime.counters();
  const auto safety = runtime.safety();

  PreparedUsbAsyncRuntime negative_runtime;
  const bool negative_started = negative_runtime.start(config);
  std::vector<PreparedUsbRequestHandle> negative_handles;
  bool negative_fill_ok = negative_started;
  for (std::uint32_t index = 0; index < config.max_live_requests; ++index) {
    const auto submit = negative_runtime.submit(UsbSlotDirection::Capture,
                                                index * config.slots_per_submit,
                                                index * config.slots_per_submit *
                                                    config.frames_per_slot,
                                                config.slots_per_submit,
                                                config.slots_per_submit *
                                                    config.frames_per_slot,
                                                config.slots_per_submit *
                                                    config.bytes_per_slot);
    negative_fill_ok = negative_fill_ok && submit.handle.valid();
    negative_handles.push_back(submit.handle);
  }
  const auto rejected_over_limit =
      negative_runtime
          .submit(UsbSlotDirection::Capture,
                  99,
                  99,
                  config.slots_per_submit,
                  config.slots_per_submit * config.frames_per_slot,
                  config.slots_per_submit * config.bytes_per_slot)
          .handle.valid() == false;
  for (const auto& handle : negative_handles) {
    (void)negative_runtime.complete(handle);
  }
  const auto rejected_bad_shape =
      negative_runtime
          .submit(UsbSlotDirection::Playback,
                  128,
                  1024,
                  config.slots_per_submit - 1U,
                  config.slots_per_submit * config.frames_per_slot,
                  config.slots_per_submit * config.bytes_per_slot)
          .handle.valid() == false;
  const auto negative_counters = negative_runtime.counters();
  std::vector<const char*> blockers;
  if (!started) blockers.push_back("runtime_start_failed");
  if (!submissions_ok) blockers.push_back("bounded_submissions_failed");
  if (!negative_started) blockers.push_back("negative_runtime_start_failed");
  if (!negative_fill_ok) blockers.push_back("negative_runtime_fill_failed");
  if (!rejected_over_limit) blockers.push_back("live_limit_not_enforced");
  if (!completions_ok) blockers.push_back("explicit_completions_failed");
  if (!rejected_bad_shape) blockers.push_back("descriptor_shape_not_enforced");
  if (counters.submit_calls != 4) blockers.push_back("unexpected_submit_count");
  if (counters.completion_calls != 4) blockers.push_back("unexpected_completion_count");
  if (negative_counters.live_limit_failures != 1) {
    blockers.push_back("unexpected_live_limit_failures");
  }
  if (negative_counters.descriptor_mismatches != 1) {
    blockers.push_back("unexpected_descriptor_mismatches");
  }
  if (counters.fallback_allocations != 0) blockers.push_back("fallback_allocations_present");
  if (counters.live_requests != 0) blockers.push_back("runtime_not_drained");
  if (!safety.product_safe) blockers.push_back("runtime_safety_failed");

  const bool pass = blockers.empty();
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-usb-async-runtime-contract.v1\",\n"
            << "  \"meaning\": \"offline async prepared submit lifecycle contract; PASS is not physical USB, HAL install, DriverKit install, or product readiness\",\n"
            << "  \"safety\": \"offline_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  print_number("slots_per_submit", config.slots_per_submit);
  print_number("frames_per_slot", config.frames_per_slot);
  print_number("bytes_per_slot", config.bytes_per_slot);
  print_number("request_slots", config.request_pool.request_slots);
  print_number("max_live_requests", config.max_live_requests);
  print_number("submit_calls", counters.submit_calls);
  print_number("capture_submit_calls", counters.capture_submit_calls);
  print_number("playback_submit_calls", counters.playback_submit_calls);
  print_number("completion_calls", counters.completion_calls);
  print_number("capture_completion_calls", counters.capture_completion_calls);
  print_number("playback_completion_calls", counters.playback_completion_calls);
  print_number("live_limit_failures", negative_counters.live_limit_failures);
  print_number("descriptor_mismatches", negative_counters.descriptor_mismatches);
  print_number("fallback_allocations", counters.fallback_allocations);
  print_number("live_requests", counters.live_requests);
  print_number("max_live_requests_observed", counters.max_live_requests);
  print_bool("preallocated_only", safety.preallocated_only);
  print_bool("bounded_live_requests", safety.bounded_live_requests);
  print_bool("completion_owned_lifecycle", safety.completion_owned_lifecycle);
  print_bool("descriptor_shape_safe", safety.descriptor_shape_safe);
  print_bool("no_fallback_allocations", safety.no_fallback_allocations);
  print_bool("drained", safety.drained);
  print_bool("product_safe", safety.product_safe);
  print_string_array("blockers", blockers);
  std::cout
      << "  \"blocked_claim\": \"NO_CPU_AUDIOPHILE_OR_TIMECODE_SUPERIORITY_CLAIM_UNTIL_THIS_ASYNC_CORE_IS_BOUND_TO_HAL_AND_VALIDATED_ON_HARDWARE\"\n"
      << "}\n";
  return pass ? 0 : 1;
}
