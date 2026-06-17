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
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kEpsilon = 1.0e-20;

struct StereoBuffer {
  std::uint32_t sample_rate = 0;
  std::vector<std::array<double, 2>> frames;
};

struct ToneMetrics {
  std::filesystem::path path;
  std::uint32_t sample_rate = 0;
  std::size_t frames = 0;
  std::size_t analyzed_frames = 0;
  double active_start_seconds = 0.0;
  double active_end_seconds = 0.0;
  double rms = 0.0;
  double peak = 0.0;
  std::uint32_t clipped_frames = 0;
  double fundamental_hz = 1000.0;
  double fundamental_amp = 0.0;
  double fundamental_dbfs = -240.0;
  double thd_ratio = 0.0;
  double thd_db = -240.0;
  double sideband_ratio = 0.0;
  double sideband_db = -240.0;
  double strongest_sideband_hz = 0.0;
  double strongest_sideband_relative_db = -240.0;
  double residual_ratio = 0.0;
  double residual_db = -240.0;
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
    const double right =
        channels >= 2U ? pcm_sample(bytes, offset + bytes_per_sample, format, bits) : left;
    buffer.frames.push_back({left, right});
  }
  return buffer;
}

double db(double value) {
  return 20.0 * std::log10(std::max(value, kEpsilon));
}

double hann(std::size_t index, std::size_t count) {
  if (count <= 1U) {
    return 1.0;
  }
  return 0.5 - 0.5 * std::cos((2.0 * kPi * static_cast<double>(index)) /
                              static_cast<double>(count - 1U));
}

double tone_amplitude(const std::vector<double>& samples, std::uint32_t rate, double hz) {
  double real = 0.0;
  double imag = 0.0;
  double window_sum = 0.0;
  const double radians_per_sample = 2.0 * kPi * hz / static_cast<double>(rate);
  for (std::size_t index = 0; index < samples.size(); ++index) {
    const double window = hann(index, samples.size());
    const double phase = radians_per_sample * static_cast<double>(index);
    const double value = samples[index] * window;
    real += value * std::cos(phase);
    imag -= value * std::sin(phase);
    window_sum += window;
  }
  return window_sum > 0.0 ? (2.0 * std::hypot(real, imag)) / window_sum : 0.0;
}

std::pair<std::size_t, std::size_t> active_range(const StereoBuffer& wav) {
  const std::size_t window_frames =
      std::max<std::size_t>(256U, static_cast<std::size_t>(wav.sample_rate / 4U));
  if (wav.frames.size() <= window_frames * 2U) {
    return {0, wav.frames.size()};
  }

  std::vector<double> rms_rows;
  for (std::size_t start = 0; start + window_frames <= wav.frames.size(); start += window_frames) {
    double energy = 0.0;
    for (std::size_t index = start; index < start + window_frames; ++index) {
      const double sample = 0.5 * (wav.frames[index][0] + wav.frames[index][1]);
      energy += sample * sample;
    }
    rms_rows.push_back(std::sqrt(energy / static_cast<double>(window_frames)));
  }
  const double max_rms = *std::max_element(rms_rows.begin(), rms_rows.end());
  if (max_rms <= 0.0) {
    return {0, wav.frames.size()};
  }
  const double threshold = max_rms * 0.35;
  std::size_t first_window = 0;
  while (first_window < rms_rows.size() && rms_rows[first_window] < threshold) {
    ++first_window;
  }
  std::size_t last_window = rms_rows.size();
  while (last_window > first_window && rms_rows[last_window - 1U] < threshold) {
    --last_window;
  }
  if (first_window >= last_window) {
    return {0, wav.frames.size()};
  }
  const std::size_t start = std::min(first_window * window_frames, wav.frames.size());
  const std::size_t end = std::min(last_window * window_frames, wav.frames.size());
  return end > start ? std::pair<std::size_t, std::size_t>{start, end}
                     : std::pair<std::size_t, std::size_t>{0, wav.frames.size()};
}

ToneMetrics analyze_tone(const std::filesystem::path& path, double fundamental_hz) {
  const auto wav = read_wav_pair(path);
  if (wav.frames.size() < static_cast<std::size_t>(wav.sample_rate / 2U)) {
    throw std::runtime_error("wav_too_short:" + path.string());
  }

  const auto [active_start, active_end] = active_range(wav);
  const std::size_t trim = std::min<std::size_t>(
      static_cast<std::size_t>(wav.sample_rate / 8U), (active_end - active_start) / 10U);
  const std::size_t start = std::min(active_start + trim, active_end);
  const std::size_t end = active_end > trim ? active_end - trim : active_end;
  std::vector<double> mono;
  mono.reserve(end - start);
  ToneMetrics metrics{};
  metrics.path = path;
  metrics.sample_rate = wav.sample_rate;
  metrics.frames = wav.frames.size();
  metrics.analyzed_frames = end > start ? end - start : 0;
  metrics.active_start_seconds = static_cast<double>(start) / static_cast<double>(wav.sample_rate);
  metrics.active_end_seconds = static_cast<double>(end) / static_cast<double>(wav.sample_rate);
  metrics.fundamental_hz = fundamental_hz;

  double energy = 0.0;
  for (std::size_t index = start; index < end; ++index) {
    const double sample = 0.5 * (wav.frames[index][0] + wav.frames[index][1]);
    mono.push_back(sample);
    energy += sample * sample;
    metrics.peak = std::max(metrics.peak, std::abs(sample));
    if (std::abs(wav.frames[index][0]) >= 0.999 || std::abs(wav.frames[index][1]) >= 0.999) {
      metrics.clipped_frames += 1;
    }
  }
  metrics.rms = mono.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(mono.size()));
  metrics.fundamental_amp = tone_amplitude(mono, wav.sample_rate, fundamental_hz);
  metrics.fundamental_dbfs = db(metrics.fundamental_amp);

  double harmonic_energy = 0.0;
  for (std::uint32_t harmonic = 2; harmonic <= 10; ++harmonic) {
    const double hz = fundamental_hz * static_cast<double>(harmonic);
    if (hz >= static_cast<double>(wav.sample_rate) * 0.475) {
      break;
    }
    const double amp = tone_amplitude(mono, wav.sample_rate, hz);
    harmonic_energy += amp * amp;
  }
  metrics.thd_ratio =
      metrics.fundamental_amp > 0.0 ? std::sqrt(harmonic_energy) / metrics.fundamental_amp : 0.0;
  metrics.thd_db = db(metrics.thd_ratio);

  double sideband_energy = 0.0;
  for (std::uint32_t multiple = 1; multiple <= 8; ++multiple) {
    for (double sign : {-1.0, 1.0}) {
      const double hz = fundamental_hz + sign * 60.0 * static_cast<double>(multiple);
      if (hz <= 20.0 || hz >= static_cast<double>(wav.sample_rate) * 0.475) {
        continue;
      }
      const double amp = tone_amplitude(mono, wav.sample_rate, hz);
      sideband_energy += amp * amp;
      if (amp > 0.0 && db(amp / std::max(metrics.fundamental_amp, kEpsilon)) >
                           metrics.strongest_sideband_relative_db) {
        metrics.strongest_sideband_relative_db = db(amp / std::max(metrics.fundamental_amp, kEpsilon));
        metrics.strongest_sideband_hz = hz;
      }
    }
  }
  metrics.sideband_ratio =
      metrics.fundamental_amp > 0.0 ? std::sqrt(sideband_energy) / metrics.fundamental_amp : 0.0;
  metrics.sideband_db = db(metrics.sideband_ratio);

  const double fundamental_rms = metrics.fundamental_amp / std::sqrt(2.0);
  const double residual_rms =
      std::sqrt(std::max(0.0, metrics.rms * metrics.rms - fundamental_rms * fundamental_rms));
  metrics.residual_ratio = fundamental_rms > 0.0 ? residual_rms / fundamental_rms : 0.0;
  metrics.residual_db = db(metrics.residual_ratio);
  metrics.pass = metrics.sample_rate >= 44100 && metrics.analyzed_frames > wav.sample_rate &&
                 metrics.fundamental_amp > 0.01 &&
                 metrics.clipped_frames == 0 && metrics.thd_ratio <= 0.035 &&
                 metrics.sideband_ratio <= 0.012 &&
                 metrics.strongest_sideband_relative_db <= -38.0 &&
                 metrics.residual_ratio <= 0.70;
  return metrics;
}

void print_metrics(const char* label, const ToneMetrics& metrics) {
  std::cout << "  \"" << label << "\": {"
            << "\"path\": \"" << metrics.path.string() << "\", "
            << "\"sample_rate\": " << metrics.sample_rate << ", "
            << "\"frames\": " << metrics.frames << ", "
            << "\"analyzed_frames\": " << metrics.analyzed_frames << ", "
            << "\"active_start_seconds\": " << metrics.active_start_seconds << ", "
            << "\"active_end_seconds\": " << metrics.active_end_seconds << ", "
            << "\"rms\": " << metrics.rms << ", "
            << "\"peak\": " << metrics.peak << ", "
            << "\"clipped_frames\": " << metrics.clipped_frames << ", "
            << "\"fundamental_hz\": " << metrics.fundamental_hz << ", "
            << "\"fundamental_dbfs\": " << metrics.fundamental_dbfs << ", "
            << "\"thd_ratio\": " << metrics.thd_ratio << ", "
            << "\"thd_db\": " << metrics.thd_db << ", "
            << "\"sideband_ratio\": " << metrics.sideband_ratio << ", "
            << "\"sideband_db\": " << metrics.sideband_db << ", "
            << "\"strongest_sideband_hz\": " << metrics.strongest_sideband_hz << ", "
            << "\"strongest_sideband_relative_db\": "
            << metrics.strongest_sideband_relative_db << ", "
            << "\"residual_ratio\": " << metrics.residual_ratio << ", "
            << "\"residual_db\": " << metrics.residual_db << ", "
            << "\"result\": \"" << (metrics.pass ? "PASS" : "FAIL") << "\"}";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto root = repo_root(argv);
    std::filesystem::path candidate =
        root / "local-analysis/physical-tone/20260617-bff59cc-default/tone-1khz-irig-pairA/tone.wav";
    std::filesystem::path baseline =
        "/Users/fer/dev/opena8dj/local-analysis/final-0324-iso5-tone-20260613-005429/tone.wav";
    double fundamental_hz = 1000.0;

    for (int index = 1; index < argc; ++index) {
      const std::string arg = argv[index];
      if (arg == "--candidate" && index + 1 < argc) {
        candidate = argv[++index];
      } else if (arg == "--baseline" && index + 1 < argc) {
        baseline = argv[++index];
      } else if (arg == "--frequency" && index + 1 < argc) {
        fundamental_hz = std::stod(argv[++index]);
      } else {
        throw std::runtime_error("usage: opena8djcpp_audiophile_tone_gate "
                                 "[--candidate WAV] [--baseline WAV] [--frequency HZ]");
      }
    }

    const auto candidate_metrics = analyze_tone(candidate, fundamental_hz);
    const auto baseline_metrics = analyze_tone(baseline, fundamental_hz);
    const bool candidate_not_worse_than_baseline =
        candidate_metrics.sideband_ratio <= baseline_metrics.sideband_ratio * 1.05 &&
        candidate_metrics.thd_ratio <= baseline_metrics.thd_ratio * 1.10 &&
        candidate_metrics.clipped_frames == 0;
    const bool pass = candidate_metrics.pass && candidate_not_worse_than_baseline;

    std::cout << std::fixed << std::setprecision(6)
              << "{\n"
              << "  \"schema\": \"opena8djcpp.audiophile-tone-gate.v1\",\n"
              << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
              << "  \"meaning\": \"offline saved-tone distortion/sideband gate; PASS is not product readiness or current route validation\",\n"
              << "  \"safety\": \"offline_existing_wav_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
              << "  \"physical_measurement_valid_for_promotion\": false,\n"
              << "  \"candidate_threshold_pass\": " << (candidate_metrics.pass ? "true" : "false")
              << ",\n"
              << "  \"baseline_threshold_pass\": " << (baseline_metrics.pass ? "true" : "false")
              << ",\n"
              << "  \"candidate_not_worse_than_baseline\": "
              << (candidate_not_worse_than_baseline ? "true" : "false") << ",\n";
    print_metrics("candidate", candidate_metrics);
    std::cout << ",\n";
    print_metrics("baseline", baseline_metrics);
    std::cout << "\n}\n";
    return pass ? 0 : 1;
  } catch (const std::exception& error) {
    std::cout << "{\n"
              << "  \"schema\": \"opena8djcpp.audiophile-tone-gate.v1\",\n"
              << "  \"result\": \"FAIL\",\n"
              << "  \"error\": \"" << error.what() << "\"\n"
              << "}\n";
    return 1;
  }
}
