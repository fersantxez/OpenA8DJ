#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"
#include "opena8djcpp/usb_request_pool.hpp"

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
    const auto base = static_cast<std::int32_t>((family + 1U) * 325000U);
    const auto motion =
        static_cast<std::int32_t>((frame_index * 97U) + (channel * 563U));
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

struct ShutdownRun {
  PreparedUsbRequestPoolCounters before_stop{};
  PreparedUsbRequestPoolCounters after_stop{};
  PreparedUsbRequestPoolSafety after_stop_safety{};
  bool started = false;
  bool stopped = false;
  bool driver_stopped = false;
  bool pass = false;
};

ShutdownRun run_shutdown_cycle() {
  constexpr std::uint32_t kIsoFrames = 8;
  constexpr std::uint32_t kPeriods = 8;
  ShutdownRun result{};
  AudioDriverSkeleton driver;
  const AudioStreamConfig config{
      .sample_rate = 48000,
      .buffer_frames = 64,
      .transport =
          PreparedTransportConfig{.iso_frames = kIsoFrames, .capture_slots = 32, .playback_slots = 32},
      .usb_slots_per_submit = 8,
      .usb_bytes_per_slot = kMode2DefaultTransferBytes,
      .usb_initial_capture_slots = 8,
      .usb_initial_playback_slots = 8,
      .usb_request_slots = 8,
      .usb_request_completion_depth = 4,
      .usb_retain_submit_descriptors = false,
  };
  result.started = driver.start_driver() && driver.configure_stream(config) &&
                   driver.start_stream();
  if (!result.started) {
    return result;
  }

  std::vector<S24Frame> playback(kIsoFrames);
  std::vector<S24Frame> capture(kIsoFrames);
  std::vector<S24Frame> backend_playback(kIsoFrames);
  for (std::uint32_t period = 0; period < kPeriods; ++period) {
    for (std::uint32_t offset = 0; offset < kIsoFrames; ++offset) {
      const auto frame_index = static_cast<std::uint64_t>(period * kIsoFrames) + offset;
      playback[offset] = frame_for(41, frame_index);
      capture[offset] = frame_for(43, frame_index);
    }
    if (!driver.write_playback(playback) ||
        !driver.complete_backend_period(capture, backend_playback, (period + 1U) * kIsoFrames)) {
      return result;
    }
  }

  result.before_stop = driver.usb_request_counters();
  result.stopped = driver.stop_stream();
  result.after_stop = driver.usb_request_counters();
  result.after_stop_safety = driver.usb_request_safety();
  result.driver_stopped = driver.stop_driver();
  result.pass =
      result.stopped && result.driver_stopped &&
      result.before_stop.live_requests == 3 &&
      result.before_stop.submit_calls == 4 &&
      result.before_stop.completion_calls == 1 &&
      result.after_stop.live_requests == 0 &&
      result.after_stop.submit_calls == 4 &&
      result.after_stop.completion_calls == 1 &&
      result.after_stop.cancel_calls == 3 &&
      result.after_stop.recycle_calls == 4 &&
      result.after_stop.fallback_allocations == 0 &&
      result.after_stop.invalid_completions == 0 &&
      result.after_stop.stale_completions == 0 &&
      result.after_stop.late_completions_after_cancel == 0 &&
      result.after_stop.submitted_bytes ==
          result.after_stop.completed_bytes + result.after_stop.cancelled_bytes &&
      result.after_stop.submitted_frames ==
          result.after_stop.completed_frames + result.after_stop.cancelled_frames &&
      result.after_stop_safety.product_safe;
  return result;
}

struct LateCompletionResult {
  PreparedUsbRequestPoolCounters counters{};
  PreparedUsbRequestPoolSafety safety{};
  bool late_rejected = false;
  bool pass = false;
};

LateCompletionResult run_late_completion_rejection() {
  LateCompletionResult result{};
  PreparedUsbRequestPool pool;
  if (!pool.start(PreparedUsbRequestPoolConfig{.request_slots = 2})) {
    return result;
  }
  const UsbSubmitDescriptor descriptor{.direction = UsbSlotDirection::Playback,
                                       .first_sequence = 0,
                                       .slot_count = 8,
                                       .first_sample_timestamp = 8,
                                       .frame_count = 176,
                                       .byte_count = 2816};
  const auto handle = pool.submit(descriptor);
  (void)pool.cancel_all();
  result.late_rejected = !pool.complete(handle);
  result.counters = pool.counters();
  result.safety = pool.safety();
  result.pass = handle.valid() && result.late_rejected &&
                result.counters.submit_calls == 1 &&
                result.counters.cancel_calls == 1 &&
                result.counters.completion_calls == 0 &&
                result.counters.recycle_calls == 1 &&
                result.counters.live_requests == 0 &&
                result.counters.stale_completions == 1 &&
                result.counters.late_completions_after_cancel == 1 &&
                !result.safety.product_safe;
  pool.stop();
  return result;
}

}  // namespace

int main() {
  const auto first = run_shutdown_cycle();
  const auto restart = run_shutdown_cycle();
  const auto late = run_late_completion_rejection();

  const bool pass = first.pass && restart.pass && late.pass;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-usb-request-shutdown-contract.v1\",\n"
            << "  \"meaning\": \"offline DriverKit skeleton USB request stop/cancel/restart lifecycle; PASS is not physical USB readiness\",\n"
            << "  \"safety\": \"offline_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"inflight_requests_at_stop\": " << first.before_stop.live_requests << ",\n"
            << "  \"cancelled_requests\": " << first.after_stop.cancel_calls << ",\n"
            << "  \"live_requests_after_stop\": " << first.after_stop.live_requests << ",\n"
            << "  \"submit_calls\": " << first.after_stop.submit_calls << ",\n"
            << "  \"completion_calls\": " << first.after_stop.completion_calls << ",\n"
            << "  \"recycle_calls\": " << first.after_stop.recycle_calls << ",\n"
            << "  \"fallback_allocations\": " << first.after_stop.fallback_allocations << ",\n"
            << "  \"invalid_completions\": " << first.after_stop.invalid_completions << ",\n"
            << "  \"stale_completions\": " << first.after_stop.stale_completions << ",\n"
            << "  \"late_completions_after_cancel\": "
            << first.after_stop.late_completions_after_cancel << ",\n"
            << "  \"submitted_bytes\": " << first.after_stop.submitted_bytes << ",\n"
            << "  \"completed_bytes\": " << first.after_stop.completed_bytes << ",\n"
            << "  \"cancelled_bytes\": " << first.after_stop.cancelled_bytes << ",\n"
            << "  \"submitted_frames\": " << first.after_stop.submitted_frames << ",\n"
            << "  \"completed_frames\": " << first.after_stop.completed_frames << ",\n"
            << "  \"cancelled_frames\": " << first.after_stop.cancelled_frames << ",\n"
            << "  \"shutdown_safe\": " << (first.after_stop_safety.product_safe ? "true" : "false")
            << ",\n"
            << "  \"restart_after_cancel_safe\": " << (restart.pass ? "true" : "false")
            << ",\n"
            << "  \"late_completion_rejected\": " << (late.late_rejected ? "true" : "false")
            << ",\n"
            << "  \"late_completion_stale_completions\": "
            << late.counters.stale_completions << ",\n"
            << "  \"late_completion_after_cancel_count\": "
            << late.counters.late_completions_after_cancel << ",\n"
            << "  \"late_completion_product_safe\": "
            << (late.safety.product_safe ? "true" : "false") << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
