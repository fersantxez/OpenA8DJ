#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/prepared_usb_runtime_submit.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

struct ContractResult {
  std::uint64_t descriptors = 0;
  std::uint64_t capture_descriptors = 0;
  std::uint64_t playback_descriptors = 0;
  std::uint64_t decoded_frames = 0;
  std::uint64_t check_errors = 0;
  std::uint64_t panic_flags = 0;
  std::uint64_t output_overflows = 0;
  std::uint64_t prefix_mismatches = 0;
  std::uint64_t descriptor_byte_mismatches = 0;
  std::uint64_t descriptor_frame_mismatches = 0;
  std::uint64_t direction_order_errors = 0;
  std::uint64_t timestamp_mismatches = 0;
  std::uint64_t sequence_mismatches = 0;
  bool runtime_safe = false;
  bool payload_equivalent = false;
  bool pass = false;
};

S24Frame frame_for(UsbSlotDirection direction, std::uint64_t frame_index) {
  S24Frame frame{};
  const auto salt = direction == UsbSlotDirection::Capture ? 17000U : 29000U;
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto pair = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    const auto base = static_cast<std::int32_t>((pair + 1U) * 250000U);
    const auto motion =
        static_cast<std::int32_t>((frame_index * 113U) + (channel * 613U) + salt);
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

std::vector<S24Frame> frames_for_descriptor(const UsbSubmitDescriptor& descriptor,
                                            std::uint32_t frames_per_slot) {
  std::vector<S24Frame> frames;
  frames.reserve(static_cast<std::size_t>(descriptor.frame_count));
  const auto first_frame = descriptor.first_sequence * frames_per_slot;
  for (std::uint64_t offset = 0; offset < descriptor.frame_count; ++offset) {
    frames.push_back(frame_for(descriptor.direction, first_frame + offset));
  }
  return frames;
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

ContractResult inspect_payloads(std::span<const UsbSubmitDescriptor> descriptors,
                                const PreparedUsbRuntimeSubmitterConfig& config) {
  ContractResult result{};
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

    const auto expected_timestamp =
        (descriptor.first_sequence + 1U) * config.planner.frames_per_slot;
    const auto expected_frames =
        (descriptor.slot_count * config.planner.bytes_per_slot) / kMode2FullFrameBytes;
    const auto expected_bytes = descriptor.slot_count * config.planner.bytes_per_slot;
    if (descriptor.first_sample_timestamp != expected_timestamp) {
      result.timestamp_mismatches += 1;
    }
    if (descriptor.frame_count != expected_frames) {
      result.descriptor_frame_mismatches += 1;
    }
    if (descriptor.byte_count != expected_bytes) {
      result.descriptor_byte_mismatches += 1;
    }
    if (descriptor.first_sequence % config.planner.slots_per_submit != 0U) {
      result.sequence_mismatches += 1;
    }

    auto frames = frames_for_descriptor(descriptor, config.planner.frames_per_slot);
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
  result.payload_equivalent =
      result.check_errors == 0 && result.panic_flags == 0 && result.output_overflows == 0 &&
      result.prefix_mismatches == 0 && result.descriptor_byte_mismatches == 0 &&
      result.descriptor_frame_mismatches == 0 && result.direction_order_errors == 0 &&
      result.timestamp_mismatches == 0 && result.sequence_mismatches == 0;
  return result;
}

}  // namespace

int main() {
  const PreparedUsbRuntimeSubmitterConfig config{
      .planner =
          PreparedUsbSubmitPlannerConfig{
              .slots_per_submit = 8,
              .frames_per_slot = 8,
              .bytes_per_slot = kMode2DefaultTransferBytes,
          },
      .request_pool = PreparedUsbRequestPoolConfig{.request_slots = 4},
      .max_live_requests = 4,
      .retain_submitted_descriptors = true,
  };

  PreparedUsbRuntimeSubmitter submitter;
  if (!submitter.start(config)) {
    std::cout << "{\"schema\":\"opena8djcpp.prepared-usb-runtime-submit-contract.v1\","
              << "\"result\":\"FAIL\",\"start_failed\":true}\n";
    return 1;
  }

  constexpr std::uint32_t kInitialSlots = 8;
  constexpr std::uint32_t kPeriods = 256;
  for (std::uint64_t sequence = 0; sequence < kInitialSlots; ++sequence) {
    (void)submitter.queue_slot(UsbSlotDirection::Capture, (sequence + 1U) * 8U);
  }
  const auto first_descriptor_snapshot = submitter.counters();
  for (std::uint64_t sequence = 0; sequence < kInitialSlots; ++sequence) {
    (void)submitter.queue_slot(UsbSlotDirection::Playback, (sequence + 1U) * 8U);
  }
  std::uint64_t next_capture_sequence = kInitialSlots;
  std::uint64_t next_playback_sequence = kInitialSlots;
  for (std::uint32_t period = 0; period < kPeriods; ++period) {
    (void)submitter.queue_slot_with_sequence(
        UsbSlotDirection::Capture, next_capture_sequence,
        (next_capture_sequence + 1U) * 8U);
    (void)submitter.queue_slot_with_sequence(
        UsbSlotDirection::Playback, next_playback_sequence,
        (next_playback_sequence + 1U) * 8U);
    next_capture_sequence += 1;
    next_playback_sequence += 1;
  }
  submitter.finish();

  auto result = inspect_payloads(submitter.submitted_descriptors(), config);
  const auto counters = submitter.counters();
  result.runtime_safe = submitter.safety().product_safe;
  result.pass = result.runtime_safe && result.payload_equivalent &&
                first_descriptor_snapshot.logical_slots == 8 &&
                first_descriptor_snapshot.usb_submit_calls == 1 &&
                first_descriptor_snapshot.request_submit_calls == 1 &&
                first_descriptor_snapshot.max_live_requests == 1 &&
                counters.logical_slots == 528 && counters.capture_logical_slots == 264 &&
                counters.playback_logical_slots == 264 && counters.usb_submit_calls == 66 &&
                counters.partial_submit_calls == 0 &&
                counters.descriptors_submitted == 66 &&
                counters.capture_descriptors_submitted == 33 &&
                counters.playback_descriptors_submitted == 33 &&
                counters.total_bytes == 185856 && counters.total_frames == 5808 &&
                counters.request_submit_calls == 66 &&
                counters.request_completion_calls == 66 &&
                counters.request_recycle_calls == 66 && counters.max_live_requests <= 4 &&
                counters.fallback_allocations == 0 && counters.submit_failures == 0 &&
                counters.retained_descriptor_overflows == 0 &&
                result.descriptors == 66 && result.capture_descriptors == 33 &&
                result.playback_descriptors == 33;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-usb-runtime-submit-contract.v1\",\n"
            << "  \"meaning\": \"offline reusable core runtime submitter contract; PASS is not physical USB, HAL install, DriverKit install, or product readiness\",\n"
            << "  \"safety\": \"offline_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"slots_per_submit\": " << config.planner.slots_per_submit << ",\n"
            << "  \"logical_slots\": " << counters.logical_slots << ",\n"
            << "  \"capture_logical_slots\": " << counters.capture_logical_slots << ",\n"
            << "  \"playback_logical_slots\": " << counters.playback_logical_slots << ",\n"
            << "  \"usb_submit_calls\": " << counters.usb_submit_calls << ",\n"
            << "  \"first_descriptor_snapshot_logical_slots\": "
            << first_descriptor_snapshot.logical_slots << ",\n"
            << "  \"first_descriptor_snapshot_usb_submit_calls\": "
            << first_descriptor_snapshot.usb_submit_calls << ",\n"
            << "  \"first_descriptor_snapshot_request_submit_calls\": "
            << first_descriptor_snapshot.request_submit_calls << ",\n"
            << "  \"first_descriptor_snapshot_max_live_requests\": "
            << first_descriptor_snapshot.max_live_requests << ",\n"
            << "  \"partial_submit_calls\": " << counters.partial_submit_calls << ",\n"
            << "  \"usb_submit_reduction_ratio\": " << counters.usb_submit_reduction_ratio << ",\n"
            << "  \"descriptors\": " << result.descriptors << ",\n"
            << "  \"capture_descriptors\": " << result.capture_descriptors << ",\n"
            << "  \"playback_descriptors\": " << result.playback_descriptors << ",\n"
            << "  \"total_bytes\": " << counters.total_bytes << ",\n"
            << "  \"total_frames\": " << counters.total_frames << ",\n"
            << "  \"decoded_frames\": " << result.decoded_frames << ",\n"
            << "  \"request_submit_calls\": " << counters.request_submit_calls << ",\n"
            << "  \"request_completion_calls\": " << counters.request_completion_calls << ",\n"
            << "  \"request_recycle_calls\": " << counters.request_recycle_calls << ",\n"
            << "  \"max_live_requests\": " << counters.max_live_requests << ",\n"
            << "  \"fallback_allocations\": " << counters.fallback_allocations << ",\n"
            << "  \"submit_failures\": " << counters.submit_failures << ",\n"
            << "  \"retained_descriptor_overflows\": "
            << counters.retained_descriptor_overflows << ",\n"
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
            << "  \"sequence_mismatches\": " << result.sequence_mismatches << ",\n"
            << "  \"runtime_safe\": " << (result.runtime_safe ? "true" : "false") << ",\n"
            << "  \"payload_equivalent\": " << (result.payload_equivalent ? "true" : "false")
            << ",\n"
            << "  \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"blocked_claim\": \"NO_RUNTIME_CPU_OR_PRODUCT_CLAIM_UNTIL_CORE_SUBMITTER_IS_BOUND_TO_REAL_USB_AND_SAME_SESSION_PHYSICAL_AB_PASSES\"\n"
            << "}\n";
  return result.pass ? 0 : 1;
}
