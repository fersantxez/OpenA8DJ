#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kEpsilon = 1.0e-20;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::uint32_t source_channels = 2;
  std::vector<std::array<double, 2>> frames;
};

struct Thresholds {
  double min_alignment_score = 0.98;
  double min_snr_db = 45.0;
  double min_mid_coherence = 0.90;
  double max_delay_p95_frames = 2.0;
  double max_leakage_db = -70.0;
  double max_residual_burst_p95_to_median_db = 12.0;
  double max_residual_signal_abs_correlation = 0.10;
  double max_residual_peak_to_rms_db = 24.0;
};

struct Cli {
  std::filesystem::path reference;
  std::filesystem::path capture;
  std::array<std::uint32_t, 2> reference_channels{0, 1};
  std::array<std::uint32_t, 2> capture_channels{0, 1};
  std::optional<double> seconds;
  double max_lag_seconds = 1.0;
  double delay_window_seconds = 1.0;
  double delay_search_ms = 8.0;
  std::string label;
  std::filesystem::path json_out;
  bool self_test = false;
  bool self_test_degraded = false;
  Thresholds thresholds;
};

struct Alignment {
  std::int32_t lag_frames = 0;
  double lag_ms = 0.0;
  double score = 0.0;
  std::size_t reference_start = 0;
  std::size_t capture_start = 0;
  std::size_t compared_frames = 0;
};

struct ChannelMetrics {
  double gain_db = 0.0;
  double signal_rms_dbfs = 0.0;
  double residual_rms_dbfs = 0.0;
  double snr_db = 0.0;
  double peak_dbfs = 0.0;
  std::uint64_t clipped_frames = 0;
  double dc_offset_dbfs = 0.0;
  double coherence_low_active_mean = 0.0;
  double coherence_mid_active_mean = 0.0;
  double coherence_high_active_mean = 0.0;
  double transfer_ripple_low_db = 0.0;
  double transfer_ripple_mid_db = 0.0;
  double transfer_ripple_high_db = 0.0;
  double residual_ratio_mid_mean = 0.0;
  double residual_ratio_high_mean = 0.0;
  double residual_burst_p95_to_median_db = 0.0;
  double residual_signal_abs_correlation = 0.0;
  double residual_peak_to_rms_db = 0.0;
};

struct StereoMatrix {
  std::array<std::array<double, 2>, 2> matrix{};
  double worst_offdiag_db_relative = -240.0;
  double condition_number = std::numeric_limits<double>::infinity();
  bool leakage_evaluable = false;
};

struct DelayWindows {
  std::uint32_t windows = 0;
  double min_frames = 0.0;
  double max_frames = 0.0;
  double p95_abs_frames = 0.0;
  double range_frames = 0.0;
  std::uint32_t lag_jumps_gt_2_frames = 0;
};

struct Analysis {
  std::string schema = "opena8djcpp.audiophile-wav-analysis-cpp.v1";
  std::string label;
  std::string reference;
  std::string capture;
  std::uint32_t sample_rate = 0;
  std::optional<double> analysis_seconds_requested;
  Alignment alignment;
  ChannelMetrics left;
  ChannelMetrics right;
  StereoMatrix stereo_matrix;
  DelayWindows delay_windows;
  Thresholds thresholds;
  std::vector<std::string> blockers;
};

double db20(double value) {
  return 20.0 * std::log10(std::max(std::abs(value), kEpsilon));
}

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

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
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

std::array<std::uint32_t, 2> parse_channels(std::string_view text) {
  std::array<std::uint32_t, 2> channels{};
  std::uint32_t count = 0;
  std::string current;
  for (const char c : text) {
    if (c == ',') {
      if (current.empty() || count >= 2U) {
        throw std::runtime_error("invalid_channel_selector");
      }
      const auto one_based = static_cast<std::uint32_t>(std::stoul(current));
      if (one_based == 0U) {
        throw std::runtime_error("invalid_channel_selector");
      }
      channels[count++] = one_based - 1U;
      current.clear();
    } else if (!std::isspace(static_cast<unsigned char>(c))) {
      current.push_back(c);
    }
  }
  if (current.empty() || count >= 2U) {
    throw std::runtime_error("invalid_channel_selector");
  }
  const auto one_based = static_cast<std::uint32_t>(std::stoul(current));
  if (one_based == 0U) {
    throw std::runtime_error("invalid_channel_selector");
  }
  channels[count++] = one_based - 1U;
  if (count != 2U) {
    throw std::runtime_error("invalid_channel_selector");
  }
  return channels;
}

StereoBuffer read_wav_pair(const std::filesystem::path& path,
                           std::array<std::uint32_t, 2> selected_channels) {
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
  if (selected_channels[0] >= channels || selected_channels[1] >= channels) {
    throw std::runtime_error("selected_channel_out_of_range:" + path.string());
  }

  StereoBuffer buffer{};
  buffer.sample_rate = sample_rate;
  buffer.source_channels = channels;
  const auto frames = data_size / static_cast<std::size_t>(block_align);
  buffer.frames.reserve(frames);
  const auto bytes_per_sample = static_cast<std::size_t>(bits_per_sample / 8U);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const auto frame_offset = data_offset + frame * static_cast<std::size_t>(block_align);
    const auto left_offset = frame_offset + selected_channels[0] * bytes_per_sample;
    const auto right_offset = frame_offset + selected_channels[1] * bytes_per_sample;
    buffer.frames.push_back({decode_pcm_sample(bytes, left_offset, format, bits_per_sample),
                             decode_pcm_sample(bytes, right_offset, format, bits_per_sample)});
  }
  return buffer;
}

void trim_seconds(StereoBuffer& buffer, std::optional<double> seconds) {
  if (!seconds || *seconds <= 0.0) {
    return;
  }
  const auto wanted = static_cast<std::size_t>(std::llround(*seconds * buffer.sample_rate));
  if (wanted < buffer.frames.size()) {
    buffer.frames.resize(wanted);
  }
}

std::vector<double> channel(const std::vector<std::array<double, 2>>& frames,
                            std::size_t channel_index) {
  std::vector<double> out;
  out.reserve(frames.size());
  for (const auto& frame : frames) {
    out.push_back(frame[channel_index]);
  }
  return out;
}

std::vector<double> mono_centered(const std::vector<std::array<double, 2>>& frames) {
  std::vector<double> out;
  out.reserve(frames.size());
  long double mean = 0.0;
  for (const auto& frame : frames) {
    const double value = 0.5 * (frame[0] + frame[1]);
    out.push_back(value);
    mean += value;
  }
  if (!out.empty()) {
    mean /= out.size();
  }
  for (double& value : out) {
    value -= static_cast<double>(mean);
  }
  return out;
}

double normalized_lag_score(std::span<const double> reference,
                            std::span<const double> capture,
                            std::int32_t lag,
                            std::size_t stride) {
  const std::size_t ref_start = lag < 0 ? static_cast<std::size_t>(-lag) : 0U;
  const std::size_t cap_start = lag > 0 ? static_cast<std::size_t>(lag) : 0U;
  if (ref_start >= reference.size() || cap_start >= capture.size()) {
    return 0.0;
  }
  const auto count = std::min(reference.size() - ref_start, capture.size() - cap_start);
  if (count < 16U) {
    return 0.0;
  }
  long double dot = 0.0;
  long double rr = 0.0;
  long double cc = 0.0;
  for (std::size_t index = 0; index < count; index += stride) {
    const double r = reference[ref_start + index];
    const double c = capture[cap_start + index];
    dot += static_cast<long double>(r) * c;
    rr += static_cast<long double>(r) * r;
    cc += static_cast<long double>(c) * c;
  }
  return static_cast<double>(dot / (std::sqrt(rr * cc) + kEpsilon));
}

Alignment align_buffers(const StereoBuffer& reference,
                        const StereoBuffer& capture,
                        double max_lag_seconds) {
  const auto ref_mono = mono_centered(reference.frames);
  const auto cap_mono = mono_centered(capture.frames);
  const auto max_lag = static_cast<std::int32_t>(std::llround(max_lag_seconds * reference.sample_rate));
  const auto coarse_step = 1;
  std::int32_t best_lag = 0;
  double best_score = -1.0;
  for (std::int32_t lag = -max_lag; lag <= max_lag; lag += coarse_step) {
    const double score = std::abs(normalized_lag_score(ref_mono, cap_mono, lag, 64U));
    if (score > best_score) {
      best_score = score;
      best_lag = lag;
    }
  }

  const auto refine_radius = std::max<std::int32_t>(128, coarse_step * 2);
  for (std::int32_t lag = std::max(-max_lag, best_lag - refine_radius);
       lag <= std::min(max_lag, best_lag + refine_radius);
       ++lag) {
    const double score = std::abs(normalized_lag_score(ref_mono, cap_mono, lag, 1U));
    if (score > best_score) {
      best_score = score;
      best_lag = lag;
    }
  }

  Alignment out{};
  out.lag_frames = best_lag;
  out.lag_ms = 1000.0 * static_cast<double>(best_lag) / reference.sample_rate;
  out.score = normalized_lag_score(ref_mono, cap_mono, best_lag, 1U);
  out.reference_start = best_lag < 0 ? static_cast<std::size_t>(-best_lag) : 0U;
  out.capture_start = best_lag > 0 ? static_cast<std::size_t>(best_lag) : 0U;
  out.compared_frames =
      std::min(reference.frames.size() - out.reference_start, capture.frames.size() - out.capture_start);
  return out;
}

struct Biquad {
  double b0 = 1.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double a1 = 0.0;
  double a2 = 0.0;
  double z1 = 0.0;
  double z2 = 0.0;

  double process(double input) {
    const double output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
  }
};

Biquad make_bandpass(double sample_rate, double low, double high) {
  const double center = std::sqrt(low * high);
  const double q = center / std::max(1.0, high - low);
  const double omega = 2.0 * kPi * center / sample_rate;
  const double alpha = std::sin(omega) / (2.0 * q);
  const double cosw = std::cos(omega);
  const double a0 = 1.0 + alpha;
  Biquad filter{};
  filter.b0 = alpha / a0;
  filter.b1 = 0.0;
  filter.b2 = -alpha / a0;
  filter.a1 = (-2.0 * cosw) / a0;
  filter.a2 = (1.0 - alpha) / a0;
  return filter;
}

std::vector<double> bandpass(std::span<const double> input,
                             double sample_rate,
                             double low,
                             double high) {
  auto forward = make_bandpass(sample_rate, low, high);
  std::vector<double> temp;
  temp.reserve(input.size());
  for (const double value : input) {
    temp.push_back(forward.process(value));
  }
  auto reverse = make_bandpass(sample_rate, low, high);
  std::vector<double> out(temp.size());
  for (std::size_t index = temp.size(); index > 0U; --index) {
    out[index - 1U] = reverse.process(temp[index - 1U]);
  }
  return out;
}

double correlation_squared(std::span<const double> reference, std::span<const double> capture) {
  if (reference.size() != capture.size() || reference.empty()) {
    return 0.0;
  }
  long double mean_r = 0.0;
  long double mean_c = 0.0;
  for (std::size_t index = 0; index < reference.size(); ++index) {
    mean_r += reference[index];
    mean_c += capture[index];
  }
  mean_r /= reference.size();
  mean_c /= capture.size();
  long double dot = 0.0;
  long double rr = 0.0;
  long double cc = 0.0;
  for (std::size_t index = 0; index < reference.size(); ++index) {
    const double r = reference[index] - static_cast<double>(mean_r);
    const double c = capture[index] - static_cast<double>(mean_c);
    dot += static_cast<long double>(r) * c;
    rr += static_cast<long double>(r) * r;
    cc += static_cast<long double>(c) * c;
  }
  const double corr = static_cast<double>(dot / (std::sqrt(rr * cc) + kEpsilon));
  return std::clamp(corr * corr, 0.0, 1.0);
}

double fitted_gain(std::span<const double> reference, std::span<const double> capture) {
  long double dot = 0.0;
  long double rr = 0.0;
  for (std::size_t index = 0; index < reference.size(); ++index) {
    dot += static_cast<long double>(capture[index]) * reference[index];
    rr += static_cast<long double>(reference[index]) * reference[index];
  }
  return static_cast<double>(dot / (rr + kEpsilon));
}

double residual_ratio(std::span<const double> reference, std::span<const double> capture) {
  const double gain = fitted_gain(reference, capture);
  long double signal = 0.0;
  long double residual = 0.0;
  for (std::size_t index = 0; index < reference.size(); ++index) {
    const double fitted = gain * reference[index];
    const double error = capture[index] - fitted;
    signal += static_cast<long double>(fitted) * fitted;
    residual += static_cast<long double>(error) * error;
  }
  return static_cast<double>(residual / (signal + kEpsilon));
}

double percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double pos = std::clamp(p, 0.0, 100.0) / 100.0 * static_cast<double>(values.size() - 1U);
  const auto lo = static_cast<std::size_t>(std::floor(pos));
  const auto hi = static_cast<std::size_t>(std::ceil(pos));
  const double frac = pos - static_cast<double>(lo);
  return values[lo] * (1.0 - frac) + values[hi] * frac;
}

double pearson_abs(std::span<const double> a, std::span<const double> b) {
  if (a.size() != b.size() || a.empty()) {
    return 0.0;
  }
  long double mean_a = 0.0;
  long double mean_b = 0.0;
  for (std::size_t index = 0; index < a.size(); ++index) {
    mean_a += a[index];
    mean_b += b[index];
  }
  mean_a /= a.size();
  mean_b /= b.size();
  long double dot = 0.0;
  long double aa = 0.0;
  long double bb = 0.0;
  for (std::size_t index = 0; index < a.size(); ++index) {
    const double da = a[index] - static_cast<double>(mean_a);
    const double db = b[index] - static_cast<double>(mean_b);
    dot += static_cast<long double>(da) * db;
    aa += static_cast<long double>(da) * da;
    bb += static_cast<long double>(db) * db;
  }
  return std::abs(static_cast<double>(dot / (std::sqrt(aa * bb) + kEpsilon)));
}

double peak_to_rms_db(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  double peak = 0.0;
  for (const double value : values) {
    peak = std::max(peak, std::abs(value));
  }
  return 20.0 * std::log10((peak + kEpsilon) / (rms(values) + kEpsilon));
}

double residual_burst_p95_to_median_db(std::span<const double> residual,
                                       std::uint32_t sample_rate) {
  const auto window = std::max<std::size_t>(64U, sample_rate / 100U);
  const auto hop = std::max<std::size_t>(1U, window / 2U);
  if (residual.size() < window * 4U) {
    return 0.0;
  }
  std::vector<double> window_db;
  for (std::size_t start = 0; start + window <= residual.size(); start += hop) {
    window_db.push_back(db20(rms(residual.subspan(start, window))));
  }
  if (window_db.size() < 4U) {
    return 0.0;
  }
  return percentile(window_db, 95.0) - percentile(window_db, 50.0);
}

double gain_ripple_db(std::span<const double> reference, std::span<const double> capture) {
  constexpr std::size_t kWindows = 16U;
  if (reference.size() < kWindows * 64U) {
    return 0.0;
  }
  const auto window = reference.size() / kWindows;
  std::vector<double> gains;
  gains.reserve(kWindows);
  for (std::size_t start = 0; start + window <= reference.size(); start += window) {
    const double gain = fitted_gain(reference.subspan(start, window), capture.subspan(start, window));
    if (std::isfinite(gain) && std::abs(gain) > kEpsilon) {
      gains.push_back(db20(gain));
    }
  }
  if (gains.size() < 4U) {
    return 0.0;
  }
  return percentile(gains, 95.0) - percentile(gains, 5.0);
}

ChannelMetrics channel_metrics(std::span<const double> reference,
                               std::span<const double> capture,
                               std::uint32_t sample_rate) {
  const double gain = fitted_gain(reference, capture);
  std::vector<double> fitted(reference.size());
  std::vector<double> residual(reference.size());
  for (std::size_t index = 0; index < reference.size(); ++index) {
    fitted[index] = gain * reference[index];
    residual[index] = capture[index] - fitted[index];
  }

  const auto low_ref = bandpass(reference, sample_rate, 20.0, 200.0);
  const auto low_cap = bandpass(capture, sample_rate, 20.0, 200.0);
  const auto mid_ref = bandpass(reference, sample_rate, 200.0, 5000.0);
  const auto mid_cap = bandpass(capture, sample_rate, 200.0, 5000.0);
  const auto high_ref = bandpass(reference, sample_rate, 5000.0, std::min(18000.0, sample_rate / 2.0 - 100.0));
  const auto high_cap = bandpass(capture, sample_rate, 5000.0, std::min(18000.0, sample_rate / 2.0 - 100.0));

  double peak = 0.0;
  std::uint64_t clipped = 0;
  long double mean = 0.0;
  for (const double value : capture) {
    peak = std::max(peak, std::abs(value));
    mean += value;
    if (std::abs(value) >= 0.999) {
      ++clipped;
    }
  }
  if (!capture.empty()) {
    mean /= capture.size();
  }

  ChannelMetrics out{};
  out.gain_db = db20(gain);
  out.signal_rms_dbfs = db20(rms(fitted));
  out.residual_rms_dbfs = db20(rms(residual));
  out.snr_db = 20.0 * std::log10((rms(fitted) + kEpsilon) / (rms(residual) + kEpsilon));
  out.peak_dbfs = db20(peak);
  out.clipped_frames = clipped;
  out.dc_offset_dbfs = db20(static_cast<double>(mean));
  out.coherence_low_active_mean = correlation_squared(low_ref, low_cap);
  out.coherence_mid_active_mean = correlation_squared(mid_ref, mid_cap);
  out.coherence_high_active_mean = correlation_squared(high_ref, high_cap);
  out.transfer_ripple_low_db = gain_ripple_db(low_ref, low_cap);
  out.transfer_ripple_mid_db = gain_ripple_db(mid_ref, mid_cap);
  out.transfer_ripple_high_db = gain_ripple_db(high_ref, high_cap);
  out.residual_ratio_mid_mean = residual_ratio(mid_ref, mid_cap);
  out.residual_ratio_high_mean = residual_ratio(high_ref, high_cap);
  out.residual_burst_p95_to_median_db =
      residual_burst_p95_to_median_db(residual, sample_rate);
  out.residual_signal_abs_correlation = pearson_abs(fitted, residual);
  out.residual_peak_to_rms_db = peak_to_rms_db(residual);
  return out;
}

StereoMatrix stereo_matrix(std::span<const std::array<double, 2>> reference,
                           std::span<const std::array<double, 2>> capture) {
  long double a = 0.0;
  long double b = 0.0;
  long double d = 0.0;
  long double y00 = 0.0;
  long double y01 = 0.0;
  long double y10 = 0.0;
  long double y11 = 0.0;
  for (std::size_t index = 0; index < reference.size(); ++index) {
    const double l = reference[index][0];
    const double r = reference[index][1];
    a += static_cast<long double>(l) * l;
    b += static_cast<long double>(l) * r;
    d += static_cast<long double>(r) * r;
    y00 += static_cast<long double>(l) * capture[index][0];
    y01 += static_cast<long double>(l) * capture[index][1];
    y10 += static_cast<long double>(r) * capture[index][0];
    y11 += static_cast<long double>(r) * capture[index][1];
  }
  const long double det = a * d - b * b;
  StereoMatrix out{};
  if (std::abs(static_cast<double>(det)) <= kEpsilon) {
    return out;
  }
  out.matrix[0][0] = static_cast<double>((d * y00 - b * y10) / det);
  out.matrix[0][1] = static_cast<double>((d * y01 - b * y11) / det);
  out.matrix[1][0] = static_cast<double>((-b * y00 + a * y10) / det);
  out.matrix[1][1] = static_cast<double>((-b * y01 + a * y11) / det);

  const double trace = static_cast<double>(a + d);
  const double disc = std::sqrt(std::max(0.0, static_cast<double>((a - d) * (a - d) + 4.0L * b * b)));
  const double lambda_max = 0.5 * (trace + disc);
  const double lambda_min = 0.5 * (trace - disc);
  out.condition_number = lambda_min > kEpsilon ? lambda_max / lambda_min
                                               : std::numeric_limits<double>::infinity();
  const double diag = std::max({std::abs(out.matrix[0][0]), std::abs(out.matrix[1][1]), kEpsilon});
  const double off = std::max(std::abs(out.matrix[0][1]), std::abs(out.matrix[1][0]));
  out.worst_offdiag_db_relative = 20.0 * std::log10(std::max(off, kEpsilon) / diag);
  out.leakage_evaluable = out.condition_number <= 20.0;
  return out;
}

DelayWindows window_lags(const StereoBuffer& reference,
                         const StereoBuffer& capture,
                         std::int32_t global_lag,
                         double window_seconds,
                         double search_ms) {
  const auto ref_mono = mono_centered(reference.frames);
  const auto cap_mono = mono_centered(capture.frames);
  const auto window = static_cast<std::size_t>(std::llround(window_seconds * reference.sample_rate));
  const auto search = static_cast<std::int32_t>(std::llround(search_ms * reference.sample_rate / 1000.0));
  std::vector<std::int32_t> lags;
  if (window == 0U || ref_mono.size() < window * 2U) {
    return {};
  }
  for (std::size_t start = 0; start + window <= ref_mono.size(); start += window) {
    double best_score = -1.0;
    std::int32_t best_lag = global_lag;
    bool found = false;
    for (std::int32_t delta = -search; delta <= search; ++delta) {
      const auto lag = static_cast<std::int32_t>(static_cast<std::int64_t>(global_lag) + delta);
      const std::size_t ref_start = start + (lag < 0 ? static_cast<std::size_t>(-lag) : 0U);
      const std::size_t cap_start = start + (lag > 0 ? static_cast<std::size_t>(lag) : 0U);
      if (ref_start + window > ref_mono.size() || cap_start + window > cap_mono.size()) {
        continue;
      }
      const double score = std::abs(normalized_lag_score(
          std::span<const double>(ref_mono).subspan(ref_start, window),
          std::span<const double>(cap_mono).subspan(cap_start, window),
          0,
          1U));
      if (score > best_score) {
        best_score = score;
        best_lag = lag;
        found = true;
      }
    }
    if (found && best_score >= 0.5) {
      lags.push_back(best_lag);
    }
  }
  if (lags.empty()) {
    return {};
  }
  std::vector<double> centered;
  centered.reserve(lags.size());
  for (const auto lag : lags) {
    centered.push_back(static_cast<double>(lag - global_lag));
  }
  std::vector<double> abs_centered;
  abs_centered.reserve(centered.size());
  for (const auto lag : centered) {
    abs_centered.push_back(std::abs(lag));
  }
  DelayWindows out{};
  out.windows = static_cast<std::uint32_t>(lags.size());
  out.min_frames = *std::min_element(centered.begin(), centered.end());
  out.max_frames = *std::max_element(centered.begin(), centered.end());
  out.p95_abs_frames = percentile(abs_centered, 95.0);
  out.range_frames = out.max_frames - out.min_frames;
  for (std::size_t index = 1; index < lags.size(); ++index) {
    if (std::abs(lags[index] - lags[index - 1U]) > 2) {
      ++out.lag_jumps_gt_2_frames;
    }
  }
  return out;
}

Analysis analyze_buffers(const StereoBuffer& reference_in,
                         const StereoBuffer& capture_in,
                         const Cli& cli) {
  if (reference_in.sample_rate != capture_in.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch");
  }
  auto reference = reference_in;
  auto capture = capture_in;
  trim_seconds(reference, cli.seconds);
  if (cli.seconds) {
    trim_seconds(capture, *cli.seconds + cli.max_lag_seconds);
  }
  const auto alignment = align_buffers(reference, capture, cli.max_lag_seconds);
  if (alignment.compared_frames == 0U) {
    throw std::runtime_error("aligned_files_have_no_overlap");
  }

  const auto ref_span = std::span<const std::array<double, 2>>(reference.frames)
                            .subspan(alignment.reference_start, alignment.compared_frames);
  const auto cap_span = std::span<const std::array<double, 2>>(capture.frames)
                            .subspan(alignment.capture_start, alignment.compared_frames);
  const auto ref_left_all = channel(reference.frames, 0U);
  const auto ref_right_all = channel(reference.frames, 1U);
  const auto cap_left_all = channel(capture.frames, 0U);
  const auto cap_right_all = channel(capture.frames, 1U);

  const auto ref_left = std::span<const double>(ref_left_all).subspan(alignment.reference_start, alignment.compared_frames);
  const auto ref_right = std::span<const double>(ref_right_all).subspan(alignment.reference_start, alignment.compared_frames);
  const auto cap_left = std::span<const double>(cap_left_all).subspan(alignment.capture_start, alignment.compared_frames);
  const auto cap_right = std::span<const double>(cap_right_all).subspan(alignment.capture_start, alignment.compared_frames);

  Analysis out{};
  out.label = cli.label;
  out.reference = cli.self_test ? "<synthetic-cpp-self-test>" : cli.reference.string();
  out.capture = cli.self_test ? "<synthetic-cpp-self-test>" : cli.capture.string();
  out.sample_rate = reference.sample_rate;
  out.analysis_seconds_requested = cli.seconds;
  out.alignment = alignment;
  out.left = channel_metrics(ref_left, cap_left, reference.sample_rate);
  out.right = channel_metrics(ref_right, cap_right, reference.sample_rate);
  out.stereo_matrix = stereo_matrix(ref_span, cap_span);
  out.delay_windows = window_lags(reference, capture, alignment.lag_frames, cli.delay_window_seconds,
                                  cli.delay_search_ms);
  out.thresholds = cli.thresholds;

  if (out.left.clipped_frames != 0U || out.right.clipped_frames != 0U) {
    out.blockers.push_back("capture_clipping_present");
  }
  if (std::abs(out.alignment.score) < cli.thresholds.min_alignment_score) {
    out.blockers.push_back("alignment_score_below_threshold");
  }
  if (std::min(out.left.snr_db, out.right.snr_db) < cli.thresholds.min_snr_db) {
    out.blockers.push_back("snr_below_threshold");
  }
  if (std::min(out.left.coherence_mid_active_mean, out.right.coherence_mid_active_mean) <
      cli.thresholds.min_mid_coherence) {
    out.blockers.push_back("mid_band_coherence_below_threshold");
  }
  if (out.delay_windows.windows == 0U ||
      out.delay_windows.p95_abs_frames > cli.thresholds.max_delay_p95_frames) {
    out.blockers.push_back("delay_p95_above_threshold");
  }
  if (!out.stereo_matrix.leakage_evaluable) {
    out.blockers.push_back("stereo_leakage_not_evaluable_reference_not_decorrelated");
  } else if (out.stereo_matrix.worst_offdiag_db_relative > cli.thresholds.max_leakage_db) {
    out.blockers.push_back("stereo_leakage_above_threshold");
  }
  if (std::max(out.left.residual_burst_p95_to_median_db,
               out.right.residual_burst_p95_to_median_db) >
      cli.thresholds.max_residual_burst_p95_to_median_db) {
    out.blockers.push_back("residual_burst_above_threshold");
  }
  if (std::max(out.left.residual_signal_abs_correlation,
               out.right.residual_signal_abs_correlation) >
      cli.thresholds.max_residual_signal_abs_correlation) {
    out.blockers.push_back("residual_signal_correlation_above_threshold");
  }
  if (std::max(out.left.residual_peak_to_rms_db, out.right.residual_peak_to_rms_db) >
      cli.thresholds.max_residual_peak_to_rms_db) {
    out.blockers.push_back("residual_peak_to_rms_above_threshold");
  }
  return out;
}

std::uint64_t next_lcg(std::uint64_t& state) {
  state = state * 6364136223846793005ULL + 1442695040888963407ULL;
  return state;
}

double noise(std::uint64_t& state) {
  const auto value = next_lcg(state) >> 11U;
  return (static_cast<double>(value) / static_cast<double>(1ULL << 53U)) * 2.0 - 1.0;
}

StereoBuffer make_self_reference(std::uint32_t sample_rate, double seconds) {
  const auto frames = static_cast<std::size_t>(std::llround(sample_rate * seconds));
  StereoBuffer out{};
  out.sample_rate = sample_rate;
  out.frames.reserve(frames);
  std::uint64_t state = 8;
  double left_lp = 0.0;
  double right_lp = 0.0;
  double left_prev = 0.0;
  double right_prev = 0.0;
  for (std::size_t index = 0; index < frames; ++index) {
    const double left_white = noise(state);
    const double right_white = noise(state);
    left_lp = 0.985 * left_lp + 0.015 * left_white;
    right_lp = 0.985 * right_lp + 0.015 * right_white;
    const double left_hp = left_white - left_lp;
    const double right_hp = right_white - right_lp;
    const double left = 0.13 * left_hp + 0.035 * left_prev;
    const double right = 0.13 * right_hp + 0.035 * right_prev;
    left_prev = left_hp;
    right_prev = right_hp;
    out.frames.push_back({std::clamp(left, -0.22, 0.22), std::clamp(right, -0.22, 0.22)});
  }
  return out;
}

StereoBuffer make_self_capture(const StereoBuffer& reference) {
  StereoBuffer out{};
  out.sample_rate = reference.sample_rate;
  out.frames.assign(reference.frames.size(), {0.0, 0.0});
  std::uint64_t state = 9;
  constexpr std::size_t delay = 37U;
  for (std::size_t index = delay; index < reference.frames.size(); ++index) {
    const auto& src = reference.frames[index - delay];
    out.frames[index][0] = 0.92 * src[0] + 0.00003 * src[1] + 1.0e-5 * noise(state);
    out.frames[index][1] = 0.92 * src[1] + 0.00002 * src[0] + 1.0e-5 * noise(state);
  }
  return out;
}

StereoBuffer make_self_degraded_capture(const StereoBuffer& reference) {
  StereoBuffer out{};
  out.sample_rate = reference.sample_rate;
  out.frames.reserve(reference.frames.size());
  std::uint64_t state = 99;
  for (std::size_t index = 0; index < reference.frames.size(); ++index) {
    const double left = 0.10 * noise(state);
    const double right = 0.10 * noise(state);
    out.frames.push_back({left, right});
  }
  return out;
}

std::string json_escape(std::string_view value) {
  std::string out;
  for (const char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

void print_number(std::ostream& out, std::string_view key, double value, bool comma = true) {
  out << "    \"" << key << "\": ";
  if (std::isfinite(value)) {
    out << value;
  } else {
    out << "null";
  }
  out << (comma ? ",\n" : "\n");
}

void print_channel(std::ostream& out, std::string_view key, const ChannelMetrics& metrics, bool comma) {
  out << "  \"" << key << "\": {\n";
  print_number(out, "gain_db", metrics.gain_db);
  print_number(out, "signal_rms_dbfs", metrics.signal_rms_dbfs);
  print_number(out, "residual_rms_dbfs", metrics.residual_rms_dbfs);
  print_number(out, "snr_db", metrics.snr_db);
  print_number(out, "peak_dbfs", metrics.peak_dbfs);
  out << "    \"clipped_frames\": " << metrics.clipped_frames << ",\n";
  print_number(out, "dc_offset_dbfs", metrics.dc_offset_dbfs);
  print_number(out, "coherence_low_active_mean", metrics.coherence_low_active_mean);
  print_number(out, "coherence_mid_active_mean", metrics.coherence_mid_active_mean);
  print_number(out, "coherence_high_active_mean", metrics.coherence_high_active_mean);
  print_number(out, "transfer_ripple_low_db", metrics.transfer_ripple_low_db);
  print_number(out, "transfer_ripple_mid_db", metrics.transfer_ripple_mid_db);
  print_number(out, "transfer_ripple_high_db", metrics.transfer_ripple_high_db);
  print_number(out, "residual_ratio_mid_mean", metrics.residual_ratio_mid_mean);
  print_number(out, "residual_ratio_high_mean", metrics.residual_ratio_high_mean);
  print_number(out, "residual_burst_p95_to_median_db",
               metrics.residual_burst_p95_to_median_db);
  print_number(out, "residual_signal_abs_correlation",
               metrics.residual_signal_abs_correlation);
  print_number(out, "residual_peak_to_rms_db", metrics.residual_peak_to_rms_db, false);
  out << "  }" << (comma ? "," : "") << "\n";
}

std::string to_json(const Analysis& analysis) {
  std::ostringstream out;
  out.precision(12);
  out << "{\n";
  out << "  \"schema\": \"" << analysis.schema << "\",\n";
  out << "  \"safety\": \"offline_wav_read_only_no_audio_coreaudio_usb_driver_or_hardware_touch\",\n";
  out << "  \"label\": \"" << json_escape(analysis.label) << "\",\n";
  out << "  \"reference\": \"" << json_escape(analysis.reference) << "\",\n";
  out << "  \"capture\": \"" << json_escape(analysis.capture) << "\",\n";
  out << "  \"sample_rate\": " << analysis.sample_rate << ",\n";
  out << "  \"analysis_seconds_requested\": ";
  if (analysis.analysis_seconds_requested) {
    out << *analysis.analysis_seconds_requested;
  } else {
    out << "null";
  }
  out << ",\n";
  out << "  \"alignment\": {\n";
  out << "    \"lag_frames\": " << analysis.alignment.lag_frames << ",\n";
  print_number(out, "lag_ms", analysis.alignment.lag_ms);
  print_number(out, "score", analysis.alignment.score);
  out << "    \"reference_start\": " << analysis.alignment.reference_start << ",\n";
  out << "    \"capture_start\": " << analysis.alignment.capture_start << ",\n";
  out << "    \"compared_frames\": " << analysis.alignment.compared_frames << "\n";
  out << "  },\n";
  print_channel(out, "left", analysis.left, true);
  print_channel(out, "right", analysis.right, true);
  out << "  \"stereo_matrix\": {\n";
  out << "    \"matrix\": [[" << analysis.stereo_matrix.matrix[0][0] << ", "
      << analysis.stereo_matrix.matrix[0][1] << "], ["
      << analysis.stereo_matrix.matrix[1][0] << ", " << analysis.stereo_matrix.matrix[1][1]
      << "]],\n";
  print_number(out, "worst_offdiag_db_relative", analysis.stereo_matrix.worst_offdiag_db_relative);
  print_number(out, "condition_number", analysis.stereo_matrix.condition_number);
  out << "    \"leakage_evaluable\": "
      << (analysis.stereo_matrix.leakage_evaluable ? "true" : "false") << "\n";
  out << "  },\n";
  out << "  \"delay_windows\": {\n";
  out << "    \"windows\": " << analysis.delay_windows.windows << ",\n";
  print_number(out, "min_frames", analysis.delay_windows.min_frames);
  print_number(out, "max_frames", analysis.delay_windows.max_frames);
  print_number(out, "p95_abs_frames", analysis.delay_windows.p95_abs_frames);
  print_number(out, "range_frames", analysis.delay_windows.range_frames);
  out << "    \"lag_jumps_gt_2_frames\": " << analysis.delay_windows.lag_jumps_gt_2_frames << "\n";
  out << "  },\n";
  out << "  \"thresholds\": {\n";
  print_number(out, "min_alignment_score", analysis.thresholds.min_alignment_score);
  print_number(out, "min_snr_db", analysis.thresholds.min_snr_db);
  print_number(out, "min_mid_coherence", analysis.thresholds.min_mid_coherence);
  print_number(out, "max_delay_p95_frames", analysis.thresholds.max_delay_p95_frames);
  print_number(out, "max_leakage_db", analysis.thresholds.max_leakage_db);
  print_number(out, "max_residual_burst_p95_to_median_db",
               analysis.thresholds.max_residual_burst_p95_to_median_db);
  print_number(out, "max_residual_signal_abs_correlation",
               analysis.thresholds.max_residual_signal_abs_correlation);
  print_number(out, "max_residual_peak_to_rms_db",
               analysis.thresholds.max_residual_peak_to_rms_db, false);
  out << "  },\n";
  out << "  \"result\": \"" << (analysis.blockers.empty() ? "PASS" : "FAIL") << "\",\n";
  out << "  \"blockers\": [";
  for (std::size_t index = 0; index < analysis.blockers.size(); ++index) {
    if (index != 0U) {
      out << ", ";
    }
    out << "\"" << analysis.blockers[index] << "\"";
  }
  out << "],\n";
  out << "  \"product_claim_allowed\": false\n";
  out << "}\n";
  return out.str();
}

Cli parse_cli(int argc, char** argv) {
  Cli cli{};
  auto next = [&](int& index) -> std::string_view {
    if (index + 1 >= argc) {
      throw std::runtime_error("missing_value");
    }
    return argv[++index];
  };
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg = argv[index];
    if (arg == "--reference") {
      cli.reference = std::string(next(index));
    } else if (arg == "--capture") {
      cli.capture = std::string(next(index));
    } else if (arg == "--reference-channels") {
      cli.reference_channels = parse_channels(next(index));
    } else if (arg == "--capture-channels") {
      cli.capture_channels = parse_channels(next(index));
    } else if (arg == "--seconds") {
      cli.seconds = std::stod(std::string(next(index)));
    } else if (arg == "--max-lag-seconds") {
      cli.max_lag_seconds = std::stod(std::string(next(index)));
    } else if (arg == "--delay-window-seconds") {
      cli.delay_window_seconds = std::stod(std::string(next(index)));
    } else if (arg == "--delay-search-ms") {
      cli.delay_search_ms = std::stod(std::string(next(index)));
    } else if (arg == "--min-alignment-score") {
      cli.thresholds.min_alignment_score = std::stod(std::string(next(index)));
    } else if (arg == "--min-snr-db") {
      cli.thresholds.min_snr_db = std::stod(std::string(next(index)));
    } else if (arg == "--min-mid-coherence") {
      cli.thresholds.min_mid_coherence = std::stod(std::string(next(index)));
    } else if (arg == "--max-delay-p95-frames") {
      cli.thresholds.max_delay_p95_frames = std::stod(std::string(next(index)));
    } else if (arg == "--max-leakage-db") {
      cli.thresholds.max_leakage_db = std::stod(std::string(next(index)));
    } else if (arg == "--max-residual-burst-p95-to-median-db") {
      cli.thresholds.max_residual_burst_p95_to_median_db =
          std::stod(std::string(next(index)));
    } else if (arg == "--max-residual-signal-abs-correlation") {
      cli.thresholds.max_residual_signal_abs_correlation =
          std::stod(std::string(next(index)));
    } else if (arg == "--max-residual-peak-to-rms-db") {
      cli.thresholds.max_residual_peak_to_rms_db = std::stod(std::string(next(index)));
    } else if (arg == "--label") {
      cli.label = std::string(next(index));
    } else if (arg == "--json-out") {
      cli.json_out = std::string(next(index));
    } else if (arg == "--self-test") {
      cli.self_test = true;
    } else if (arg == "--self-test-degraded") {
      cli.self_test_degraded = true;
    } else {
      throw std::runtime_error("unknown_argument:" + std::string(arg));
    }
  }
  return cli;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    auto cli = parse_cli(argc, argv);
    Analysis analysis;
    if (cli.self_test || cli.self_test_degraded) {
      cli.seconds = 3.0;
      cli.label = cli.label.empty()
                      ? (cli.self_test_degraded ? "self-test-degraded-cpp" : "self-test-cpp")
                      : cli.label;
      const auto reference = make_self_reference(48000U, 3.0);
      const auto capture =
          cli.self_test_degraded ? make_self_degraded_capture(reference) : make_self_capture(reference);
      analysis = analyze_buffers(reference, capture, cli);
    } else {
      if (cli.reference.empty() || cli.capture.empty()) {
        throw std::runtime_error("missing_reference_or_capture");
      }
      const auto reference = read_wav_pair(cli.reference, cli.reference_channels);
      const auto capture = read_wav_pair(cli.capture, cli.capture_channels);
      analysis = analyze_buffers(reference, capture, cli);
    }
    const auto json = to_json(analysis);
    if (!cli.json_out.empty()) {
      std::filesystem::create_directories(cli.json_out.parent_path());
      std::ofstream output(cli.json_out);
      output << json;
    }
    std::cout << json;
    return analysis.blockers.empty() ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "opena8djcpp_audiophile_wav_analysis: " << error.what() << "\n";
    return 2;
  }
}
