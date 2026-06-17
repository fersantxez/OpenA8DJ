#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kEpsilon = 1.0e-20;
constexpr std::array<double, 5> kLeftTones{110.0, 440.0, 997.0, 3137.0, 7210.0};
constexpr std::array<double, 5> kRightTones{173.0, 661.0, 1663.0, 5003.0, 9181.0};

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
  std::uint32_t source_channels = 2;
};

struct Thresholds {
  double min_snr_db = 45.0;
  double min_correlation = 0.98;
  std::uint32_t max_clicks = 0;
  double max_leakage_db = -45.0;
  double min_expected_amplitude = 0.005;
  std::uint32_t max_clipped_frames = 0;
};

struct Cli {
  std::vector<std::filesystem::path> run_dirs;
  Thresholds thresholds;
  double analysis_seconds = 8.0;
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

struct ToneMetrics {
  std::array<double, kLeftTones.size()> left_tones{};
  std::array<double, kRightTones.size()> right_tones{};
};

struct RunMetrics {
  std::filesystem::path run_dir;
  std::uint32_t sample_rate = 0;
  std::size_t reference_frames = 0;
  std::size_t capture_frames = 0;
  std::size_t analysis_frames = 0;
  Alignment alignment;
  ChannelMetrics left;
  ChannelMetrics right;
  ToneMetrics reference_left;
  ToneMetrics reference_right;
  ToneMetrics capture_left;
  ToneMetrics capture_right;
  double min_snr_db = 0.0;
  double min_correlation = 0.0;
  std::uint32_t click_outliers = 0;
  double click_threshold = 0.0;
  double left_expected_max_amplitude = 0.0;
  double right_expected_max_amplitude = 0.0;
  double expected_floor_amplitude = 0.0;
  double left_to_right_leakage_db = -240.0;
  double right_to_left_leakage_db = -240.0;
  double max_wrong_source_leakage_db = -240.0;
  std::uint32_t capture_clipped_frames = 0;
  bool expected_pass = true;
  std::vector<std::string> failures;

  [[nodiscard]] bool passed() const { return failures.empty(); }
  [[nodiscard]] bool matched_expectation() const { return passed() == expected_pass; }
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

[[nodiscard]] std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot_open:" + path.string());
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

[[nodiscard]] StereoBuffer read_wav_pair(const std::filesystem::path& path) {
  const auto bytes = read_file_bytes(path);
  if (bytes.size() < 44U || std::memcmp(bytes.data(), "RIFF", 4U) != 0 ||
      std::memcmp(bytes.data() + 8U, "WAVE", 4U) != 0) {
    throw std::runtime_error("invalid_wav:" + path.string());
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
      throw std::runtime_error("truncated_wav_chunk:" + path.string());
    }
    if (chunk_id == "fmt ") {
      if (chunk_size < 16U) {
        throw std::runtime_error("invalid_fmt_chunk:" + path.string());
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
    throw std::runtime_error("missing_wav_audio:" + path.string());
  }
  if (bits_per_sample % 8U != 0U) {
    throw std::runtime_error("unsupported_wav_bits");
  }

  StereoBuffer buffer{};
  buffer.sample_rate = sample_rate;
  buffer.source_channels = channels;
  const auto frames = data_size / static_cast<std::size_t>(block_align);
  const auto bytes_per_sample = static_cast<std::size_t>(bits_per_sample / 8U);
  buffer.frames.reserve(frames);
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

[[nodiscard]] std::vector<double> mono(const std::vector<std::array<double, 2>>& frames) {
  std::vector<double> out;
  out.reserve(frames.size());
  for (const auto& frame : frames) {
    out.push_back(0.5 * (frame[0] + frame[1]));
  }
  return out;
}

[[nodiscard]] std::size_t first_signal_index(std::span<const double> samples) {
  if (samples.empty()) {
    return 0;
  }
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
  if (ref_start >= ref_mono.size() || cap_start >= cap_mono.size()) {
    throw std::runtime_error("not_enough_audio_for_alignment");
  }

  const auto rate = reference.sample_rate;
  const auto max_lag = static_cast<std::int32_t>(std::max<std::uint32_t>(1U, rate / 2U));
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

[[nodiscard]] double ratio_db(double numerator, double denominator) {
  if (numerator <= 0.0) {
    return -240.0;
  }
  if (denominator <= 0.0) {
    return 240.0;
  }
  return 20.0 * std::log10(numerator / denominator);
}

[[nodiscard]] double max_value(const std::array<double, kLeftTones.size()>& values) {
  return *std::max_element(values.begin(), values.end());
}

[[nodiscard]] ChannelMetrics channel_metrics(const StereoBuffer& reference,
                                             const StereoBuffer& capture,
                                             const Alignment& alignment,
                                             std::size_t analysis_frames,
                                             std::uint32_t channel) {
  double ref_power = 0.0;
  double cap_power = 0.0;
  double dot = 0.0;
  double peak = 0.0;
  const auto count = std::min(analysis_frames, alignment.compared_frames);
  for (std::size_t index = 0; index < count; ++index) {
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
  metrics.capture_rms = count > 0U ? std::sqrt(cap_power / static_cast<double>(count)) : 0.0;
  metrics.signal_rms =
      count > 0U ? std::abs(metrics.gain) * std::sqrt(ref_power / static_cast<double>(count))
                 : 0.0;
  metrics.peak = peak;
  metrics.residual.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
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

[[nodiscard]] double sample_channel(const StereoBuffer& buffer,
                                    std::size_t start,
                                    std::size_t index,
                                    std::uint32_t channel) {
  return buffer.frames[start + index][channel];
}

template <std::size_t Size>
[[nodiscard]] std::array<double, Size> tone_set(const StereoBuffer& buffer,
                                                std::size_t start,
                                                std::size_t count,
                                                std::uint32_t channel,
                                                const std::array<double, Size>& tones) {
  std::array<double, Size> amplitudes{};
  if (count == 0U) {
    return amplitudes;
  }
  for (std::size_t tone_index = 0; tone_index < tones.size(); ++tone_index) {
    double cosine = 0.0;
    double sine = 0.0;
    const auto frequency = tones[tone_index];
    for (std::size_t index = 0; index < count; ++index) {
      const auto phase = 2.0 * kPi * frequency * static_cast<double>(index) /
                         static_cast<double>(buffer.sample_rate);
      const auto sample = sample_channel(buffer, start, index, channel);
      cosine += sample * std::cos(phase);
      sine += sample * std::sin(phase);
    }
    amplitudes[tone_index] = 2.0 * std::hypot(cosine, sine) / static_cast<double>(count);
  }
  return amplitudes;
}

[[nodiscard]] std::uint32_t clipped_frames(const StereoBuffer& capture,
                                           std::size_t capture_start,
                                           std::size_t count) {
  std::uint32_t clipped = 0;
  for (std::size_t index = 0; index < count; ++index) {
    const auto& frame = capture.frames[capture_start + index];
    if (std::abs(frame[0]) >= 0.999 || std::abs(frame[1]) >= 0.999) {
      clipped += 1U;
    }
  }
  return clipped;
}

[[nodiscard]] RunMetrics analyze_run(const std::filesystem::path& run_dir,
                                     const StereoBuffer& reference,
                                     const StereoBuffer& capture,
                                     const Thresholds& thresholds,
                                     double analysis_seconds) {
  if (reference.sample_rate != capture.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch");
  }
  const auto alignment = align(reference, capture);
  const auto requested_frames =
      analysis_seconds > 0.0
          ? static_cast<std::size_t>(std::llround(analysis_seconds * reference.sample_rate))
          : alignment.compared_frames;
  const auto analysis_frames = std::min(requested_frames, alignment.compared_frames);
  if (analysis_frames < std::max<std::size_t>(256U, reference.sample_rate / 10U)) {
    throw std::runtime_error("not_enough_aligned_audio");
  }

  RunMetrics metrics{};
  metrics.run_dir = run_dir;
  metrics.sample_rate = reference.sample_rate;
  metrics.reference_frames = reference.frames.size();
  metrics.capture_frames = capture.frames.size();
  metrics.analysis_frames = analysis_frames;
  metrics.alignment = alignment;
  metrics.left = channel_metrics(reference, capture, alignment, analysis_frames, 0U);
  metrics.right = channel_metrics(reference, capture, alignment, analysis_frames, 1U);
  metrics.min_snr_db = std::min(metrics.left.snr_db, metrics.right.snr_db);
  metrics.min_correlation = std::min(metrics.left.correlation, metrics.right.correlation);
  const auto [clicks, click_threshold] = click_count(metrics.left.residual, metrics.right.residual);
  metrics.click_outliers = clicks;
  metrics.click_threshold = click_threshold;

  metrics.reference_left.left_tones =
      tone_set(reference, alignment.reference_start, analysis_frames, 0U, kLeftTones);
  metrics.reference_left.right_tones =
      tone_set(reference, alignment.reference_start, analysis_frames, 0U, kRightTones);
  metrics.reference_right.left_tones =
      tone_set(reference, alignment.reference_start, analysis_frames, 1U, kLeftTones);
  metrics.reference_right.right_tones =
      tone_set(reference, alignment.reference_start, analysis_frames, 1U, kRightTones);
  metrics.capture_left.left_tones =
      tone_set(capture, alignment.capture_start, analysis_frames, 0U, kLeftTones);
  metrics.capture_left.right_tones =
      tone_set(capture, alignment.capture_start, analysis_frames, 0U, kRightTones);
  metrics.capture_right.left_tones =
      tone_set(capture, alignment.capture_start, analysis_frames, 1U, kLeftTones);
  metrics.capture_right.right_tones =
      tone_set(capture, alignment.capture_start, analysis_frames, 1U, kRightTones);

  metrics.left_expected_max_amplitude = max_value(metrics.capture_left.left_tones);
  metrics.right_expected_max_amplitude = max_value(metrics.capture_right.right_tones);
  metrics.expected_floor_amplitude =
      std::min(metrics.left_expected_max_amplitude, metrics.right_expected_max_amplitude);
  const auto left_to_right = max_value(metrics.capture_right.left_tones);
  const auto right_to_left = max_value(metrics.capture_left.right_tones);
  const auto max_wrong_source = std::max(left_to_right, right_to_left);
  metrics.left_to_right_leakage_db = ratio_db(left_to_right, metrics.left_expected_max_amplitude);
  metrics.right_to_left_leakage_db =
      ratio_db(right_to_left, metrics.right_expected_max_amplitude);
  metrics.max_wrong_source_leakage_db =
      ratio_db(max_wrong_source,
               std::max(metrics.left_expected_max_amplitude, metrics.right_expected_max_amplitude));
  metrics.capture_clipped_frames = clipped_frames(capture, alignment.capture_start, analysis_frames);

  if (metrics.min_snr_db < thresholds.min_snr_db) {
    metrics.failures.push_back("snr_db");
  }
  if (metrics.min_correlation < thresholds.min_correlation) {
    metrics.failures.push_back("correlation");
  }
  if (metrics.click_outliers > thresholds.max_clicks) {
    metrics.failures.push_back("clicks");
  }
  if (metrics.expected_floor_amplitude < thresholds.min_expected_amplitude) {
    metrics.failures.push_back("expected_amplitude");
  }
  if (std::max({metrics.left_to_right_leakage_db,
                metrics.right_to_left_leakage_db,
                metrics.max_wrong_source_leakage_db}) > thresholds.max_leakage_db) {
    metrics.failures.push_back("leakage");
  }
  if (metrics.capture_clipped_frames > thresholds.max_clipped_frames) {
    metrics.failures.push_back("clipped_frames");
  }
  return metrics;
}

[[nodiscard]] double reference_signal(std::uint32_t frame, std::uint32_t rate, bool right) {
  const auto t = static_cast<double>(frame) / static_cast<double>(rate);
  const auto& tones = right ? kRightTones : kLeftTones;
  double value = 0.0;
  for (std::size_t index = 0; index < tones.size(); ++index) {
    value += 0.06 * std::sin(2.0 * kPi * tones[index] * t + 0.19 * static_cast<double>(index));
  }
  return value;
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
    buffer.frames.push_back({fade * reference_signal(frame, sample_rate, false),
                             fade * reference_signal(frame, sample_rate, true)});
  }
  return buffer;
}

[[nodiscard]] StereoBuffer make_capture(const StereoBuffer& reference,
                                        std::uint32_t delay_frames,
                                        bool degraded) {
  StereoBuffer capture{};
  capture.sample_rate = reference.sample_rate;
  capture.frames.assign(delay_frames, {0.0, 0.0});
  capture.frames.reserve(reference.frames.size() + delay_frames);
  for (std::size_t index = 0; index < reference.frames.size(); ++index) {
    double left = 0.8 * reference.frames[index][0];
    double right = 0.8 * reference.frames[index][1];
    if (degraded) {
      left += 0.08 * reference.frames[index][1];
      right += 0.08 * reference.frames[index][0];
      if (index == reference.sample_rate) {
        left = 1.0;
        right = -1.0;
      }
    }
    capture.frames.push_back({left, right});
  }
  return capture;
}

void print_string_array(const std::vector<std::string>& values) {
  std::cout << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::cout << (index == 0U ? "" : ", ") << "\"" << values[index] << "\"";
  }
  std::cout << "]";
}

template <std::size_t Size>
void print_tone_map(const std::array<double, Size>& tones,
                    const std::array<double, Size>& values,
                    const std::string& indent) {
  std::cout << "{\n";
  for (std::size_t index = 0; index < tones.size(); ++index) {
    std::cout << indent << "  \"" << static_cast<std::uint32_t>(std::llround(tones[index]))
              << "\": " << values[index] << (index + 1U < tones.size() ? "," : "") << "\n";
  }
  std::cout << indent << "}";
}

void print_channel_metrics(const ChannelMetrics& metrics) {
  std::cout << "{\"gain\": " << metrics.gain << ", \"correlation\": " << metrics.correlation
            << ", \"signal_rms\": " << metrics.signal_rms << ", \"capture_rms\": "
            << metrics.capture_rms << ", \"residual_rms\": " << metrics.residual_rms
            << ", \"residual_peak\": " << metrics.residual_peak << ", \"snr_db\": "
            << metrics.snr_db << ", \"peak\": " << metrics.peak << "}";
}

void print_tone_metrics(const ToneMetrics& tones, const std::string& indent) {
  std::cout << "{\n" << indent << "  \"left_tones\": ";
  print_tone_map(kLeftTones, tones.left_tones, indent + "  ");
  std::cout << ",\n" << indent << "  \"right_tones\": ";
  print_tone_map(kRightTones, tones.right_tones, indent + "  ");
  std::cout << "\n" << indent << "}";
}

void print_run(const RunMetrics& metrics, bool trailing_comma) {
  std::cout << "    {\n"
            << "      \"run_dir\": \"" << metrics.run_dir.string() << "\",\n"
            << "      \"result\": \"" << (metrics.matched_expectation() ? "PASS" : "FAIL")
            << "\",\n"
            << "      \"expected_pass\": " << (metrics.expected_pass ? "true" : "false") << ",\n"
            << "      \"actual_pass\": " << (metrics.passed() ? "true" : "false") << ",\n"
            << "      \"sample_rate\": " << metrics.sample_rate << ",\n"
            << "      \"reference_frames\": " << metrics.reference_frames << ",\n"
            << "      \"capture_frames\": " << metrics.capture_frames << ",\n"
            << "      \"analysis_frames\": " << metrics.analysis_frames << ",\n"
            << "      \"alignment\": {\"reference_start\": " << metrics.alignment.reference_start
            << ", \"capture_start\": " << metrics.alignment.capture_start
            << ", \"lag_frames\": " << metrics.alignment.lag_frames
            << ", \"score\": " << metrics.alignment.score
            << ", \"compared_frames\": " << metrics.alignment.compared_frames << "},\n"
            << "      \"channels\": {\"left\": ";
  print_channel_metrics(metrics.left);
  std::cout << ", \"right\": ";
  print_channel_metrics(metrics.right);
  std::cout << "},\n"
            << "      \"reference\": {\"left_channel\": ";
  print_tone_metrics(metrics.reference_left, "      ");
  std::cout << ", \"right_channel\": ";
  print_tone_metrics(metrics.reference_right, "      ");
  std::cout << "},\n"
            << "      \"capture\": {\"left_channel\": ";
  print_tone_metrics(metrics.capture_left, "      ");
  std::cout << ", \"right_channel\": ";
  print_tone_metrics(metrics.capture_right, "      ");
  std::cout << "},\n"
            << "      \"metrics\": {\"min_snr_db\": " << metrics.min_snr_db
            << ", \"min_correlation\": " << metrics.min_correlation
            << ", \"click_outliers\": " << metrics.click_outliers
            << ", \"click_threshold\": " << metrics.click_threshold
            << ", \"left_expected_max_amplitude\": " << metrics.left_expected_max_amplitude
            << ", \"right_expected_max_amplitude\": " << metrics.right_expected_max_amplitude
            << ", \"expected_floor_amplitude\": " << metrics.expected_floor_amplitude
            << ", \"left_to_right_leakage_db\": " << metrics.left_to_right_leakage_db
            << ", \"right_to_left_leakage_db\": " << metrics.right_to_left_leakage_db
            << ", \"max_wrong_source_leakage_db\": " << metrics.max_wrong_source_leakage_db
            << ", \"capture_clipped_frames\": " << metrics.capture_clipped_frames << "},\n"
            << "      \"failures\": ";
  print_string_array(metrics.failures);
  std::cout << "\n"
            << "    }" << (trailing_comma ? "," : "") << "\n";
}

void print_report(const std::vector<RunMetrics>& runs,
                  const Thresholds& thresholds,
                  double analysis_seconds,
                  const std::string& mode) {
  std::uint32_t failures = 0;
  for (const auto& run : runs) {
    if (!run.matched_expectation()) {
      failures += 1U;
    }
  }
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.capture-matrix-quality-analysis.v1\",\n"
            << "  \"mode\": \"" << mode << "\",\n"
            << "  \"analysis_seconds\": " << analysis_seconds << ",\n"
            << "  \"thresholds\": {\"min_snr_db\": " << thresholds.min_snr_db
            << ", \"min_correlation\": " << thresholds.min_correlation
            << ", \"max_clicks\": " << thresholds.max_clicks
            << ", \"max_leakage_db\": " << thresholds.max_leakage_db
            << ", \"min_expected_amplitude\": " << thresholds.min_expected_amplitude
            << ", \"max_clipped_frames\": " << thresholds.max_clipped_frames << "},\n"
            << "  \"runs\": [\n";
  for (std::size_t index = 0; index < runs.size(); ++index) {
    print_run(runs[index], index + 1U < runs.size());
  }
  std::cout << "  ],\n"
            << "  \"run_count\": " << runs.size() << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (failures == 0U ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
}

[[nodiscard]] int run_selftest() {
  Thresholds thresholds{};
  thresholds.min_snr_db = 65.0;
  thresholds.min_correlation = 0.995;
  thresholds.max_leakage_db = -30.0;
  thresholds.min_expected_amplitude = 0.02;
  thresholds.max_clipped_frames = 0;
  const auto reference = make_reference(48000U, 4.0);
  auto clean = analyze_run("selftest_clean", reference, make_capture(reference, 127U, false),
                           thresholds, 3.0);
  auto degraded = analyze_run("selftest_degraded", reference, make_capture(reference, 257U, true),
                              thresholds, 3.0);
  degraded.expected_pass = false;
  std::vector<RunMetrics> rows{clean, degraded};
  print_report(rows, thresholds, 3.0, "selftest");
  return clean.matched_expectation() && degraded.matched_expectation() ? 0 : 1;
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
    if (arg == "--min-snr-db") {
      cli.thresholds.min_snr_db = std::stod(next());
    } else if (arg == "--min-correlation") {
      cli.thresholds.min_correlation = std::stod(next());
    } else if (arg == "--max-clicks") {
      cli.thresholds.max_clicks = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--max-leakage-db") {
      cli.thresholds.max_leakage_db = std::stod(next());
    } else if (arg == "--min-expected-amplitude") {
      cli.thresholds.min_expected_amplitude = std::stod(next());
    } else if (arg == "--max-clipped-frames") {
      cli.thresholds.max_clipped_frames = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--analysis-seconds") {
      cli.analysis_seconds = std::stod(next());
    } else if (arg == "--help" || arg == "-h") {
      throw std::runtime_error(
          "usage: opena8djcpp_capture_matrix_quality_analysis [thresholds] RUN_DIR...");
    } else if (!arg.empty() && arg[0] == '-') {
      throw std::runtime_error("unknown_arg:" + arg);
    } else {
      cli.run_dirs.emplace_back(arg);
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
    if (cli.run_dirs.empty()) {
      throw std::runtime_error("missing_run_dir");
    }

    std::vector<RunMetrics> runs;
    runs.reserve(cli.run_dirs.size());
    for (const auto& run_dir : cli.run_dirs) {
      const auto reference = read_wav_pair(run_dir / "fixture" / "reference.wav");
      const auto capture = read_wav_pair(run_dir / "captured.wav");
      runs.push_back(analyze_run(run_dir, reference, capture, cli.thresholds, cli.analysis_seconds));
    }
    print_report(runs, cli.thresholds, cli.analysis_seconds, "capture");
    for (const auto& run : runs) {
      if (!run.passed()) {
        return 1;
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error=" << error.what() << "\n";
    return 2;
  }
}
