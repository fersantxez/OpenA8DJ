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
#include <map>
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
  std::int32_t max_lag = 256;
  double offset_min = -5.0;
  double offset_max = 5.0;
  double offset_step = 0.25;
  double strong_corr = 0.70;
  std::uint32_t max_reported_correlations = 12;
  bool self_test = false;
  std::vector<std::filesystem::path> soundcheck_dirs;
};

using NumericRow = std::map<std::string, double>;

struct AlignedRun {
  std::filesystem::path run_dir;
  std::uint32_t rate = 0;
  std::size_t capture_start = 0;
  std::vector<std::array<double, 2>> reference;
  std::vector<std::array<double, 2>> capture;
  std::vector<NumericRow> cpu;
  std::vector<NumericRow> stream;
  double source_quality_alignment_score = 0.0;
  double source_lag_jumps_gt_2_frames = 0.0;
  double source_mid_band_residual_ratio = 0.0;
  double source_high_band_residual_ratio = 0.0;
  double source_quiet_mid_band_noise_dbfs = 0.0;
  double source_capture_clipped_frames = 0.0;
};

struct AudioWindow {
  double audio_seconds = 0.0;
  double relative_seconds = 0.0;
  double lag_frames = 0.0;
  double abs_lag_jump_frames = 0.0;
  double lag_score = 0.0;
  double scalar_gain = 0.0;
  double predicted_rms = 0.0;
  double residual_rms = 0.0;
  double scalar_snr_db = 0.0;
};

struct Correlation {
  std::string metric;
  std::string column;
  bool delta = false;
  double offset_seconds = 0.0;
  double correlation = 0.0;
  double abs_correlation = 0.0;
  std::string source;
};

double db20(double value) {
  if (value <= 0.0 || !std::isfinite(value)) {
    return -240.0;
  }
  return 20.0 * std::log10(std::max(value, kEpsilon));
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

std::vector<std::string> split_tab(const std::string& line) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= line.size()) {
    const auto end = line.find('\t', start);
    out.push_back(line.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) {
      break;
    }
    start = end + 1U;
  }
  return out;
}

std::vector<NumericRow> read_numeric_tsv(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  std::string header_line;
  if (!std::getline(input, header_line)) {
    return {};
  }
  const auto headers = split_tab(header_line);
  std::vector<NumericRow> rows;
  std::string line;
  while (std::getline(input, line)) {
    const auto cells = split_tab(line);
    NumericRow parsed;
    for (std::size_t index = 0; index < std::min(headers.size(), cells.size()); ++index) {
      if (cells[index].empty()) {
        continue;
      }
      try {
        const double value = std::stod(cells[index]);
        if (std::isfinite(value)) {
          parsed[headers[index]] = value;
        }
      } catch (const std::exception&) {
      }
    }
    if (!parsed.empty()) {
      rows.push_back(std::move(parsed));
    }
  }
  return rows;
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
  const auto reference = read_wav_pair(run_dir / "fixture/reference.wav");
  const auto capture = read_wav_pair(run_dir / "captured.wav");
  if (reference.sample_rate != capture.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch:" + run_dir.string());
  }
  const auto ref_start = static_cast<std::size_t>(std::max(
      0.0, json_number_or(metrics_json, "reference_start", 0.0)));
  const auto cap_start = static_cast<std::size_t>(std::max(
      0.0, json_number_or(metrics_json, "capture_start", 0.0)));
  std::size_t frames = static_cast<std::size_t>(std::max(
      0.0, json_number_or(metrics_json, "compared_frames", 0.0)));
  if (cli.analysis_seconds > 0.0) {
    frames = std::min(frames, static_cast<std::size_t>(
                                  std::llround(cli.analysis_seconds * reference.sample_rate)));
  }
  frames = std::min({frames, reference.frames.size() - std::min(ref_start, reference.frames.size()),
                     capture.frames.size() - std::min(cap_start, capture.frames.size())});
  if (frames == 0U) {
    throw std::runtime_error("no_aligned_frames:" + run_dir.string());
  }
  AlignedRun out{};
  out.run_dir = run_dir;
  out.rate = reference.sample_rate;
  out.capture_start = cap_start;
  out.reference = slice(reference.frames, ref_start, frames);
  out.capture = slice(capture.frames, cap_start, frames);
  out.cpu = read_numeric_tsv(run_dir / "cpu-profile.tsv");
  out.stream = read_numeric_tsv(run_dir / "stream-stats-during.tsv");
  out.source_quality_alignment_score =
      json_number_or(metrics_json, "quality_alignment_score", 0.0);
  out.source_lag_jumps_gt_2_frames =
      json_number_or(metrics_json, "lag_jumps_gt_2_frames", 0.0);
  out.source_mid_band_residual_ratio =
      json_number_or(metrics_json, "mid_band_residual_ratio", 0.0);
  out.source_high_band_residual_ratio =
      json_number_or(metrics_json, "high_band_residual_ratio", 0.0);
  out.source_quiet_mid_band_noise_dbfs =
      json_number_or(metrics_json, "quiet_mid_band_noise_dbfs", 0.0);
  out.source_capture_clipped_frames =
      json_number_or(metrics_json, "capture_clipped_frames", 0.0);
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

std::pair<double, double> best_lag(std::span<const double> ref,
                                   std::span<const double> cap,
                                   std::int32_t max_lag) {
  auto ref_zero = zero_mean(ref);
  auto cap_zero = zero_mean(cap);
  if (ref_zero.empty() || cap_zero.empty()) {
    return {0.0, 0.0};
  }
  const auto pad = static_cast<std::size_t>(max_lag);
  std::vector<double> search(cap_zero.size() + pad * 2U, 0.0);
  std::copy(cap_zero.begin(), cap_zero.end(), search.begin() + static_cast<std::ptrdiff_t>(pad));
  std::vector<double> corr(search.size() - ref_zero.size() + 1U, 0.0);
  for (std::size_t offset = 0; offset < corr.size(); ++offset) {
    long double sum = 0.0;
    for (std::size_t index = 0; index < ref_zero.size(); ++index) {
      sum += static_cast<long double>(search[offset + index]) * ref_zero[index];
    }
    corr[offset] = static_cast<double>(sum);
  }
  const auto peak = static_cast<std::size_t>(
      std::distance(corr.begin(), std::max_element(corr.begin(), corr.end(), [](double a, double b) {
        return std::abs(a) < std::abs(b);
      })));
  const double lag = static_cast<double>(static_cast<std::int64_t>(peak) - max_lag);
  const double denom =
      std::sqrt(std::max(dot(ref_zero, ref_zero), 0.0) *
                std::max(dot(std::span<const double>(search.data() + peak, ref_zero.size()),
                             std::span<const double>(search.data() + peak, ref_zero.size())),
                         0.0));
  const double score = denom > 0.0 ? corr[peak] / denom : 0.0;
  return {lag, score};
}

std::array<double, 4> scalar_residual(std::span<const std::array<double, 2>> ref,
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
  std::vector<double> pred;
  std::vector<double> residual;
  pred.reserve(ref.size() * 2U);
  residual.reserve(ref.size() * 2U);
  for (std::size_t index = 0; index < std::min(ref.size(), cap.size()); ++index) {
    const double pl = gain * ref[index][0];
    const double pr = gain * ref[index][1];
    pred.push_back(pl);
    pred.push_back(pr);
    residual.push_back(cap[index][0] - pl);
    residual.push_back(cap[index][1] - pr);
  }
  const double pred_rms = rms(pred);
  const double res_rms = rms(residual);
  return {gain, pred_rms, res_rms, res_rms > 0.0 ? db20(pred_rms / res_rms) : 999.0};
}

std::vector<AudioWindow> audio_windows(const AlignedRun& run, const Cli& cli) {
  const auto window = std::max<std::size_t>(256U, std::llround(cli.window_seconds * run.rate));
  const auto hop = std::max<std::size_t>(1U, std::llround(cli.hop_seconds * run.rate));
  const auto ref_mono = mono(run.reference);
  const auto cap_mono = mono(run.capture);
  std::vector<AudioWindow> rows;
  for (std::size_t start = 0; start + window <= run.reference.size(); start += hop) {
    const auto stop = start + window;
    const auto lag = best_lag(std::span<const double>(ref_mono.data() + start, stop - start),
                              std::span<const double>(cap_mono.data() + start, stop - start),
                              cli.max_lag);
    const auto ref_window = slice(run.reference, start, window);
    const auto cap_window = slice(run.capture, start, window);
    const auto scalar = scalar_residual(ref_window, cap_window);
    rows.push_back(AudioWindow{
        .audio_seconds = (static_cast<double>(run.capture_start) + static_cast<double>(start) +
                          static_cast<double>(window) / 2.0) /
                         run.rate,
        .relative_seconds = (static_cast<double>(start) + static_cast<double>(window) / 2.0) /
                            run.rate,
        .lag_frames = lag.first,
        .abs_lag_jump_frames = 0.0,
        .lag_score = std::abs(lag.second),
        .scalar_gain = scalar[0],
        .predicted_rms = scalar[1],
        .residual_rms = scalar[2],
        .scalar_snr_db = scalar[3],
    });
  }
  for (std::size_t index = 1; index < rows.size(); ++index) {
    rows[index].abs_lag_jump_frames =
        std::abs(rows[index].lag_frames - rows[index - 1U].lag_frames);
  }
  return rows;
}

std::optional<double> value_for(const NumericRow& row, const std::string& column) {
  const auto it = row.find(column);
  if (it == row.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<double> telemetry_times(const std::vector<NumericRow>& rows) {
  std::vector<double> out;
  out.reserve(rows.size());
  for (const auto& row : rows) {
    out.push_back(value_for(row, "elapsed_seconds").value_or(std::numeric_limits<double>::quiet_NaN()));
  }
  return out;
}

std::vector<double> series_from_nearest(const std::vector<NumericRow>& rows,
                                        std::span<const double> seconds,
                                        double offset,
                                        const std::string& column,
                                        bool delta) {
  std::vector<double> values(seconds.size(), std::numeric_limits<double>::quiet_NaN());
  if (rows.empty()) {
    return values;
  }
  const auto times = telemetry_times(rows);
  std::optional<double> previous;
  for (std::size_t index = 0; index < seconds.size(); ++index) {
    const double target = seconds[index] + offset;
    std::size_t best = 0;
    double best_distance = std::numeric_limits<double>::infinity();
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
      if (!std::isfinite(times[row_index])) {
        continue;
      }
      const double distance = std::abs(times[row_index] - target);
      if (distance < best_distance) {
        best_distance = distance;
        best = row_index;
      }
    }
    const auto value = value_for(rows[best], column);
    if (!value) {
      continue;
    }
    if (delta) {
      values[index] = previous ? *value - *previous : 0.0;
      previous = *value;
    } else {
      values[index] = *value;
    }
  }
  return values;
}

std::optional<double> corrcoef(std::span<const double> a, std::span<const double> b) {
  std::vector<double> aa;
  std::vector<double> bb;
  for (std::size_t index = 0; index < std::min(a.size(), b.size()); ++index) {
    if (std::isfinite(a[index]) && std::isfinite(b[index])) {
      aa.push_back(a[index]);
      bb.push_back(b[index]);
    }
  }
  if (aa.size() < 4U) {
    return std::nullopt;
  }
  const double ma = mean(aa);
  const double mb = mean(bb);
  long double num = 0.0;
  long double da = 0.0;
  long double db = 0.0;
  for (std::size_t index = 0; index < aa.size(); ++index) {
    const double xa = aa[index] - ma;
    const double xb = bb[index] - mb;
    num += static_cast<long double>(xa) * xb;
    da += static_cast<long double>(xa) * xa;
    db += static_cast<long double>(xb) * xb;
  }
  if (da <= 1.0e-24L || db <= 1.0e-24L) {
    return std::nullopt;
  }
  return static_cast<double>(num / std::sqrt(da * db));
}

double metric_value(const AudioWindow& row, const std::string& metric) {
  if (metric == "residual_rms") {
    return row.residual_rms;
  }
  if (metric == "abs_lag_jump_frames") {
    return row.abs_lag_jump_frames;
  }
  if (metric == "scalar_snr_db") {
    return row.scalar_snr_db;
  }
  return std::numeric_limits<double>::quiet_NaN();
}

std::optional<Correlation> best_offset_correlation(const std::vector<AudioWindow>& windows,
                                                   const std::vector<NumericRow>& telemetry,
                                                   const std::string& metric,
                                                   const std::string& column,
                                                   const std::vector<double>& offsets,
                                                   bool delta) {
  if (windows.empty() || telemetry.empty()) {
    return std::nullopt;
  }
  std::vector<double> seconds;
  std::vector<double> target;
  seconds.reserve(windows.size());
  target.reserve(windows.size());
  for (const auto& row : windows) {
    seconds.push_back(row.audio_seconds);
    target.push_back(metric_value(row, metric));
  }
  std::optional<Correlation> best;
  for (const double offset : offsets) {
    const auto series = series_from_nearest(telemetry, seconds, offset, column, delta);
    const auto corr = corrcoef(target, series);
    if (!corr) {
      continue;
    }
    Correlation candidate{metric, column, delta, offset, *corr, std::abs(*corr), ""};
    if (!best || candidate.abs_correlation > best->abs_correlation) {
      best = candidate;
    }
  }
  return best;
}

double percentile(std::vector<double> values, double pct) {
  values.erase(std::remove_if(values.begin(), values.end(),
                              [](double value) { return !std::isfinite(value); }),
               values.end());
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double pos = (pct / 100.0) * static_cast<double>(values.size() - 1U);
  const auto low = static_cast<std::size_t>(std::floor(pos));
  const auto high = std::min(low + 1U, values.size() - 1U);
  const double frac = pos - static_cast<double>(low);
  return values[low] * (1.0 - frac) + values[high] * frac;
}

std::vector<double> offset_values(const Cli& cli) {
  std::vector<double> out;
  for (double value = cli.offset_min; value <= cli.offset_max + 0.0001; value += cli.offset_step) {
    out.push_back(std::round(value * 1000.0) / 1000.0);
  }
  return out;
}

std::vector<std::string> interpretations(const std::vector<Correlation>& strong,
                                          const std::vector<AudioWindow>& windows,
                                          const AlignedRun& run) {
  std::vector<std::string> out;
  if (strong.empty()) {
    out.push_back("no_strong_cpu_or_stream_correlation_found");
  } else {
    if (std::any_of(strong.begin(), strong.end(),
                    [](const Correlation& row) { return row.source == "stream_delta"; })) {
      out.push_back("runtime_stream_counter_correlation_present");
    }
    if (std::any_of(strong.begin(), strong.end(),
                    [](const Correlation& row) { return row.source == "cpu"; })) {
      out.push_back("cpu_correlation_present");
    }
  }
  if (run.source_capture_clipped_frames == 0.0) {
    out.push_back("no_capture_clipping");
  }
  if (windows.size() >= 4U) {
    std::vector<double> jumps;
    for (std::size_t index = 1; index < windows.size(); ++index) {
      jumps.push_back(windows[index].abs_lag_jump_frames);
    }
    if (percentile(jumps, 95.0) > 2.0) {
      out.push_back("window_lag_jumps_present");
    }
  }
  return out;
}

struct RunSummary {
  AlignedRun run;
  std::vector<AudioWindow> windows;
  std::vector<Correlation> strong;
  std::vector<Correlation> top;
};

RunSummary summarize_run(const std::filesystem::path& run_dir, const Cli& cli) {
  RunSummary out{};
  out.run = load_run(run_dir, cli);
  out.windows = audio_windows(out.run, cli);
  const auto offsets = offset_values(cli);
  const std::vector<std::string> cpu_columns{
      "opena8dj_driver", "coreaudiod", "audio_services", "total_audio_ui", "system_load1"};
  const std::vector<std::string> stream_columns{
      "outputUnderruns",
      "outputActiveUnderruns",
      "outputElasticReplays",
      "outputElasticDrops",
      "outputTimelineResets",
      "outputLateWriteFrames",
      "playbackTransferErrors",
      "playbackCompletionDeltaOutliers",
      "captureCompletionDeltaOutliers",
      "captureToPlaybackQueueDeltaOutliers",
      "playbackTransfersCompleted",
      "captureTransfersCompleted"};
  for (const std::string& metric : {"residual_rms", "abs_lag_jump_frames", "scalar_snr_db"}) {
    for (const auto& column : cpu_columns) {
      auto item = best_offset_correlation(out.windows, out.run.cpu, metric, column, offsets, false);
      if (item) {
        item->source = "cpu";
        out.top.push_back(*item);
      }
    }
    for (const auto& column : stream_columns) {
      auto item = best_offset_correlation(out.windows, out.run.stream, metric, column, offsets, true);
      if (item) {
        item->source = "stream_delta";
        out.top.push_back(*item);
      }
    }
  }
  std::sort(out.top.begin(), out.top.end(),
            [](const Correlation& a, const Correlation& b) {
              return a.abs_correlation > b.abs_correlation;
            });
  if (out.top.size() > cli.max_reported_correlations) {
    out.top.resize(cli.max_reported_correlations);
  }
  for (const auto& row : out.top) {
    if (row.abs_correlation >= cli.strong_corr) {
      out.strong.push_back(row);
    }
  }
  if (out.strong.size() > cli.max_reported_correlations) {
    out.strong.resize(cli.max_reported_correlations);
  }
  return out;
}

void print_correlation(std::ostream& out, const Correlation& row, const std::string& indent) {
  out << indent << "{\n"
      << indent << "  \"abs_correlation\": " << row.abs_correlation << ",\n"
      << indent << "  \"column\": \"" << row.column << "\",\n"
      << indent << "  \"correlation\": " << row.correlation << ",\n"
      << indent << "  \"delta\": " << (row.delta ? "true" : "false") << ",\n"
      << indent << "  \"metric\": \"" << row.metric << "\",\n"
      << indent << "  \"offset_seconds\": " << row.offset_seconds << ",\n"
      << indent << "  \"source\": \"" << row.source << "\"\n"
      << indent << "}";
}

std::string render_json(const std::vector<RunSummary>& rows, const Cli& cli) {
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"opena8djcpp.runtime-discontinuity-analysis-cpp.v1\",\n"
      << "  \"result\": \"" << (rows.empty() ? "FAIL" : "PASS_DIAGNOSTIC") << "\",\n"
      << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"threshold_notes\": {\n"
      << "    \"strong_corr\": " << cli.strong_corr << ",\n"
      << "    \"offset_search_seconds\": [" << cli.offset_min << ", " << cli.offset_max << "],\n"
      << "    \"correlation_is_hypothesis_not_proof\": true\n"
      << "  },\n"
      << "  \"rows\": [\n";
  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    const auto& row = rows[row_index];
    std::vector<double> lag_jumps;
    std::vector<double> residual;
    std::vector<double> snr;
    for (std::size_t index = 0; index < row.windows.size(); ++index) {
      if (index > 0U) {
        lag_jumps.push_back(row.windows[index].abs_lag_jump_frames);
      }
      residual.push_back(row.windows[index].residual_rms);
      snr.push_back(row.windows[index].scalar_snr_db);
    }
    const auto interp = interpretations(row.strong, row.windows, row.run);
    out << "    {\n"
        << "      \"run_dir\": \"" << row.run.run_dir.string() << "\",\n"
        << "      \"rate\": " << row.run.rate << ",\n"
        << "      \"window_seconds\": " << cli.window_seconds << ",\n"
        << "      \"hop_seconds\": " << cli.hop_seconds << ",\n"
        << "      \"windows\": " << row.windows.size() << ",\n"
        << "      \"source_metrics\": {\n"
        << "        \"quality_alignment_score\": " << row.run.source_quality_alignment_score << ",\n"
        << "        \"lag_jumps_gt_2_frames\": " << row.run.source_lag_jumps_gt_2_frames << ",\n"
        << "        \"mid_band_residual_ratio\": " << row.run.source_mid_band_residual_ratio << ",\n"
        << "        \"high_band_residual_ratio\": " << row.run.source_high_band_residual_ratio << ",\n"
        << "        \"quiet_mid_band_noise_dbfs\": " << row.run.source_quiet_mid_band_noise_dbfs
        << ",\n"
        << "        \"capture_clipped_frames\": " << row.run.source_capture_clipped_frames << "\n"
        << "      },\n"
        << "      \"audio_window_summary\": {\n"
        << "        \"lag_jump_max_frames\": " << percentile(lag_jumps, 100.0) << ",\n"
        << "        \"lag_jump_p95_frames\": " << percentile(lag_jumps, 95.0) << ",\n"
        << "        \"residual_rms_median\": " << percentile(residual, 50.0) << ",\n"
        << "        \"residual_rms_p95\": " << percentile(residual, 95.0) << ",\n"
        << "        \"scalar_snr_db_median\": " << percentile(snr, 50.0) << ",\n"
        << "        \"scalar_snr_db_min\": " << percentile(snr, 0.0) << "\n"
        << "      },\n"
        << "      \"telemetry_rows\": {\n"
        << "        \"cpu\": " << row.run.cpu.size() << ",\n"
        << "        \"stream\": " << row.run.stream.size() << "\n"
        << "      },\n"
        << "      \"strong_correlations\": [";
    for (std::size_t index = 0; index < row.strong.size(); ++index) {
      out << (index == 0U ? "\n" : ",\n");
      print_correlation(out, row.strong[index], "        ");
    }
    out << (row.strong.empty() ? "" : "\n      ") << "],\n"
        << "      \"top_correlations\": [";
    for (std::size_t index = 0; index < row.top.size(); ++index) {
      out << (index == 0U ? "\n" : ",\n");
      print_correlation(out, row.top[index], "        ");
    }
    out << (row.top.empty() ? "" : "\n      ") << "],\n"
        << "      \"interpretation\": [";
    for (std::size_t index = 0; index < interp.size(); ++index) {
      out << (index == 0U ? "" : ", ") << "\"" << interp[index] << "\"";
    }
    out << "]\n"
        << "    }" << (row_index + 1U == rows.size() ? "\n" : ",\n");
  }
  out << "  ]\n"
      << "}\n";
  return out.str();
}

int self_test(const Cli& cli) {
  std::vector<double> a{1.0, 2.0, 3.0, 4.0, 5.0};
  std::vector<double> b{2.0, 4.0, 6.0, 8.0, 10.0};
  const auto corr = corrcoef(a, b).value_or(0.0);
  const bool pass = std::abs(corr - 1.0) <= 1.0e-12;
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"opena8djcpp.runtime-discontinuity-analysis-cpp-self-test.v1\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"correlation\": " << corr << ",\n"
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
    } else if (arg == "--offset-min") {
      cli.offset_min = std::stod(require_value(arg));
    } else if (arg == "--offset-max") {
      cli.offset_max = std::stod(require_value(arg));
    } else if (arg == "--offset-step") {
      cli.offset_step = std::stod(require_value(arg));
    } else if (arg == "--strong-corr") {
      cli.strong_corr = std::stod(require_value(arg));
    } else if (arg == "--max-reported-correlations") {
      cli.max_reported_correlations = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
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
    std::vector<RunSummary> rows;
    rows.reserve(cli.soundcheck_dirs.size());
    for (const auto& dir : cli.soundcheck_dirs) {
      rows.push_back(summarize_run(dir, cli));
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
    std::cerr << "runtime_discontinuity_analysis_error: " << error.what() << "\n";
    return 1;
  }
}
