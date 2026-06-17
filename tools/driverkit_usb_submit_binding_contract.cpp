#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"
#include "opena8djcpp/mode2_packet.hpp"

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
    const auto pair = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    const auto base = static_cast<std::int32_t>((family + 1U) * 400000U);
    const auto motion =
        static_cast<std::int32_t>((frame_index * 101U) + (channel * 503U));
    frame[channel] = side == 0 ? base + motion + static_cast<std::int32_t>(pair)
                               : -base - motion - static_cast<std::int32_t>(pair);
  }
  return frame;
}

std::vector<S24Frame> payload_frames_for_descriptor(const UsbSubmitDescriptor& descriptor) {
  std::vector<S24Frame> frames;
  frames.reserve(static_cast<std::size_t>(descriptor.frame_count));
  constexpr std::uint64_t kFramesPerSlot = 8;
  const auto first_frame = descriptor.first_sequence * kFramesPerSlot;
  const auto family = descriptor.direction == UsbSlotDirection::Capture ? 17U : 19U;
  for (std::uint64_t offset = 0; offset < descriptor.frame_count; ++offset) {
    frames.push_back(frame_for(family, first_frame + offset));
  }
  return frames;
}

bool frames_equal(std::span<const S24Frame> left, std::span<const S24Frame> right) {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

std::uint64_t compare_prefix(std::span<const S24Frame> decoded,
                             std::span<const S24Frame> expected) {
  constexpr std::size_t kSourceOffset = kMode2DefaultStartByte == 0 ? 0U : 1U;
  const auto comparable =
      std::min(decoded.size(), expected.size() > kSourceOffset ? expected.size() - kSourceOffset : 0U);
  std::uint64_t mismatches = 0;
  for (std::size_t index = 0; index < comparable; ++index) {
    if (decoded[index] != expected[index + kSourceOffset]) {
      mismatches += 1;
    }
  }
  return mismatches;
}

struct BindingResult {
  std::uint64_t periods = 0;
  std::uint64_t transport_frame_mismatches = 0;
  std::uint64_t descriptors = 0;
  std::uint64_t capture_descriptors = 0;
  std::uint64_t playback_descriptors = 0;
  std::uint64_t logical_slots = 0;
  std::uint64_t usb_submit_calls = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t decoded_frames = 0;
  std::uint64_t check_errors = 0;
  std::uint64_t panic_flags = 0;
  std::uint64_t output_overflows = 0;
  std::uint64_t prefix_mismatches = 0;
  std::uint64_t descriptor_byte_mismatches = 0;
  std::uint64_t descriptor_frame_mismatches = 0;
  std::uint64_t direction_order_errors = 0;
  std::uint64_t timestamp_mismatches = 0;
  bool transport_safe = false;
  bool usb_submit_safe = false;
  bool stopped = false;
  bool pass = false;
};

BindingResult run_binding_contract() {
  constexpr std::uint32_t kIsoFrames = 8;
  constexpr std::uint32_t kPeriods = 256;
  BindingResult result{};
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
  };
  if (!driver.start_driver() || !driver.configure_stream(config) || !driver.start_stream()) {
    return result;
  }

  std::vector<S24Frame> playback(kIsoFrames);
  std::vector<S24Frame> capture(kIsoFrames);
  std::vector<S24Frame> backend_playback(kIsoFrames);
  std::vector<S24Frame> hal_capture(kIsoFrames);
  for (std::uint32_t period = 0; period < kPeriods; ++period) {
    for (std::uint32_t offset = 0; offset < kIsoFrames; ++offset) {
      const auto frame_index = static_cast<std::uint64_t>(period * kIsoFrames) + offset;
      playback[offset] = frame_for(23, frame_index);
      capture[offset] = frame_for(29, frame_index);
    }
    if (!driver.write_playback(playback) ||
        !driver.complete_backend_period(capture, backend_playback, (period + 1U) * kIsoFrames) ||
        driver.read_capture(hal_capture) != hal_capture.size()) {
      result.transport_frame_mismatches += kIsoFrames;
      continue;
    }
    if (!frames_equal(playback, backend_playback)) {
      result.transport_frame_mismatches += kIsoFrames;
    }
    if (!frames_equal(capture, hal_capture)) {
      result.transport_frame_mismatches += kIsoFrames;
    }
  }
  result.periods = kPeriods;
  driver.finish_usb_submit_binding();

  const auto counters = driver.usb_submit_counters();
  const auto descriptors = driver.usb_submit_descriptors();
  result.logical_slots = counters.logical_slots;
  result.usb_submit_calls = counters.usb_submit_calls;
  result.total_bytes = counters.total_bytes;
  result.total_frames = counters.total_frames;
  result.transport_safe = driver.transport_safety().product_safe;
  result.usb_submit_safe = driver.usb_submit_safety().product_safe;

  UsbSlotDirection expected_direction = UsbSlotDirection::Capture;
  for (const auto& descriptor : descriptors) {
    result.descriptors += 1;
    if (descriptor.direction == UsbSlotDirection::Capture) {
      result.capture_descriptors += 1;
    } else {
      result.playback_descriptors += 1;
    }
    if (descriptor.direction != expected_direction) {
      result.direction_order_errors += 1;
    }
    expected_direction = expected_direction == UsbSlotDirection::Capture
                             ? UsbSlotDirection::Playback
                             : UsbSlotDirection::Capture;

    const auto expected_timestamp = (descriptor.first_sequence + 1U) * kIsoFrames;
    const auto expected_frames =
        (descriptor.slot_count * config.usb_bytes_per_slot) / kMode2FullFrameBytes;
    const auto expected_bytes = descriptor.slot_count * config.usb_bytes_per_slot;
    if (descriptor.first_sample_timestamp != expected_timestamp) {
      result.timestamp_mismatches += 1;
    }
    if (descriptor.frame_count != expected_frames) {
      result.descriptor_frame_mismatches += 1;
    }
    if (descriptor.byte_count != expected_bytes) {
      result.descriptor_byte_mismatches += 1;
    }

    auto frames = payload_frames_for_descriptor(descriptor);
    std::vector<std::uint8_t> payload(static_cast<std::size_t>(descriptor.byte_count));
    Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
    const auto written = packer.fill_into(payload);
    if (written != payload.size()) {
      result.descriptor_byte_mismatches += 1;
      payload.resize(written);
    }
    std::vector<S24Frame> decoded(frames.size() + 16U);
    const auto decode = decode_mode2_usb_bytes_into(
        payload, kMode2DefaultStartByte, kMode2DefaultTransferBytes, decoded);
    decoded.resize(static_cast<std::size_t>(decode.stats.decoded_frames));
    result.decoded_frames += decode.stats.decoded_frames;
    result.check_errors += decode.stats.check_errors;
    result.panic_flags += decode.stats.panic_flags;
    result.output_overflows += decode.output_overflows;
    result.prefix_mismatches += compare_prefix(decoded, frames);
  }

  result.stopped = driver.stop_stream() && driver.stop_driver();
  result.pass = result.stopped && result.transport_safe && result.usb_submit_safe &&
                result.transport_frame_mismatches == 0 && result.periods == 256 &&
                result.logical_slots == 528 && result.usb_submit_calls == 66 &&
                result.descriptors == 66 && result.capture_descriptors == 33 &&
                result.playback_descriptors == 33 && result.total_bytes == 185856 &&
                result.total_frames == 5808 && result.check_errors == 0 &&
                result.panic_flags == 0 && result.output_overflows == 0 &&
                result.prefix_mismatches == 0 && result.descriptor_byte_mismatches == 0 &&
                result.descriptor_frame_mismatches == 0 &&
                result.direction_order_errors == 0 && result.timestamp_mismatches == 0;
  return result;
}

}  // namespace

int main() {
  const auto result = run_binding_contract();
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-usb-submit-binding-contract.v1\",\n"
            << "  \"meaning\": \"offline DriverKit skeleton to prepared USB submit payload binding; PASS is not dext install, physical USB, or product readiness\",\n"
            << "  \"safety\": \"offline_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"periods\": " << result.periods << ",\n"
            << "  \"transport_frame_mismatches\": " << result.transport_frame_mismatches
            << ",\n"
            << "  \"transport_safe\": " << (result.transport_safe ? "true" : "false")
            << ",\n"
            << "  \"usb_submit_safe\": " << (result.usb_submit_safe ? "true" : "false")
            << ",\n"
            << "  \"logical_slots\": " << result.logical_slots << ",\n"
            << "  \"usb_submit_calls\": " << result.usb_submit_calls << ",\n"
            << "  \"descriptors\": " << result.descriptors << ",\n"
            << "  \"capture_descriptors\": " << result.capture_descriptors << ",\n"
            << "  \"playback_descriptors\": " << result.playback_descriptors << ",\n"
            << "  \"total_bytes\": " << result.total_bytes << ",\n"
            << "  \"total_frames\": " << result.total_frames << ",\n"
            << "  \"decoded_frames\": " << result.decoded_frames << ",\n"
            << "  \"check_errors\": " << result.check_errors << ",\n"
            << "  \"panic_flags\": " << result.panic_flags << ",\n"
            << "  \"output_overflows\": " << result.output_overflows << ",\n"
            << "  \"prefix_mismatches\": " << result.prefix_mismatches << ",\n"
            << "  \"descriptor_byte_mismatches\": " << result.descriptor_byte_mismatches
            << ",\n"
            << "  \"descriptor_frame_mismatches\": " << result.descriptor_frame_mismatches
            << ",\n"
            << "  \"direction_order_errors\": " << result.direction_order_errors << ",\n"
            << "  \"timestamp_mismatches\": " << result.timestamp_mismatches << ",\n"
            << "  \"stopped\": " << (result.stopped ? "true" : "false") << ",\n"
            << "  \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return result.pass ? 0 : 1;
}
