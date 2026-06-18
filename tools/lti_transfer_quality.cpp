#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kEpsilon = 1.0e-24;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
};

struct Cli {
  std::filesystem::path json_out;
  double max_seconds = 16.0;
  std::int32_t max_lag = 8192;
  std::uint32_t nperseg = 16384;
  std::uint32_t smooth_bins = 9;
  bool self_test = false;
  std::vector<std::filesystem::path> soundcheck_dirs;
};

struct Alignment {
  std::size_t reference_start = 0;
  std::size_t capture_start = 0;
  std::int32_t lag = 0;
};

struct ChannelMetrics {
  double scalar_gain = 0.0;
  double scalar_snr_db = 0.0;
  double lti_snr_db = 0.0;
  double scalar_residual_rms = 0.0;
  double lti_residual_rms = 0.0;
  double scalar_mid_ratio = 0.0;
  double lti_mid_ratio = 0.0;
  double scalar_high_ratio = 0.0;
  double lti_high_ratio = 0.0;
  double coherence_low_mean = 0.0;
  double coherence_mid_mean = 0.0;
  double coherence_high_mean = 0.0;
  double transfer_mag_low_db = 0.0;
  double transfer_mag_mid_db = 0.0;
  double transfer_mag_high_db = 0.0;
  double lti_snr_delta_db = 0.0;
  double lti_mid_ratio_delta = 0.0;
  double lti_high_ratio_delta = 0.0;
};

struct RunMetrics {
  std::string run_dir;
  std::string reference;
  std::string capture;
  std::uint32_t rate = 0;
  std::int32_t alignment_lag = 0;
  std::size_t compared_frames = 0;
  double compared_seconds = 0.0;
  ChannelMetrics left;
  ChannelMetrics right;
  double min_lti_snr_delta_db = 0.0;
  double max_lti_mid_ratio_delta = 0.0;
  double min_mid_coherence = 0.0;
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
  const auto frames = data_size / block_align;

  StereoBuffer out{};
  out.sample_rate = sample_rate;
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
  const auto direct = run_dir / "fixture/reference.wav";
  if (std::filesystem::is_regular_file(direct)) {
    return direct;
  }
  const auto prepare = run_dir / "prepare.log";
  if (std::filesystem::is_regular_file(prepare)) {
    std::ifstream input(prepare);
    std::string line;
    while (std::getline(input, line)) {
      constexpr std::string_view prefix = "reference=";
      if (line.rfind(prefix, 0) == 0) {
        auto path = std::filesystem::path(line.substr(prefix.size()));
        if (std::filesystem::is_regular_file(path)) {
          return path;
        }
        path = run_dir / path;
        if (std::filesystem::is_regular_file(path)) {
          return path;
        }
      }
    }
  }
  throw std::runtime_error("cannot_find_reference:" + run_dir.string());
}

std::size_t first_signal_index(const StereoBuffer& buffer) {
  double peak = 0.0;
  for (const auto& frame : buffer.frames) {
    peak = std::max(peak, std::max(std::abs(frame[0]), std::abs(frame[1])));
  }
  const double threshold = std::max(0.0005, peak * 0.02);
  for (std::size_t index = 0; index < buffer.frames.size(); ++index) {
    if (std::max(std::abs(buffer.frames[index][0]), std::abs(buffer.frames[index][1])) >=
        threshold) {
      return index;
    }
  }
  return 0;
}

double mono_at(const StereoBuffer& buffer, std::size_t index) {
  return 0.5 * (buffer.frames[index][0] + buffer.frames[index][1]);
}

Alignment align_pair(const StereoBuffer& reference,
                     const StereoBuffer& capture,
                     std::int32_t max_lag) {
  const auto ref_start = first_signal_index(reference);
  const auto cap_start = first_signal_index(capture);
  const auto fit = std::min<std::size_t>(
      {reference.frames.size() - ref_start, capture.frames.size() - cap_start,
       reference.sample_rate});
  if (fit == 0U) {
    throw std::runtime_error("not_enough_signal_for_alignment");
  }

  const auto region_start = cap_start > static_cast<std::size_t>(max_lag)
                                ? cap_start - static_cast<std::size_t>(max_lag)
                                : 0U;
  const auto region_end =
      std::min(capture.frames.size(), cap_start + fit + static_cast<std::size_t>(max_lag));
  const std::size_t stride = 8U;
  double best_score = -1.0;
  std::size_t best_capture_start = cap_start;
  for (std::size_t candidate = region_start; candidate + fit <= region_end; ++candidate) {
    long double dot_sum = 0.0;
    long double ref_power = 0.0;
    long double cap_power = 0.0;
    for (std::size_t index = 0; index < fit; index += stride) {
      const double r = mono_at(reference, ref_start + index);
      const double c = mono_at(capture, candidate + index);
      dot_sum += static_cast<long double>(r) * c;
      ref_power += static_cast<long double>(r) * r;
      cap_power += static_cast<long double>(c) * c;
    }
    const double denom = std::sqrt(static_cast<double>(ref_power * cap_power));
    const double score = denom > 0.0 ? std::abs(static_cast<double>(dot_sum) / denom) : 0.0;
    if (score > best_score) {
      best_score = score;
      best_capture_start = candidate;
    }
  }
  return {ref_start, best_capture_start,
          static_cast<std::int32_t>(best_capture_start) - static_cast<std::int32_t>(ref_start)};
}

std::vector<double> channel_vector(const StereoBuffer& buffer,
                                   std::size_t start,
                                   std::size_t frames,
                                   std::uint32_t channel) {
  std::vector<double> out;
  out.reserve(frames);
  for (std::size_t index = 0; index < frames; ++index) {
    out.push_back(buffer.frames[start + index][channel]);
  }
  return out;
}

std::complex<double> dft_at(std::span<const double> values,
                            double frequency,
                            double sample_rate) {
  std::complex<double> sum{};
  const auto count = values.size();
  for (std::size_t index = 0; index < count; ++index) {
    const double window =
        count > 1U ? 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(index) /
                                          static_cast<double>(count - 1U))
                   : 1.0;
    const double phase = -2.0 * kPi * frequency * static_cast<double>(index) / sample_rate;
    sum += values[index] * window * std::complex<double>(std::cos(phase), std::sin(phase));
  }
  return sum;
}

struct SpectralPoint {
  double frequency = 0.0;
  std::complex<double> transfer{};
  double coherence = 0.0;
  double scalar_residual_power = 0.0;
  double scalar_signal_power = 0.0;
  double lti_residual_power = 0.0;
  double lti_signal_power = 0.0;
};

std::vector<double> frequency_grid() {
  return {64.0,   125.0,  250.0,  500.0,  800.0,  1000.0, 1500.0,
          2000.0, 3000.0, 4000.0, 5000.0, 7000.0, 9000.0, 11000.0};
}

std::size_t floor_power_of_two(std::size_t value) {
  std::size_t out = 1U;
  while ((out << 1U) <= value) {
    out <<= 1U;
  }
  return out;
}

void fft_radix2(std::vector<std::complex<double>>& values, bool inverse) {
  const auto count = values.size();
  for (std::size_t index = 1U, bit = 0U; index < count; ++index) {
    std::size_t mask = count >> 1U;
    for (; (bit & mask) != 0U; mask >>= 1U) {
      bit &= ~mask;
    }
    bit |= mask;
    if (index < bit) {
      std::swap(values[index], values[bit]);
    }
  }

  for (std::size_t length = 2U; length <= count; length <<= 1U) {
    const double angle = (inverse ? 2.0 : -2.0) * kPi / static_cast<double>(length);
    const std::complex<double> root(std::cos(angle), std::sin(angle));
    for (std::size_t start = 0U; start < count; start += length) {
      std::complex<double> step(1.0, 0.0);
      for (std::size_t offset = 0U; offset < length / 2U; ++offset) {
        const auto even = values[start + offset];
        const auto odd = values[start + offset + length / 2U] * step;
        values[start + offset] = even + odd;
        values[start + offset + length / 2U] = even - odd;
        step *= root;
      }
    }
  }

  if (inverse) {
    const double scale = 1.0 / static_cast<double>(count);
    for (auto& value : values) {
      value *= scale;
    }
  }
}

std::vector<double> unwrap_phase(const std::vector<std::complex<double>>& values) {
  std::vector<double> phase;
  phase.reserve(values.size());
  double previous = 0.0;
  double correction = 0.0;
  for (std::size_t index = 0; index < values.size(); ++index) {
    const double raw = std::arg(values[index]);
    if (index > 0U) {
      const double delta = raw - previous;
      if (delta > kPi) {
        correction -= 2.0 * kPi;
      } else if (delta < -kPi) {
        correction += 2.0 * kPi;
      }
    }
    phase.push_back(raw + correction);
    previous = raw;
  }
  return phase;
}

std::vector<double> moving_average_same(const std::vector<double>& values, std::uint32_t bins) {
  if (bins <= 1U || values.empty()) {
    return values;
  }
  std::vector<double> out(values.size(), 0.0);
  const auto half = static_cast<std::int64_t>(bins / 2U);
  for (std::size_t index = 0; index < values.size(); ++index) {
    double sum = 0.0;
    for (std::uint32_t tap = 0; tap < bins; ++tap) {
      const auto source =
          static_cast<std::int64_t>(index) + static_cast<std::int64_t>(tap) - half;
      if (source >= 0 && source < static_cast<std::int64_t>(values.size())) {
        sum += values[static_cast<std::size_t>(source)];
      }
    }
    out[index] = sum / static_cast<double>(bins);
  }
  return out;
}

std::vector<std::complex<double>> windowed_fft(std::span<const double> values,
                                               std::size_t offset,
                                               std::size_t nperseg,
                                               const std::vector<double>& window) {
  double mean = 0.0;
  for (std::size_t index = 0; index < nperseg; ++index) {
    mean += values[offset + index];
  }
  mean /= static_cast<double>(nperseg);

  std::vector<std::complex<double>> segment(nperseg);
  for (std::size_t index = 0; index < nperseg; ++index) {
    segment[index] = (values[offset + index] - mean) * window[index];
  }
  fft_radix2(segment, false);
  return segment;
}

std::vector<SpectralPoint> estimate_transfer(std::span<const double> reference,
                                             std::span<const double> capture,
                                             double scalar_gain,
                                             std::uint32_t sample_rate,
                                             std::uint32_t requested_nperseg,
                                             std::uint32_t smooth_bins) {
  const auto nperseg =
      floor_power_of_two(std::min<std::size_t>(requested_nperseg, reference.size()));
  const auto hop = std::max<std::size_t>(1U, nperseg / 2U);
  const auto bins = nperseg / 2U + 1U;
  std::vector<double> window(nperseg);
  for (std::size_t index = 0; index < nperseg; ++index) {
    window[index] =
        0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(index) /
                             static_cast<double>(nperseg));
  }

  std::vector<std::complex<double>> pxy(bins);
  std::vector<double> pxx(bins, 0.0);
  std::vector<double> pyy(bins, 0.0);
  std::vector<double> scalar_residual_power(bins, 0.0);
  std::vector<double> scalar_signal_power(bins, 0.0);
  std::uint32_t windows = 0;
  for (std::size_t offset = 0; offset + nperseg <= reference.size(); offset += hop) {
    const auto x_fft = windowed_fft(reference, offset, nperseg, window);
    const auto y_fft = windowed_fft(capture, offset, nperseg, window);
    for (std::size_t bin = 0; bin < bins; ++bin) {
      const auto x = x_fft[bin];
      const auto y = y_fft[bin];
      pxy[bin] += std::conj(y) * x;
      pxx[bin] += std::norm(x);
      pyy[bin] += std::norm(y);
      const auto scalar_pred = scalar_gain * x;
      scalar_signal_power[bin] += std::norm(scalar_pred);
      scalar_residual_power[bin] += std::norm(y - scalar_pred);
    }
    ++windows;
  }

  std::vector<SpectralPoint> points;
  if (windows == 0U) {
    return points;
  }

  std::vector<std::complex<double>> transfer(bins);
  std::vector<double> magnitude(bins);
  for (std::size_t bin = 0; bin < bins; ++bin) {
    transfer[bin] = pxy[bin] / std::max(pxx[bin], kEpsilon);
    magnitude[bin] = std::abs(transfer[bin]);
  }
  auto phase = unwrap_phase(transfer);
  if (smooth_bins > 1U) {
    magnitude = moving_average_same(magnitude, smooth_bins);
    phase = moving_average_same(phase, smooth_bins);
  }
  for (std::size_t bin = 0; bin < bins; ++bin) {
    transfer[bin] =
        magnitude[bin] * std::complex<double>(std::cos(phase[bin]), std::sin(phase[bin]));
  }

  std::vector<double> lti_signal_power(bins, 0.0);
  std::vector<double> lti_residual_power(bins, 0.0);
  for (std::size_t offset = 0; offset + nperseg <= reference.size(); offset += hop) {
    const auto x_fft = windowed_fft(reference, offset, nperseg, window);
    const auto y_fft = windowed_fft(capture, offset, nperseg, window);
    for (std::size_t bin = 0; bin < bins; ++bin) {
      const auto predicted = transfer[bin] * x_fft[bin];
      lti_signal_power[bin] += std::norm(predicted);
      lti_residual_power[bin] += std::norm(y_fft[bin] - predicted);
    }
  }

  points.reserve(bins);
  for (std::size_t bin = 0; bin < bins; ++bin) {
    const double frequency = static_cast<double>(bin) * static_cast<double>(sample_rate) /
                             static_cast<double>(nperseg);
    const auto coherence = std::norm(pxy[bin]) / std::max(pxx[bin] * pyy[bin], kEpsilon);
    points.push_back({frequency,
                      transfer[bin],
                      std::clamp(coherence, 0.0, 1.0),
                      scalar_residual_power[bin] / windows,
                      scalar_signal_power[bin] / windows,
                      lti_residual_power[bin] / windows,
                      lti_signal_power[bin] / windows});
  }
  return points;
}

bool in_band(double frequency, double low, double high) {
  return frequency >= low && frequency < high;
}

template <typename Fn>
double band_mean(const std::vector<SpectralPoint>& points, double low, double high, Fn fn) {
  double sum = 0.0;
  std::uint32_t count = 0;
  for (const auto& point : points) {
    if (in_band(point.frequency, low, high)) {
      sum += fn(point);
      ++count;
    }
  }
  return count == 0U ? 0.0 : sum / count;
}

double band_ratio(const std::vector<SpectralPoint>& points,
                  double low,
                  double high,
                  bool lti) {
  const double res = band_mean(points, low, high, [lti](const SpectralPoint& point) {
    return lti ? point.lti_residual_power : point.scalar_residual_power;
  });
  const double sig = band_mean(points, low, high, [lti](const SpectralPoint& point) {
    return lti ? point.lti_signal_power : point.scalar_signal_power;
  });
  return std::sqrt(res) / std::sqrt(std::max(sig, kEpsilon));
}

double total_ratio(const std::vector<SpectralPoint>& points, bool lti) {
  double res = 0.0;
  double sig = 0.0;
  for (const auto& point : points) {
    res += lti ? point.lti_residual_power : point.scalar_residual_power;
    sig += lti ? point.lti_signal_power : point.scalar_signal_power;
  }
  return std::sqrt(res) / std::sqrt(std::max(sig, kEpsilon));
}

ChannelMetrics analyze_channel(std::span<const double> reference,
                               std::span<const double> capture,
                               std::uint32_t sample_rate,
                               std::uint32_t nperseg,
                               std::uint32_t smooth_bins) {
  ChannelMetrics out{};
  const double ref_power = dot(reference, reference);
  out.scalar_gain = ref_power > 0.0 ? dot(reference, capture) / ref_power : 0.0;

  std::vector<double> scalar_pred(reference.size());
  std::vector<double> scalar_res(reference.size());
  for (std::size_t index = 0; index < reference.size(); ++index) {
    scalar_pred[index] = out.scalar_gain * reference[index];
    scalar_res[index] = capture[index] - scalar_pred[index];
  }
  out.scalar_residual_rms = rms(scalar_res);
  const double scalar_signal_rms = rms(scalar_pred);
  out.scalar_snr_db =
      out.scalar_residual_rms > 0.0 ? db20(scalar_signal_rms / out.scalar_residual_rms) : 999.0;

  const auto points =
      estimate_transfer(reference, capture, out.scalar_gain, sample_rate, nperseg, smooth_bins);
  out.scalar_mid_ratio = band_ratio(points, 1000.0, 5000.0, false);
  out.scalar_high_ratio = band_ratio(points, 5000.0, 12000.0, false);
  out.lti_mid_ratio = band_ratio(points, 1000.0, 5000.0, true);
  out.lti_high_ratio = band_ratio(points, 5000.0, 12000.0, true);
  const double lti_total_ratio = total_ratio(points, true);
  out.lti_residual_rms = lti_total_ratio * scalar_signal_rms;
  out.lti_snr_db = lti_total_ratio > 0.0 ? db20(1.0 / lti_total_ratio) : 999.0;
  out.coherence_low_mean =
      band_mean(points, 40.0, 1000.0, [](const SpectralPoint& p) { return p.coherence; });
  out.coherence_mid_mean =
      band_mean(points, 1000.0, 5000.0, [](const SpectralPoint& p) { return p.coherence; });
  out.coherence_high_mean =
      band_mean(points, 5000.0, 12000.0, [](const SpectralPoint& p) { return p.coherence; });
  out.transfer_mag_low_db = db20(band_mean(points, 40.0, 1000.0, [](const SpectralPoint& p) {
    return std::abs(p.transfer);
  }));
  out.transfer_mag_mid_db = db20(band_mean(points, 1000.0, 5000.0, [](const SpectralPoint& p) {
    return std::abs(p.transfer);
  }));
  out.transfer_mag_high_db = db20(band_mean(points, 5000.0, 12000.0, [](const SpectralPoint& p) {
    return std::abs(p.transfer);
  }));
  out.lti_snr_delta_db = out.lti_snr_db - out.scalar_snr_db;
  out.lti_mid_ratio_delta = out.scalar_mid_ratio - out.lti_mid_ratio;
  out.lti_high_ratio_delta = out.scalar_high_ratio - out.lti_high_ratio;
  return out;
}

RunMetrics analyze_buffers(const std::string& run_label,
                           const std::string& reference_label,
                           const std::string& capture_label,
                           const StereoBuffer& reference,
                           const StereoBuffer& capture,
                           const Cli& cli) {
  if (reference.sample_rate != capture.sample_rate) {
    throw std::runtime_error("rate_mismatch");
  }
  const auto alignment = align_pair(reference, capture, cli.max_lag);
  const auto usable = std::min<std::size_t>(
      {reference.frames.size() - alignment.reference_start,
       capture.frames.size() - alignment.capture_start,
       static_cast<std::size_t>(cli.max_seconds * reference.sample_rate)});
  if (usable <= reference.sample_rate) {
    throw std::runtime_error("not_enough_aligned_audio");
  }
  const auto ref_left = channel_vector(reference, alignment.reference_start, usable, 0U);
  const auto ref_right = channel_vector(reference, alignment.reference_start, usable, 1U);
  const auto cap_left = channel_vector(capture, alignment.capture_start, usable, 0U);
  const auto cap_right = channel_vector(capture, alignment.capture_start, usable, 1U);

  RunMetrics out{};
  out.run_dir = run_label;
  out.reference = reference_label;
  out.capture = capture_label;
  out.rate = reference.sample_rate;
  out.alignment_lag = alignment.lag;
  out.compared_frames = usable;
  out.compared_seconds = static_cast<double>(usable) / reference.sample_rate;
  out.left =
      analyze_channel(ref_left, cap_left, reference.sample_rate, cli.nperseg, cli.smooth_bins);
  out.right =
      analyze_channel(ref_right, cap_right, reference.sample_rate, cli.nperseg, cli.smooth_bins);
  out.min_lti_snr_delta_db = std::min(out.left.lti_snr_delta_db, out.right.lti_snr_delta_db);
  out.max_lti_mid_ratio_delta =
      std::max(out.left.lti_mid_ratio_delta, out.right.lti_mid_ratio_delta);
  out.min_mid_coherence = std::min(out.left.coherence_mid_mean, out.right.coherence_mid_mean);
  return out;
}

RunMetrics analyze_run_dir(const std::filesystem::path& run_dir, const Cli& cli) {
  const auto reference_path = find_reference(run_dir);
  const auto capture_path = run_dir / "captured.wav";
  const auto reference = read_wav_pair(reference_path);
  const auto capture = read_wav_pair(capture_path);
  return analyze_buffers(run_dir.string(), reference_path.string(), capture_path.string(), reference,
                         capture, cli);
}

StereoBuffer make_self_test_reference(std::uint32_t sample_rate, double seconds) {
  StereoBuffer out{};
  out.sample_rate = sample_rate;
  const auto frames = static_cast<std::size_t>(sample_rate * seconds);
  out.frames.reserve(frames);
  std::uint32_t left_state = 0x12345678U;
  std::uint32_t right_state = 0x87654321U;
  auto next = [](std::uint32_t& state) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return (static_cast<double>(state & 0x00ffffffU) / static_cast<double>(0x00800000U)) - 1.0;
  };
  double left_lp = 0.0;
  double right_lp = 0.0;
  for (std::size_t index = 0; index < frames; ++index) {
    left_lp = 0.985 * left_lp + 0.015 * next(left_state);
    right_lp = 0.982 * right_lp + 0.018 * next(right_state);
    const double left = 0.65 * next(left_state) + 0.35 * left_lp;
    const double right = 0.65 * next(right_state) + 0.35 * right_lp;
    out.frames.push_back({0.045 * left, 0.045 * right});
  }
  return out;
}

StereoBuffer make_self_test_capture(const StereoBuffer& reference, std::uint32_t delay) {
  StereoBuffer out{};
  out.sample_rate = reference.sample_rate;
  out.frames.assign(delay, {0.0, 0.0});
  for (std::size_t index = 0; index < reference.frames.size(); ++index) {
    const double slow = static_cast<double>(index % 97U) / 97.0 - 0.5;
    out.frames.push_back({0.92 * reference.frames[index][0] + 0.0002 * slow,
                          0.89 * reference.frames[index][1] - 0.00015 * slow});
  }
  return out;
}

std::string escape_json(std::string_view value) {
  std::string out;
  for (const char c : value) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

void print_number(std::ostream& out, const char* key, double value, const char* indent) {
  out << indent << "\"" << key << "\": ";
  if (std::isfinite(value)) {
    out << value;
  } else {
    out << "null";
  }
}

void print_channel(std::ostream& out, const ChannelMetrics& metrics, const char* indent) {
  out << indent << "{\n";
  print_number(out, "coherence_high_mean", metrics.coherence_high_mean, "        ");
  out << ",\n";
  print_number(out, "coherence_low_mean", metrics.coherence_low_mean, "        ");
  out << ",\n";
  print_number(out, "coherence_mid_mean", metrics.coherence_mid_mean, "        ");
  out << ",\n";
  print_number(out, "lti_high_ratio", metrics.lti_high_ratio, "        ");
  out << ",\n";
  print_number(out, "lti_high_ratio_delta", metrics.lti_high_ratio_delta, "        ");
  out << ",\n";
  print_number(out, "lti_mid_ratio", metrics.lti_mid_ratio, "        ");
  out << ",\n";
  print_number(out, "lti_mid_ratio_delta", metrics.lti_mid_ratio_delta, "        ");
  out << ",\n";
  print_number(out, "lti_residual_rms", metrics.lti_residual_rms, "        ");
  out << ",\n";
  print_number(out, "lti_snr_db", metrics.lti_snr_db, "        ");
  out << ",\n";
  print_number(out, "lti_snr_delta_db", metrics.lti_snr_delta_db, "        ");
  out << ",\n";
  print_number(out, "scalar_gain", metrics.scalar_gain, "        ");
  out << ",\n";
  print_number(out, "scalar_high_ratio", metrics.scalar_high_ratio, "        ");
  out << ",\n";
  print_number(out, "scalar_mid_ratio", metrics.scalar_mid_ratio, "        ");
  out << ",\n";
  print_number(out, "scalar_residual_rms", metrics.scalar_residual_rms, "        ");
  out << ",\n";
  print_number(out, "scalar_snr_db", metrics.scalar_snr_db, "        ");
  out << ",\n";
  print_number(out, "transfer_mag_high_db", metrics.transfer_mag_high_db, "        ");
  out << ",\n";
  print_number(out, "transfer_mag_low_db", metrics.transfer_mag_low_db, "        ");
  out << ",\n";
  print_number(out, "transfer_mag_mid_db", metrics.transfer_mag_mid_db, "        ");
  out << "\n" << indent << "}";
}

void print_run(std::ostream& out, const RunMetrics& run, bool trailing_comma) {
  out << "    {\n"
      << "      \"alignment_lag\": " << run.alignment_lag << ",\n"
      << "      \"capture\": \"" << escape_json(run.capture) << "\",\n"
      << "      \"compared_frames\": " << run.compared_frames << ",\n";
  print_number(out, "compared_seconds", run.compared_seconds, "      ");
  out << ",\n"
      << "      \"left\": ";
  print_channel(out, run.left, "      ");
  out << ",\n";
  print_number(out, "max_lti_mid_ratio_delta", run.max_lti_mid_ratio_delta, "      ");
  out << ",\n";
  print_number(out, "min_lti_snr_delta_db", run.min_lti_snr_delta_db, "      ");
  out << ",\n";
  print_number(out, "min_mid_coherence", run.min_mid_coherence, "      ");
  out << ",\n"
      << "      \"rate\": " << run.rate << ",\n"
      << "      \"reference\": \"" << escape_json(run.reference) << "\",\n"
      << "      \"right\": ";
  print_channel(out, run.right, "      ");
  out << ",\n"
      << "      \"run_dir\": \"" << escape_json(run.run_dir) << "\"\n"
      << "    }";
  if (trailing_comma) {
    out << ",";
  }
  out << "\n";
}

void write_report(const Cli& cli, const std::vector<RunMetrics>& runs, bool self_test) {
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"opena8djcpp.lti-transfer-quality-cpp.v1\",\n"
      << "  \"safety\": \"offline_wav_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"PASS_DIAGNOSTIC\",\n"
      << "  \"self_test\": " << (self_test ? "true" : "false") << ",\n"
      << "  \"product_claim_allowed\": false,\n"
      << "  \"nperseg\": " << cli.nperseg << ",\n"
      << "  \"smooth_bins\": " << cli.smooth_bins << ",\n"
      << "  \"rows\": [\n";
  for (std::size_t index = 0; index < runs.size(); ++index) {
    print_run(out, runs[index], index + 1U < runs.size());
  }
  out << "  ],\n"
      << "  \"blocked_claim\": \"CPP_LTI_TRANSFER_ANALYSIS_IS_DIAGNOSTIC_UNTIL_PARITY_WITH_PYTHON_AND_SAME_SESSION_PHYSICAL_EVIDENCE_PASS\"\n"
      << "}\n";

  const auto text = out.str();
  std::cout << text;
  if (!cli.json_out.empty()) {
    std::filesystem::create_directories(cli.json_out.parent_path());
    std::ofstream output(cli.json_out);
    output << text;
  }
}

Cli parse_cli(int argc, char** argv) {
  Cli cli{};
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg = argv[index];
    auto next = [&]() -> std::string {
      if (index + 1 >= argc) {
        throw std::runtime_error("missing_value");
      }
      return argv[++index];
    };
    if (arg == "--json-out") {
      cli.json_out = next();
    } else if (arg == "--max-seconds") {
      cli.max_seconds = std::stod(next());
    } else if (arg == "--max-lag") {
      cli.max_lag = std::stoi(next());
    } else if (arg == "--nperseg") {
      cli.nperseg = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--smooth-bins") {
      cli.smooth_bins = static_cast<std::uint32_t>(std::stoul(next()));
    } else if (arg == "--self-test") {
      cli.self_test = true;
    } else {
      cli.soundcheck_dirs.emplace_back(std::string(arg));
    }
  }
  return cli;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    auto cli = parse_cli(argc, argv);
    std::vector<RunMetrics> runs;
    if (cli.self_test) {
      cli.max_seconds = 2.5;
      cli.nperseg = std::min<std::uint32_t>(cli.nperseg, 4096U);
      const auto reference = make_self_test_reference(48000U, 3.0);
      const auto capture = make_self_test_capture(reference, 137U);
      runs.push_back(analyze_buffers("selftest_lti", "generated-reference", "generated-capture",
                                     reference, capture, cli));
      if (runs.front().min_mid_coherence < 0.95) {
        throw std::runtime_error("selftest_mid_coherence_too_low");
      }
    } else {
      if (cli.soundcheck_dirs.empty()) {
        throw std::runtime_error(
            "usage: opena8djcpp_lti_transfer_quality [--json-out PATH] [--max-seconds N] "
            "[--max-lag N] [--nperseg N] SOUNDHECK_DIR...");
      }
      for (const auto& dir : cli.soundcheck_dirs) {
        runs.push_back(analyze_run_dir(dir, cli));
      }
    }
    write_report(cli, runs, cli.self_test);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "opena8djcpp_lti_transfer_quality: " << error.what() << "\n";
    return 1;
  }
}
