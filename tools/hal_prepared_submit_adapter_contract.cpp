#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/usb_request_pool.hpp"
#include "opena8djcpp/usb_submit_plan.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

struct HalPreparedSubmitAdapterConfig {
  std::uint32_t logical_iso_frames = 8;
  std::uint32_t capture_iso_frames = 8;
  std::uint32_t playback_base_iso_frames = 8;
  std::uint32_t playback_coalesce_transfers = 1;
  std::uint32_t slots_per_submit = 8;
  std::uint32_t request_slots = 4;
  std::uint32_t periods = 256;
  std::uint32_t initial_capture_slots = 8;
  std::uint32_t initial_playback_slots = 8;
};

struct AdapterResult {
  std::uint64_t logical_slots = 0;
  std::uint64_t capture_logical_slots = 0;
  std::uint64_t playback_logical_slots = 0;
  std::uint64_t descriptors = 0;
  std::uint64_t capture_descriptors = 0;
  std::uint64_t playback_descriptors = 0;
  std::uint64_t usb_submit_calls = 0;
  std::uint64_t partial_submit_calls = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t total_frames = 0;
  std::uint64_t decoded_frames = 0;
  std::uint64_t request_submit_calls = 0;
  std::uint64_t request_completion_calls = 0;
  std::uint64_t request_recycle_calls = 0;
  std::uint64_t max_live_requests = 0;
  std::uint64_t fallback_allocations = 0;
  std::uint64_t check_errors = 0;
  std::uint64_t panic_flags = 0;
  std::uint64_t output_overflows = 0;
  std::uint64_t prefix_mismatches = 0;
  std::uint64_t descriptor_byte_mismatches = 0;
  std::uint64_t descriptor_frame_mismatches = 0;
  std::uint64_t direction_order_errors = 0;
  std::uint64_t timestamp_mismatches = 0;
  std::uint64_t sequence_mismatches = 0;
  bool planner_safe = false;
  bool request_pool_safe = false;
  bool hal_geometry_preserved = false;
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

void complete_when_needed(PreparedUsbRequestPool& request_pool,
                          std::vector<PreparedUsbRequestHandle>& live,
                          std::uint32_t max_live) {
  while (live.size() >= max_live) {
    (void)request_pool.complete(live.front());
    live.erase(live.begin());
  }
}

AdapterResult run_adapter_contract(const HalPreparedSubmitAdapterConfig& config) {
  AdapterResult result{};
  PreparedUsbSubmitPlanner planner;
  PreparedUsbRequestPool request_pool;
  if (!planner.start(PreparedUsbSubmitPlannerConfig{
          .slots_per_submit = config.slots_per_submit,
          .frames_per_slot = config.logical_iso_frames,
          .bytes_per_slot = kMode2DefaultTransferBytes,
      })) {
    return result;
  }
  if (!request_pool.start(PreparedUsbRequestPoolConfig{
          .request_slots = config.request_slots,
      })) {
    return result;
  }

  for (std::uint64_t sequence = 0; sequence < config.initial_capture_slots; ++sequence) {
    (void)planner.queue_slot(UsbSlotDirection::Capture,
                             (sequence + 1U) * config.logical_iso_frames);
  }
  for (std::uint64_t sequence = 0; sequence < config.initial_playback_slots; ++sequence) {
    (void)planner.queue_slot(UsbSlotDirection::Playback,
                             (sequence + 1U) * config.logical_iso_frames);
  }
  std::uint64_t next_capture_sequence = config.initial_capture_slots;
  std::uint64_t next_playback_sequence = config.initial_playback_slots;
  for (std::uint32_t period = 0; period < config.periods; ++period) {
    (void)planner.queue_slot_with_sequence(
        UsbSlotDirection::Capture, next_capture_sequence,
        (next_capture_sequence + 1U) * config.logical_iso_frames);
    (void)planner.queue_slot_with_sequence(
        UsbSlotDirection::Playback, next_playback_sequence,
        (next_playback_sequence + 1U) * config.logical_iso_frames);
    next_capture_sequence += 1;
    next_playback_sequence += 1;
  }
  planner.finish();

  const auto planner_counters = planner.counters();
  result.logical_slots = planner_counters.logical_slots;
  result.capture_logical_slots = planner_counters.capture_logical_slots;
  result.playback_logical_slots = planner_counters.playback_logical_slots;
  result.usb_submit_calls = planner_counters.usb_submit_calls;
  result.partial_submit_calls = planner_counters.partial_submit_calls;
  result.total_bytes = planner_counters.total_bytes;
  result.total_frames = planner_counters.total_frames;

  std::vector<PreparedUsbRequestHandle> live;
  live.reserve(config.request_slots);
  UsbSlotDirection expected_direction = UsbSlotDirection::Capture;
  for (const auto& descriptor : planner.descriptors()) {
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
        (descriptor.first_sequence + 1U) * config.logical_iso_frames;
    const auto expected_frames =
        (descriptor.slot_count * kMode2DefaultTransferBytes) / kMode2FullFrameBytes;
    const auto expected_bytes = descriptor.slot_count * kMode2DefaultTransferBytes;
    if (descriptor.first_sample_timestamp != expected_timestamp) {
      result.timestamp_mismatches += 1;
    }
    if (descriptor.frame_count != expected_frames) {
      result.descriptor_frame_mismatches += 1;
    }
    if (descriptor.byte_count != expected_bytes) {
      result.descriptor_byte_mismatches += 1;
    }
    if ((descriptor.direction == UsbSlotDirection::Capture &&
         descriptor.first_sequence % config.slots_per_submit != 0U) ||
        (descriptor.direction == UsbSlotDirection::Playback &&
         descriptor.first_sequence % config.slots_per_submit != 0U)) {
      result.sequence_mismatches += 1;
    }

    auto frames = frames_for_descriptor(descriptor, config.logical_iso_frames);
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

    complete_when_needed(request_pool, live, config.request_slots);
    auto handle = request_pool.submit(descriptor);
    if (!handle.valid()) {
      result.fallback_allocations += 1;
    } else {
      live.push_back(handle);
    }
  }
  for (auto handle : live) {
    (void)request_pool.complete(handle);
  }

  const auto request_counters = request_pool.counters();
  result.request_submit_calls = request_counters.submit_calls;
  result.request_completion_calls = request_counters.completion_calls;
  result.request_recycle_calls = request_counters.recycle_calls;
  result.max_live_requests = request_counters.max_live_requests;
  result.fallback_allocations += request_counters.fallback_allocations;
  result.planner_safe = planner.safety().product_safe;
  result.request_pool_safe = request_pool.safety().product_safe;
  result.hal_geometry_preserved =
      config.logical_iso_frames == 8 && config.capture_iso_frames == 8 &&
      config.playback_base_iso_frames == 8 && config.playback_coalesce_transfers == 1 &&
      config.slots_per_submit == 8;
  result.payload_equivalent =
      result.check_errors == 0 && result.panic_flags == 0 && result.output_overflows == 0 &&
      result.prefix_mismatches == 0 && result.descriptor_byte_mismatches == 0 &&
      result.descriptor_frame_mismatches == 0;
  result.pass =
      result.planner_safe && result.request_pool_safe && result.hal_geometry_preserved &&
      result.payload_equivalent && result.logical_slots == 528 && result.capture_logical_slots == 264 &&
      result.playback_logical_slots == 264 && result.usb_submit_calls == 66 &&
      result.partial_submit_calls == 0 && result.descriptors == 66 &&
      result.capture_descriptors == 33 && result.playback_descriptors == 33 &&
      result.total_bytes == 185856 && result.total_frames == 5808 &&
      result.request_submit_calls == 66 && result.request_completion_calls == 66 &&
      result.request_recycle_calls == 66 && result.max_live_requests <= 4 &&
      result.fallback_allocations == 0 && result.direction_order_errors == 0 &&
      result.timestamp_mismatches == 0 && result.sequence_mismatches == 0;
  return result;
}

}  // namespace

int main() {
  const HalPreparedSubmitAdapterConfig config{};
  const auto result = run_adapter_contract(config);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.hal-prepared-submit-adapter-contract.v1\",\n"
            << "  \"meaning\": \"offline HAL geometry to prepared USB submit adapter contract; PASS is not physical USB, HAL install, DriverKit install, or product readiness\",\n"
            << "  \"safety\": \"offline_model_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"logical_iso_frames\": " << config.logical_iso_frames << ",\n"
            << "  \"capture_iso_frames\": " << config.capture_iso_frames << ",\n"
            << "  \"playback_base_iso_frames\": " << config.playback_base_iso_frames << ",\n"
            << "  \"playback_coalesce_transfers\": " << config.playback_coalesce_transfers << ",\n"
            << "  \"slots_per_submit\": " << config.slots_per_submit << ",\n"
            << "  \"logical_slots\": " << result.logical_slots << ",\n"
            << "  \"capture_logical_slots\": " << result.capture_logical_slots << ",\n"
            << "  \"playback_logical_slots\": " << result.playback_logical_slots << ",\n"
            << "  \"usb_submit_calls\": " << result.usb_submit_calls << ",\n"
            << "  \"partial_submit_calls\": " << result.partial_submit_calls << ",\n"
            << "  \"descriptors\": " << result.descriptors << ",\n"
            << "  \"capture_descriptors\": " << result.capture_descriptors << ",\n"
            << "  \"playback_descriptors\": " << result.playback_descriptors << ",\n"
            << "  \"total_bytes\": " << result.total_bytes << ",\n"
            << "  \"total_frames\": " << result.total_frames << ",\n"
            << "  \"decoded_frames\": " << result.decoded_frames << ",\n"
            << "  \"request_submit_calls\": " << result.request_submit_calls << ",\n"
            << "  \"request_completion_calls\": " << result.request_completion_calls << ",\n"
            << "  \"request_recycle_calls\": " << result.request_recycle_calls << ",\n"
            << "  \"max_live_requests\": " << result.max_live_requests << ",\n"
            << "  \"fallback_allocations\": " << result.fallback_allocations << ",\n"
            << "  \"check_errors\": " << result.check_errors << ",\n"
            << "  \"panic_flags\": " << result.panic_flags << ",\n"
            << "  \"output_overflows\": " << result.output_overflows << ",\n"
            << "  \"prefix_mismatches\": " << result.prefix_mismatches << ",\n"
            << "  \"descriptor_byte_mismatches\": " << result.descriptor_byte_mismatches << ",\n"
            << "  \"descriptor_frame_mismatches\": " << result.descriptor_frame_mismatches << ",\n"
            << "  \"direction_order_errors\": " << result.direction_order_errors << ",\n"
            << "  \"timestamp_mismatches\": " << result.timestamp_mismatches << ",\n"
            << "  \"sequence_mismatches\": " << result.sequence_mismatches << ",\n"
            << "  \"planner_safe\": " << (result.planner_safe ? "true" : "false") << ",\n"
            << "  \"request_pool_safe\": " << (result.request_pool_safe ? "true" : "false") << ",\n"
            << "  \"hal_geometry_preserved\": "
            << (result.hal_geometry_preserved ? "true" : "false") << ",\n"
            << "  \"payload_equivalent\": " << (result.payload_equivalent ? "true" : "false")
            << ",\n"
            << "  \"result\": \"" << (result.pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"blocked_claim\": \"NO_RUNTIME_CPU_OR_PRODUCT_CLAIM_UNTIL_THIS_ADAPTER_IS_BOUND_TO_REAL_HAL_OR_DRIVERKIT_USB_AND_SAME_SESSION_PHYSICAL_AB_PASSES\"\n"
            << "}\n";
  return result.pass ? 0 : 1;
}
