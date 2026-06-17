#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kEpsilon = 1.0e-20;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
  std::uint32_t source_channels = 2;
};

struct Thresholds {
  double min_snr_db = 45.0;
  double min_correlation = 0.98;
  std::uint32_t max_clicks = 0;
};

struct Alignment {
  std::size_t reference_start = 0;
  std::size_t capture_start = 0;
  std::int32_t lag_frames = 0;
  double score = 0.0;
  std::size_t compared_frames = 0;
};

struct ChannelMetrics {
  double gain = 0.0;
  double correlation = 0.0;
  double signal_rms = 0.0;
  double capture_rms = 0.0;
  double residual_rms = 0.0;
  double residual_peak = 0.0;
  double snr_db = 0.0;
  double peak = 0.0;
  std::vector<double> residual;
};

struct QualityMetrics {
  std::string name;
  std::uint32_t sample_rate = 0;
  std::size_t reference_frames = 0;
  std::size_t capture_frames = 0;
  Alignment alignment;
  ChannelMetrics left;
  ChannelMetrics right;
  double min_snr_db = 0.0;
  double min_correlation = 0.0;
  double peak = 0.0;
  double residual_rms = 0.0;
  double residual_peak = 0.0;
  std::uint32_t click_outliers = 0;
  double click_threshold = 0.0;
  std::vector<std::string> failures;

  [[nodiscard]] bool passed() const {
    return failures.empty();
  }
};

struct Cli {
  std::string reference_wav;
  std::string capture_wav;
  std::string captured_f32;
  std::uint32_t sample_rate = 48000;
  std::uint32_t capture_channels = 8;
  std::uint32_t pair = 0;
  Thresholds thresholds;
};

[[nodiscard]] std::uint16_t read_u16_le(const std::vector<std::uint8_t>& data,
                                        std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t read_u32_le(const std::vector<std::uint8_t>& data,
                                        std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
         (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
         (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

[[nodiscard]] std::vector<std::uint8_t> read_file_bytes(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot_open:" + path);
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::int32_t sign_extend_24(std::uint32_t value) {
  if ((value & 0x00800000U) != 0U) {
    value |= 0xff000000U;
  }
  return static_cast<std::int32_t>(value);
}

[[nodiscard]] double decode_pcm_sample(const std::vector<std::uint8_t>& data,
                                       std::size_t offset,
                                       std::uint16_t format,
                                       std::uint16_t bits_per_sample) {
  if (format == 3U && bits_per_sample == 32U) {
    float value = 0.0F;
    std::memcpy(&value, data.data() + offset, sizeof(float));
    return std::isfinite(value) ? std::clamp(static_cast<double>(value), -1.0, 1.0) : 0.0;
  }
  if (format != 1U) {
    throw std::runtime_error("unsupported_wav_format");
  }
  if (bits_per_sample == 16U) {
    const auto raw = static_cast<std::int16_t>(read_u16_le(data, offset));
    return static_cast<double>(raw) / 32768.0;
  }
  if (bits_per_sample == 24U) {
    const auto raw = static_cast<std::uint32_t>(data[offset]) |
                     (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
                     (static_cast<std::uint32_t>(data[offset + 2U]) << 16U);
    return static_cast<double>(sign_extend_24(raw)) / 8388608.0;
  }
  if (bits_per_sample == 32U) {
    const auto raw = static_cast<std::int32_t>(read_u32_le(data, offset));
    return static_cast<double>(raw) / 2147483648.0;
  }
  throw std::runtime_error("unsupported_wav_bits");
}

[[nodiscard]] StereoBuffer read_wav_pair(const std::string& path) {
  const auto bytes = read_file_bytes(path);
  if (bytes.size() < 44U || std::memcmp(bytes.data(), "RIFF", 4U) != 0 ||
      std::memcmp(bytes.data() + 8U, "WAVE", 4U) != 0) {
    throw std::runtime_error("invalid_wav:" + path);
  }

  std::uint16_t format = 0;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint16_t block_align = 0;
  std::uint16_t bits_per_sample = 0;
  std::size_t data_offset = 0;
  std::size_t data_size = 0;

  std::size_t offset = 12U;
  while (offset + 8U <= bytes.size()) {
    const std::string chunk_id(reinterpret_cast<const char*>(bytes.data() + offset), 4U);
    const auto chunk_size = static_cast<std::size_t>(read_u32_le(bytes, offset + 4U));
    const auto chunk_data = offset + 8U;
    if (chunk_data + chunk_size > bytes.size()) {
      throw std::runtime_error("truncated_wav_chunk:" + path);
    }
    if (chunk_id == "fmt ") {
      if (chunk_size < 16U) {
        throw std::runtime_error("invalid_fmt_chunk:" + path);
      }
      format = read_u16_le(bytes, chunk_data);
      channels = read_u16_le(bytes, chunk_data + 2U);
      sample_rate = read_u32_le(bytes, chunk_data + 4U);
      block_align = read_u16_le(bytes, chunk_data + 12U);
      bits_per_sample = read_u16_le(bytes, chunk_data + 14U);
    } else if (chunk_id == "data") {
      data_offset = chunk_data;
      data_size = chunk_size;
    }
    offset = chunk_data + chunk_size + (chunk_size % 2U);
  }

  if (channels == 0U || sample_rate == 0U || block_align == 0U || data_size == 0U) {
    throw std::runtime_error("missing_wav_audio:" + path);
  }
  if (channels < 1U) {
    throw std::runtime_error("invalid_wav_channels:" + path);
  }

  StereoBuffer buffer{};
  buffer.sample_rate = sample_rate;
  buffer.source_channels = channels;
  const auto frames = data_size / static_cast<std::size_t>(block_align);
  buffer.frames.reserve(frames);
  const auto bytes_per_sample = static_cast<std::size_t>(bits_per_sample / 8U);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const auto frame_offset = data_offset + frame * static_cast<std::size_t>(block_align);
    const double left = decode_pcm_sample(bytes, frame_offset, format, bits_per_sample);
    const double right = channels >= 2U
                             ? decode_pcm_sample(bytes, frame_offset + bytes_per_sample, format,
                                                 bits_per_sample)
                             : left;
    buffer.frames.push_back({left, right});
  }
  return buffer;
}

[[nodiscard]] StereoBuffer read_raw_f32_pair(const std::string& path,
                                             std::uint32_t sample_rate,
                                             std::uint32_t channels,
                                             std::uint32_t pair) {
  if (channels == 0U || (pair * 2U) + 1U >= channels) {
    throw std::runtime_error("invalid_capture_pair");
  }
  const auto bytes = read_file_bytes(path);
  const auto samples = bytes.size() / sizeof(float);
  const auto frames = samples / channels;
  StereoBuffer buffer{};
  buffer.sample_rate = sample_rate;
  buffer.source_channels = channels;
  buffer.frames.reserve(frames);
  const auto left_channel = pair * 2U;
  const auto right_channel = left_channel + 1U;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    float left = 0.0F;
    float right = 0.0F;
    const auto base = (frame * static_cast<std::size_t>(channels)) * sizeof(float);
    std::memcpy(&left, bytes.data() + base + left_channel * sizeof(float), sizeof(float));
    std::memcpy(&right, bytes.data() + base + right_channel * sizeof(float), sizeof(float));
    buffer.frames.push_back({std::isfinite(left) ? static_cast<double>(left) : 0.0,
                             std::isfinite(right) ? static_cast<double>(right) : 0.0});
  }
  return buffer;
}

[[nodiscard]] std::vector<double> mono(const std::vector<std::array<double, 2>>& frames) {
  std::vector<double> out;
  out.reserve(frames.size());
  for (const auto& frame : frames) {
    out.push_back(0.5 * (frame[0] + frame[1]));
  }
  return out;
}

[[nodiscard]] std::size_t first_signal_index(std::span<const double> samples) {
  double peak = 0.0;
  for (const auto value : samples) {
    peak = std::max(peak, std::abs(value));
  }
  const double threshold = std::max(1.0e-5, peak * 0.01);
  for (std::size_t index = 0; index < samples.size(); ++index) {
    if (std::abs(samples[index]) >= threshold) {
      return index;
    }
  }
  return 0;
}

[[nodiscard]] double normalized_dot(std::span<const double> reference,
                                    std::span<const double> capture,
                                    std::size_t reference_start,
                                    std::size_t capture_start,
                                    std::size_t count,
                                    std::size_t stride) {
  double dot = 0.0;
  double reference_power = 0.0;
  double capture_power = 0.0;
  std::size_t used = 0;
  for (std::size_t n = 0; n < count; n += std::max<std::size_t>(1U, stride)) {
    const auto reference_index = reference_start + n;
    const auto capture_index = capture_start + n;
    if (reference_index >= reference.size() || capture_index >= capture.size()) {
      continue;
    }
    const auto rv = reference[reference_index];
    const auto cv = capture[capture_index];
    dot += rv * cv;
    reference_power += rv * rv;
    capture_power += cv * cv;
    used += 1U;
  }
  if (used == 0U || reference_power <= 0.0 || capture_power <= 0.0) {
    return -1.0;
  }
  return dot / std::sqrt(reference_power * capture_power);
}

[[nodiscard]] Alignment align(const StereoBuffer& reference, const StereoBuffer& capture) {
  const auto ref_mono = mono(reference.frames);
  const auto cap_mono = mono(capture.frames);
  auto ref_start = first_signal_index(ref_mono);
  auto cap_start = first_signal_index(cap_mono);
  const auto rate = reference.sample_rate;
  const auto max_lag = static_cast<std::int32_t>(std::max<std::uint32_t>(1U, rate / 10U));
  const auto base_count = std::min({ref_mono.size() - ref_start,
                                    cap_mono.size() - cap_start,
                                    static_cast<std::size_t>(rate * 2U)});
  if (base_count < std::max<std::size_t>(256U, rate / 20U)) {
    throw std::runtime_error("not_enough_audio_for_alignment");
  }
  const auto stride = std::max<std::size_t>(1U, base_count / 2048U);
  double best_score = -1.0;
  std::int32_t best_lag = 0;
  for (std::int32_t lag = -max_lag; lag <= max_lag; ++lag) {
    std::size_t candidate_ref = ref_start;
    std::size_t candidate_cap = cap_start;
    if (lag < 0) {
      const auto adjust = static_cast<std::size_t>(-lag);
      if (candidate_ref + adjust >= ref_mono.size()) {
        continue;
      }
      candidate_ref += adjust;
    } else {
      const auto adjust = static_cast<std::size_t>(lag);
      if (candidate_cap + adjust >= cap_mono.size()) {
        continue;
      }
      candidate_cap += adjust;
    }
    const auto score =
        normalized_dot(ref_mono, cap_mono, candidate_ref, candidate_cap, base_count, stride);
    if (score > best_score) {
      best_score = score;
      best_lag = lag;
    }
  }
  if (best_lag < 0) {
    ref_start += static_cast<std::size_t>(-best_lag);
  } else {
    cap_start += static_cast<std::size_t>(best_lag);
  }
  const auto compared = std::min(reference.frames.size() - ref_start,
                                 capture.frames.size() - cap_start);
  return {ref_start, cap_start, best_lag, best_score, compared};
}

[[nodiscard]] double rms(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (const auto value : values) {
    sum += value * value;
  }
  return std::sqrt(sum / static_cast<double>(values.size()));
}

[[nodiscard]] ChannelMetrics channel_metrics(const StereoBuffer& reference,
                                             const StereoBuffer& capture,
                                             const Alignment& alignment,
                                             std::uint32_t channel) {
  double ref_power = 0.0;
  double cap_power = 0.0;
  double dot = 0.0;
  double peak = 0.0;
  for (std::size_t index = 0; index < alignment.compared_frames; ++index) {
    const auto rv = reference.frames[alignment.reference_start + index][channel];
    const auto cv = capture.frames[alignment.capture_start + index][channel];
    ref_power += rv * rv;
    cap_power += cv * cv;
    dot += rv * cv;
    peak = std::max(peak, std::abs(cv));
  }
  ChannelMetrics metrics{};
  metrics.gain = ref_power > 0.0 ? dot / ref_power : 0.0;
  metrics.correlation =
      ref_power > 0.0 && cap_power > 0.0 ? dot / std::sqrt(ref_power * cap_power) : 0.0;
  metrics.capture_rms =
      alignment.compared_frames > 0U
          ? std::sqrt(cap_power / static_cast<double>(alignment.compared_frames))
          : 0.0;
  metrics.signal_rms =
      alignment.compared_frames > 0U
          ? std::abs(metrics.gain) *
                std::sqrt(ref_power / static_cast<double>(alignment.compared_frames))
          : 0.0;
  metrics.peak = peak;
  metrics.residual.reserve(alignment.compared_frames);
  for (std::size_t index = 0; index < alignment.compared_frames; ++index) {
    const auto rv = reference.frames[alignment.reference_start + index][channel];
    const auto cv = capture.frames[alignment.capture_start + index][channel];
    const auto residual = cv - (metrics.gain * rv);
    metrics.residual.push_back(residual);
    metrics.residual_peak = std::max(metrics.residual_peak, std::abs(residual));
  }
  metrics.residual_rms = rms(metrics.residual);
  metrics.snr_db = metrics.signal_rms > 0.0 && metrics.residual_rms > 0.0
                       ? 20.0 * std::log10(metrics.signal_rms / metrics.residual_rms)
                       : 999.0;
  return metrics;
}

[[nodiscard]] std::pair<std::uint32_t, double> click_count(std::span<const double> left,
                                                           std::span<const double> right) {
  if (left.size() < 3U || right.size() < 3U) {
    return {0U, 0.0};
  }
  std::vector<double> deltas;
  deltas.reserve(std::min(left.size(), right.size()) - 1U);
  for (std::size_t index = 1; index < std::min(left.size(), right.size()); ++index) {
    deltas.push_back(std::max(std::abs(left[index] - left[index - 1U]),
                              std::abs(right[index] - right[index - 1U])));
  }
  auto median_source = deltas;
  std::nth_element(median_source.begin(),
                   median_source.begin() + static_cast<std::ptrdiff_t>(median_source.size() / 2U),
                   median_source.end());
  const auto median = median_source[median_source.size() / 2U];
  std::vector<double> deviations;
  deviations.reserve(deltas.size());
  for (const auto delta : deltas) {
    deviations.push_back(std::abs(delta - median));
  }
  std::nth_element(deviations.begin(),
                   deviations.begin() + static_cast<std::ptrdiff_t>(deviations.size() / 2U),
                   deviations.end());
  const auto mad = std::max(deviations[deviations.size() / 2U], kEpsilon);
  const auto threshold = std::max(0.02, median + 18.0 * mad);
  std::uint32_t clicks = 0;
  bool in_click = false;
  std::uint32_t quiet = 0;
  for (const auto delta : deltas) {
    if (delta > threshold) {
      if (!in_click) {
        clicks += 1U;
        in_click = true;
      }
      quiet = 0;
    } else if (in_click) {
      quiet += 1U;
      if (quiet >= 2U) {
        in_click = false;
      }
    }
  }
  return {clicks, threshold};
}

[[nodiscard]] QualityMetrics analyze(const std::string& name,
                                     const StereoBuffer& reference,
                                     const StereoBuffer& capture,
                                     const Thresholds& thresholds) {
  if (reference.sample_rate != capture.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch");
  }
  const auto alignment = align(reference, capture);
  if (alignment.compared_frames < std::max<std::size_t>(256U, reference.sample_rate / 10U)) {
    throw std::runtime_error("not_enough_aligned_audio");
  }
  QualityMetrics metrics{};
  metrics.name = name;
  metrics.sample_rate = reference.sample_rate;
  metrics.reference_frames = reference.frames.size();
  metrics.capture_frames = capture.frames.size();
  metrics.alignment = alignment;
  metrics.left = channel_metrics(reference, capture, alignment, 0U);
  metrics.right = channel_metrics(reference, capture, alignment, 1U);
  metrics.min_snr_db = std::min(metrics.left.snr_db, metrics.right.snr_db);
  metrics.min_correlation = std::min(metrics.left.correlation, metrics.right.correlation);
  metrics.peak = std::max(metrics.left.peak, metrics.right.peak);
  metrics.residual_rms = std::max(metrics.left.residual_rms, metrics.right.residual_rms);
  metrics.residual_peak = std::max(metrics.left.residual_peak, metrics.right.residual_peak);
  const auto [clicks, threshold] = click_count(metrics.left.residual, metrics.right.residual);
  metrics.click_outliers = clicks;
  metrics.click_threshold = threshold;

  if (metrics.min_snr_db < thresholds.min_snr_db) {
    metrics.failures.push_back("snr_db");
  }
  if (metrics.min_correlation < thresholds.min_correlation) {
    metrics.failures.push_back("correlation");
  }
  if (metrics.click_outliers > thresholds.max_clicks) {
    metrics.failures.push_back("clicks");
  }
  return metrics;
}

[[nodiscard]] double signal_value(std::uint32_t frame, std::uint32_t rate, bool right) {
  const auto t = static_cast<double>(frame) / static_cast<double>(rate);
  const double phase = right ? 0.41 : 0.0;
  return 0.22 * std::sin(2.0 * kPi * 110.0 * t + phase) +
         0.13 * std::sin(2.0 * kPi * 997.0 * t + 0.17 + phase) +
         0.08 * std::sin(2.0 * kPi * 3137.0 * t + 0.73) +
         0.04 * std::sin(2.0 * kPi * 7210.0 * t + 1.10 + phase);
}

[[nodiscard]] StereoBuffer make_reference(std::uint32_t sample_rate, double seconds) {
  StereoBuffer buffer{};
  buffer.sample_rate = sample_rate;
  const auto frames = static_cast<std::size_t>(std::llround(seconds * sample_rate));
  buffer.frames.reserve(frames);
  for (std::size_t index = 0; index < frames; ++index) {
    const double fade = std::min({1.0,
                                  static_cast<double>(index) / (0.05 * sample_rate),
                                  static_cast<double>(frames - 1U - index) /
                                      (0.05 * sample_rate)});
    const auto frame = static_cast<std::uint32_t>(index);
    buffer.frames.push_back({fade * signal_value(frame, sample_rate, false),
                             fade * signal_value(frame, sample_rate, true)});
  }
  return buffer;
}

[[nodiscard]] StereoBuffer delayed_capture(const StereoBuffer& reference,
                                           std::uint32_t delay_frames,
                                           double gain,
                                           bool degraded) {
  StereoBuffer capture{};
  capture.sample_rate = reference.sample_rate;
  capture.frames.assign(delay_frames, {0.0, 0.0});
  capture.frames.reserve(reference.frames.size() + delay_frames);
  for (std::size_t index = 0; index < reference.frames.size(); ++index) {
    const double shaped_left = gain * reference.frames[index][0];
    const double shaped_right = gain * reference.frames[index][1];
    double left = shaped_left;
    double right = shaped_right;
    if (degraded) {
      left = shaped_left + 0.035 * std::sin(2.0 * kPi * 3800.0 *
                                           static_cast<double>(index) /
                                           static_cast<double>(reference.sample_rate));
      right = shaped_right + 0.025 * reference.frames[index][0];
      if (index == reference.sample_rate) {
        left += 0.25;
        right -= 0.25;
      }
    }
    capture.frames.push_back({left, right});
  }
  return capture;
}

void print_channel_json(const ChannelMetrics& metrics, const std::string& indent) {
  std::cout << indent << "\"gain\": " << metrics.gain << ", \"correlation\": "
            << metrics.correlation << ", \"signal_rms\": " << metrics.signal_rms
            << ", \"capture_rms\": " << metrics.capture_rms << ", \"residual_rms\": "
            << metrics.residual_rms << ", \"residual_peak\": " << metrics.residual_peak
            << ", \"snr_db\": " << metrics.snr_db << ", \"peak\": " << metrics.peak;
}

void print_failures(const std::vector<std::string>& failures) {
  std::cout << "[";
  for (std::size_t index = 0; index < failures.size(); ++index) {
    std::cout << (index == 0U ? "" : ", ") << "\"" << failures[index] << "\"";
  }
  std::cout << "]";
}

void print_metrics_json(const QualityMetrics& metrics,
                        bool expected_pass,
                        bool trailing_comma) {
  std::cout << "    {\n"
            << "      \"name\": \"" << metrics.name << "\",\n"
            << "      \"sample_rate\": " << metrics.sample_rate << ",\n"
            << "      \"reference_frames\": " << metrics.reference_frames << ",\n"
            << "      \"capture_frames\": " << metrics.capture_frames << ",\n"
            << "      \"reference_start\": " << metrics.alignment.reference_start << ",\n"
            << "      \"capture_start\": " << metrics.alignment.capture_start << ",\n"
            << "      \"lag_frames\": " << metrics.alignment.lag_frames << ",\n"
            << "      \"alignment_score\": " << metrics.alignment.score << ",\n"
            << "      \"compared_frames\": " << metrics.alignment.compared_frames << ",\n"
            << "      \"left\": {";
  print_channel_json(metrics.left, "");
  std::cout << "},\n"
            << "      \"right\": {";
  print_channel_json(metrics.right, "");
  std::cout << "},\n"
            << "      \"min_snr_db\": " << metrics.min_snr_db << ",\n"
            << "      \"min_correlation\": " << metrics.min_correlation << ",\n"
            << "      \"peak\": " << metrics.peak << ",\n"
            << "      \"residual_rms\": " << metrics.residual_rms << ",\n"
            << "      \"residual_peak\": " << metrics.residual_peak << ",\n"
            << "      \"click_outliers\": " << metrics.click_outliers << ",\n"
            << "      \"click_threshold\": " << metrics.click_threshold << ",\n"
            << "      \"expected_pass\": " << (expected_pass ? "true" : "false") << ",\n"
            << "      \"actual_pass\": " << (metrics.passed() ? "true" : "false") << ",\n"
            << "      \"failures\": ";
  print_failures(metrics.failures);
  std::cout << ",\n"
            << "      \"result\": \""
            << (metrics.passed() == expected_pass ? "PASS" : "FAIL") << "\"\n"
            << "    }" << (trailing_comma ? ",\n" : "\n");
}

[[nodiscard]] int run_selftest() {
  Thresholds thresholds{};
  thresholds.min_snr_db = 70.0;
  thresholds.min_correlation = 0.995;
  thresholds.max_clicks = 0;
  const auto reference = make_reference(48000U, 4.0);
  const auto clean = analyze("clean_loopback", reference, delayed_capture(reference, 123U, 0.94, false),
                             thresholds);
  const auto degraded =
      analyze("degraded_loopback", reference, delayed_capture(reference, 321U, 0.94, true),
              thresholds);
  const bool clean_ok = clean.passed();
  const bool degraded_ok = !degraded.passed();
  const bool pass = clean_ok && degraded_ok;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.loopback-quality-analysis.v1\",\n"
            << "  \"mode\": \"selftest\",\n"
            << "  \"threshold_min_snr_db\": " << thresholds.min_snr_db << ",\n"
            << "  \"threshold_min_correlation\": " << thresholds.min_correlation << ",\n"
            << "  \"threshold_max_clicks\": " << thresholds.max_clicks << ",\n"
            << "  \"rows\": [\n";
  print_metrics_json(clean, true, true);
  print_metrics_json(degraded, false, false);
  std::cout << "  ],\n"
            << "  \"row_count\": 2,\n"
            << "  \"failures\": " << (pass ? 0 : 1) << ",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}

[[nodiscard]] Cli parse_cli(int argc, char** argv) {
  Cli cli{};
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    auto next = [&]() -> std::string {
      if (index + 1 >= argc) {
        throw std::runtime_error("missing_value_for:" + arg);
      }
      index += 1;
      return argv[index];
    };
    if (arg == "--reference-wav") {
      cli.reference_wav = next();
    } else if (arg == "--capture-wav") {
      cli.capture_wav = next();
    } else if (arg == "--captured-f32") {
      cli.captured_f32 = next();
    } else if (arg == "--sample-rate") {
      cli.sample_rate = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--channels") {
      cli.capture_channels = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--pair") {
      cli.pair = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--min-snr-db") {
      cli.thresholds.min_snr_db = std::stod(next());
    } else if (arg == "--min-correlation") {
      cli.thresholds.min_correlation = std::stod(next());
    } else if (arg == "--max-clicks") {
      cli.thresholds.max_clicks = static_cast<std::uint32_t>(std::stoul(next()));
    } else {
      throw std::runtime_error("unknown_arg:" + arg);
    }
  }
  return cli;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 1) {
      return run_selftest();
    }
    const auto cli = parse_cli(argc, argv);
    if (cli.reference_wav.empty()) {
      throw std::runtime_error("missing_reference_wav");
    }
    const auto reference = read_wav_pair(cli.reference_wav);
    const auto capture = !cli.capture_wav.empty()
                             ? read_wav_pair(cli.capture_wav)
                             : read_raw_f32_pair(cli.captured_f32, cli.sample_rate,
                                                 cli.capture_channels, cli.pair);
    const auto metrics = analyze("capture", reference, capture, cli.thresholds);
    std::cout << "{\n"
              << "  \"schema\": \"opena8djcpp.loopback-quality-analysis.v1\",\n"
              << "  \"mode\": \"capture\",\n"
              << "  \"threshold_min_snr_db\": " << cli.thresholds.min_snr_db << ",\n"
              << "  \"threshold_min_correlation\": " << cli.thresholds.min_correlation << ",\n"
              << "  \"threshold_max_clicks\": " << cli.thresholds.max_clicks << ",\n"
              << "  \"rows\": [\n";
    print_metrics_json(metrics, true, false);
    std::cout << "  ],\n"
              << "  \"row_count\": 1,\n"
              << "  \"failures\": " << (metrics.passed() ? 0 : 1) << ",\n"
              << "  \"result\": \"" << (metrics.passed() ? "PASS" : "FAIL") << "\"\n"
              << "}\n";
    return metrics.passed() ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "error=" << error.what() << "\n";
    return 2;
  }
}
