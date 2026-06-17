#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kEpsilon = 1.0e-20;
constexpr double kQualityGate = 0.98;
constexpr double kSnrGate = 35.0;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
};

struct RunMeta {
  std::filesystem::path dir;
  double quality = std::numeric_limits<double>::quiet_NaN();
  double left_snr = std::numeric_limits<double>::quiet_NaN();
  double right_snr = std::numeric_limits<double>::quiet_NaN();
  std::int32_t recorded_lag = 0;
  bool has_lag = false;
};

struct RunForensics {
  RunMeta meta;
  std::size_t compared_frames = 0;
  std::int32_t base_lag = 0;
  double fixed_lag_score = 0.0;
  std::uint32_t window_count = 0;
  std::int32_t lag_min = 0;
  std::int32_t lag_max = 0;
  std::uint32_t lag_jumps_gt_2 = 0;
  double lag_stddev = 0.0;
  double scalar_snr_db = 0.0;
  double matrix_snr_db = 0.0;
  double matrix_explain_db = 0.0;
  double left_to_right = 0.0;
  double right_to_left = 0.0;
  double residual_rms = 0.0;
  double residual_diff_rms = 0.0;
  double residual_diff_ratio = 0.0;
  std::string classification;
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

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot_open:" + path.string());
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::uint16_t u16le(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}

std::uint32_t u32le(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
         (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
         (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

double pcm_sample(const std::vector<std::uint8_t>& data,
                  std::size_t offset,
                  std::uint16_t format,
                  std::uint16_t bits) {
  if (format == 3U && bits == 32U) {
    float value = 0.0F;
    std::memcpy(&value, data.data() + offset, sizeof(float));
    return std::isfinite(value) ? std::clamp(static_cast<double>(value), -1.0, 1.0) : 0.0;
  }
  if (format != 1U) {
    throw std::runtime_error("unsupported_wav_format");
  }
  if (bits == 16U) {
    const auto raw = static_cast<std::int16_t>(u16le(data, offset));
    return static_cast<double>(raw) / 32768.0;
  }
  if (bits == 24U) {
    std::uint32_t raw = static_cast<std::uint32_t>(data[offset]) |
                        (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
                        (static_cast<std::uint32_t>(data[offset + 2U]) << 16U);
    if ((raw & 0x00800000U) != 0U) {
      raw |= 0xff000000U;
    }
    return static_cast<double>(static_cast<std::int32_t>(raw)) / 8388608.0;
  }
  if (bits == 32U) {
    return static_cast<double>(static_cast<std::int32_t>(u32le(data, offset))) / 2147483648.0;
  }
  throw std::runtime_error("unsupported_wav_bits");
}

StereoBuffer read_wav_pair(const std::filesystem::path& path) {
  const auto bytes = read_bytes(path);
  if (bytes.size() < 44U || std::memcmp(bytes.data(), "RIFF", 4U) != 0 ||
      std::memcmp(bytes.data() + 8U, "WAVE", 4U) != 0) {
    throw std::runtime_error("invalid_wav:" + path.string());
  }
  std::uint16_t format = 0;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint16_t block_align = 0;
  std::uint16_t bits = 0;
  std::size_t data_offset = 0;
  std::size_t data_size = 0;
  for (std::size_t offset = 12U; offset + 8U <= bytes.size();) {
    const std::string id(reinterpret_cast<const char*>(bytes.data() + offset), 4U);
    const std::size_t size = u32le(bytes, offset + 4U);
    const std::size_t payload = offset + 8U;
    if (payload + size > bytes.size()) {
      throw std::runtime_error("truncated_wav:" + path.string());
    }
    if (id == "fmt ") {
      format = u16le(bytes, payload);
      channels = u16le(bytes, payload + 2U);
      sample_rate = u32le(bytes, payload + 4U);
      block_align = u16le(bytes, payload + 12U);
      bits = u16le(bytes, payload + 14U);
    } else if (id == "data") {
      data_offset = payload;
      data_size = size;
    }
    offset = payload + size + (size % 2U);
  }
  if (channels == 0U || sample_rate == 0U || block_align == 0U || data_size == 0U ||
      bits % 8U != 0U) {
    throw std::runtime_error("missing_wav_audio:" + path.string());
  }
  StereoBuffer buffer{};
  buffer.sample_rate = sample_rate;
  const auto frames = data_size / static_cast<std::size_t>(block_align);
  const auto bytes_per_sample = static_cast<std::size_t>(bits / 8U);
  buffer.frames.reserve(frames);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const auto offset = data_offset + frame * static_cast<std::size_t>(block_align);
    const double left = pcm_sample(bytes, offset, format, bits);
    const double right = channels >= 2U ? pcm_sample(bytes, offset + bytes_per_sample, format, bits)
                                        : left;
    buffer.frames.push_back({left, right});
  }
  return buffer;
}

double mono_at(const std::vector<std::array<double, 2>>& frames, std::size_t index) {
  return 0.5 * (frames[index][0] + frames[index][1]);
}

double score_lag(const StereoBuffer& reference,
                 const StereoBuffer& capture,
                 std::size_t reference_start,
                 std::size_t capture_start,
                 std::int32_t lag,
                 std::size_t count,
                 std::size_t stride) {
  double dot = 0.0;
  double re = 0.0;
  double ce = 0.0;
  std::size_t used = 0;
  for (std::size_t i = 0; i < count; i += std::max<std::size_t>(1U, stride)) {
    const auto ri = static_cast<std::int64_t>(reference_start + i);
    const auto ci = static_cast<std::int64_t>(capture_start + i) + lag;
    if (ri < 0 || ci < 0 || ri >= static_cast<std::int64_t>(reference.frames.size()) ||
        ci >= static_cast<std::int64_t>(capture.frames.size())) {
      continue;
    }
    const double r = mono_at(reference.frames, static_cast<std::size_t>(ri));
    const double c = mono_at(capture.frames, static_cast<std::size_t>(ci));
    dot += r * c;
    re += r * r;
    ce += c * c;
    used += 1U;
  }
  return used > 0U && re > 0.0 && ce > 0.0 ? dot / std::sqrt(re * ce) : -1.0;
}

std::pair<std::int32_t, double> refine_lag(const StereoBuffer& reference,
                                           const StereoBuffer& capture,
                                           std::size_t reference_start,
                                           std::size_t capture_start,
                                           std::int32_t center,
                                           std::int32_t radius,
                                           std::size_t count,
                                           std::size_t stride) {
  std::int32_t best_lag = center;
  double best = -1.0;
  for (std::int32_t lag = center - radius; lag <= center + radius; ++lag) {
    const double score =
        score_lag(reference, capture, reference_start, capture_start, lag, count, stride);
    if (score > best) {
      best = score;
      best_lag = lag;
    }
  }
  return {best_lag, best};
}

double db(double value) {
  return value > 0.0 ? 20.0 * std::log10(value) : -240.0;
}

double snr_db(double signal, double residual) {
  return signal > 0.0 && residual > 0.0 ? 20.0 * std::log10(signal / residual) : 999.0;
}

double rms_from_sum(double sum, std::size_t count) {
  return count > 0U ? std::sqrt(sum / static_cast<double>(count)) : 0.0;
}

std::string json_escape(const std::string& input) {
  std::string out;
  for (const char c : input) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

void print_number(double value) {
  if (std::isfinite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

RunMeta read_meta(const std::filesystem::path& metrics_path) {
  RunMeta meta{};
  meta.dir = metrics_path.parent_path();
  const auto json = read_file(metrics_path);
  meta.quality = json_number(json, "quality_alignment_score").value_or(meta.quality);
  meta.left_snr = json_number(json, "left_snr_db").value_or(meta.left_snr);
  meta.right_snr = json_number(json, "right_snr_db").value_or(meta.right_snr);
  if (const auto lag = json_number(json, "alignment_lag")) {
    meta.recorded_lag = static_cast<std::int32_t>(std::llround(*lag));
    meta.has_lag = true;
  }
  return meta;
}

std::filesystem::path capture_path_for(const std::filesystem::path& dir) {
  if (std::filesystem::is_regular_file(dir / "captured.wav")) {
    return dir / "captured.wav";
  }
  if (std::filesystem::is_regular_file(dir / "soundcheck/captured.wav")) {
    return dir / "soundcheck/captured.wav";
  }
  return {};
}

std::filesystem::path reference_path_for(const std::filesystem::path& dir) {
  if (std::filesystem::is_regular_file(dir / "fixture/reference.wav")) {
    return dir / "fixture/reference.wav";
  }
  if (std::filesystem::is_regular_file(dir / "soundcheck/fixture/reference.wav")) {
    return dir / "soundcheck/fixture/reference.wav";
  }
  return {};
}

RunForensics analyze_run(const RunMeta& meta) {
  const auto reference = read_wav_pair(reference_path_for(meta.dir));
  const auto capture = read_wav_pair(capture_path_for(meta.dir));
  if (reference.sample_rate != capture.sample_rate) {
    throw std::runtime_error("sample_rate_mismatch:" + meta.dir.string());
  }
  const auto rate = reference.sample_rate;
  const std::size_t initial_skip = std::min<std::size_t>(rate / 2U, reference.frames.size() / 10U);
  const std::size_t compared =
      std::min({reference.frames.size(), capture.frames.size(), static_cast<std::size_t>(rate * 10U)});
  const std::size_t count = compared > initial_skip ? compared - initial_skip : compared;
  const auto [base_lag, base_score] =
      refine_lag(reference,
                 capture,
                 initial_skip,
                 initial_skip,
                 meta.has_lag ? meta.recorded_lag : 0,
                 meta.has_lag ? 512 : 4096,
                 std::min<std::size_t>(count, rate * 4U),
                 16U);

  double rr = 0.0;
  double ll = 0.0;
  double lr = 0.0;
  double cl = 0.0;
  double cr = 0.0;
  double signal_sum = 0.0;
  double scalar_residual_sum = 0.0;
  std::size_t used = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const auto ri = initial_skip + i;
    const auto ci_signed = static_cast<std::int64_t>(initial_skip + i) + base_lag;
    if (ci_signed < 0 || ri >= reference.frames.size() ||
        ci_signed >= static_cast<std::int64_t>(capture.frames.size())) {
      continue;
    }
    const auto ci = static_cast<std::size_t>(ci_signed);
    const double r0 = reference.frames[ri][0];
    const double r1 = reference.frames[ri][1];
    const double c0 = capture.frames[ci][0];
    const double c1 = capture.frames[ci][1];
    rr += r0 * r0;
    ll += r1 * r1;
    lr += r0 * r1;
    cl += c0 * r0 + c1 * r0;
    cr += c0 * r1 + c1 * r1;
    signal_sum += r0 * r0 + r1 * r1;
    scalar_residual_sum += (c0 - r0) * (c0 - r0) + (c1 - r1) * (c1 - r1);
    used += 2U;
  }

  const double det = rr * ll - lr * lr;
  double m00 = 1.0;
  double m01 = 0.0;
  double m10 = 0.0;
  double m11 = 1.0;
  if (std::abs(det) > kEpsilon) {
    double c0r0 = 0.0;
    double c0r1 = 0.0;
    double c1r0 = 0.0;
    double c1r1 = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
      const auto ri = initial_skip + i;
      const auto ci_signed = static_cast<std::int64_t>(initial_skip + i) + base_lag;
      if (ci_signed < 0 || ri >= reference.frames.size() ||
          ci_signed >= static_cast<std::int64_t>(capture.frames.size())) {
        continue;
      }
      const auto ci = static_cast<std::size_t>(ci_signed);
      const double r0 = reference.frames[ri][0];
      const double r1 = reference.frames[ri][1];
      c0r0 += capture.frames[ci][0] * r0;
      c0r1 += capture.frames[ci][0] * r1;
      c1r0 += capture.frames[ci][1] * r0;
      c1r1 += capture.frames[ci][1] * r1;
    }
    m00 = (c0r0 * ll - c0r1 * lr) / det;
    m01 = (c0r1 * rr - c0r0 * lr) / det;
    m10 = (c1r0 * ll - c1r1 * lr) / det;
    m11 = (c1r1 * rr - c1r0 * lr) / det;
  }

  double matrix_residual_sum = 0.0;
  double residual_diff_sum = 0.0;
  double residual_sum = 0.0;
  double prev0 = 0.0;
  double prev1 = 0.0;
  bool have_prev = false;
  for (std::size_t i = 0; i < count; ++i) {
    const auto ri = initial_skip + i;
    const auto ci_signed = static_cast<std::int64_t>(initial_skip + i) + base_lag;
    if (ci_signed < 0 || ri >= reference.frames.size() ||
        ci_signed >= static_cast<std::int64_t>(capture.frames.size())) {
      continue;
    }
    const auto ci = static_cast<std::size_t>(ci_signed);
    const double r0 = reference.frames[ri][0];
    const double r1 = reference.frames[ri][1];
    const double e0 = capture.frames[ci][0] - (m00 * r0 + m01 * r1);
    const double e1 = capture.frames[ci][1] - (m10 * r0 + m11 * r1);
    matrix_residual_sum += e0 * e0 + e1 * e1;
    residual_sum += e0 * e0 + e1 * e1;
    if (have_prev) {
      const double d0 = e0 - prev0;
      const double d1 = e1 - prev1;
      residual_diff_sum += d0 * d0 + d1 * d1;
    }
    prev0 = e0;
    prev1 = e1;
    have_prev = true;
  }

  std::vector<std::int32_t> lags;
  const std::size_t window = std::max<std::size_t>(2048U, rate / 4U);
  const std::size_t hop = window;
  for (std::size_t start = initial_skip; start + window < initial_skip + count; start += hop) {
    const auto [lag, score] =
        refine_lag(reference, capture, start, start, base_lag, 64, window, 16U);
    if (score > 0.20) {
      lags.push_back(lag);
    }
  }

  RunForensics out{};
  out.meta = meta;
  out.compared_frames = count;
  out.base_lag = base_lag;
  out.fixed_lag_score = base_score;
  out.window_count = static_cast<std::uint32_t>(lags.size());
  if (!lags.empty()) {
    out.lag_min = *std::min_element(lags.begin(), lags.end());
    out.lag_max = *std::max_element(lags.begin(), lags.end());
    double mean = 0.0;
    for (const auto lag : lags) {
      mean += static_cast<double>(lag);
    }
    mean /= static_cast<double>(lags.size());
    double var = 0.0;
    for (std::size_t i = 0; i < lags.size(); ++i) {
      const double delta = static_cast<double>(lags[i]) - mean;
      var += delta * delta;
      if (i > 0U && std::abs(lags[i] - lags[i - 1U]) > 2) {
        out.lag_jumps_gt_2 += 1U;
      }
    }
    out.lag_stddev = std::sqrt(var / static_cast<double>(lags.size()));
  }
  const double signal_rms = rms_from_sum(signal_sum, used);
  const double scalar_residual = rms_from_sum(scalar_residual_sum, used);
  const double matrix_residual = rms_from_sum(matrix_residual_sum, used);
  out.scalar_snr_db = snr_db(signal_rms, scalar_residual);
  out.matrix_snr_db = snr_db(signal_rms, matrix_residual);
  out.matrix_explain_db = out.matrix_snr_db - out.scalar_snr_db;
  out.left_to_right = std::abs(m10);
  out.right_to_left = std::abs(m01);
  out.residual_rms = rms_from_sum(residual_sum, used);
  out.residual_diff_rms = rms_from_sum(residual_diff_sum, used > 2U ? used - 2U : used);
  out.residual_diff_ratio = out.residual_rms > 0.0 ? out.residual_diff_rms / out.residual_rms : 0.0;
  const double snr_floor = std::min(meta.left_snr, meta.right_snr);
  if (std::isfinite(meta.quality) && meta.quality >= kQualityGate && snr_floor >= kSnrGate) {
    out.classification = "strict_quality_candidate";
  } else if (out.matrix_explain_db >= 6.0) {
    out.classification = "static_stereo_route_or_gain_dominant";
  } else if (out.lag_stddev > 8.0 || out.lag_jumps_gt_2 > 8U) {
    out.classification = "variable_timebase_or_route_capture_instability";
  } else if (out.residual_diff_ratio > 0.80) {
    out.classification = "high_frequency_or_nonlinear_residual_dominant";
  } else {
    out.classification = "broadband_residual_not_explained_by_static_matrix";
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto root = repo_root(argv);
    const auto soundcheck_root = root / "local-analysis/soundcheck";
    std::vector<RunMeta> candidates;
    if (std::filesystem::is_directory(soundcheck_root)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(soundcheck_root)) {
        if (!entry.is_regular_file() || entry.path().filename() != "metrics.json") {
          continue;
        }
        const auto meta = read_meta(entry.path());
        if (!reference_path_for(meta.dir).empty() && !capture_path_for(meta.dir).empty() &&
            std::isfinite(meta.quality)) {
          candidates.push_back(meta);
        }
      }
    }
    std::sort(candidates.begin(), candidates.end(), [](const RunMeta& left, const RunMeta& right) {
      return left.quality > right.quality;
    });

    std::vector<RunMeta> selected;
    std::set<std::string> seen;
    for (const auto& meta : candidates) {
      if (selected.size() >= 8U) {
        break;
      }
      if (seen.insert(meta.dir.string()).second) {
        selected.push_back(meta);
      }
    }
    for (auto it = candidates.rbegin(); it != candidates.rend() && selected.size() < 12U; ++it) {
      if (seen.insert(it->dir.string()).second) {
        selected.push_back(*it);
      }
    }

    std::vector<RunForensics> analyses;
    for (const auto& meta : selected) {
      analyses.push_back(analyze_run(meta));
    }

    std::uint32_t strict_candidates = 0;
    std::uint32_t variable_timebase = 0;
    std::uint32_t static_matrix_dominant = 0;
    std::uint32_t high_frequency_dominant = 0;
    const RunForensics* best = nullptr;
    for (const auto& run : analyses) {
      const double snr_floor = std::min(run.meta.left_snr, run.meta.right_snr);
      strict_candidates +=
          (run.meta.quality >= kQualityGate && snr_floor >= kSnrGate) ? 1U : 0U;
      variable_timebase +=
          run.classification == "variable_timebase_or_route_capture_instability" ? 1U : 0U;
      static_matrix_dominant +=
          run.classification == "static_stereo_route_or_gain_dominant" ? 1U : 0U;
      high_frequency_dominant +=
          run.classification == "high_frequency_or_nonlinear_residual_dominant" ? 1U : 0U;
      if (best == nullptr || run.meta.quality > best->meta.quality) {
        best = &run;
      }
    }

    const bool pass = !analyses.empty();
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "{\n"
              << "  \"schema\": \"opena8djcpp.physical-capture-forensics.v1\",\n"
              << "  \"safety\": \"offline_existing_wav_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
              << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
              << "  \"meaning\": \"diagnostic classifier over archived iRig WAV captures; PASS is analyzer health, not product readiness\",\n"
              << "  \"candidate_runs_with_wav\": " << candidates.size() << ",\n"
              << "  \"analyzed_runs\": " << analyses.size() << ",\n"
              << "  \"strict_quality_candidates\": " << strict_candidates << ",\n"
              << "  \"variable_timebase_or_route_capture_instability_runs\": " << variable_timebase
              << ",\n"
              << "  \"static_stereo_route_or_gain_dominant_runs\": " << static_matrix_dominant
              << ",\n"
              << "  \"high_frequency_or_nonlinear_residual_dominant_runs\": "
              << high_frequency_dominant << ",\n";
    std::cout << "  \"best_analyzed_run\": ";
    if (best != nullptr) {
      std::cout << "{\"path\": \"" << json_escape(best->meta.dir.string())
                << "\", \"quality_alignment_score\": ";
      print_number(best->meta.quality);
      std::cout << ", \"snr_floor_db\": ";
      print_number(std::min(best->meta.left_snr, best->meta.right_snr));
      std::cout << ", \"classification\": \"" << best->classification
                << "\", \"matrix_explain_db\": ";
      print_number(best->matrix_explain_db);
      std::cout << ", \"lag_stddev_frames\": ";
      print_number(best->lag_stddev);
      std::cout << ", \"residual_diff_ratio\": ";
      print_number(best->residual_diff_ratio);
      std::cout << "},\n";
    } else {
      std::cout << "null,\n";
    }
    std::cout << "  \"runs\": [\n";
    for (std::size_t i = 0; i < analyses.size(); ++i) {
      const auto& run = analyses[i];
      std::cout << "    {\"path\": \"" << json_escape(run.meta.dir.string()) << "\""
                << ", \"quality_alignment_score\": ";
      print_number(run.meta.quality);
      std::cout << ", \"snr_floor_db\": ";
      print_number(std::min(run.meta.left_snr, run.meta.right_snr));
      std::cout << ", \"base_lag_frames\": " << run.base_lag
                << ", \"fixed_lag_score\": " << run.fixed_lag_score
                << ", \"window_count\": " << run.window_count
                << ", \"lag_min_frames\": " << run.lag_min
                << ", \"lag_max_frames\": " << run.lag_max
                << ", \"lag_jumps_gt_2_frames\": " << run.lag_jumps_gt_2
                << ", \"lag_stddev_frames\": " << run.lag_stddev
                << ", \"scalar_snr_db\": " << run.scalar_snr_db
                << ", \"matrix_snr_db\": " << run.matrix_snr_db
                << ", \"matrix_explain_db\": " << run.matrix_explain_db
                << ", \"left_to_right_matrix_abs\": " << run.left_to_right
                << ", \"right_to_left_matrix_abs\": " << run.right_to_left
                << ", \"residual_rms_dbfs\": " << db(run.residual_rms)
                << ", \"residual_diff_ratio\": " << run.residual_diff_ratio
                << ", \"classification\": \"" << run.classification << "\"}";
      if (i + 1U < analyses.size()) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "  ],\n"
              << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_NO_CAPTURE_PROVES_AUDIOPHILE_SUPERIORITY\"\n"
              << "}\n";
    return pass ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "physical_capture_forensics_error=" << error.what() << "\n";
    return 1;
  }
}
