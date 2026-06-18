#include "evidence_json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kEpsilon = 1.0e-24;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
};

struct Cli {
  std::filesystem::path json_out;
  double analysis_seconds = 12.0;
  double window_seconds = 0.25;
  double hop_seconds = 0.125;
  std::int32_t max_lag = 64;
  std::int32_t median_filter_windows = 5;
  double partial_improvement_db = 3.0;
  double strong_improvement_db = 6.0;
  bool self_test = false;
  std::vector<std::filesystem::path> soundcheck_dirs;
};

struct AlignedRun {
  std::filesystem::path run_dir;
  std::uint32_t rate = 0;
  std::vector<std::array<double, 2>> reference;
  std::vector<std::array<double, 2>> capture;
  double source_quality_alignment_score = 0.0;
  double source_lag_jumps_gt_2_frames = 0.0;
  double source_mid_band_residual_ratio = 0.0;
  double source_high_band_residual_ratio = 0.0;
};

struct FitResult {
  double gain = 0.0;
  double snr_db = 0.0;
  double signal_rms = 0.0;
  double residual_rms = 0.0;
};

struct MatrixFitResult {
  std::array<std::array<double, 2>, 2> matrix{};
  double snr_db = 0.0;
  double signal_rms = 0.0;
  double residual_rms = 0.0;
};

struct WindowDelay {
  double lag = 0.0;
  double score = 0.0;
};

double rms(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  long double sum = 0.0;
  for (const double value : values) {
    sum += static_cast<long double>(value) * value;
  }
  return std::sqrt(static_cast<double>(sum / values.size()));
}

double rms_stereo(std::span<const std::array<double, 2>> frames) {
  if (frames.empty()) {
    return 0.0;
  }
  long double sum = 0.0;
  for (const auto& frame : frames) {
    sum += static_cast<long double>(frame[0]) * frame[0];
    sum += static_cast<long double>(frame[1]) * frame[1];
  }
  return std::sqrt(static_cast<double>(sum / (frames.size() * 2U)));
}

double db20(double value) {
  if (value <= 0.0 || !std::isfinite(value)) {
    return -240.0;
  }
  return 20.0 * std::log10(std::max(value, kEpsilon));
}

double dot(std::span<const double> a, std::span<const double> b) {
  const auto count = std::min(a.size(), b.size());
  long double sum = 0.0;
  for (std::size_t index = 0; index < count; ++index) {
    sum += static_cast<long double>(a[index]) * b[index];
  }
  return static_cast<double>(sum);
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset]) |
         static_cast<std::uint16_t>(data[offset + 1U] << 8U);
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
         (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
         (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot_open:" + path.string());
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string read_file_text(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("cannot_open:" + path.string());
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::int32_t sign_extend_24(std::uint32_t value) {
  if ((value & 0x00800000U) != 0U) {
    value |= 0xff000000U;
  }
  return static_cast<std::int32_t>(value);
}

double decode_pcm_sample(const std::vector<std::uint8_t>& data,
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

StereoBuffer read_wav_pair(const std::filesystem::path& path) {
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
  const auto bytes_per_sample = static_cast<std::size_t>((bits_per_sample + 7U) / 8U);
  if (bytes_per_sample == 0U || block_align < channels * bytes_per_sample) {
    throw std::runtime_error("invalid_wav_layout:" + path.string());
  }

  StereoBuffer out{};
  out.sample_rate = sample_rate;
  const auto frames = data_size / block_align;
  out.frames.reserve(frames);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const auto base = data_offset + frame * block_align;
    const double left = decode_pcm_sample(bytes, base, format, bits_per_sample);
    const double right = channels > 1U
                             ? decode_pcm_sample(bytes, base + bytes_per_sample, format,
                                                 bits_per_sample)
                             : left;
    out.frames.push_back({left, right});
  }
  return out;
}

std::filesystem::path find_reference(const std::filesystem::path& run_dir) {
  const auto reference = run_dir / "fixture/reference.wav";
  if (std::filesystem::is_regular_file(reference)) {
    return reference;
  }
  throw std::runtime_error("missing_fixture_reference:" + run_dir.string());
}

double json_number_or(std::string_view json, std::string_view key, double fallback) {
  return opena8djcpp::evidence_json::json_number(json, key).value_or(fallback);
}

std::vector<std::array<double, 2>> slice(const std::vector<std::array<double, 2>>& frames,
                                         std::size_t start,
                                         std::size_t count) {
  if (start > frames.size()) {
    return {};
  }
  const auto end = std::min(frames.size(), start + count);
  return {frames.begin() + static_cast<std::ptrdiff_t>(start),
          frames.begin() + static_cast<std::ptrdiff_t>(end)};
}

AlignedRun load_run(const std::filesystem::path& run_dir, const Cli& cli) {
  const auto metrics_json = read_file_text(run_dir / "metrics.json");
  const auto reference_wav = read_wav_pair(find_reference(run_dir));
  const auto capture_wav = read_wav_pair(run_dir / "captured.wav");
  if (reference_wav.sample_rate != capture_wav.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch:" + run_dir.string());
  }

  const auto ref_start = static_cast<std::size_t>(std::max(
      0.0, json_number_or(metrics_json, "reference_start", 0.0)));
  const auto cap_start = static_cast<std::size_t>(std::max(
      0.0, json_number_or(metrics_json, "capture_start", 0.0)));
  std::size_t usable = static_cast<std::size_t>(std::max(
      0.0, json_number_or(metrics_json, "compared_frames", 0.0)));
  if (cli.analysis_seconds > 0.0) {
    usable = std::min(usable, static_cast<std::size_t>(
                                  std::llround(cli.analysis_seconds * reference_wav.sample_rate)));
  }
  usable = std::min({usable, reference_wav.frames.size() - std::min(ref_start, reference_wav.frames.size()),
                     capture_wav.frames.size() - std::min(cap_start, capture_wav.frames.size())});
  if (usable <= reference_wav.sample_rate) {
    throw std::runtime_error("not_enough_aligned_audio:" + run_dir.string());
  }

  AlignedRun out{};
  out.run_dir = run_dir;
  out.rate = reference_wav.sample_rate;
  out.reference = slice(reference_wav.frames, ref_start, usable);
  out.capture = slice(capture_wav.frames, cap_start, usable);
  out.source_quality_alignment_score =
      json_number_or(metrics_json, "quality_alignment_score", 0.0);
  out.source_lag_jumps_gt_2_frames =
      json_number_or(metrics_json, "lag_jumps_gt_2_frames", 0.0);
  out.source_mid_band_residual_ratio =
      json_number_or(metrics_json, "mid_band_residual_ratio", 0.0);
  out.source_high_band_residual_ratio =
      json_number_or(metrics_json, "high_band_residual_ratio", 0.0);
  return out;
}

std::vector<double> mono(std::span<const std::array<double, 2>> frames) {
  std::vector<double> out;
  out.reserve(frames.size());
  for (const auto& frame : frames) {
    out.push_back((frame[0] + frame[1]) * 0.5);
  }
  return out;
}

double mean(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

std::vector<double> zero_mean(std::span<const double> values) {
  std::vector<double> out(values.begin(), values.end());
  const double center = mean(values);
  for (double& value : out) {
    value -= center;
  }
  return out;
}

double sample_at(std::span<const double> values, double index) {
  if (index < 0.0 || index > static_cast<double>(values.size() - 1U)) {
    return 0.0;
  }
  const auto low = static_cast<std::size_t>(std::floor(index));
  const auto high = std::min(low + 1U, values.size() - 1U);
  const double frac = index - static_cast<double>(low);
  return values[low] * (1.0 - frac) + values[high] * frac;
}

std::vector<double> shifted(std::span<const double> input, double delay) {
  std::vector<double> out(input.size(), 0.0);
  for (std::size_t index = 0; index < input.size(); ++index) {
    out[index] = sample_at(input, static_cast<double>(index) + delay);
  }
  return out;
}

std::vector<std::array<double, 2>> shifted_stereo(
    std::span<const std::array<double, 2>> input,
    double delay) {
  std::vector<double> left;
  std::vector<double> right;
  left.reserve(input.size());
  right.reserve(input.size());
  for (const auto& frame : input) {
    left.push_back(frame[0]);
    right.push_back(frame[1]);
  }
  const auto shifted_left = shifted(left, delay);
  const auto shifted_right = shifted(right, delay);
  std::vector<std::array<double, 2>> out;
  out.reserve(input.size());
  for (std::size_t index = 0; index < input.size(); ++index) {
    out.push_back({shifted_left[index], shifted_right[index]});
  }
  return out;
}

double fractional_peak(std::span<const double> corr, std::size_t index) {
  if (index == 0U || index + 1U >= corr.size()) {
    return static_cast<double>(index);
  }
  const double left = std::abs(corr[index - 1U]);
  const double center = std::abs(corr[index]);
  const double right = std::abs(corr[index + 1U]);
  const double denom = left - 2.0 * center + right;
  if (std::abs(denom) <= 1.0e-18) {
    return static_cast<double>(index);
  }
  const double offset = std::clamp(0.5 * (left - right) / denom, -0.5, 0.5);
  return static_cast<double>(index) + offset;
}

WindowDelay best_window_delay(std::span<const double> ref_mono,
                              std::span<const double> cap_mono,
                              std::int32_t max_lag) {
  auto ref_zero = zero_mean(ref_mono);
  auto cap_zero = zero_mean(cap_mono);
  if (ref_zero.size() < 4U || cap_zero.size() != ref_zero.size()) {
    return {};
  }
  const auto pad = static_cast<std::size_t>(max_lag + 2);
  std::vector<double> padded(ref_zero.size() + pad * 2U, 0.0);
  std::copy(ref_zero.begin(), ref_zero.end(), padded.begin() + static_cast<std::ptrdiff_t>(pad));

  std::vector<double> corr(padded.size() - cap_zero.size() + 1U, 0.0);
  for (std::size_t offset = 0; offset < corr.size(); ++offset) {
    long double sum = 0.0;
    for (std::size_t index = 0; index < cap_zero.size(); ++index) {
      sum += static_cast<long double>(padded[offset + index]) * cap_zero[index];
    }
    corr[offset] = static_cast<double>(sum);
  }
  const auto peak = static_cast<std::size_t>(
      std::distance(corr.begin(), std::max_element(corr.begin(), corr.end(), [](double a, double b) {
        return std::abs(a) < std::abs(b);
      })));
  const double refined = fractional_peak(corr, peak);
  const double lag = refined - static_cast<double>(pad);
  const auto shifted_ref = shifted(ref_zero, lag);
  const double denom =
      std::sqrt(std::max(dot(shifted_ref, shifted_ref), 0.0) * std::max(dot(cap_zero, cap_zero), 0.0));
  const double score = denom > 0.0 ? dot(shifted_ref, cap_zero) / denom : 0.0;
  return {lag, std::abs(score)};
}

std::vector<double> median_filter_zero_padded(std::span<const double> values, std::int32_t kernel) {
  if (kernel <= 1 || values.size() < static_cast<std::size_t>(kernel)) {
    return {values.begin(), values.end()};
  }
  if ((kernel % 2) == 0) {
    ++kernel;
  }
  const auto radius = kernel / 2;
  std::vector<double> out(values.size(), 0.0);
  std::vector<double> window;
  window.reserve(static_cast<std::size_t>(kernel));
  for (std::size_t index = 0; index < values.size(); ++index) {
    window.clear();
    for (std::int32_t rel = -radius; rel <= radius; ++rel) {
      const auto source = static_cast<std::int64_t>(index) + rel;
      window.push_back(source < 0 || source >= static_cast<std::int64_t>(values.size())
                           ? 0.0
                           : values[static_cast<std::size_t>(source)]);
    }
    std::nth_element(window.begin(), window.begin() + window.size() / 2U, window.end());
    out[index] = window[window.size() / 2U];
  }
  return out;
}

double interp_curve(std::span<const double> centers, std::span<const double> values, double x) {
  if (centers.empty() || values.empty()) {
    return 0.0;
  }
  if (x <= centers.front()) {
    return values.front();
  }
  if (x >= centers.back()) {
    return values.back();
  }
  const auto upper = std::upper_bound(centers.begin(), centers.end(), x);
  const auto index = static_cast<std::size_t>(std::distance(centers.begin(), upper));
  const double x0 = centers[index - 1U];
  const double x1 = centers[index];
  const double frac = (x - x0) / std::max(x1 - x0, kEpsilon);
  return values[index - 1U] * (1.0 - frac) + values[index] * frac;
}

std::vector<std::array<double, 2>> apply_time_warp(
    std::span<const std::array<double, 2>> reference,
    std::span<const double> centers,
    std::span<const double> delays) {
  if (centers.empty() || delays.empty()) {
    return {reference.begin(), reference.end()};
  }
  std::vector<double> left;
  std::vector<double> right;
  left.reserve(reference.size());
  right.reserve(reference.size());
  for (const auto& frame : reference) {
    left.push_back(frame[0]);
    right.push_back(frame[1]);
  }
  std::vector<std::array<double, 2>> out;
  out.reserve(reference.size());
  for (std::size_t index = 0; index < reference.size(); ++index) {
    const double delay = interp_curve(centers, delays, static_cast<double>(index));
    const double source = static_cast<double>(index) + delay;
    out.push_back({sample_at(left, source), sample_at(right, source)});
  }
  return out;
}

FitResult fit_scalar(std::span<const std::array<double, 2>> ref,
                     std::span<const std::array<double, 2>> cap) {
  long double xy = 0.0;
  long double xx = 0.0;
  for (std::size_t index = 0; index < std::min(ref.size(), cap.size()); ++index) {
    xy += static_cast<long double>(ref[index][0]) * cap[index][0] +
          static_cast<long double>(ref[index][1]) * cap[index][1];
    xx += static_cast<long double>(ref[index][0]) * ref[index][0] +
          static_cast<long double>(ref[index][1]) * ref[index][1];
  }
  const double gain = xx > 1.0e-18L ? static_cast<double>(xy / xx) : 0.0;
  std::vector<std::array<double, 2>> pred;
  std::vector<std::array<double, 2>> residual;
  pred.reserve(ref.size());
  residual.reserve(ref.size());
  for (std::size_t index = 0; index < std::min(ref.size(), cap.size()); ++index) {
    const std::array<double, 2> p{gain * ref[index][0], gain * ref[index][1]};
    pred.push_back(p);
    residual.push_back({cap[index][0] - p[0], cap[index][1] - p[1]});
  }
  const double signal = rms_stereo(pred);
  const double res = rms_stereo(residual);
  return {gain, res > 0.0 ? db20(signal / res) : 999.0, signal, res};
}

MatrixFitResult fit_matrix(std::span<const std::array<double, 2>> ref,
                           std::span<const std::array<double, 2>> cap) {
  long double a00 = 0.0;
  long double a01 = 0.0;
  long double a11 = 0.0;
  long double b00 = 0.0;
  long double b01 = 0.0;
  long double b10 = 0.0;
  long double b11 = 0.0;
  for (std::size_t index = 0; index < std::min(ref.size(), cap.size()); ++index) {
    const double l = ref[index][0];
    const double r = ref[index][1];
    a00 += static_cast<long double>(l) * l;
    a01 += static_cast<long double>(l) * r;
    a11 += static_cast<long double>(r) * r;
    b00 += static_cast<long double>(l) * cap[index][0];
    b01 += static_cast<long double>(l) * cap[index][1];
    b10 += static_cast<long double>(r) * cap[index][0];
    b11 += static_cast<long double>(r) * cap[index][1];
  }
  const long double det = a00 * a11 - a01 * a01;
  MatrixFitResult out{};
  if (std::abs(det) > 1.0e-24L) {
    const long double inv00 = a11 / det;
    const long double inv01 = -a01 / det;
    const long double inv11 = a00 / det;
    out.matrix = {{{static_cast<double>(inv00 * b00 + inv01 * b10),
                    static_cast<double>(inv00 * b01 + inv01 * b11)},
                   {static_cast<double>(inv01 * b00 + inv11 * b10),
                    static_cast<double>(inv01 * b01 + inv11 * b11)}}};
  }

  std::vector<std::array<double, 2>> pred;
  std::vector<std::array<double, 2>> residual;
  pred.reserve(ref.size());
  residual.reserve(ref.size());
  for (std::size_t index = 0; index < std::min(ref.size(), cap.size()); ++index) {
    const std::array<double, 2> p{
        ref[index][0] * out.matrix[0][0] + ref[index][1] * out.matrix[1][0],
        ref[index][0] * out.matrix[0][1] + ref[index][1] * out.matrix[1][1]};
    pred.push_back(p);
    residual.push_back({cap[index][0] - p[0], cap[index][1] - p[1]});
  }
  out.signal_rms = rms_stereo(pred);
  out.residual_rms = rms_stereo(residual);
  out.snr_db = out.residual_rms > 0.0 ? db20(out.signal_rms / out.residual_rms) : 999.0;
  return out;
}

double percentile(std::vector<double> values, double pct) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double position = (pct / 100.0) * static_cast<double>(values.size() - 1U);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = std::min(lower + 1U, values.size() - 1U);
  const double frac = position - static_cast<double>(lower);
  return values[lower] * (1.0 - frac) + values[upper] * frac;
}

double median(std::vector<double> values) {
  return percentile(std::move(values), 50.0);
}

std::string classification(double scalar_delta, double matrix_delta, const Cli& cli) {
  const double best = std::max(scalar_delta, matrix_delta);
  if (best >= cli.strong_improvement_db) {
    return "fractional_time_warp_explains_large_residual";
  }
  if (best >= cli.partial_improvement_db) {
    return "fractional_time_warp_partial_factor";
  }
  return "fractional_time_warp_rejected";
}

void print_number(std::ostream& out, double value) {
  if (std::isfinite(value)) {
    out << value;
  } else {
    out << "null";
  }
}

struct RunAnalysis {
  AlignedRun run;
  std::vector<double> centers;
  std::vector<double> delays;
  std::vector<double> scores;
  std::vector<double> window_snr_before;
  std::vector<double> window_snr_after;
  FitResult scalar_before;
  FitResult scalar_after;
  MatrixFitResult matrix_before;
  MatrixFitResult matrix_after;
  double delay_min = 0.0;
  double delay_max = 0.0;
  double delay_median = 0.0;
  double delay_p95_abs = 0.0;
  double delay_jump_p95 = 0.0;
  double score_median = 0.0;
  double score_min = 0.0;
  std::string classification;
};

RunAnalysis analyze_run(const std::filesystem::path& run_dir, const Cli& cli) {
  RunAnalysis out{};
  out.run = load_run(run_dir, cli);
  const auto ref_mono = mono(out.run.reference);
  const auto cap_mono = mono(out.run.capture);
  const auto window = std::max<std::size_t>(512U,
                                            std::llround(cli.window_seconds * out.run.rate));
  const auto hop = std::max<std::size_t>(1U, std::llround(cli.hop_seconds * out.run.rate));
  for (std::size_t start = 0; start + window <= out.run.reference.size(); start += hop) {
    const auto stop = start + window;
    const auto delay = best_window_delay(
        std::span<const double>(ref_mono.data() + start, stop - start),
        std::span<const double>(cap_mono.data() + start, stop - start), cli.max_lag);
    const auto ref_window = slice(out.run.reference, start, window);
    const auto cap_window = slice(out.run.capture, start, window);
    const auto warped_window = shifted_stereo(ref_window, delay.lag);
    out.centers.push_back(static_cast<double>(start) + static_cast<double>(window) / 2.0);
    out.delays.push_back(delay.lag);
    out.scores.push_back(delay.score);
    out.window_snr_before.push_back(fit_scalar(ref_window, cap_window).snr_db);
    out.window_snr_after.push_back(fit_scalar(warped_window, cap_window).snr_db);
  }

  out.delays = median_filter_zero_padded(out.delays, cli.median_filter_windows);
  const auto warped = apply_time_warp(out.run.reference, out.centers, out.delays);
  out.scalar_before = fit_scalar(out.run.reference, out.run.capture);
  out.scalar_after = fit_scalar(warped, out.run.capture);
  out.matrix_before = fit_matrix(out.run.reference, out.run.capture);
  out.matrix_after = fit_matrix(warped, out.run.capture);

  std::vector<double> abs_delays;
  std::vector<double> jumps;
  abs_delays.reserve(out.delays.size());
  for (std::size_t index = 0; index < out.delays.size(); ++index) {
    abs_delays.push_back(std::abs(out.delays[index]));
    if (index > 0U) {
      jumps.push_back(std::abs(out.delays[index] - out.delays[index - 1U]));
    }
  }
  if (!out.delays.empty()) {
    out.delay_min = *std::min_element(out.delays.begin(), out.delays.end());
    out.delay_max = *std::max_element(out.delays.begin(), out.delays.end());
    out.delay_median = median(out.delays);
    out.delay_p95_abs = percentile(abs_delays, 95.0);
    out.delay_jump_p95 = percentile(jumps, 95.0);
    out.score_median = median(out.scores);
    out.score_min = *std::min_element(out.scores.begin(), out.scores.end());
  }
  out.classification =
      classification(out.scalar_after.snr_db - out.scalar_before.snr_db,
                     out.matrix_after.snr_db - out.matrix_before.snr_db, cli);
  return out;
}

std::string render_json(const std::vector<RunAnalysis>& rows, const Cli& cli) {
  std::ostringstream out;
  std::uint32_t large = 0;
  std::uint32_t partial = 0;
  std::uint32_t rejected = 0;
  for (const auto& row : rows) {
    if (row.classification == "fractional_time_warp_explains_large_residual") {
      ++large;
    } else if (row.classification == "fractional_time_warp_partial_factor") {
      ++partial;
    } else if (row.classification == "fractional_time_warp_rejected") {
      ++rejected;
    }
  }

  out << "{\n"
      << "  \"schema\": \"opena8djcpp.fractional-time-warp-cpp.v1\",\n"
      << "  \"result\": \"" << (rows.empty() ? "FAIL" : "PASS_DIAGNOSTIC") << "\",\n"
      << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"thresholds\": {\n"
      << "    \"partial_improvement_db\": " << cli.partial_improvement_db << ",\n"
      << "    \"strong_improvement_db\": " << cli.strong_improvement_db << "\n"
      << "  },\n"
      << "  \"summary\": {\n"
      << "    \"runs\": " << rows.size() << ",\n"
      << "    \"large_residual_explained\": " << large << ",\n"
      << "    \"partial_factor\": " << partial << ",\n"
      << "    \"rejected\": " << rejected << "\n"
      << "  },\n"
      << "  \"rows\": [\n";
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    out << "    {\n"
        << "      \"run_dir\": \"" << row.run.run_dir.string() << "\",\n"
        << "      \"rate\": " << row.run.rate << ",\n"
        << "      \"analysis_seconds\": "
        << static_cast<double>(row.run.reference.size()) / row.run.rate << ",\n"
        << "      \"window_seconds\": " << cli.window_seconds << ",\n"
        << "      \"hop_seconds\": " << cli.hop_seconds << ",\n"
        << "      \"max_lag_frames\": " << cli.max_lag << ",\n"
        << "      \"windows\": " << row.centers.size() << ",\n"
        << "      \"source_metrics\": {\n"
        << "        \"quality_alignment_score\": " << row.run.source_quality_alignment_score << ",\n"
        << "        \"lag_jumps_gt_2_frames\": " << row.run.source_lag_jumps_gt_2_frames << ",\n"
        << "        \"mid_band_residual_ratio\": " << row.run.source_mid_band_residual_ratio << ",\n"
        << "        \"high_band_residual_ratio\": " << row.run.source_high_band_residual_ratio << "\n"
        << "      },\n"
        << "      \"delay\": {\n"
        << "        \"min_frames\": " << row.delay_min << ",\n"
        << "        \"max_frames\": " << row.delay_max << ",\n"
        << "        \"median_frames\": " << row.delay_median << ",\n"
        << "        \"p95_abs_frames\": " << row.delay_p95_abs << ",\n"
        << "        \"jump_p95_frames\": " << row.delay_jump_p95 << ",\n"
        << "        \"score_median\": " << row.score_median << ",\n"
        << "        \"score_min\": " << row.score_min << "\n"
        << "      },\n"
        << "      \"window_snr\": {\n"
        << "        \"before_median_db\": " << median(row.window_snr_before) << ",\n"
        << "        \"after_median_db\": " << median(row.window_snr_after) << ",\n"
        << "        \"median_delta_db\": "
        << median(row.window_snr_after) - median(row.window_snr_before) << "\n"
        << "      },\n"
        << "      \"global_scalar\": {\n"
        << "        \"before_snr_db\": " << row.scalar_before.snr_db << ",\n"
        << "        \"after_snr_db\": " << row.scalar_after.snr_db << ",\n"
        << "        \"improvement_db\": "
        << row.scalar_after.snr_db - row.scalar_before.snr_db << ",\n"
        << "        \"before_residual_rms\": " << row.scalar_before.residual_rms << ",\n"
        << "        \"after_residual_rms\": " << row.scalar_after.residual_rms << "\n"
        << "      },\n"
        << "      \"global_matrix\": {\n"
        << "        \"before_snr_db\": " << row.matrix_before.snr_db << ",\n"
        << "        \"after_snr_db\": " << row.matrix_after.snr_db << ",\n"
        << "        \"improvement_db\": "
        << row.matrix_after.snr_db - row.matrix_before.snr_db << ",\n"
        << "        \"before_residual_rms\": " << row.matrix_before.residual_rms << ",\n"
        << "        \"after_residual_rms\": " << row.matrix_after.residual_rms << "\n"
        << "      },\n"
        << "      \"band_residual_ratios\": {\n"
        << "        \"before_mid\": null,\n"
        << "        \"after_mid\": null,\n"
        << "        \"before_high\": null,\n"
        << "        \"after_high\": null\n"
        << "      },\n"
        << "      \"classification\": \"" << row.classification << "\"\n"
        << "    }" << (index + 1U == rows.size() ? "\n" : ",\n");
  }
  out << "  ]\n"
      << "}\n";
  return out.str();
}

std::vector<std::array<double, 2>> make_noise(std::size_t frames, double gain) {
  std::uint32_t state = 0x12345678U;
  std::vector<std::array<double, 2>> out;
  out.reserve(frames);
  for (std::size_t index = 0; index < frames; ++index) {
    state = state * 1664525U + 1013904223U;
    const double a = (static_cast<double>((state >> 8U) & 0xffffU) / 32768.0 - 1.0) * gain;
    state = state * 1664525U + 1013904223U;
    const double b = (static_cast<double>((state >> 8U) & 0xffffU) / 32768.0 - 1.0) * gain;
    out.push_back({a, b});
  }
  return out;
}

int self_test(const Cli& cli) {
  AlignedRun run{};
  run.run_dir = "self-test";
  run.rate = 48000;
  run.reference = make_noise(run.rate * 2U, 0.25);
  run.capture = shifted_stereo(run.reference, 1.5);
  RunAnalysis analysis{};
  analysis.run = std::move(run);
  const auto ref_mono = mono(analysis.run.reference);
  const auto cap_mono = mono(analysis.run.capture);
  const auto delay = best_window_delay(ref_mono, cap_mono, cli.max_lag);
  const bool pass = std::abs(delay.lag - 1.5) <= 0.2 && delay.score >= 0.9;
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"opena8djcpp.fractional-time-warp-cpp-self-test.v1\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"estimated_delay_frames\": " << delay.lag << ",\n"
      << "  \"score\": " << delay.score << ",\n"
      << "  \"product_claim_allowed\": false,\n"
      << "  \"safety\": \"synthetic_offline_only_no_audio_coreaudio_usb_or_hardware_touch\"\n"
      << "}\n";
  const auto text = out.str();
  std::cout << text;
  if (!cli.json_out.empty()) {
    std::filesystem::create_directories(cli.json_out.parent_path());
    std::ofstream output(cli.json_out);
    output << text;
  }
  return pass ? 0 : 1;
}

Cli parse_cli(int argc, char** argv) {
  Cli cli{};
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg = argv[index];
    auto require_value = [&](std::string_view name) -> const char* {
      if (index + 1 >= argc) {
        throw std::runtime_error("missing_value:" + std::string(name));
      }
      return argv[++index];
    };
    if (arg == "--self-test") {
      cli.self_test = true;
    } else if (arg == "--json-out") {
      cli.json_out = require_value(arg);
    } else if (arg == "--analysis-seconds") {
      cli.analysis_seconds = std::stod(require_value(arg));
    } else if (arg == "--window-seconds") {
      cli.window_seconds = std::stod(require_value(arg));
    } else if (arg == "--hop-seconds") {
      cli.hop_seconds = std::stod(require_value(arg));
    } else if (arg == "--max-lag") {
      cli.max_lag = std::stoi(require_value(arg));
    } else if (arg == "--median-filter-windows") {
      cli.median_filter_windows = std::stoi(require_value(arg));
    } else if (arg == "--partial-improvement-db") {
      cli.partial_improvement_db = std::stod(require_value(arg));
    } else if (arg == "--strong-improvement-db") {
      cli.strong_improvement_db = std::stod(require_value(arg));
    } else {
      cli.soundcheck_dirs.emplace_back(arg);
    }
  }
  return cli;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto cli = parse_cli(argc, argv);
    if (cli.self_test) {
      return self_test(cli);
    }
    std::vector<RunAnalysis> rows;
    rows.reserve(cli.soundcheck_dirs.size());
    for (const auto& dir : cli.soundcheck_dirs) {
      rows.push_back(analyze_run(dir, cli));
    }
    const auto json = render_json(rows, cli);
    std::cout << json;
    if (!cli.json_out.empty()) {
      std::filesystem::create_directories(cli.json_out.parent_path());
      std::ofstream output(cli.json_out);
      output << json;
    }
    return rows.empty() ? 1 : 0;
  } catch (const std::exception& error) {
    std::cerr << "fractional_time_warp_error: " << error.what() << "\n";
    return 1;
  }
}
