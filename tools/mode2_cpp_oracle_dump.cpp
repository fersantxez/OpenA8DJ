#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/policies.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

using namespace opena8djcpp;

namespace {

struct Args {
  std::uint32_t sample_rate = 48000;
  std::uint32_t transfer_bytes = kMode2DefaultTransferBytes;
  std::uint32_t start_byte = kMode2DefaultStartByte;
  std::uint32_t frames = 96;
  float gain = 1.0F;
};

template <typename T>
bool parse_integer(std::string_view text, T& value) {
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  T parsed{};
  const auto [ptr, ec] = std::from_chars(begin, end, parsed);
  if (ec != std::errc{} || ptr != end) {
    return false;
  }
  value = parsed;
  return true;
}

bool parse_float(std::string_view text, float& value) {
  char* end = nullptr;
  const std::string owned(text);
  const auto parsed = std::strtof(owned.c_str(), &end);
  if (end != owned.c_str() + owned.size() || !std::isfinite(parsed)) {
    return false;
  }
  value = parsed;
  return true;
}

void usage(const char* name) {
  std::cerr << "usage: " << name
            << " --sample-rate <44100|48000> --transfer-bytes <n> --start-byte <0..5>"
               " --frames <n> --gain <float>\n";
}

bool parse_args(int argc, char** argv, Args& args) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view key(argv[index]);
    if (index + 1 >= argc) {
      usage(argv[0]);
      return false;
    }
    const std::string_view value(argv[++index]);
    if (key == "--sample-rate") {
      if (!parse_integer(value, args.sample_rate)) {
        return false;
      }
    } else if (key == "--transfer-bytes") {
      if (!parse_integer(value, args.transfer_bytes)) {
        return false;
      }
    } else if (key == "--start-byte") {
      if (!parse_integer(value, args.start_byte)) {
        return false;
      }
    } else if (key == "--frames") {
      if (!parse_integer(value, args.frames)) {
        return false;
      }
    } else if (key == "--gain") {
      if (!parse_float(value, args.gain)) {
        return false;
      }
    } else {
      usage(argv[0]);
      return false;
    }
  }
  return true;
}

std::int32_t synthetic_s24_value(std::uint32_t frame_index, std::uint32_t channel) {
  const auto stream = channel / kChannelsPerPair;
  const auto side = channel % kChannelsPerPair;
  auto magnitude = static_cast<std::int32_t>(((stream + 1U) * 1000000U) +
                                            (side * 250000U) +
                                            ((frame_index % 8192U) * 257U));
  if (side != 0) {
    magnitude = -magnitude;
  }
  return magnitude;
}

std::vector<S24Frame> make_frames(std::uint32_t count, float gain) {
  std::vector<S24Frame> frames;
  frames.reserve(count);
  for (std::uint32_t frame_index = 0; frame_index < count; ++frame_index) {
    S24Frame frame{};
    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      const auto s24 = synthetic_s24_value(frame_index, channel);
      const float sample = static_cast<float>(static_cast<double>(s24) /
                                             static_cast<double>(kS24Max));
      frame[channel] = float_to_s24(sample, gain);
    }
    frames.push_back(frame);
  }
  return frames;
}

std::string hex_bytes(const std::vector<std::uint8_t>& bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2U);
  for (const auto byte : bytes) {
    out.push_back(kHex[(byte >> 4U) & 0x0FU]);
    out.push_back(kHex[byte & 0x0FU]);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Args args{};
  if (!parse_args(argc, argv, args)) {
    return 2;
  }

  const bool args_valid = SampleRatePolicy::is_supported(args.sample_rate) &&
                          args.transfer_bytes > 0 &&
                          (args.transfer_bytes % kMode2GroupBytes) == 0 &&
                          args.start_byte < kMode2FrameBytesPerStream && args.frames > 1;
  if (!args_valid) {
    std::cerr << "invalid mode2 oracle dump arguments\n";
    return 2;
  }

  const auto frames = make_frames(args.frames, args.gain);
  const std::uint32_t source_start_frame = args.start_byte == 0 ? 0 : 1;
  const auto expected_count = frames.size() - source_start_frame;
  const std::uint32_t max_transfers =
      std::max<std::uint32_t>(4, static_cast<std::uint32_t>(
                                     std::ceil((static_cast<double>(frames.size()) + 8.0) *
                                               32.0 / args.transfer_bytes)) +
                                     4);

  Mode2OutputPacker packer(frames, args.start_byte);
  std::vector<std::uint8_t> packed;
  Mode2DecodeResult decoded{};
  for (std::uint32_t transfer = 0; transfer < max_transfers; ++transfer) {
    const auto chunk = packer.fill(args.transfer_bytes);
    packed.insert(packed.end(), chunk.begin(), chunk.end());
    decoded = decode_mode2_usb_bytes(packed, args.start_byte, args.transfer_bytes);
    if (decoded.frames.size() >= expected_count) {
      break;
    }
  }

  const bool pass = decoded.frames.size() >= expected_count && decoded.stats.check_errors == 0 &&
                    decoded.stats.panic_flags == 0;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.mode2-cpp-oracle-dump.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"byte_order\": \"big\",\n"
            << "  \"sample_rate\": " << args.sample_rate << ",\n"
            << "  \"transfer_bytes\": " << args.transfer_bytes << ",\n"
            << "  \"start_byte\": " << args.start_byte << ",\n"
            << "  \"frames\": " << args.frames << ",\n"
            << "  \"gain\": " << args.gain << ",\n"
            << "  \"source_start_frame\": " << source_start_frame << ",\n"
            << "  \"expected_decoded_frames\": " << expected_count << ",\n"
            << "  \"packed_bytes\": " << packed.size() << ",\n"
            << "  \"decoded_frames\": " << decoded.stats.decoded_frames << ",\n"
            << "  \"checks\": " << decoded.stats.checks << ",\n"
            << "  \"check_errors\": " << decoded.stats.check_errors << ",\n"
            << "  \"panic_flags\": " << decoded.stats.panic_flags << ",\n"
            << "  \"sample_bytes\": " << decoded.stats.sample_bytes << ",\n"
            << "  \"packed_hex\": \"" << hex_bytes(packed) << "\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
