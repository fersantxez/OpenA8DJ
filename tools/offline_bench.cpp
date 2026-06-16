#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/routing.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace opena8djcpp;

namespace {

std::vector<S24Frame> make_frames(std::size_t count) {
  std::vector<S24Frame> frames;
  frames.reserve(count);
  for (std::size_t frame_index = 0; frame_index < count; ++frame_index) {
    S24Frame frame{};
    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      const auto stream = channel / kChannelsPerPair;
      const auto side = channel % kChannelsPerPair;
      auto magnitude = static_cast<std::int32_t>(((stream + 1U) * 1000000U) +
                                                (side * 250000U) +
                                                ((frame_index % 8192U) * 257U));
      frame[channel] = side == 0 ? magnitude : -magnitude;
    }
    frames.push_back(frame);
  }
  return frames;
}

template <typename Fn>
double seconds_for(Fn&& fn) {
  const auto start = std::chrono::steady_clock::now();
  fn();
  const auto stop = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(stop - start).count();
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

double minimum(const std::vector<double>& values) {
  return *std::min_element(values.begin(), values.end());
}

double maximum(const std::vector<double>& values) {
  return *std::max_element(values.begin(), values.end());
}

}  // namespace

int main() {
  constexpr std::size_t kFrames = 262144;
  constexpr std::uint32_t kTransferBytes = kMode2DefaultTransferBytes;
  constexpr std::uint32_t kRepeats = 5;
  const auto frames = make_frames(kFrames);

  std::vector<std::uint8_t> packed(kFrames * kMode2GroupBytes);
  std::size_t packed_bytes = 0;
  std::vector<double> pack_mib_s_values;
  std::vector<double> decode_allocating_mib_s_values;
  std::vector<double> decode_into_mib_s_values;
  std::vector<double> float_to_s24_frames_s_values;
  std::vector<double> route_frames_s_values;
  std::vector<double> route_reversed_frames_s_values;
  pack_mib_s_values.reserve(kRepeats);
  decode_allocating_mib_s_values.reserve(kRepeats);
  decode_into_mib_s_values.reserve(kRepeats);
  float_to_s24_frames_s_values.reserve(kRepeats);
  route_frames_s_values.reserve(kRepeats);
  route_reversed_frames_s_values.reserve(kRepeats);

  const double expected_packed_mib = static_cast<double>(packed.size()) / (1024.0 * 1024.0);
  for (std::uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
    packed_bytes = 0;
    packed.assign(kFrames * kMode2GroupBytes, 0);
    const double pack_seconds = seconds_for([&]() {
      Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
      while (packed_bytes < packed.size()) {
        const auto remaining = packed.size() - packed_bytes;
        const auto chunk_bytes =
            static_cast<std::uint32_t>(std::min<std::size_t>(remaining, kTransferBytes));
        const auto written =
            packer.fill_into(std::span<std::uint8_t>(packed.data() + packed_bytes, chunk_bytes));
        packed_bytes += written;
      }
    });
    pack_mib_s_values.push_back(expected_packed_mib / pack_seconds);
  }

  packed.resize(packed_bytes);

  Mode2DecodeResult decoded;
  std::vector<S24Frame> decoded_into_storage(kFrames + 2);
  Mode2DecodeIntoResult decoded_into;
  const double packed_mib = static_cast<double>(packed.size()) / (1024.0 * 1024.0);
  for (std::uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
    const double decode_allocating_seconds = seconds_for([&]() {
      decoded = decode_mode2_usb_bytes(packed, kMode2DefaultStartByte, kTransferBytes);
    });
    decode_allocating_mib_s_values.push_back(packed_mib / decode_allocating_seconds);
  }

  for (std::uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
    const double decode_into_seconds = seconds_for([&]() {
      decoded_into = decode_mode2_usb_bytes_into(packed,
                                                 kMode2DefaultStartByte,
                                                 kTransferBytes,
                                                 decoded_into_storage);
    });
    decode_into_mib_s_values.push_back(packed_mib / decode_into_seconds);
  }

  std::vector<float> host_float_samples(kFrames * kOutputChannels, 0.0F);
  std::vector<std::int32_t> converted_s24(kFrames * kOutputChannels, 0);
  for (std::size_t index = 0; index < host_float_samples.size(); ++index) {
    host_float_samples[index] = static_cast<float>((index % 4096U) - 2048) / 2048.0F;
  }
  for (std::uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
    const double convert_seconds = seconds_for([&]() {
      for (std::size_t index = 0; index < host_float_samples.size(); ++index) {
        converted_s24[index] = float_to_s24(host_float_samples[index], 0.5F);
      }
    });
    float_to_s24_frames_s_values.push_back(static_cast<double>(kFrames) / convert_seconds);
  }

  std::vector<float> route_input(kFrames * kInputChannels, 0.0F);
  std::vector<float> route_output(kFrames * kOutputChannels, 0.0F);
  for (std::size_t index = 0; index < route_input.size(); ++index) {
    route_input[index] = static_cast<float>((index % 1024U) - 512) / 512.0F;
  }

  const RoutingPlan identity_plan(RoutingMatrix::identity());
  const RoutingPlan reversed_plan(RoutingMatrix({7, 6, 5, 4, 3, 2, 1, 0}));
  for (std::uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
    const double route_seconds = seconds_for([&]() {
      const bool routed =
          route_interleaved_f32(route_input,
                                route_output,
                                static_cast<std::uint32_t>(kFrames),
                                identity_plan);
      if (!routed) {
        std::cerr << "routing failed\n";
      }
    });
    route_frames_s_values.push_back(static_cast<double>(kFrames) / route_seconds);
  }

  for (std::uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
    const double route_seconds = seconds_for([&]() {
      const bool routed =
          route_interleaved_f32(route_input,
                                route_output,
                                static_cast<std::uint32_t>(kFrames),
                                reversed_plan);
      if (!routed) {
        std::cerr << "reversed routing failed\n";
      }
    });
    route_reversed_frames_s_values.push_back(static_cast<double>(kFrames) / route_seconds);
  }

  const double pack_mib_s = median(pack_mib_s_values);
  const double decode_allocating_mib_s = median(decode_allocating_mib_s_values);
  const double decode_into_mib_s = median(decode_into_mib_s_values);
  const double float_to_s24_frames_s = median(float_to_s24_frames_s_values);
  const double route_frames_s = median(route_frames_s_values);
  const double route_reversed_frames_s = median(route_reversed_frames_s_values);

  const bool pass = pack_mib_s >= 100.0 && decode_into_mib_s >= 100.0 &&
                    float_to_s24_frames_s >= 1000000.0 &&
                    route_frames_s >= 1000000.0 &&
                    route_reversed_frames_s >= 1000000.0 &&
                    identity_plan.valid() && reversed_plan.valid() &&
                    decoded.stats.check_errors == 0 &&
                    decoded.stats.panic_flags == 0 && !decoded.frames.empty() &&
                    decoded_into.stats.check_errors == 0 && decoded_into.stats.panic_flags == 0 &&
                    decoded_into.output_overflows == 0 && decoded_into.stats.decoded_frames > 0;

  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"frames\": " << kFrames << ",\n"
            << "  \"repeats\": " << kRepeats << ",\n"
            << "  \"packed_bytes\": " << packed.size() << ",\n"
            << "  \"pack_mib_s\": " << pack_mib_s << ",\n"
            << "  \"pack_mib_s_min\": " << minimum(pack_mib_s_values) << ",\n"
            << "  \"pack_mib_s_max\": " << maximum(pack_mib_s_values) << ",\n"
            << "  \"decode_mib_s\": " << decode_into_mib_s << ",\n"
            << "  \"decode_mib_s_min\": " << minimum(decode_into_mib_s_values) << ",\n"
            << "  \"decode_mib_s_max\": " << maximum(decode_into_mib_s_values) << ",\n"
            << "  \"decode_into_mib_s\": " << decode_into_mib_s << ",\n"
            << "  \"decode_into_mib_s_min\": " << minimum(decode_into_mib_s_values) << ",\n"
            << "  \"decode_into_mib_s_max\": " << maximum(decode_into_mib_s_values) << ",\n"
            << "  \"decode_allocating_mib_s\": " << decode_allocating_mib_s << ",\n"
            << "  \"decode_allocating_mib_s_min\": " << minimum(decode_allocating_mib_s_values)
            << ",\n"
            << "  \"decode_allocating_mib_s_max\": " << maximum(decode_allocating_mib_s_values)
            << ",\n"
            << "  \"float_to_s24_frames_s\": " << float_to_s24_frames_s << ",\n"
            << "  \"float_to_s24_frames_s_min\": " << minimum(float_to_s24_frames_s_values)
            << ",\n"
            << "  \"float_to_s24_frames_s_max\": " << maximum(float_to_s24_frames_s_values)
            << ",\n"
            << "  \"route_frames_s\": " << route_frames_s << ",\n"
            << "  \"route_frames_s_min\": " << minimum(route_frames_s_values) << ",\n"
            << "  \"route_frames_s_max\": " << maximum(route_frames_s_values) << ",\n"
            << "  \"route_reversed_frames_s\": " << route_reversed_frames_s << ",\n"
            << "  \"route_reversed_frames_s_min\": " << minimum(route_reversed_frames_s_values)
            << ",\n"
            << "  \"route_reversed_frames_s_max\": " << maximum(route_reversed_frames_s_values)
            << ",\n"
            << "  \"decoded_frames\": " << decoded.stats.decoded_frames << ",\n"
            << "  \"decode_into_frames\": " << decoded_into.stats.decoded_frames << ",\n"
            << "  \"decode_into_output_overflows\": " << decoded_into.output_overflows << ",\n"
            << "  \"check_errors\": " << decoded.stats.check_errors << ",\n"
            << "  \"panic_flags\": " << decoded.stats.panic_flags << ",\n"
            << "  \"decode_into_check_errors\": " << decoded_into.stats.check_errors << ",\n"
            << "  \"decode_into_panic_flags\": " << decoded_into.stats.panic_flags << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
