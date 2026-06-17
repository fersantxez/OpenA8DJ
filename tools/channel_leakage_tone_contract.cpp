#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/mode2_packet.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace opena8djcpp;

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
struct Row {
  std::string scenario;
  std::uint32_t sample_rate = 0;
  std::uint32_t active_pair = 0;
  double expected_floor_amplitude = 0.0;
  double max_wrong_source_leakage_db = -240.0;
  double max_inactive_deck_leakage_db = -240.0;
  double threshold_max_leakage_db = -90.0;
  std::uint64_t decoded_frames = 0;
  std::uint64_t check_errors = 0;
  std::uint64_t panic_flags = 0;
  bool expected_pass = true;
  bool actual_pass = false;
};

[[nodiscard]] std::string deck_name(std::uint32_t pair) {
  return std::string(1, static_cast<char>('A' + pair));
}

[[nodiscard]] double db_ratio(double numerator, double denominator) {
  if (numerator <= 0.0) {
    return -240.0;
  }
  if (denominator <= 0.0) {
    return 240.0;
  }
  return 20.0 * std::log10(numerator / denominator);
}

[[nodiscard]] double s24_to_double(std::int32_t sample) {
  return std::clamp(static_cast<double>(sample) / 8388608.0, -1.0, 1.0);
}

[[nodiscard]] std::array<double, kOutputChannels> channel_frequencies() {
  return {211.0, 337.0, 443.0, 587.0, 733.0, 947.0, 1193.0, 1459.0};
}

[[nodiscard]] std::vector<S24Frame> make_active_pair_frames(std::uint32_t sample_rate,
                                                            std::uint32_t active_pair) {
  const auto freqs = channel_frequencies();
  const auto left = active_pair * kChannelsPerPair;
  const auto right = left + 1U;
  std::vector<S24Frame> frames;
  frames.reserve(sample_rate);
  for (std::uint32_t frame_index = 0; frame_index < sample_rate; ++frame_index) {
    S24Frame frame{};
    const auto t = static_cast<double>(frame_index) / static_cast<double>(sample_rate);
    frame[left] =
        float_to_s24(static_cast<float>(0.24 * std::sin(2.0 * kPi * freqs[left] * t)), 1.0F);
    frame[right] =
        float_to_s24(static_cast<float>(0.21 * std::sin(2.0 * kPi * freqs[right] * t)), 1.0F);
    frames.push_back(frame);
  }
  return frames;
}

[[nodiscard]] std::vector<S24Frame> pack_decode(std::span<const S24Frame> frames) {
  Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
  std::vector<std::uint8_t> packed;
  const auto target_bytes =
      (frames.size() + 8U) * static_cast<std::size_t>(kMode2FullFrameBytes);
  packed.reserve(target_bytes);
  while (packed.size() < target_bytes) {
    const auto chunk = packer.fill(kMode2DefaultTransferBytes);
    packed.insert(packed.end(), chunk.begin(), chunk.end());
  }
  return decode_mode2_usb_bytes(packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes).frames;
}

[[nodiscard]] Mode2DecodeStats pack_decode_stats(std::span<const S24Frame> frames) {
  Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
  std::vector<std::uint8_t> packed;
  const auto target_bytes =
      (frames.size() + 8U) * static_cast<std::size_t>(kMode2FullFrameBytes);
  packed.reserve(target_bytes);
  while (packed.size() < target_bytes) {
    const auto chunk = packer.fill(kMode2DefaultTransferBytes);
    packed.insert(packed.end(), chunk.begin(), chunk.end());
  }
  return decode_mode2_usb_bytes(packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes).stats;
}

void inject_leakage(std::vector<S24Frame>& frames, std::uint32_t active_pair) {
  const auto left = active_pair * kChannelsPerPair;
  const auto first_inactive = active_pair == 0U ? 2U : 0U;
  for (auto& frame : frames) {
    const auto leaked = static_cast<float>(0.012 * s24_to_double(frame[left]));
    frame[first_inactive] = float_to_s24(leaked, 1.0F);
  }
}

[[nodiscard]] double tone_amplitude(const std::vector<S24Frame>& frames,
                                    std::uint32_t channel,
                                    std::uint32_t sample_rate,
                                    double frequency) {
  if (frames.empty()) {
    return 0.0;
  }
  double cosine = 0.0;
  double sine = 0.0;
  for (std::size_t index = 0; index < frames.size(); ++index) {
    const auto phase = 2.0 * kPi * frequency * static_cast<double>(index) /
                       static_cast<double>(sample_rate);
    const auto sample = s24_to_double(frames[index][channel]);
    cosine += sample * std::cos(phase);
    sine += sample * std::sin(phase);
  }
  return 2.0 * std::hypot(cosine, sine) / static_cast<double>(frames.size());
}

[[nodiscard]] Row run_row(std::uint32_t sample_rate,
                          std::uint32_t active_pair,
                          bool injected_leakage) {
  auto frames = make_active_pair_frames(sample_rate, active_pair);
  auto decoded = pack_decode(frames);
  const auto stats = pack_decode_stats(frames);
  if (injected_leakage) {
    inject_leakage(decoded, active_pair);
  }

  const auto freqs = channel_frequencies();
  const auto left = active_pair * kChannelsPerPair;
  const auto right = left + 1U;
  const auto left_amp = tone_amplitude(decoded, left, sample_rate, freqs[left]);
  const auto right_amp = tone_amplitude(decoded, right, sample_rate, freqs[right]);
  const auto expected_floor = std::min(left_amp, right_amp);
  double max_wrong_source = 0.0;
  double max_inactive = 0.0;

  for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
    const auto left_tone = tone_amplitude(decoded, channel, sample_rate, freqs[left]);
    const auto right_tone = tone_amplitude(decoded, channel, sample_rate, freqs[right]);
    if (channel == left) {
      max_wrong_source = std::max(max_wrong_source, right_tone);
    } else if (channel == right) {
      max_wrong_source = std::max(max_wrong_source, left_tone);
    } else {
      max_inactive = std::max({max_inactive, left_tone, right_tone});
    }
  }

  Row row{};
  row.scenario = injected_leakage ? "injected_leakage_rejected" : "clean_pair";
  row.sample_rate = sample_rate;
  row.active_pair = active_pair;
  row.expected_floor_amplitude = expected_floor;
  row.max_wrong_source_leakage_db = db_ratio(max_wrong_source, expected_floor);
  row.max_inactive_deck_leakage_db = db_ratio(max_inactive, expected_floor);
  row.decoded_frames = stats.decoded_frames;
  row.check_errors = stats.check_errors;
  row.panic_flags = stats.panic_flags;
  row.expected_pass = !injected_leakage;
  row.actual_pass = expected_floor >= 0.10 && stats.check_errors == 0U && stats.panic_flags == 0U &&
                    row.max_wrong_source_leakage_db <= row.threshold_max_leakage_db &&
                    row.max_inactive_deck_leakage_db <= row.threshold_max_leakage_db;
  return row;
}

void print_row(const Row& row, bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << row.scenario << "\", \"sample_rate\": "
            << row.sample_rate << ", \"pair\": \"" << deck_name(row.active_pair)
            << "\", \"expected_floor_amplitude\": " << row.expected_floor_amplitude
            << ", \"max_wrong_source_leakage_db\": " << row.max_wrong_source_leakage_db
            << ", \"max_inactive_deck_leakage_db\": " << row.max_inactive_deck_leakage_db
            << ", \"threshold_max_leakage_db\": " << row.threshold_max_leakage_db
            << ", \"decoded_frames\": " << row.decoded_frames
            << ", \"check_errors\": " << row.check_errors
            << ", \"panic_flags\": " << row.panic_flags
            << ", \"expected_pass\": " << (row.expected_pass ? "true" : "false")
            << ", \"actual_pass\": " << (row.actual_pass ? "true" : "false")
            << ", \"result\": \""
            << (row.expected_pass == row.actual_pass ? "PASS" : "FAIL") << "\"}"
            << (trailing_comma ? ",\n" : "\n");
}

}  // namespace

int main() {
  const std::array<std::uint32_t, 2> sample_rates{44100, 48000};
  std::vector<Row> rows;
  for (const auto rate : sample_rates) {
    for (std::uint32_t pair = 0; pair < kStereoPairs; ++pair) {
      rows.push_back(run_row(rate, pair, false));
      rows.push_back(run_row(rate, pair, true));
    }
  }

  std::uint32_t failures = 0;
  double max_clean_wrong_source = -240.0;
  double max_clean_inactive = -240.0;
  for (const auto& row : rows) {
    if (row.expected_pass != row.actual_pass) {
      failures += 1U;
    }
    if (row.expected_pass) {
      max_clean_wrong_source = std::max(max_clean_wrong_source, row.max_wrong_source_leakage_db);
      max_clean_inactive = std::max(max_clean_inactive, row.max_inactive_deck_leakage_db);
    }
  }

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.channel-leakage-tone-contract.v1\",\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < rows.size(); ++index) {
    print_row(rows[index], index + 1U < rows.size());
  }
  std::cout << "  ],\n"
            << "  \"row_count\": " << rows.size() << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"max_clean_wrong_source_leakage_db\": " << max_clean_wrong_source << ",\n"
            << "  \"max_clean_inactive_deck_leakage_db\": " << max_clean_inactive << ",\n"
            << "  \"result\": \"" << (failures == 0U ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return failures == 0U ? 0 : 1;
}
