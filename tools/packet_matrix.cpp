#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/policies.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace opena8djcpp;

namespace {

S24Frame synthetic_frame(std::uint32_t frame_index, double gain) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto stream = channel / kChannelsPerPair;
    const auto side = channel % kChannelsPerPair;
    auto magnitude = static_cast<std::int32_t>(((stream + 1U) * 1000000U) +
                                              (side * 250000U) +
                                              ((frame_index % 8192U) * 257U));
    magnitude = static_cast<std::int32_t>(static_cast<double>(magnitude) * gain);
    frame[channel] = side == 0 ? magnitude : -magnitude;
  }
  return frame;
}

std::vector<S24Frame> make_frames(std::uint32_t count, double gain) {
  std::vector<S24Frame> frames;
  frames.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    frames.push_back(synthetic_frame(index, gain));
  }
  return frames;
}

bool validate_row(std::uint32_t start_byte,
                  std::uint32_t transfer_bytes,
                  const std::vector<S24Frame>& frames,
                  Mode2DecodeStats& stats,
                  std::uint64_t& mismatches) {
  Mode2OutputPacker packer(frames, start_byte);
  std::vector<std::uint8_t> packed;
  while (packed.size() < 8192) {
    const auto chunk = packer.fill(transfer_bytes);
    packed.insert(packed.end(), chunk.begin(), chunk.end());
  }

  const auto decoded = decode_mode2_usb_bytes(packed, start_byte, transfer_bytes);
  stats = decoded.stats;
  const std::uint32_t source_start = start_byte == 0 ? 0 : 1;
  const auto expected_count = frames.size() - source_start;
  if (decoded.frames.size() < expected_count) {
    mismatches += expected_count - decoded.frames.size();
    return false;
  }

  for (std::size_t index = 0; index < expected_count; ++index) {
    if (decoded.frames[index] != frames[index + source_start]) {
      mismatches += 1;
    }
  }

  return stats.check_errors == 0 && stats.panic_flags == 0 && mismatches == 0;
}

}  // namespace

int main() {
  const std::uint32_t sample_rates[] = {44100, 48000};
  const std::uint32_t transfer_bytes[] = {48, 80, kMode2DefaultTransferBytes};
  const double gains[] = {1.0, 0.5};

  std::uint32_t rows = 0;
  std::uint32_t failures = 0;

  std::cout << "{\n  \"rows\": [\n";
  bool first = true;
  for (const auto sample_rate : sample_rates) {
    for (const auto bytes : transfer_bytes) {
      for (const auto gain : gains) {
        const auto frames = make_frames(96, gain);
        for (std::uint32_t start_byte = 0; start_byte < kMode2FrameBytesPerStream; ++start_byte) {
          Mode2DecodeStats stats{};
          std::uint64_t mismatches = 0;
          const bool ok = SampleRatePolicy::is_supported(sample_rate) &&
                          validate_row(start_byte, bytes, frames, stats, mismatches);
          rows += 1;
          if (!ok) {
            failures += 1;
          }

          if (!first) {
            std::cout << ",\n";
          }
          first = false;
          std::cout << "    {\"sample_rate\": " << sample_rate << ", \"transfer_bytes\": "
                    << bytes << ", \"gain\": " << gain << ", \"start_byte\": " << start_byte
                    << ", \"result\": \"" << (ok ? "PASS" : "FAIL")
                    << "\", \"decoded_frames\": " << stats.decoded_frames
                    << ", \"check_errors\": " << stats.check_errors
                    << ", \"panic_flags\": " << stats.panic_flags
                    << ", \"mismatches\": " << mismatches << "}";
        }
      }
    }
  }
  std::cout << "\n  ],\n  \"row_count\": " << rows << ",\n  \"failures\": " << failures
            << ",\n  \"result\": \"" << (failures == 0 ? "PASS" : "FAIL") << "\"\n}\n";

  return failures == 0 ? 0 : 1;
}
