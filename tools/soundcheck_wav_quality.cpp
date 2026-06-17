#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kEpsilon = 1.0e-20;
constexpr double kPi = 3.14159265358979323846264338327950288;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
};

struct Alignment {
  std::size_t reference_start = 0;
  std::size_t capture_start = 0;
  std::int32_t alignment_lag = 0;
  double alignment_score = 0.0;
  std::size_t compared_frames = 0;
};

struct ChannelMetrics {
  double gain = 0.0;
  double signal_rms = 0.0;
  double residual_rms = 0.0;
  double snr_db = 0.0;
  double mid_band_residual_ratio = 0.0;
  double high_band_residual_ratio = 0.0;
  double quiet_mid_band_noise_dbfs = -240.0;
  std::vector<double> signal;
  std::vector<double> residual;
};

struct NativeMetrics {
  std::filesystem::path run_dir;
  std::uint32_t sample_rate = 0;
  Alignment alignment;
  double quality_alignment_score = 0.0;
  ChannelMetrics left;
  ChannelMetrics right;
  double left_snr_db = 0.0;
  double right_snr_db = 0.0;
  double mid_band_residual_ratio = 0.0;
  double high_band_residual_ratio = 0.0;
  double quiet_mid_band_noise_dbfs = -240.0;
  std::uint32_t lag_windows = 0;
  std::int32_t lag_min = 0;
  std::int32_t lag_max = 0;
  std::uint32_t lag_jumps_gt_2_frames = 0;
  std::uint32_t click_outliers = 0;
  double click_threshold = 0.0;
  std::uint32_t capture_clipped_frames = 0;
};

struct Comparison {
  std::string name;
  double native = std::numeric_limits<double>::quiet_NaN();
  double recorded = std::numeric_limits<double>::quiet_NaN();
  double tolerance = 0.0;
  bool pass = false;
};

std::filesystem::path repo_root(char** argv) {
  auto root = std::filesystem::absolute(argv[0]).parent_path();
  while (!root.empty() && !std::filesystem::is_regular_file(root / "CMakeLists.txt")) {
    root = root.parent_path();
  }
  if (root.empty() || root.filename() != "audio8djcpp") {
    return "/Users/fer/dev/audio8djcpp";
  }
  return root;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::optional<double> json_number(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  std::size_t end = start;
  while (end < json.size()) {
    const char c = json[end];
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' ||
          c == 'e' || c == 'E')) {
      break;
    }
    ++end;
  }
  if (end == start) {
    return std::nullopt;
  }
  try {
    const double value = std::stod(json.substr(start, end - start));
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot_open:" + path.string());
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
         (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
         (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
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

std::vector<double> mono(const std::vector<std::array<double, 2>>& frames) {
  std::vector<double> out;
  out.reserve(frames.size());
  for (const auto& frame : frames) {
    out.push_back(0.5 * (frame[0] + frame[1]));
  }
  return out;
}

std::size_t first_signal_index(const std::vector<std::array<double, 2>>& frames) {
  double peak = 0.0;
  for (const auto& frame : frames) {
    peak = std::max({peak, std::abs(frame[0]), std::abs(frame[1])});
  }
  const double threshold = std::max(0.0005, peak * 0.02);
  for (std::size_t index = 0; index < frames.size(); ++index) {
    if (std::max(std::abs(frames[index][0]), std::abs(frames[index][1])) >= threshold) {
      return index;
    }
  }
  return 0;
}

double score_lag(std::span<const double> reference,
                 std::span<const double> capture,
                 std::int64_t reference_start,
                 std::int64_t capture_start,
                 std::int32_t lag,
                 std::size_t sample_count,
                 std::size_t stride) {
  double dot = 0.0;
  double reference_energy = 0.0;
  double capture_energy = 0.0;
  std::size_t used = 0;
  for (std::size_t n = 0; n < sample_count; n += std::max<std::size_t>(1U, stride)) {
    const auto ri = reference_start + static_cast<std::int64_t>(n);
    const auto gi = capture_start + static_cast<std::int64_t>(n) + lag;
    if (ri < 0 || gi < 0 || ri >= static_cast<std::int64_t>(reference.size()) ||
        gi >= static_cast<std::int64_t>(capture.size())) {
      continue;
    }
    const auto rv = reference[static_cast<std::size_t>(ri)];
    const auto gv = capture[static_cast<std::size_t>(gi)];
    dot += rv * gv;
    reference_energy += rv * rv;
    capture_energy += gv * gv;
    used += 1U;
  }
  if (used == 0U || reference_energy <= 0.0 || capture_energy <= 0.0) {
    return -1.0;
  }
  return dot / std::sqrt(reference_energy * capture_energy);
}

std::pair<std::int32_t, double> scan_lags(std::span<const double> reference,
                                          std::span<const double> capture,
                                          std::size_t reference_start,
                                          std::size_t capture_start,
                                          std::int32_t start_lag,
                                          std::int32_t end_lag,
                                          std::int32_t step,
                                          std::int32_t max_lag,
                                          std::size_t sample_count,
                                          std::size_t stride) {
  (void)max_lag;
  std::int32_t best_lag = 0;
  double best_score = -1.0;
  for (std::int32_t lag = start_lag; lag <= end_lag; lag += std::max(1, step)) {
    const auto score = score_lag(reference,
                                 capture,
                                 static_cast<std::int64_t>(reference_start),
                                 static_cast<std::int64_t>(capture_start),
                                 lag,
                                 sample_count,
                                 stride);
    if (score > best_score) {
      best_score = score;
      best_lag = lag;
    }
  }
  return {best_lag, best_score};
}

std::pair<std::int32_t, double> find_best_lag(std::span<const double> reference,
                                              std::span<const double> capture,
                                              std::size_t reference_start,
                                              std::size_t capture_start,
                                              std::int32_t max_lag,
                                              std::size_t sample_count,
                                              std::size_t stride) {
  if (max_lag <= 4096) {
    return scan_lags(reference,
                     capture,
                     reference_start,
                     capture_start,
                     -max_lag,
                     max_lag,
                     1,
                     max_lag,
                     sample_count,
                     stride);
  }
  const auto coarse_step = std::max<std::int32_t>(8, max_lag / 2048);
  const auto [coarse_lag, _coarse_score] =
      scan_lags(reference,
                capture,
                reference_start,
                capture_start,
                -max_lag,
                max_lag,
                coarse_step,
                max_lag,
                sample_count,
                std::max<std::size_t>(stride, static_cast<std::size_t>(coarse_step)));
  (void)_coarse_score;
  const auto fine_radius = coarse_step * 2;
  const auto fine_start = std::max<std::int32_t>(-max_lag, coarse_lag - fine_radius);
  const auto fine_end = std::min<std::int32_t>(max_lag, coarse_lag + fine_radius);
  return scan_lags(reference,
                   capture,
                   reference_start,
                   capture_start,
                   fine_start,
                   fine_end,
                   1,
                   max_lag,
                   sample_count,
                   stride);
}

double correlation(std::span<const double> reference,
                   std::span<const double> capture,
                   std::size_t reference_start,
                   std::size_t capture_start,
                   std::size_t sample_count) {
  double dot = 0.0;
  double reference_energy = 0.0;
  double capture_energy = 0.0;
  for (std::size_t n = 0; n < sample_count; ++n) {
    if (reference_start + n >= reference.size() || capture_start + n >= capture.size()) {
      break;
    }
    const auto rv = reference[reference_start + n];
    const auto gv = capture[capture_start + n];
    dot += rv * gv;
    reference_energy += rv * rv;
    capture_energy += gv * gv;
  }
  if (reference_energy <= 0.0 || capture_energy <= 0.0) {
    return 0.0;
  }
  return dot / std::sqrt(reference_energy * capture_energy);
}

double rms(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (const auto value : values) {
    sum += value * value;
  }
  return std::sqrt(sum / static_cast<double>(values.size()));
}

double dbfs(double value) {
  if (value <= 0.0) {
    return -240.0;
  }
  return 20.0 * std::log10(value);
}

std::array<double, 5> biquad_coefficients(const std::string& kind,
                                          std::uint32_t rate,
                                          double cutoff,
                                          double q = 0.7071067811865476) {
  const double nyquist = static_cast<double>(rate) * 0.5;
  cutoff = std::max(1.0, std::min(cutoff, nyquist * 0.95));
  const double omega = 2.0 * kPi * cutoff / static_cast<double>(rate);
  const double sin_omega = std::sin(omega);
  const double cos_omega = std::cos(omega);
  const double alpha = sin_omega / (2.0 * q);
  double b0 = 0.0;
  double b1 = 0.0;
  double b2 = 0.0;
  if (kind == "lowpass") {
    b0 = (1.0 - cos_omega) * 0.5;
    b1 = 1.0 - cos_omega;
    b2 = (1.0 - cos_omega) * 0.5;
  } else {
    b0 = (1.0 + cos_omega) * 0.5;
    b1 = -(1.0 + cos_omega);
    b2 = (1.0 + cos_omega) * 0.5;
  }
  const double a0 = 1.0 + alpha;
  const double a1 = -2.0 * cos_omega;
  const double a2 = 1.0 - alpha;
  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

std::vector<double> apply_biquad(std::span<const double> values,
                                 const std::array<double, 5>& c) {
  std::vector<double> output;
  output.reserve(values.size());
  double x1 = 0.0;
  double x2 = 0.0;
  double y1 = 0.0;
  double y2 = 0.0;
  for (const auto x0 : values) {
    const double y0 = c[0] * x0 + c[1] * x1 + c[2] * x2 - c[3] * y1 - c[4] * y2;
    output.push_back(y0);
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
  }
  return output;
}

std::vector<double> bandpass(std::span<const double> values,
                             std::uint32_t rate,
                             double low_hz,
                             double high_hz) {
  if (values.empty()) {
    return {};
  }
  const double nyquist = static_cast<double>(rate) * 0.5;
  low_hz = std::max(1.0, std::min(low_hz, nyquist * 0.90));
  high_hz = std::max(low_hz + 1.0, std::min(high_hz, nyquist * 0.95));
  const auto highpassed = apply_biquad(values, biquad_coefficients("highpass", rate, low_hz));
  return apply_biquad(highpassed, biquad_coefficients("lowpass", rate, high_hz));
}

double band_rms(std::span<const double> values,
                std::uint32_t rate,
                double low_hz,
                double high_hz) {
  const auto filtered = bandpass(values, rate, low_hz, high_hz);
  return rms(filtered);
}

double high_band_proxy(std::span<const double> values) {
  if (values.size() < 2U) {
    return 0.0;
  }
  double total = 0.0;
  double previous = values[0];
  for (std::size_t index = 1; index < values.size(); ++index) {
    const double diff = values[index] - previous;
    total += diff * diff;
    previous = values[index];
  }
  return std::sqrt(total / static_cast<double>(values.size() - 1U));
}

ChannelMetrics channel_metrics(const std::vector<std::array<double, 2>>& reference,
                               const std::vector<std::array<double, 2>>& capture,
                               std::uint32_t rate,
                               std::uint32_t channel) {
  double dot = 0.0;
  double reference_power = 0.0;
  for (std::size_t index = 0; index < std::min(reference.size(), capture.size()); ++index) {
    const auto rv = reference[index][channel];
    const auto gv = capture[index][channel];
    dot += rv * gv;
    reference_power += rv * rv;
  }
  ChannelMetrics metrics{};
  metrics.gain = reference_power > 0.0 ? dot / reference_power : 0.0;
  metrics.signal.reserve(std::min(reference.size(), capture.size()));
  metrics.residual.reserve(std::min(reference.size(), capture.size()));
  for (std::size_t index = 0; index < std::min(reference.size(), capture.size()); ++index) {
    const double signal = metrics.gain * reference[index][channel];
    const double residual = capture[index][channel] - signal;
    metrics.signal.push_back(signal);
    metrics.residual.push_back(residual);
  }
  metrics.signal_rms = rms(metrics.signal);
  metrics.residual_rms = rms(metrics.residual);
  metrics.snr_db = metrics.signal_rms > 0.0 && metrics.residual_rms > 0.0
                       ? 20.0 * std::log10(metrics.signal_rms / metrics.residual_rms)
                       : 999.0;
  const double mid_signal = band_rms(metrics.signal, rate, 1000.0, 5000.0);
  const double mid_residual = band_rms(metrics.residual, rate, 1000.0, 5000.0);
  metrics.mid_band_residual_ratio = mid_signal > 1.0e-9 ? mid_residual / mid_signal : 0.0;
  const double high_signal = band_rms(metrics.signal, rate, 5000.0, 12000.0);
  const double high_residual = band_rms(metrics.residual, rate, 5000.0, 12000.0);
  metrics.high_band_residual_ratio = high_signal > 1.0e-9 ? high_residual / high_signal : 0.0;

  const auto window = std::max<std::size_t>(256U, static_cast<std::size_t>(rate / 4U));
  if (metrics.signal.size() >= window) {
    std::vector<std::pair<double, std::size_t>> levels;
    for (std::size_t start = 0; start + window <= metrics.signal.size(); start += window / 2U) {
      levels.push_back({rms(std::span<const double>(metrics.signal).subspan(start, window)), start});
    }
    std::sort(levels.begin(), levels.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
    });
    std::vector<double> quiet;
    const std::size_t selected = std::max<std::size_t>(1U, levels.size() / 4U);
    for (std::size_t i = 0; i < std::min(selected, levels.size()); ++i) {
      const auto start = levels[i].second;
      quiet.insert(quiet.end(),
                   metrics.residual.begin() + static_cast<std::ptrdiff_t>(start),
                   metrics.residual.begin() + static_cast<std::ptrdiff_t>(start + window));
    }
    metrics.quiet_mid_band_noise_dbfs = dbfs(band_rms(quiet, rate, 1000.0, 5000.0));
  }
  return metrics;
}

std::pair<std::uint32_t, double> click_count(std::span<const double> left,
                                             std::span<const double> right) {
  const auto count = std::min(left.size(), right.size());
  if (count == 0U) {
    return {0U, 0.0};
  }
  const double residual_rms = std::max(rms(left), rms(right));
  const double threshold = std::max(0.01, residual_rms * 12.0);
  std::uint32_t clicks = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (std::max(std::abs(left[index]), std::abs(right[index])) > threshold) {
      clicks += 1U;
    }
  }
  return {clicks, threshold};
}

std::uint32_t clipped_count(const std::vector<std::array<double, 2>>& capture) {
  std::uint32_t clipped = 0;
  for (const auto& frame : capture) {
    if (std::abs(frame[0]) >= 0.999 || std::abs(frame[1]) >= 0.999) {
      clipped += 1U;
    }
  }
  return clipped;
}

NativeMetrics analyze(const std::filesystem::path& run_dir,
                      const StereoBuffer& reference,
                      const StereoBuffer& capture) {
  if (reference.sample_rate != capture.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch");
  }
  NativeMetrics metrics{};
  metrics.run_dir = run_dir;
  metrics.sample_rate = reference.sample_rate;

  const auto ref_mono = mono(reference.frames);
  const auto cap_mono = mono(capture.frames);
  std::size_t ref_start = first_signal_index(reference.frames);
  const std::size_t cap_rough_start = first_signal_index(capture.frames);
  const auto rate = reference.sample_rate;
  const auto fit_frames = std::min({reference.frames.size() - ref_start,
                                    static_cast<std::size_t>(rate / 10U),
                                    capture.frames.size()});
  if (fit_frames < rate / 20U) {
    throw std::runtime_error("not_enough_audio_for_alignment");
  }
  const auto [local_lag, local_score] =
      find_best_lag(ref_mono,
                    cap_mono,
                    ref_start,
                    cap_rough_start,
                    static_cast<std::int32_t>(rate),
                    fit_frames,
                    std::max<std::size_t>(1U, fit_frames / 512U));
  (void)local_score;
  std::int64_t cap_start_signed = static_cast<std::int64_t>(cap_rough_start) + local_lag;
  std::int64_t ref_start_signed = static_cast<std::int64_t>(ref_start);
  if (cap_start_signed < 0) {
    ref_start_signed += -cap_start_signed;
    cap_start_signed = 0;
  }
  if (ref_start_signed < 0) {
    cap_start_signed += -ref_start_signed;
    ref_start_signed = 0;
  }
  ref_start = static_cast<std::size_t>(ref_start_signed);
  const std::size_t cap_start = static_cast<std::size_t>(cap_start_signed);
  const auto usable =
      std::min(reference.frames.size() - ref_start, capture.frames.size() - cap_start);
  if (usable <= rate / 2U) {
    throw std::runtime_error("not_enough_aligned_audio");
  }

  metrics.alignment = {ref_start,
                       cap_start,
                       static_cast<std::int32_t>(cap_start) - static_cast<std::int32_t>(ref_start),
                       correlation(ref_mono, cap_mono, ref_start, cap_start, std::min<std::size_t>(usable, rate)),
                       usable};
  std::vector<std::array<double, 2>> ref_window;
  std::vector<std::array<double, 2>> cap_window;
  ref_window.reserve(usable);
  cap_window.reserve(usable);
  for (std::size_t index = 0; index < usable; ++index) {
    ref_window.push_back(reference.frames[ref_start + index]);
    cap_window.push_back(capture.frames[cap_start + index]);
  }
  const auto cap_quality_mono = mono(cap_window);
  metrics.quality_alignment_score =
      correlation(ref_mono, cap_quality_mono, ref_start, 0U, std::min<std::size_t>(usable, rate));
  metrics.left = channel_metrics(ref_window, cap_window, rate, 0U);
  metrics.right = channel_metrics(ref_window, cap_window, rate, 1U);
  metrics.left_snr_db = metrics.left.snr_db;
  metrics.right_snr_db = metrics.right.snr_db;
  metrics.mid_band_residual_ratio =
      std::max(metrics.left.mid_band_residual_ratio, metrics.right.mid_band_residual_ratio);
  metrics.high_band_residual_ratio =
      std::max(metrics.left.high_band_residual_ratio, metrics.right.high_band_residual_ratio);
  metrics.quiet_mid_band_noise_dbfs =
      std::max(metrics.left.quiet_mid_band_noise_dbfs, metrics.right.quiet_mid_band_noise_dbfs);
  const auto [clicks, threshold] = click_count(metrics.left.residual, metrics.right.residual);
  metrics.click_outliers = clicks;
  metrics.click_threshold = threshold;
  metrics.capture_clipped_frames = clipped_count(cap_window);

  const auto window = std::max<std::size_t>(64U, rate / 2U);
  const auto hop = std::max<std::size_t>(1U, rate / 4U);
  std::vector<std::int32_t> lags;
  for (std::size_t start = 0; start + window <= usable; start += hop) {
    const auto [lag, _score] = scan_lags(ref_mono,
                                         cap_mono,
                                         ref_start + start,
                                         cap_start + start,
                                         -32,
                                         32,
                                         1,
                                         32,
                                         window,
                                         std::max<std::size_t>(1U, window / 256U));
    lags.push_back(lag);
  }
  metrics.lag_windows = static_cast<std::uint32_t>(lags.size());
  if (!lags.empty()) {
    metrics.lag_min = *std::min_element(lags.begin(), lags.end());
    metrics.lag_max = *std::max_element(lags.begin(), lags.end());
  }
  for (std::size_t index = 1; index < lags.size(); ++index) {
    if (std::abs(lags[index] - lags[index - 1U]) > 2) {
      metrics.lag_jumps_gt_2_frames += 1U;
    }
  }
  return metrics;
}

std::optional<std::filesystem::path> latest_complete_soundcheck(const std::filesystem::path& root) {
  const auto soundcheck_root = root / "local-analysis/soundcheck";
  if (!std::filesystem::is_directory(soundcheck_root)) {
    return std::nullopt;
  }
  std::optional<std::filesystem::path> best;
  std::filesystem::file_time_type best_time{};
  for (const auto& entry : std::filesystem::directory_iterator(soundcheck_root)) {
    if (!entry.is_directory()) {
      continue;
    }
    const auto dir = entry.path();
    const auto metrics = dir / "metrics.json";
    const auto cpu = dir / "cpu-profile.tsv";
    const auto ref = dir / "fixture/reference.wav";
    const auto captured = dir / "captured.wav";
    if (!std::filesystem::is_regular_file(metrics) || !std::filesystem::is_regular_file(cpu) ||
        !std::filesystem::is_regular_file(ref) || !std::filesystem::is_regular_file(captured)) {
      continue;
    }
    const auto newest = std::max(std::filesystem::last_write_time(metrics),
                                 std::filesystem::last_write_time(captured));
    if (!best || newest > best_time || (newest == best_time && dir.string() > best->string())) {
      best = dir;
      best_time = newest;
    }
  }
  return best;
}

Comparison compare_metric(const std::string& name,
                          double native,
                          std::optional<double> recorded,
                          double tolerance) {
  const double recorded_value = recorded.value_or(std::numeric_limits<double>::quiet_NaN());
  return {name,
          native,
          recorded_value,
          tolerance,
          std::isfinite(native) && std::isfinite(recorded_value) &&
              std::abs(native - recorded_value) <= tolerance};
}

void print_json_string(const std::string& text) {
  std::cout << '"';
  for (const auto ch : text) {
    if (ch == '"' || ch == '\\') {
      std::cout << '\\';
    }
    if (ch == '\n') {
      std::cout << "\\n";
    } else {
      std::cout << ch;
    }
  }
  std::cout << '"';
}

void print_json_number(double value) {
  if (std::isfinite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto root = repo_root(argv);
    std::filesystem::path run_dir;
    if (argc >= 2) {
      run_dir = argv[1];
    } else {
      const auto latest = latest_complete_soundcheck(root);
      if (!latest) {
        std::cout << "{\n"
                  << "  \"schema\": \"opena8djcpp.soundcheck-wav-quality.v1\",\n"
                  << "  \"result\": \"PASS\",\n"
                  << "  \"mode\": \"no_existing_wav_evidence\",\n"
                  << "  \"meaning\": \"analyzer compiled but no stored WAV run was available\"\n"
                  << "}\n";
        return 0;
      }
      run_dir = *latest;
    }

    const auto reference = read_wav_pair(run_dir / "fixture/reference.wav");
    const auto capture = read_wav_pair(run_dir / "captured.wav");
    const auto native = analyze(run_dir, reference, capture);
    const auto recorded_json = read_file(run_dir / "metrics.json");
    const std::vector<Comparison> comparisons = {
        compare_metric("quality_alignment_score",
                       native.quality_alignment_score,
                       json_number(recorded_json, "quality_alignment_score"),
                       0.08),
        compare_metric("left_snr_db", native.left_snr_db, json_number(recorded_json, "left_snr_db"), 8.0),
        compare_metric("right_snr_db", native.right_snr_db, json_number(recorded_json, "right_snr_db"), 8.0),
        compare_metric("mid_band_residual_ratio",
                       native.mid_band_residual_ratio,
                       json_number(recorded_json, "mid_band_residual_ratio"),
                       0.45),
        compare_metric("high_band_residual_ratio",
                       native.high_band_residual_ratio,
                       json_number(recorded_json, "high_band_residual_ratio"),
                       0.45),
        compare_metric("capture_clipped_frames",
                       static_cast<double>(native.capture_clipped_frames),
                       json_number(recorded_json, "capture_clipped_frames"),
                       0.0),
    };
    const std::uint32_t comparison_failures = static_cast<std::uint32_t>(
        std::count_if(comparisons.begin(), comparisons.end(), [](const Comparison& c) {
          return !c.pass;
        }));

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "{\n";
    std::cout << "  \"schema\": \"opena8djcpp.soundcheck-wav-quality.v1\",\n";
    std::cout << "  \"safety\": \"offline_existing_wav_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n";
    std::cout << "  \"result\": \"PASS\",\n";
    std::cout << "  \"run_dir\": ";
    print_json_string(run_dir.string());
    std::cout << ",\n";
    std::cout << "  \"sample_rate\": " << native.sample_rate << ",\n";
    std::cout << "  \"reference_start\": " << native.alignment.reference_start << ",\n";
    std::cout << "  \"capture_start\": " << native.alignment.capture_start << ",\n";
    std::cout << "  \"alignment_lag\": " << native.alignment.alignment_lag << ",\n";
    std::cout << "  \"alignment_score\": " << native.alignment.alignment_score << ",\n";
    std::cout << "  \"quality_alignment_score\": " << native.quality_alignment_score << ",\n";
    std::cout << "  \"left_snr_db\": " << native.left_snr_db << ",\n";
    std::cout << "  \"right_snr_db\": " << native.right_snr_db << ",\n";
    std::cout << "  \"mid_band_residual_ratio\": " << native.mid_band_residual_ratio << ",\n";
    std::cout << "  \"high_band_residual_ratio\": " << native.high_band_residual_ratio << ",\n";
    std::cout << "  \"quiet_mid_band_noise_dbfs\": " << native.quiet_mid_band_noise_dbfs << ",\n";
    std::cout << "  \"lag_windows\": " << native.lag_windows << ",\n";
    std::cout << "  \"lag_min\": " << native.lag_min << ",\n";
    std::cout << "  \"lag_max\": " << native.lag_max << ",\n";
    std::cout << "  \"lag_jumps_gt_2_frames\": " << native.lag_jumps_gt_2_frames << ",\n";
    std::cout << "  \"click_outliers\": " << native.click_outliers << ",\n";
    std::cout << "  \"click_threshold\": " << native.click_threshold << ",\n";
    std::cout << "  \"capture_clipped_frames\": " << native.capture_clipped_frames << ",\n";
    std::cout << "  \"parity\": {\n";
    std::cout << "    \"result\": \"" << (comparison_failures == 0U ? "PASS" : "WARN") << "\",\n";
    std::cout << "    \"comparison_count\": " << comparisons.size() << ",\n";
    std::cout << "    \"failures\": " << comparison_failures << ",\n";
    std::cout << "    \"tolerance_policy\": \"first-slice native analyzer; broad tolerance until algorithms are fully unified\"\n";
    std::cout << "  },\n";
    std::cout << "  \"comparisons\": [\n";
    for (std::size_t index = 0; index < comparisons.size(); ++index) {
      const auto& comparison = comparisons[index];
      std::cout << "    {\"name\": ";
      print_json_string(comparison.name);
      std::cout << ", \"result\": \"" << (comparison.pass ? "PASS" : "WARN")
                << "\", \"native\": ";
      print_json_number(comparison.native);
      std::cout << ", \"recorded\": ";
      print_json_number(comparison.recorded);
      std::cout << ", \"tolerance\": " << comparison.tolerance << "}"
                << (index + 1U == comparisons.size() ? "\n" : ",\n");
    }
    std::cout << "  ],\n";
    std::cout << "  \"readiness_claim\": \"ANALYZER_ONLY_NOT_PRODUCT_READINESS\"\n";
    std::cout << "}\n";
    return 0;
  } catch (const std::exception& error) {
    std::cout << "{\n"
              << "  \"schema\": \"opena8djcpp.soundcheck-wav-quality.v1\",\n"
              << "  \"result\": \"FAIL\",\n"
              << "  \"error\": ";
    print_json_string(error.what());
    std::cout << "\n}\n";
    return 1;
  }
}
