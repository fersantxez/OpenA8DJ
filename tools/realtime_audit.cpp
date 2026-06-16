#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/audio_ring.hpp"
#include "opena8djcpp/routing.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

using namespace opena8djcpp;

namespace {

std::atomic<bool> g_count_allocations{false};
std::atomic<std::uint64_t> g_allocations{0};

S24Frame make_frame(std::uint32_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto stream = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    auto magnitude = static_cast<std::int32_t>(((stream + 1U) * 1000000U) +
                                              (side * 250000U) +
                                              ((frame_index % 8192U) * 257U));
    frame[channel] = side == 0 ? magnitude : -magnitude;
  }
  return frame;
}

}  // namespace

void* operator new(std::size_t size) {
  if (g_count_allocations.load(std::memory_order_relaxed)) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
  }
  if (void* ptr = std::malloc(size)) {
    return ptr;
  }
  throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
  if (g_count_allocations.load(std::memory_order_relaxed)) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
  }
  if (void* ptr = std::malloc(size)) {
    return ptr;
  }
  throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept {
  std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
  std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
  std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
  std::free(ptr);
}

int main() {
  constexpr std::uint32_t kFrames = 4096;
  constexpr std::uint32_t kTransferCount = 256;
  constexpr std::uint32_t kTransferBytes = kMode2DefaultTransferBytes;

  std::vector<S24Frame> frames;
  frames.reserve(kFrames);
  for (std::uint32_t index = 0; index < kFrames; ++index) {
    frames.push_back(make_frame(index));
  }

  std::vector<std::uint8_t> packed(static_cast<std::size_t>(kTransferCount) * kTransferBytes);
  std::vector<S24Frame> decoded(kFrames + 1024);
  std::vector<S24Frame> ring_output(kFrames + 1024);
  std::vector<float> route_input(static_cast<std::size_t>(kFrames) * kInputChannels, 0.0F);
  std::vector<float> route_output(static_cast<std::size_t>(kFrames) * kOutputChannels, 0.0F);
  for (std::size_t index = 0; index < route_input.size(); ++index) {
    route_input[index] = static_cast<float>((index % 1024U) - 512U) / 512.0F;
  }

  Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
  g_allocations.store(0, std::memory_order_relaxed);
  g_count_allocations.store(true, std::memory_order_relaxed);

  for (std::uint32_t transfer = 0; transfer < kTransferCount; ++transfer) {
    const auto offset = static_cast<std::size_t>(transfer) * kTransferBytes;
    const auto written = packer.fill_into(
        std::span<std::uint8_t>(packed.data() + offset, static_cast<std::size_t>(kTransferBytes)));
    if (written != kTransferBytes) {
      g_count_allocations.store(false, std::memory_order_relaxed);
      std::cerr << "short pack write\n";
      return 1;
    }
  }

  const auto decode = decode_mode2_usb_bytes_into(packed, kMode2DefaultStartByte, kTransferBytes,
                                                  decoded);
  const RoutingPlan identity_plan(RoutingMatrix::identity());
  const bool routed =
      route_interleaved_f32(route_input, route_output, kFrames, identity_plan);
  SpscFrameRing<S24Frame, 8192> capture_ring;
  const auto ring_pushed =
      capture_ring.push_many(std::span<const S24Frame>(decoded.data(),
                                                       static_cast<std::size_t>(
                                                           decode.stats.decoded_frames)));
  const auto ring_popped = capture_ring.pop_many(ring_output);

  g_count_allocations.store(false, std::memory_order_relaxed);

  const auto allocations = g_allocations.load(std::memory_order_relaxed);
  const bool pass = allocations == 0 && decode.output_overflows == 0 &&
                    decode.stats.check_errors == 0 && decode.stats.panic_flags == 0 && routed &&
                    ring_pushed == decode.stats.decoded_frames &&
                    ring_popped == decode.stats.decoded_frames;

  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"hot_path_allocations\": " << allocations << ",\n"
            << "  \"decode_output_overflows\": " << decode.output_overflows << ",\n"
            << "  \"decoded_frames\": " << decode.stats.decoded_frames << ",\n"
            << "  \"check_errors\": " << decode.stats.check_errors << ",\n"
            << "  \"panic_flags\": " << decode.stats.panic_flags << ",\n"
            << "  \"ring_pushed_frames\": " << ring_pushed << ",\n"
            << "  \"ring_popped_frames\": " << ring_popped << ",\n"
            << "  \"ring_remaining_frames\": " << capture_ring.readable() << ",\n"
            << "  \"routing_ok\": " << (routed ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
