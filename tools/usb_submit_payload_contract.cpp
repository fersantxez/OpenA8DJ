#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/usb_submit_plan.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

S24Frame frame_for(UsbSlotDirection direction, std::uint64_t frame_index) {
  S24Frame frame{};
  const auto salt = direction == UsbSlotDirection::Capture ? 1000U : 5000U;
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto pair = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    const auto base = static_cast<std::int32_t>((pair + 1U) * 300000U);
    const auto motion =
        static_cast<std::int32_t>((frame_index * 97U) + (channel * 409U) + salt);
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

std::vector<S24Frame> frames_for_descriptor(const UsbSubmitDescriptor& descriptor) {
  std::vector<S24Frame> frames;
  frames.reserve(static_cast<std::size_t>(descriptor.frame_count));
  constexpr std::uint64_t kFramesPerSlot = 8;
  const auto first_frame = descriptor.first_sequence * kFramesPerSlot;
  for (std::uint64_t offset = 0; offset < descriptor.frame_count; ++offset) {
    frames.push_back(frame_for(descriptor.direction, first_frame + offset));
  }
  return frames;
}

std::uint32_t compare_prefix(std::span<const S24Frame> decoded,
                             std::span<const S24Frame> expected) {
  constexpr std::size_t kSourceOffset = kMode2DefaultStartByte == 0 ? 0U : 1U;
  const auto comparable =
      std::min(decoded.size(), expected.size() > kSourceOffset ? expected.size() - kSourceOffset : 0U);
  std::uint32_t mismatches = 0;
  for (std::size_t index = 0; index < comparable; ++index) {
    if (decoded[index] != expected[index + kSourceOffset]) {
      mismatches += 1;
    }
  }
  return mismatches;
}

struct PayloadResult {
  std::uint64_t descriptors = 0;
  std::uint64_t capture_descriptors = 0;
  std::uint64_t playback_descriptors = 0;
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
  bool pass = false;
};

PayloadResult run_payload_contract() {
  PreparedUsbSubmitPlanner planner;
  PayloadResult result{};
  const PreparedUsbSubmitPlannerConfig config{
      .slots_per_submit = 8,
      .frames_per_slot = 8,
      .bytes_per_slot = kMode2DefaultTransferBytes,
  };
  if (!planner.start(config)) {
    return result;
  }

  for (std::uint64_t sequence = 0; sequence < 264U; ++sequence) {
    const auto timestamp = (sequence + 1U) * config.frames_per_slot;
    (void)planner.queue_slot(UsbSlotDirection::Capture, timestamp);
    (void)planner.queue_slot(UsbSlotDirection::Playback, timestamp);
  }
  planner.finish();

  const auto descriptors = planner.descriptors();
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
        (descriptor.first_sequence + 1U) * config.frames_per_slot;
    if (descriptor.first_sample_timestamp != expected_timestamp) {
      result.timestamp_mismatches += 1;
    }
    const auto expected_frames =
        (descriptor.slot_count * config.bytes_per_slot) / kMode2FullFrameBytes;
    const auto expected_bytes = descriptor.slot_count * config.bytes_per_slot;
    if (descriptor.frame_count != expected_frames) {
      result.descriptor_frame_mismatches += 1;
    }
    if (descriptor.byte_count != expected_bytes) {
      result.descriptor_byte_mismatches += 1;
    }

    auto frames = frames_for_descriptor(descriptor);
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

    result.total_bytes += written;
    result.total_frames += descriptor.frame_count;
    result.decoded_frames += decode.stats.decoded_frames;
    result.check_errors += decode.stats.check_errors;
    result.panic_flags += decode.stats.panic_flags;
    result.output_overflows += decode.output_overflows;
    result.prefix_mismatches += compare_prefix(decoded, frames);
  }

  const auto counters = planner.counters();
  const auto safety = planner.safety();
  result.pass = safety.product_safe && result.descriptors == 66 &&
                result.capture_descriptors == 33 && result.playback_descriptors == 33 &&
                counters.logical_slots == 528 && counters.usb_submit_calls == 66 &&
                result.total_bytes == counters.total_bytes &&
                result.total_frames == counters.total_frames &&
                result.check_errors == 0 && result.panic_flags == 0 &&
                result.output_overflows == 0 && result.prefix_mismatches == 0 &&
                result.descriptor_byte_mismatches == 0 &&
                result.descriptor_frame_mismatches == 0 &&
                result.direction_order_errors == 0 && result.timestamp_mismatches == 0;
  return result;
}

}  // namespace

int main() {
  const auto result = run_payload_contract();
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.usb-submit-payload-contract.v1\",\n"
            << "  \"meaning\": \"offline batched USB submit payload contract using prepared descriptors and Mode2 pack/decode; PASS is not physical USB readiness\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
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
            << "  \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return result.pass ? 0 : 1;
}
