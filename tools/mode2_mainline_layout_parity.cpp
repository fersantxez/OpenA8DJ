#include "opena8djcpp/mode2_packet.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

struct MainlineStylePacker {
  explicit MainlineStylePacker(std::span<const S24Frame> frames, std::uint32_t start_byte)
      : frames_(frames), output_byte_in_frame(start_byte) {}

  std::vector<std::uint8_t> fill(std::uint32_t length) {
    std::vector<std::uint8_t> bytes(length, 0);
    std::uint32_t i = 0;
    std::uint32_t group = 0;

    while (i + kMode2GroupBytes <= length) {
      load_output_frame_if_needed();
      bytes[i + 0] = output_frame_bytes[0][output_byte_in_frame];
      bytes[i + 1] = output_frame_bytes[1][output_byte_in_frame];
      bytes[i + 2] = output_frame_bytes[2][output_byte_in_frame];
      bytes[i + 3] = output_frame_bytes[3][output_byte_in_frame];
      advance_output_byte();

      load_output_frame_if_needed();
      bytes[i + 4] = output_frame_bytes[0][output_byte_in_frame];
      bytes[i + 5] = output_frame_bytes[1][output_byte_in_frame];
      bytes[i + 6] = output_frame_bytes[2][output_byte_in_frame];
      bytes[i + 7] = output_frame_bytes[3][output_byte_in_frame];
      advance_output_byte();

      const auto check_low_bit = static_cast<std::uint8_t>((~group) & 1U);
      bytes[i + 8] = check_low_bit;
      bytes[i + 9] = static_cast<std::uint8_t>(2U | check_low_bit);
      bytes[i + 10] = static_cast<std::uint8_t>(4U | check_low_bit);
      bytes[i + 11] = static_cast<std::uint8_t>(6U | check_low_bit);

      load_output_frame_if_needed();
      bytes[i + 12] = output_frame_bytes[0][output_byte_in_frame];
      bytes[i + 13] = output_frame_bytes[1][output_byte_in_frame];
      bytes[i + 14] = output_frame_bytes[2][output_byte_in_frame];
      bytes[i + 15] = output_frame_bytes[3][output_byte_in_frame];
      advance_output_byte();

      i += kMode2GroupBytes;
      ++group;
    }

    while (i < length) {
      if ((i % kMode2GroupBytes) == kMode2CheckOffset) {
        for (std::uint32_t stream = 0; stream < kMode2Streams && i < length; ++stream, ++i) {
          bytes[i] = mode2_check_byte(stream, i);
        }
        continue;
      }

      load_output_frame_if_needed();
      for (std::uint32_t stream = 0; stream < kMode2Streams && i < length; ++stream, ++i) {
        bytes[i] = output_frame_bytes[stream][output_byte_in_frame];
      }
      advance_output_byte();
    }

    return bytes;
  }

  void load_output_frame_if_needed() {
    if (output_frame_loaded && output_byte_in_frame != 0) {
      return;
    }
    if (frame_index < frames_.size()) {
      const auto& frame = frames_[frame_index++];
      for (std::uint32_t stream = 0; stream < kMode2Streams; ++stream) {
        const auto left = encode_s24_big_endian(frame[(stream * kChannelsPerPair) + 0]);
        const auto right = encode_s24_big_endian(frame[(stream * kChannelsPerPair) + 1]);
        output_frame_bytes[stream] = {left[0], left[1], left[2], right[0], right[1], right[2]};
      }
    } else {
      for (auto& stream : output_frame_bytes) {
        stream.fill(0);
      }
    }
    output_frame_loaded = true;
  }

  void advance_output_byte() {
    ++output_byte_in_frame;
    if (output_byte_in_frame >= kMode2FrameBytesPerStream) {
      output_byte_in_frame = 0;
    }
  }

  std::span<const S24Frame> frames_;
  std::uint32_t frame_index = 0;
  std::uint32_t output_byte_in_frame = 0;
  bool output_frame_loaded = false;
  std::array<std::array<std::uint8_t, kMode2FrameBytesPerStream>, kMode2Streams>
      output_frame_bytes{};
};

S24Frame make_frame(std::uint32_t frame_index) {
  S24Frame frame{};
  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto sign = ((frame_index + channel) % 2U) == 0 ? 1 : -1;
    const auto magnitude = static_cast<std::int32_t>(
        1000U + (frame_index * 1297U) + (channel * 65537U));
    frame[channel] = sign * magnitude;
  }
  if (frame_index == 3) {
    frame[0] = kS24Max;
    frame[1] = kS24Min;
  }
  return frame;
}

std::vector<S24Frame> make_frames() {
  std::vector<S24Frame> frames;
  frames.reserve(512);
  for (std::uint32_t index = 0; index < 512; ++index) {
    frames.push_back(make_frame(index));
  }
  return frames;
}

struct Row {
  std::uint32_t start_byte;
  std::uint32_t transfer_bytes;
  std::uint32_t transfer_count;
  bool pass;
  std::uint32_t first_mismatch_transfer;
  std::uint32_t first_mismatch_byte;
};

Row validate_row(std::uint32_t start_byte,
                 std::uint32_t transfer_bytes,
                 std::uint32_t transfer_count,
                 std::span<const S24Frame> frames) {
  Mode2OutputPacker actual(frames, start_byte);
  MainlineStylePacker expected(frames, start_byte);
  for (std::uint32_t transfer = 0; transfer < transfer_count; ++transfer) {
    const auto actual_bytes = actual.fill(transfer_bytes);
    const auto expected_bytes = expected.fill(transfer_bytes);
    if (actual_bytes != expected_bytes) {
      const auto limit = actual_bytes.size() < expected_bytes.size() ? actual_bytes.size()
                                                                     : expected_bytes.size();
      for (std::uint32_t index = 0; index < limit; ++index) {
        if (actual_bytes[index] != expected_bytes[index]) {
          return {start_byte, transfer_bytes, transfer_count, false, transfer, index};
        }
      }
      return {start_byte,
              transfer_bytes,
              transfer_count,
              false,
              transfer,
              static_cast<std::uint32_t>(limit)};
    }
  }
  return {start_byte, transfer_bytes, transfer_count, true, 0, 0};
}

}  // namespace

int main() {
  const auto frames = make_frames();
  const std::uint32_t transfer_bytes[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                          13, 14, 15, 16, 17, 31, 32, 48, 80,
                                          kMode2DefaultTransferBytes};
  std::vector<Row> rows;
  std::uint32_t failures = 0;
  for (std::uint32_t start_byte = 0; start_byte < kMode2FrameBytesPerStream; ++start_byte) {
    for (const auto bytes : transfer_bytes) {
      auto row = validate_row(start_byte, bytes, 9, frames);
      if (!row.pass) {
        ++failures;
      }
      rows.push_back(row);
    }
  }

  std::cout << "{\n  \"schema\": \"opena8djcpp.mode2-mainline-layout-parity.v1\",\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    if (index != 0) {
      std::cout << ",\n";
    }
    std::cout << "    {\"start_byte\": " << row.start_byte
              << ", \"transfer_bytes\": " << row.transfer_bytes
              << ", \"transfer_count\": " << row.transfer_count << ", \"result\": \""
              << (row.pass ? "PASS" : "FAIL") << "\"";
    if (!row.pass) {
      std::cout << ", \"first_mismatch_transfer\": " << row.first_mismatch_transfer
                << ", \"first_mismatch_byte\": " << row.first_mismatch_byte;
    }
    std::cout << "}";
  }
  std::cout << "\n  ],\n  \"row_count\": " << rows.size()
            << ",\n  \"failures\": " << failures << ",\n  \"result\": \""
            << (failures == 0 ? "PASS" : "FAIL") << "\"\n}\n";
  return failures == 0 ? 0 : 1;
}
