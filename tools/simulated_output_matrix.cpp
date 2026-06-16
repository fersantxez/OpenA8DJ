#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/policies.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace opena8djcpp;

namespace {

struct RowMetrics {
  std::string pair;
  std::string mode;
  std::uint32_t sample_rate = 0;
  double gain = 0.0;
  std::uint32_t frames = 0;
  std::uint64_t decoded_frames = 0;
  std::uint64_t compared_frames = 0;
  std::uint64_t check_errors = 0;
  std::uint64_t panic_flags = 0;
  double alignment = 0.0;
  double snr_db = 0.0;
  double residual_ratio = 0.0;
  double leakage_dbfs = -240.0;
  bool pass = false;
};

double dbfs(double value) {
  if (value <= 0.0) {
    return -240.0;
  }
  return 20.0 * std::log10(value);
}

double rms(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (const auto value : values) {
    sum += value * value;
  }
  return std::sqrt(sum / static_cast<double>(values.size()));
}

double correlation(const std::vector<double>& left, const std::vector<double>& right) {
  if (left.size() != right.size() || left.empty()) {
    return 0.0;
  }
  double dot = 0.0;
  double left_energy = 0.0;
  double right_energy = 0.0;
  for (std::size_t index = 0; index < left.size(); ++index) {
    dot += left[index] * right[index];
    left_energy += left[index] * left[index];
    right_energy += right[index] * right[index];
  }
  if (left_energy <= 0.0 || right_energy <= 0.0) {
    return 0.0;
  }
  return dot / std::sqrt(left_energy * right_energy);
}

double signal_value(const std::string& mode, std::uint32_t frame, std::uint32_t rate, bool right) {
  constexpr double kPi = 3.14159265358979323846264338327950288;
  const auto t = static_cast<double>(frame) / static_cast<double>(rate);
  const double side_phase = right ? 0.37 : 0.0;
  double value = 0.0;
  if (mode == "dense") {
    value = 0.34 * std::sin(2.0 * kPi * 227.0 * t + side_phase) +
            0.19 * std::sin(2.0 * kPi * 937.0 * t + 0.21 + side_phase) +
            0.11 * std::sin(2.0 * kPi * 3100.0 * t + 0.47) +
            0.07 * std::sin(2.0 * kPi * 7111.0 * t + 0.13 + side_phase);
  } else if (mode == "transient") {
    const auto period = rate / 20U;
    const auto local = frame % std::max<std::uint32_t>(1, period);
    const double decay = std::exp(-static_cast<double>(local) / (0.0035 * rate));
    value = 0.55 * decay * std::sin(2.0 * kPi * 1800.0 * t + side_phase) +
            0.14 * std::sin(2.0 * kPi * 433.0 * t + 0.31);
  } else {
    value = 0.20 * std::sin(2.0 * kPi * 173.0 * t + side_phase) +
            0.16 * std::sin(2.0 * kPi * 1499.0 * t + 0.11) +
            0.13 * std::sin(2.0 * kPi * 5021.0 * t + 0.29 + side_phase) +
            0.09 * std::sin(2.0 * kPi * 10007.0 * t + 0.43);
  }
  return std::clamp(value, -0.95, 0.95);
}

std::string pair_name(std::uint32_t pair_index) {
  return std::string(1, static_cast<char>('A' + pair_index));
}

double s24_to_double(std::int32_t sample) {
  return std::clamp(static_cast<double>(sample) / 8388608.0, -1.0, 1.0);
}

RowMetrics run_row(std::uint32_t sample_rate,
                   std::uint32_t pair_index,
                   const std::string& mode,
                   double gain) {
  constexpr std::uint32_t kFrames = 8192;
  const std::uint32_t left_channel = pair_index * kChannelsPerPair;
  const std::uint32_t right_channel = left_channel + 1;

  std::vector<S24Frame> frames;
  std::vector<double> reference_left;
  std::vector<double> reference_right;
  frames.reserve(kFrames);
  reference_left.reserve(kFrames);
  reference_right.reserve(kFrames);

  for (std::uint32_t frame_index = 0; frame_index < kFrames; ++frame_index) {
    S24Frame frame{};
    const double left = signal_value(mode, frame_index, sample_rate, false) * gain;
    const double right = signal_value(mode, frame_index, sample_rate, true) * gain;
    frame[left_channel] = float_to_s24(static_cast<float>(left), 1.0F);
    frame[right_channel] = float_to_s24(static_cast<float>(right), 1.0F);
    frames.push_back(frame);
    reference_left.push_back(left);
    reference_right.push_back(right);
  }

  Mode2OutputPacker packer(frames, kMode2DefaultStartByte);
  std::vector<std::uint8_t> packed;
  const std::size_t target_bytes =
      (static_cast<std::size_t>(kFrames) + 8U) * static_cast<std::size_t>(kMode2FullFrameBytes);
  packed.reserve(target_bytes);
  while (packed.size() < target_bytes) {
    const auto chunk = packer.fill(kMode2DefaultTransferBytes);
    packed.insert(packed.end(), chunk.begin(), chunk.end());
  }

  const auto decoded =
      decode_mode2_usb_bytes(packed, kMode2DefaultStartByte, kMode2DefaultTransferBytes);
  const std::uint32_t source_start = kMode2DefaultStartByte == 0 ? 0 : 1;
  const auto compared =
      std::min<std::size_t>(decoded.frames.size(), frames.size() - source_start);

  std::vector<double> ref;
  std::vector<double> got;
  std::vector<double> residual;
  ref.reserve(compared * 2U);
  got.reserve(compared * 2U);
  residual.reserve(compared * 2U);

  double leakage_peak = 0.0;
  for (std::size_t index = 0; index < compared; ++index) {
    const auto source_index = index + source_start;
    const double left_ref = reference_left[source_index];
    const double right_ref = reference_right[source_index];
    const double left_got = s24_to_double(decoded.frames[index][left_channel]);
    const double right_got = s24_to_double(decoded.frames[index][right_channel]);
    ref.push_back(left_ref);
    ref.push_back(right_ref);
    got.push_back(left_got);
    got.push_back(right_got);
    residual.push_back(left_got - left_ref);
    residual.push_back(right_got - right_ref);

    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      if (channel != left_channel && channel != right_channel) {
        leakage_peak = std::max(leakage_peak, std::abs(s24_to_double(decoded.frames[index][channel])));
      }
    }
  }

  const double signal_rms = rms(ref);
  const double residual_rms = rms(residual);
  RowMetrics row{};
  row.pair = pair_name(pair_index);
  row.mode = mode;
  row.sample_rate = sample_rate;
  row.gain = gain;
  row.frames = kFrames;
  row.decoded_frames = decoded.stats.decoded_frames;
  row.compared_frames = static_cast<std::uint64_t>(compared);
  row.check_errors = decoded.stats.check_errors;
  row.panic_flags = decoded.stats.panic_flags;
  row.alignment = correlation(ref, got);
  row.snr_db = residual_rms > 0.0 ? 20.0 * std::log10(signal_rms / residual_rms) : 999.0;
  row.residual_ratio = signal_rms > 0.0 ? residual_rms / signal_rms : 1.0;
  row.leakage_dbfs = dbfs(leakage_peak);
  row.pass = SampleRatePolicy::is_supported(sample_rate) && compared >= kFrames - 2U &&
             row.check_errors == 0 && row.panic_flags == 0 && row.alignment >= 0.999999 &&
             row.snr_db >= 90.0 && row.residual_ratio <= 0.0001 && row.leakage_dbfs <= -120.0;
  return row;
}

void print_row(const RowMetrics& row, bool trailing_comma) {
  std::cout << "    {\"pair\": \"" << row.pair << "\", \"sample_rate\": "
            << row.sample_rate << ", \"mode\": \"" << row.mode << "\", \"gain\": "
            << row.gain << ", \"frames\": " << row.frames << ", \"decoded_frames\": "
            << row.decoded_frames << ", \"compared_frames\": " << row.compared_frames
            << ", \"check_errors\": " << row.check_errors << ", \"panic_flags\": "
            << row.panic_flags << ", \"alignment_score\": " << row.alignment
            << ", \"snr_db\": " << row.snr_db << ", \"residual_ratio\": "
            << row.residual_ratio << ", \"leakage_dbfs\": " << row.leakage_dbfs
            << ", \"result\": \"" << (row.pass ? "PASS" : "FAIL") << "\"}"
            << (trailing_comma ? ",\n" : "\n");
}

}  // namespace

int main() {
  const std::uint32_t rates[] = {44100, 48000};
  const std::string modes[] = {"dense", "transient", "wideband"};
  const double gains[] = {1.0, 0.5};

  std::vector<RowMetrics> rows;
  for (const auto rate : rates) {
    for (std::uint32_t pair = 0; pair < kStereoPairs; ++pair) {
      for (const auto& mode : modes) {
        for (const auto gain : gains) {
          rows.push_back(run_row(rate, pair, mode, gain));
        }
      }
    }
  }

  std::uint32_t failures = 0;
  double min_alignment = std::numeric_limits<double>::infinity();
  double min_snr = std::numeric_limits<double>::infinity();
  double max_residual_ratio = 0.0;
  double max_leakage_dbfs = -240.0;
  for (const auto& row : rows) {
    if (!row.pass) {
      failures += 1;
    }
    min_alignment = std::min(min_alignment, row.alignment);
    min_snr = std::min(min_snr, row.snr_db);
    max_residual_ratio = std::max(max_residual_ratio, row.residual_ratio);
    max_leakage_dbfs = std::max(max_leakage_dbfs, row.leakage_dbfs);
  }

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.simulated-output-matrix.v1\",\n"
            << "  \"result\": \"" << (failures == 0 ? "PASS" : "FAIL") << "\",\n"
            << "  \"row_count\": " << rows.size() << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"min_alignment_score\": " << min_alignment << ",\n"
            << "  \"min_snr_db\": " << min_snr << ",\n"
            << "  \"max_residual_ratio\": " << max_residual_ratio << ",\n"
            << "  \"max_leakage_dbfs\": " << max_leakage_dbfs << ",\n"
            << "  \"thresholds\": {\"alignment_score_min\": 0.999999, \"snr_db_min\": 90.0, "
               "\"residual_ratio_max\": 0.0001, \"leakage_dbfs_max\": -120.0},\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < rows.size(); ++index) {
    print_row(rows[index], index + 1U < rows.size());
  }
  std::cout << "  ]\n"
            << "}\n";

  return failures == 0 ? 0 : 1;
}
