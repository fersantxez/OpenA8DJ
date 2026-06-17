#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/prepared_transport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace opena8djcpp;

namespace {

S24Frame synthetic_frame(std::uint32_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto pair = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    const auto base = static_cast<std::int32_t>((pair + 1U) * 900000U);
    const auto motion = static_cast<std::int32_t>((frame_index * 257U) + (channel * 409U));
    frame[channel] = side == 0 ? base + motion : -base - motion;
  }
  return frame;
}

std::vector<S24Frame> make_frames(std::uint32_t count, std::uint32_t offset) {
  std::vector<S24Frame> frames;
  frames.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    frames.push_back(synthetic_frame(index + offset));
  }
  return frames;
}

std::uint32_t compare_decoded_prefix(std::span<const S24Frame> decoded,
                                     std::span<const S24Frame> expected,
                                     std::uint32_t start_byte) {
  const std::size_t source_offset = start_byte == 0 ? 0U : 1U;
  const auto comparable =
      std::min(decoded.size(), expected.size() > source_offset ? expected.size() - source_offset : 0U);
  std::uint32_t mismatches = 0;
  for (std::size_t index = 0; index < comparable; ++index) {
    if (decoded[index] != expected[index + source_offset]) {
      mismatches += 1;
    }
  }
  return mismatches;
}

}  // namespace

int main() {
  constexpr std::uint32_t kSourceFrames = 256;
  constexpr std::uint32_t kTransfers = 12;
  constexpr std::uint32_t kUsbBytes = kMode2DefaultTransferBytes * kTransfers;
  constexpr std::uint32_t kStartByte = kMode2DefaultStartByte;

  const auto capture_source = make_frames(kSourceFrames, 0);
  const auto playback_source = make_frames(kSourceFrames, 1000);

  std::vector<std::uint8_t> capture_usb(kUsbBytes);
  Mode2OutputPacker capture_packer(capture_source, kStartByte);
  const auto capture_usb_written = capture_packer.fill_into(capture_usb);

  std::vector<S24Frame> decoded_capture(kSourceFrames);
  const auto capture_decode = decode_mode2_usb_bytes_into(
      capture_usb, kStartByte, kMode2DefaultTransferBytes, decoded_capture);
  decoded_capture.resize(static_cast<std::size_t>(capture_decode.stats.decoded_frames));

  PreparedTransportBackend transport;
  const bool started = transport.start(PreparedTransportConfig{.iso_frames = 8,
                                                              .capture_slots = 64,
                                                              .playback_slots = 64});
  const auto playback_written = transport.hal_write_playback(
      std::span<const S24Frame>(playback_source.data(), decoded_capture.size()));
  std::vector<S24Frame> backend_playback(decoded_capture.size());
  const bool completed =
      transport.backend_complete_period(decoded_capture, backend_playback, 8);
  std::vector<S24Frame> hal_capture(decoded_capture.size());
  const auto hal_capture_read = transport.hal_read_capture(hal_capture);

  std::vector<std::uint8_t> playback_usb(kUsbBytes);
  Mode2OutputPacker playback_packer(backend_playback, kStartByte);
  const auto playback_usb_written = playback_packer.fill_into(playback_usb);
  std::vector<S24Frame> decoded_playback(kSourceFrames);
  const auto playback_decode = decode_mode2_usb_bytes_into(
      playback_usb, kStartByte, kMode2DefaultTransferBytes, decoded_playback);
  decoded_playback.resize(static_cast<std::size_t>(playback_decode.stats.decoded_frames));

  const auto capture_prefix_mismatches =
      compare_decoded_prefix(decoded_capture, capture_source, kStartByte);
  const auto playback_prefix_mismatches =
      compare_decoded_prefix(decoded_playback, backend_playback, kStartByte);
  const bool capture_roundtrip_ok = capture_prefix_mismatches == 0 && hal_capture == decoded_capture;
  const bool playback_roundtrip_ok = playback_prefix_mismatches == 0;
  const auto counters = transport.counters();
  const auto safety = transport.safety();

  const bool pass = started && completed && capture_usb_written == capture_usb.size() &&
                    playback_usb_written == playback_usb.size() &&
                    capture_decode.stats.decoded_frames > 0 &&
                    capture_decode.stats.check_errors == 0 &&
                    capture_decode.stats.panic_flags == 0 &&
                    capture_decode.output_overflows == 0 &&
                    playback_decode.stats.decoded_frames > 0 &&
                    playback_decode.stats.check_errors == 0 &&
                    playback_decode.stats.panic_flags == 0 &&
                    playback_decode.output_overflows == 0 &&
                    playback_written == decoded_capture.size() &&
                    hal_capture_read == decoded_capture.size() &&
                    capture_roundtrip_ok && playback_roundtrip_ok &&
                    counters.hal_steady_requeues == 0 && counters.fallback_allocations == 0 &&
                    counters.capture_ring_overruns == 0 &&
                    counters.capture_ring_underruns == 0 &&
                    counters.playback_ring_overruns == 0 &&
                    counters.playback_ring_underruns == 0 &&
                    counters.timestamp_regressions == 0 &&
                    counters.channel_identity_failures == 0 && safety.product_safe;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-transport-packet-contract.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"offline packet/ring contract using Mode2 pack/decode and PreparedTransportBackend; PASS does not mean physical readiness\",\n"
            << "  \"start_byte\": " << kStartByte << ",\n"
            << "  \"transfer_bytes\": " << kMode2DefaultTransferBytes << ",\n"
            << "  \"transfers\": " << kTransfers << ",\n"
            << "  \"capture_usb_written\": " << capture_usb_written << ",\n"
            << "  \"playback_usb_written\": " << playback_usb_written << ",\n"
            << "  \"capture_decoded_frames\": " << capture_decode.stats.decoded_frames << ",\n"
            << "  \"playback_decoded_frames\": " << playback_decode.stats.decoded_frames << ",\n"
            << "  \"capture_check_errors\": " << capture_decode.stats.check_errors << ",\n"
            << "  \"playback_check_errors\": " << playback_decode.stats.check_errors << ",\n"
            << "  \"capture_panic_flags\": " << capture_decode.stats.panic_flags << ",\n"
            << "  \"playback_panic_flags\": " << playback_decode.stats.panic_flags << ",\n"
            << "  \"capture_output_overflows\": " << capture_decode.output_overflows << ",\n"
            << "  \"playback_output_overflows\": " << playback_decode.output_overflows << ",\n"
            << "  \"playback_written\": " << playback_written << ",\n"
            << "  \"hal_capture_read\": " << hal_capture_read << ",\n"
            << "  \"capture_prefix_mismatches\": " << capture_prefix_mismatches << ",\n"
            << "  \"playback_prefix_mismatches\": " << playback_prefix_mismatches << ",\n"
            << "  \"backend_capture_frames\": " << counters.backend_capture_frames << ",\n"
            << "  \"backend_playback_frames\": " << counters.backend_playback_frames << ",\n"
            << "  \"hal_steady_requeues\": " << counters.hal_steady_requeues << ",\n"
            << "  \"fallback_allocations\": " << counters.fallback_allocations << ",\n"
            << "  \"capture_ring_overruns\": " << counters.capture_ring_overruns << ",\n"
            << "  \"capture_ring_underruns\": " << counters.capture_ring_underruns << ",\n"
            << "  \"playback_ring_overruns\": " << counters.playback_ring_overruns << ",\n"
            << "  \"playback_ring_underruns\": " << counters.playback_ring_underruns << ",\n"
            << "  \"timestamp_regressions\": " << counters.timestamp_regressions << ",\n"
            << "  \"channel_identity_failures\": " << counters.channel_identity_failures << ",\n"
            << "  \"product_safe\": " << (safety.product_safe ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
