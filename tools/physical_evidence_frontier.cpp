#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kQualityGate = 0.98;
constexpr double kSnrGate = 35.0;
constexpr double kDriverCpuGate = 6.5;
constexpr double kCoreaudiodCpuGate = 1.7;
constexpr double kLagJumpGate = 0.0;

struct CpuStats {
  double p95 = std::numeric_limits<double>::quiet_NaN();
  double p95_after_5s = std::numeric_limits<double>::quiet_NaN();
  std::size_t samples = 0;
  std::size_t samples_after_5s = 0;
};

struct Run {
  std::filesystem::path dir;
  std::string family;
  double quality = std::numeric_limits<double>::quiet_NaN();
  double snr_floor = std::numeric_limits<double>::quiet_NaN();
  double mid_ratio = std::numeric_limits<double>::quiet_NaN();
  double high_ratio = std::numeric_limits<double>::quiet_NaN();
  double quiet_mid_dbfs = std::numeric_limits<double>::quiet_NaN();
  double lag_jumps = std::numeric_limits<double>::quiet_NaN();
  double click_outliers = std::numeric_limits<double>::quiet_NaN();
  double clipped_frames = std::numeric_limits<double>::quiet_NaN();
  CpuStats driver_cpu;
  CpuStats coreaudiod_cpu;
  bool metrics_present = false;
  bool cpu_present = false;
};

struct Family {
  std::string name;
  std::vector<Run> runs;
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

std::optional<double> parse_number_at(const std::string& text, std::size_t start) {
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  if (text.compare(start, 4U, "null") == 0) {
    return std::nullopt;
  }
  std::size_t end = start;
  while (end < text.size()) {
    const char c = text[end];
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
    const double value = std::stod(text.substr(start, end - start));
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
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
  return parse_number_at(json, colon + 1U);
}

double number_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

bool finite(double value) {
  return std::isfinite(value);
}

std::vector<std::string> split_tab(const std::string& line) {
  std::vector<std::string> out;
  std::string cell;
  std::istringstream input(line);
  while (std::getline(input, cell, '\t')) {
    out.push_back(cell);
  }
  return out;
}

std::optional<double> parse_double(const std::string& value) {
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    return consumed > 0 && std::isfinite(parsed) ? std::optional<double>(parsed) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

double percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(
      std::ceil(std::clamp(p, 0.0, 1.0) * static_cast<double>(values.size())) - 1.0);
  return values[std::min(index, values.size() - 1U)];
}

CpuStats read_cpu(const std::filesystem::path& path, const std::string& column) {
  CpuStats stats{};
  std::ifstream input(path);
  if (!input) {
    return stats;
  }
  std::string header_line;
  if (!std::getline(input, header_line)) {
    return stats;
  }
  const auto header = split_tab(header_line);
  auto column_it = std::find(header.begin(), header.end(), column);
  auto elapsed_it = std::find(header.begin(), header.end(), "elapsed_seconds");
  if (column_it == header.end()) {
    return stats;
  }
  const auto column_index = static_cast<std::size_t>(std::distance(header.begin(), column_it));
  const auto elapsed_index =
      elapsed_it == header.end()
          ? std::numeric_limits<std::size_t>::max()
          : static_cast<std::size_t>(std::distance(header.begin(), elapsed_it));
  std::vector<double> values;
  std::vector<double> values_after_5s;
  std::string line;
  while (std::getline(input, line)) {
    const auto fields = split_tab(line);
    if (column_index >= fields.size()) {
      continue;
    }
    const auto value = parse_double(fields[column_index]);
    if (!value.has_value()) {
      continue;
    }
    values.push_back(*value);
    if (elapsed_index < fields.size()) {
      const auto elapsed = parse_double(fields[elapsed_index]);
      if (elapsed.has_value() && *elapsed >= 5.0) {
        values_after_5s.push_back(*value);
      }
    }
  }
  stats.samples = values.size();
  stats.samples_after_5s = values_after_5s.size();
  stats.p95 = percentile(values, 0.95);
  stats.p95_after_5s = percentile(values_after_5s, 0.95);
  return stats;
}

std::string family_for(const std::filesystem::path& dir) {
  const auto text = dir.string();
  std::smatch match;
  static const std::regex iso_q_pattern("iso([0-9]+).*q([0-9]+)",
                                        std::regex_constants::icase);
  if (text.find("mainline") != std::string::npos) {
    return "mainline_reference";
  }
  if (std::regex_search(text, match, iso_q_pattern) && match.size() >= 3U) {
    return "iso" + match[1].str() + "_q" + match[2].str();
  }
  if (text.find("inputdecode-off") != std::string::npos) {
    return "inputdecode_off";
  }
  if (text.find("output-only") != std::string::npos) {
    return "output_only";
  }
  if (text.find("queue8") != std::string::npos) {
    return "queue8";
  }
  if (text.find("default") != std::string::npos) {
    return "default_cpp";
  }
  if (text.find("cpp-hal") != std::string::npos || text.find("cpp-") != std::string::npos) {
    return "cpp_other";
  }
  return "unknown";
}

Run read_run(const std::filesystem::path& metrics_path) {
  Run run{};
  run.dir = metrics_path.parent_path();
  run.family = family_for(run.dir);
  const auto metrics = read_file(metrics_path);
  run.metrics_present = !metrics.empty();
  run.quality = number_or_nan(json_number(metrics, "quality_alignment_score"));
  const double left_snr = number_or_nan(json_number(metrics, "left_snr_db"));
  const double right_snr = number_or_nan(json_number(metrics, "right_snr_db"));
  if (finite(left_snr) && finite(right_snr)) {
    run.snr_floor = std::min(left_snr, right_snr);
  }
  run.mid_ratio = number_or_nan(json_number(metrics, "mid_band_residual_ratio"));
  run.high_ratio = number_or_nan(json_number(metrics, "high_band_residual_ratio"));
  run.quiet_mid_dbfs = number_or_nan(json_number(metrics, "quiet_mid_band_noise_dbfs"));
  run.lag_jumps = number_or_nan(json_number(metrics, "lag_jumps_gt_2_frames"));
  const double click = number_or_nan(json_number(metrics, "click_outliers"));
  const double left_click = number_or_nan(json_number(metrics, "left_click_outliers"));
  const double right_click = number_or_nan(json_number(metrics, "right_click_outliers"));
  const double window_click = number_or_nan(json_number(metrics, "window_click_outliers_max"));
  run.click_outliers =
      finite(click) ? click : std::max({finite(left_click) ? left_click : 0.0,
                                        finite(right_click) ? right_click : 0.0,
                                        finite(window_click) ? window_click : 0.0});
  run.clipped_frames = number_or_nan(json_number(metrics, "capture_clipped_frames"));
  const auto cpu_path = run.dir / "cpu-profile.tsv";
  run.cpu_present = std::filesystem::is_regular_file(cpu_path);
  run.driver_cpu = read_cpu(cpu_path, "opena8dj_driver");
  run.coreaudiod_cpu = read_cpu(cpu_path, "coreaudiod");
  return run;
}

bool quality_pass(const Run& run) {
  return finite(run.quality) && finite(run.snr_floor) && finite(run.mid_ratio) &&
         finite(run.high_ratio) && finite(run.quiet_mid_dbfs) && finite(run.lag_jumps) &&
         finite(run.click_outliers) && finite(run.clipped_frames) && run.quality >= kQualityGate &&
         run.snr_floor >= kSnrGate && run.mid_ratio <= 1.36 && run.high_ratio <= 1.35 &&
         run.quiet_mid_dbfs <= -58.0 && run.lag_jumps <= kLagJumpGate &&
         run.click_outliers == 0.0 && run.clipped_frames == 0.0;
}

bool cpu_pass(const Run& run) {
  return finite(run.driver_cpu.p95) && finite(run.coreaudiod_cpu.p95) &&
         run.driver_cpu.p95 <= kDriverCpuGate && run.coreaudiod_cpu.p95 <= kCoreaudiodCpuGate;
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
  if (finite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

double best_quality(const std::vector<Run>& runs) {
  double best = std::numeric_limits<double>::quiet_NaN();
  for (const auto& run : runs) {
    if (finite(run.quality) && (!finite(best) || run.quality > best)) {
      best = run.quality;
    }
  }
  return best;
}

double min_value(const std::vector<Run>& runs, double Run::*member) {
  double best = std::numeric_limits<double>::quiet_NaN();
  for (const auto& run : runs) {
    const double value = run.*member;
    if (finite(value) && (!finite(best) || value < best)) {
      best = value;
    }
  }
  return best;
}

double max_value(const std::vector<Run>& runs, double Run::*member) {
  double best = std::numeric_limits<double>::quiet_NaN();
  for (const auto& run : runs) {
    const double value = run.*member;
    if (finite(value) && (!finite(best) || value > best)) {
      best = value;
    }
  }
  return best;
}

double min_driver_p95(const std::vector<Run>& runs) {
  double best = std::numeric_limits<double>::quiet_NaN();
  for (const auto& run : runs) {
    if (finite(run.driver_cpu.p95) && (!finite(best) || run.driver_cpu.p95 < best)) {
      best = run.driver_cpu.p95;
    }
  }
  return best;
}

double min_coreaudiod_p95(const std::vector<Run>& runs) {
  double best = std::numeric_limits<double>::quiet_NaN();
  for (const auto& run : runs) {
    if (finite(run.coreaudiod_cpu.p95) && (!finite(best) || run.coreaudiod_cpu.p95 < best)) {
      best = run.coreaudiod_cpu.p95;
    }
  }
  return best;
}

const Run* best_quality_run(const std::vector<Run>& runs) {
  const Run* best = nullptr;
  for (const auto& run : runs) {
    if (finite(run.quality) && (best == nullptr || run.quality > best->quality)) {
      best = &run;
    }
  }
  return best;
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto soundcheck_root = root / "local-analysis/soundcheck";
  std::vector<Run> runs;
  if (std::filesystem::is_directory(soundcheck_root)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(soundcheck_root)) {
      if (entry.is_regular_file() && entry.path().filename() == "metrics.json") {
        runs.push_back(read_run(entry.path()));
      }
    }
  }
  std::sort(runs.begin(), runs.end(), [](const Run& left, const Run& right) {
    return left.dir.string() < right.dir.string();
  });

  std::map<std::string, Family> families;
  std::uint32_t quality_passing_runs = 0;
  std::uint32_t cpu_passing_runs = 0;
  std::uint32_t product_candidate_runs = 0;
  for (const auto& run : runs) {
    families[run.family].name = run.family;
    families[run.family].runs.push_back(run);
    const bool q = quality_pass(run);
    const bool c = cpu_pass(run);
    if (q) {
      quality_passing_runs += 1;
    }
    if (c) {
      cpu_passing_runs += 1;
    }
    if (q && c) {
      product_candidate_runs += 1;
    }
  }

  const bool pass = !runs.empty() && product_candidate_runs == 0;
  const auto* best_run = best_quality_run(runs);
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.physical-evidence-frontier.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"diagnostic frontier over existing physical runs; PASS means no product candidate was found, not readiness\",\n"
            << "  \"run_count\": " << runs.size() << ",\n"
            << "  \"family_count\": " << families.size() << ",\n"
            << "  \"quality_passing_runs\": " << quality_passing_runs << ",\n"
            << "  \"cpu_passing_runs\": " << cpu_passing_runs << ",\n"
            << "  \"product_candidate_runs\": " << product_candidate_runs << ",\n"
            << "  \"best_quality_run\": ";
  if (best_run != nullptr) {
    std::cout << "{\"path\": \"" << json_escape(best_run->dir.string()) << "\", \"family\": \""
              << json_escape(best_run->family) << "\", \"quality_alignment_score\": ";
    print_number(best_run->quality);
    std::cout << ", \"snr_floor_db\": ";
    print_number(best_run->snr_floor);
    std::cout << ", \"driver_cpu_p95\": ";
    print_number(best_run->driver_cpu.p95);
    std::cout << "},\n";
  } else {
    std::cout << "null,\n";
  }
  std::cout << "  \"families\": [\n";
  std::size_t index = 0;
  for (const auto& [name, family] : families) {
    const auto* family_best = best_quality_run(family.runs);
    std::uint32_t family_quality_passes = 0;
    std::uint32_t family_cpu_passes = 0;
    std::uint32_t family_product_candidates = 0;
    for (const auto& run : family.runs) {
      const bool q = quality_pass(run);
      const bool c = cpu_pass(run);
      family_quality_passes += q ? 1U : 0U;
      family_cpu_passes += c ? 1U : 0U;
      family_product_candidates += (q && c) ? 1U : 0U;
    }
    std::cout << "    {\"family\": \"" << json_escape(name) << "\""
              << ", \"run_count\": " << family.runs.size()
              << ", \"quality_passing_runs\": " << family_quality_passes
              << ", \"cpu_passing_runs\": " << family_cpu_passes
              << ", \"product_candidate_runs\": " << family_product_candidates
              << ", \"best_quality_alignment_score\": ";
    print_number(best_quality(family.runs));
    std::cout << ", \"best_snr_floor_db\": ";
    print_number(max_value(family.runs, &Run::snr_floor));
    std::cout << ", \"min_lag_jumps_gt_2_frames\": ";
    print_number(min_value(family.runs, &Run::lag_jumps));
    std::cout << ", \"min_driver_cpu_p95\": ";
    print_number(min_driver_p95(family.runs));
    std::cout << ", \"min_coreaudiod_cpu_p95\": ";
    print_number(min_coreaudiod_p95(family.runs));
    std::cout << ", \"best_run\": ";
    if (family_best != nullptr) {
      std::cout << "\"" << json_escape(family_best->dir.string()) << "\"";
    } else {
      std::cout << "null";
    }
    std::cout << "}";
    if (++index < families.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }
  std::cout << "  ],\n"
            << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_NO_EXISTING_PHYSICAL_RUN_PROVES_SUPERIORITY\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
