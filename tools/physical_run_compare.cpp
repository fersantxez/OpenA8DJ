#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct CpuStats {
  double avg = 0.0;
  double p95 = 0.0;
  double max = 0.0;
  std::size_t samples = 0;
};

struct RunStats {
  std::filesystem::path path;
  double quality = std::numeric_limits<double>::quiet_NaN();
  double snr = std::numeric_limits<double>::quiet_NaN();
  double mid = std::numeric_limits<double>::quiet_NaN();
  double high = std::numeric_limits<double>::quiet_NaN();
  double quiet_mid = std::numeric_limits<double>::quiet_NaN();
  double lag_jumps = std::numeric_limits<double>::quiet_NaN();
  double clipping = std::numeric_limits<double>::quiet_NaN();
  CpuStats driver;
  CpuStats coreaudiod;
};

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
  std::size_t start = colon + 1;
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
    return std::stod(json.substr(start, end - start));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::vector<std::string> split_tab(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  std::istringstream input(line);
  while (std::getline(input, field, '\t')) {
    fields.push_back(field);
  }
  return fields;
}

std::optional<double> parse_double(const std::string& value) {
  if (value.empty()) {
    return std::nullopt;
  }
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed == 0) {
      return std::nullopt;
    }
    return parsed;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

CpuStats summarize_values(std::vector<double> values) {
  CpuStats stats;
  stats.samples = values.size();
  if (values.empty()) {
    return stats;
  }
  double sum = 0.0;
  for (const double value : values) {
    sum += value;
  }
  std::sort(values.begin(), values.end());
  const std::size_t p95_index =
      static_cast<std::size_t>(std::floor(static_cast<double>(values.size() - 1) * 0.95));
  stats.avg = sum / static_cast<double>(values.size());
  stats.p95 = values[p95_index];
  stats.max = values.back();
  return stats;
}

CpuStats read_cpu_column(const std::filesystem::path& tsv, const std::string& column) {
  std::ifstream input(tsv);
  if (!input) {
    return {};
  }
  std::string header_line;
  if (!std::getline(input, header_line)) {
    return {};
  }
  const auto header = split_tab(header_line);
  const auto it = std::find(header.begin(), header.end(), column);
  if (it == header.end()) {
    return {};
  }
  const std::size_t index = static_cast<std::size_t>(std::distance(header.begin(), it));
  std::vector<double> values;
  std::string line;
  while (std::getline(input, line)) {
    const auto row = split_tab(line);
    if (index >= row.size()) {
      continue;
    }
    if (auto parsed = parse_double(row[index])) {
      values.push_back(*parsed);
    }
  }
  return summarize_values(std::move(values));
}

double number_or_nan(const std::optional<double>& value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

std::optional<double> min_present(std::optional<double> lhs, std::optional<double> rhs) {
  if (lhs && rhs) {
    return std::min(*lhs, *rhs);
  }
  return lhs ? lhs : rhs;
}

RunStats read_run(const std::filesystem::path& path) {
  RunStats run;
  run.path = path;
  const std::string metrics = read_file(path / "metrics.json");
  run.quality = number_or_nan(json_number(metrics, "quality_alignment_score"));
  run.snr = number_or_nan(min_present(json_number(metrics, "left_snr_db"),
                                      json_number(metrics, "right_snr_db")));
  run.mid = number_or_nan(json_number(metrics, "mid_band_residual_ratio"));
  run.high = number_or_nan(json_number(metrics, "high_band_residual_ratio"));
  run.quiet_mid = number_or_nan(json_number(metrics, "quiet_mid_band_noise_dbfs"));
  run.lag_jumps = number_or_nan(json_number(metrics, "lag_jumps_gt_2_frames"));
  run.clipping = number_or_nan(json_number(metrics, "capture_clipped_frames"));
  run.driver = read_cpu_column(path / "cpu-profile.tsv", "opena8dj_driver");
  run.coreaudiod = read_cpu_column(path / "cpu-profile.tsv", "coreaudiod");
  return run;
}

bool finite_le(double value, double limit) {
  return std::isfinite(value) && value <= limit;
}

bool finite_ge(double value, double limit) {
  return std::isfinite(value) && value >= limit;
}

bool practical_pass(const RunStats& run) {
  return finite_ge(run.quality, 0.925) && finite_le(run.mid, 1.45) &&
         finite_le(run.high, 1.355) && finite_le(run.quiet_mid, -32.5) &&
         finite_le(run.lag_jumps, 45.0) && finite_le(run.clipping, 0.0) &&
         run.driver.samples > 0 && finite_le(run.driver.p95, 12.0) &&
         run.coreaudiod.samples > 0 && finite_le(run.coreaudiod.p95, 8.0);
}

std::string run_label(const std::filesystem::path& path) {
  const std::filesystem::path filename = path.filename();
  if (filename == "soundcheck" && path.has_parent_path()) {
    return path.parent_path().filename().string();
  }
  return filename.string();
}

void print_json_string(const std::string& text) {
  std::cout << '"';
  for (const char c : text) {
    if (c == '"' || c == '\\') {
      std::cout << '\\' << c;
    } else if (c == '\n') {
      std::cout << "\\n";
    } else {
      std::cout << c;
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

void print_run(const RunStats& run, bool last) {
  std::cout << "    {\n";
  std::cout << "      \"run\": ";
  print_json_string(run_label(run.path));
  std::cout << ",\n";
  std::cout << "      \"path\": ";
  print_json_string(run.path.string());
  std::cout << ",\n";
  std::cout << "      \"practical_pass\": " << (practical_pass(run) ? "true" : "false") << ",\n";
  std::cout << "      \"quality_alignment_score\": ";
  print_json_number(run.quality);
  std::cout << ",\n";
  std::cout << "      \"snr_floor_db\": ";
  print_json_number(run.snr);
  std::cout << ",\n";
  std::cout << "      \"mid_band_residual_ratio\": ";
  print_json_number(run.mid);
  std::cout << ",\n";
  std::cout << "      \"high_band_residual_ratio\": ";
  print_json_number(run.high);
  std::cout << ",\n";
  std::cout << "      \"quiet_mid_band_dbfs\": ";
  print_json_number(run.quiet_mid);
  std::cout << ",\n";
  std::cout << "      \"lag_jumps_gt_2_frames\": ";
  print_json_number(run.lag_jumps);
  std::cout << ",\n";
  std::cout << "      \"capture_clipped_frames\": ";
  print_json_number(run.clipping);
  std::cout << ",\n";
  std::cout << "      \"driver_cpu_p95\": " << run.driver.p95 << ",\n";
  std::cout << "      \"driver_cpu_avg\": " << run.driver.avg << ",\n";
  std::cout << "      \"driver_cpu_max\": " << run.driver.max << ",\n";
  std::cout << "      \"coreaudiod_cpu_p95\": " << run.coreaudiod.p95 << ",\n";
  std::cout << "      \"cpu_samples\": " << run.driver.samples << "\n";
  std::cout << "    }" << (last ? "\n" : ",\n");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: physical-run-compare RUN_DIR [RUN_DIR ...]\n";
    return 2;
  }

  std::vector<RunStats> runs;
  runs.reserve(static_cast<std::size_t>(argc - 1));
  for (int i = 1; i < argc; ++i) {
    runs.push_back(read_run(argv[i]));
  }

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n";
  std::cout << "  \"schema_version\": 1,\n";
  std::cout << "  \"thresholds\": {\n";
  std::cout << "    \"quality_alignment_score_min\": 0.925,\n";
  std::cout << "    \"mid_band_residual_ratio_max\": 1.45,\n";
  std::cout << "    \"high_band_residual_ratio_max\": 1.355,\n";
  std::cout << "    \"quiet_mid_band_dbfs_max\": -32.5,\n";
  std::cout << "    \"lag_jumps_gt_2_frames_max\": 45,\n";
  std::cout << "    \"driver_cpu_p95_max\": 12,\n";
  std::cout << "    \"coreaudiod_cpu_p95_max\": 8\n";
  std::cout << "  },\n";
  std::cout << "  \"runs\": [\n";
  for (std::size_t i = 0; i < runs.size(); ++i) {
    print_run(runs[i], i + 1 == runs.size());
  }
  std::cout << "  ]\n";
  std::cout << "}\n";
  return 0;
}
