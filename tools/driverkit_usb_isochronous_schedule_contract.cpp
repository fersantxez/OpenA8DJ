#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"
#include "opena8djcpp/usb_isochronous_schedule.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

S24Frame frame_for(std::uint32_t family, std::uint64_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto side = channel % kChannelsPerPair;
    const auto motion =
        static_cast<std::int32_t>((frame_index * 97U) + (channel * 431U));
    const auto base = static_cast<std::int32_t>((family + 1U) * 300000U);
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

std::vector<UsbSubmitDescriptor> driverkit_descriptors() {
  constexpr std::uint32_t kIsoFrames = 8;
  constexpr std::uint32_t kPeriods = 256;
  std::vector<UsbSubmitDescriptor> out;
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
      .usb_retain_submit_descriptors = true,
  };
  if (!driver.start_driver() || !driver.configure_stream(config) || !driver.start_stream()) {
    return out;
  }

  std::vector<S24Frame> playback(kIsoFrames);
  std::vector<S24Frame> capture(kIsoFrames);
  std::vector<S24Frame> backend_playback(kIsoFrames);
  for (std::uint32_t period = 0; period < kPeriods; ++period) {
    for (std::uint32_t offset = 0; offset < kIsoFrames; ++offset) {
      const auto index = static_cast<std::uint64_t>(period * kIsoFrames) + offset;
      playback[offset] = frame_for(41, index);
      capture[offset] = frame_for(43, index);
    }
    (void)driver.write_playback(playback);
    (void)driver.complete_backend_period(capture, backend_playback, (period + 1U) * kIsoFrames);
  }
  driver.finish_usb_submit_binding();
  const auto descriptors = driver.usb_submit_descriptors();
  out.assign(descriptors.begin(), descriptors.end());
  (void)driver.stop_stream();
  (void)driver.stop_driver();
  return out;
}

struct ScheduleResult {
  UsbIsochronousScheduleCounters counters{};
  UsbIsochronousScheduleSafety safety{};
  bool pass = false;
};

ScheduleResult run_schedule(std::span<const UsbSubmitDescriptor> descriptors,
                            const UsbIsochronousScheduleConfig& config) {
  ScheduleResult result{};
  UsbIsochronousSchedule schedule;
  if (!schedule.start(config)) {
    return result;
  }
  bool ok = true;
  for (const auto& descriptor : descriptors) {
    const auto submit_time = descriptor.first_sample_timestamp - config.min_submit_lead_frames;
    ok = schedule.schedule(descriptor, submit_time) && ok;
  }
  result.counters = schedule.counters();
  result.safety = schedule.safety();
  result.pass = ok && result.safety.product_safe;
  schedule.stop();
  return result;
}

ScheduleResult run_late_submit(std::span<const UsbSubmitDescriptor> descriptors,
                               const UsbIsochronousScheduleConfig& config) {
  ScheduleResult result{};
  UsbIsochronousSchedule schedule;
  if (!schedule.start(config) || descriptors.empty()) {
    return result;
  }
  const auto& descriptor = descriptors.front();
  const bool accepted = schedule.schedule(descriptor, descriptor.first_sample_timestamp);
  result.counters = schedule.counters();
  result.safety = schedule.safety();
  result.pass = !accepted && !result.safety.product_safe && result.counters.late_submits == 1;
  schedule.stop();
  return result;
}

ScheduleResult run_timestamp_regression(std::span<const UsbSubmitDescriptor> descriptors,
                                        const UsbIsochronousScheduleConfig& config) {
  ScheduleResult result{};
  UsbIsochronousSchedule schedule;
  if (!schedule.start(config) || descriptors.size() < 2) {
    return result;
  }
  const auto& first = descriptors[0];
  const bool first_ok =
      schedule.schedule(first, first.first_sample_timestamp - config.min_submit_lead_frames);
  const bool second_ok =
      schedule.schedule(first, first.first_sample_timestamp - config.min_submit_lead_frames);
  result.counters = schedule.counters();
  result.safety = schedule.safety();
  result.pass = first_ok && !second_ok && !result.safety.product_safe &&
                result.counters.timestamp_regressions == 1 &&
                result.counters.sequence_regressions == 1;
  schedule.stop();
  return result;
}

ScheduleResult run_bad_shape(std::span<const UsbSubmitDescriptor> descriptors,
                             const UsbIsochronousScheduleConfig& config) {
  ScheduleResult result{};
  UsbIsochronousSchedule schedule;
  if (!schedule.start(config) || descriptors.empty()) {
    return result;
  }
  auto descriptor = descriptors.front();
  descriptor.slot_count -= 1;
  const bool accepted =
      schedule.schedule(descriptor, descriptor.first_sample_timestamp - config.min_submit_lead_frames);
  result.counters = schedule.counters();
  result.safety = schedule.safety();
  result.pass = !accepted && !result.safety.product_safe &&
                result.counters.descriptor_shape_errors == 1;
  schedule.stop();
  return result;
}

void print_result(const char* prefix, const ScheduleResult& result) {
  const auto& counters = result.counters;
  const auto& safety = result.safety;
  std::cout << "  \"" << prefix << "_scheduled_descriptors\": "
            << counters.scheduled_descriptors << ",\n"
            << "  \"" << prefix << "_capture_descriptors\": " << counters.capture_descriptors
            << ",\n"
            << "  \"" << prefix << "_playback_descriptors\": " << counters.playback_descriptors
            << ",\n"
            << "  \"" << prefix << "_late_submits\": " << counters.late_submits << ",\n"
            << "  \"" << prefix << "_descriptor_shape_errors\": "
            << counters.descriptor_shape_errors << ",\n"
            << "  \"" << prefix << "_timestamp_regressions\": "
            << counters.timestamp_regressions << ",\n"
            << "  \"" << prefix << "_sequence_regressions\": " << counters.sequence_regressions
            << ",\n"
            << "  \"" << prefix << "_total_frames\": " << counters.total_frames << ",\n"
            << "  \"" << prefix << "_total_bytes\": " << counters.total_bytes << ",\n"
            << "  \"" << prefix << "_min_submit_lead_frames\": "
            << counters.min_submit_lead_frames << ",\n"
            << "  \"" << prefix << "_max_submit_lead_frames\": "
            << counters.max_submit_lead_frames << ",\n"
            << "  \"" << prefix << "_descriptor_shape_safe\": "
            << (safety.descriptor_shape_safe ? "true" : "false") << ",\n"
            << "  \"" << prefix << "_deadline_safe\": "
            << (safety.deadline_safe ? "true" : "false") << ",\n"
            << "  \"" << prefix << "_monotonic_safe\": "
            << (safety.monotonic_safe ? "true" : "false") << ",\n"
            << "  \"" << prefix << "_product_safe\": "
            << (safety.product_safe ? "true" : "false") << ",\n"
            << "  \"" << prefix << "_result\": \"" << (result.pass ? "PASS" : "FAIL")
            << "\",\n";
}

}  // namespace

int main() {
  const auto descriptors = driverkit_descriptors();
  constexpr UsbIsochronousScheduleConfig kConfig{
      .frames_per_slot = 8,
      .slots_per_submit = 8,
      .bytes_per_slot = kMode2DefaultTransferBytes,
      .min_submit_lead_frames = 8,
  };

  const auto stable = run_schedule(descriptors, kConfig);
  const auto late = run_late_submit(descriptors, kConfig);
  const auto regression = run_timestamp_regression(descriptors, kConfig);
  const auto bad_shape = run_bad_shape(descriptors, kConfig);

  const bool pass = descriptors.size() == 66 && stable.pass &&
                    stable.counters.scheduled_descriptors == 66 &&
                    stable.counters.capture_descriptors == 33 &&
                    stable.counters.playback_descriptors == 33 &&
                    stable.counters.late_submits == 0 &&
                    stable.counters.descriptor_shape_errors == 0 &&
                    stable.counters.timestamp_regressions == 0 &&
                    stable.counters.sequence_regressions == 0 &&
                    stable.counters.total_frames == 5808 &&
                    stable.counters.total_bytes == 185856 &&
                    stable.counters.min_submit_lead_frames == 8 &&
                    stable.counters.max_submit_lead_frames == 8 &&
                    late.pass && regression.pass && bad_shape.pass;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-usb-isochronous-schedule-contract.v1\",\n"
            << "  \"meaning\": \"offline DriverKit USB isochronous descriptor deadline contract; PASS is not physical USB, dext install, or product readiness\",\n"
            << "  \"safety\": \"offline_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"source_descriptors\": " << descriptors.size() << ",\n"
            << "  \"frames_per_slot\": " << kConfig.frames_per_slot << ",\n"
            << "  \"slots_per_submit\": " << kConfig.slots_per_submit << ",\n"
            << "  \"bytes_per_slot\": " << kConfig.bytes_per_slot << ",\n"
            << "  \"min_submit_lead_frames\": " << kConfig.min_submit_lead_frames << ",\n";
  print_result("stable", stable);
  print_result("late_submit_rejected", late);
  print_result("timestamp_regression_rejected", regression);
  print_result("bad_shape_rejected", bad_shape);
  std::cout
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"blocked_claim\": \"NO_DRIVERKIT_USB_CPU_OR_AUDIO_QUALITY_CLAIM_UNTIL_THIS_DEADLINE_MODEL_IS_BOUND_TO_REAL_USB_SUBMITS_AND_PHYSICAL_AB_PASSES\"\n"
      << "}\n";
  return pass ? 0 : 1;
}
