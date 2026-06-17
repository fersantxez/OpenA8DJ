#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr double kIdleMaxRmsDbfs = -55.0;
constexpr double kIdleMaxPeakDbfs = -30.0;
constexpr double kIdleMaxDiffRmsDbfs = -58.0;

struct IdleRun {
  std::filesystem::path path;
  bool unhealthy = true;
  double max_rms_dbfs = std::numeric_limits<double>::quiet_NaN();
  double max_peak_dbfs = std::numeric_limits<double>::quiet_NaN();
  double max_diff_rms_dbfs = std::numeric_limits<double>::quiet_NaN();
  double duration_seconds = std::numeric_limits<double>::quiet_NaN();
  double sample_rate = std::numeric_limits<double>::quiet_NaN();
  std::filesystem::file_time_type evidence_time{};
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

bool finite(double value) {
  return std::isfinite(value);
}

std::optional<double> parse_double(const std::string& text) {
  try {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    return consumed > 0U && finite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<double> json_number(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const auto key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const auto colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  auto start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  auto end = start;
  while (end < json.size()) {
    const char c = json[end];
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' ||
          c == 'e' || c == 'E')) {
      break;
    }
    ++end;
  }
  return end > start ? parse_double(json.substr(start, end - start)) : std::nullopt;
}

std::optional<bool> json_bool(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const auto key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const auto colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  auto start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (json.compare(start, 4U, "true") == 0) {
    return true;
  }
  if (json.compare(start, 5U, "false") == 0) {
    return false;
  }
  return std::nullopt;
}

double number_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

void print_number(double value) {
  if (finite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
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

IdleRun read_idle_run(const std::filesystem::path& path) {
  const auto json = read_file(path);
  IdleRun run{};
  run.path = path;
  run.unhealthy = json_bool(json, "idle_capture_unhealthy").value_or(true);
  run.max_rms_dbfs = number_or_nan(json_number(json, "max_rms_dbfs"));
  run.max_peak_dbfs = number_or_nan(json_number(json, "max_peak_dbfs"));
  run.max_diff_rms_dbfs = number_or_nan(json_number(json, "max_diff_rms_dbfs"));
  run.duration_seconds = number_or_nan(json_number(json, "duration_seconds"));
  run.sample_rate = number_or_nan(json_number(json, "sample_rate"));
  std::error_code error;
  run.evidence_time = std::filesystem::last_write_time(path, error);
  return run;
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto idle_root = root / "local-analysis/irig-capture-isolation";
  std::vector<IdleRun> runs;
  if (std::filesystem::is_directory(idle_root)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(idle_root)) {
      if (entry.is_regular_file() && entry.path().filename() == "idle-capture-analysis.json") {
        runs.push_back(read_idle_run(entry.path()));
      }
    }
  }

  const IdleRun* latest = nullptr;
  for (const auto& run : runs) {
    if (latest == nullptr || run.evidence_time > latest->evidence_time) {
      latest = &run;
    }
  }

  const bool latest_clean =
      latest != nullptr && !latest->unhealthy && finite(latest->max_rms_dbfs) &&
      finite(latest->max_peak_dbfs) && finite(latest->max_diff_rms_dbfs) &&
      latest->max_rms_dbfs <= kIdleMaxRmsDbfs && latest->max_peak_dbfs <= kIdleMaxPeakDbfs &&
      latest->max_diff_rms_dbfs <= kIdleMaxDiffRmsDbfs;
  const bool pass = latest_clean;

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.irig-idle-capture-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_irig_idle_capture_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means the latest saved iRig idle capture is below idle-noise guardrails; not product readiness\",\n"
            << "  \"run_count\": " << runs.size() << ",\n"
            << "  \"thresholds\": {\"max_rms_dbfs\": " << kIdleMaxRmsDbfs
            << ", \"max_peak_dbfs\": " << kIdleMaxPeakDbfs
            << ", \"max_diff_rms_dbfs\": " << kIdleMaxDiffRmsDbfs << "},\n"
            << "  \"latest_run\": ";
  if (latest != nullptr) {
    std::cout << "{\"path\": \"" << json_escape(latest->path.parent_path().string())
              << "\", \"idle_capture_unhealthy\": "
              << (latest->unhealthy ? "true" : "false") << ", \"sample_rate\": ";
    print_number(latest->sample_rate);
    std::cout << ", \"duration_seconds\": ";
    print_number(latest->duration_seconds);
    std::cout << ", \"max_rms_dbfs\": ";
    print_number(latest->max_rms_dbfs);
    std::cout << ", \"max_peak_dbfs\": ";
    print_number(latest->max_peak_dbfs);
    std::cout << ", \"max_diff_rms_dbfs\": ";
    print_number(latest->max_diff_rms_dbfs);
    std::cout << "},\n";
  } else {
    std::cout << "null,\n";
  }
  std::cout << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_IRIG_IDLE_CAPTURE_DOES_NOT_VALIDATE_AUDIO8_OUTPUT\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
