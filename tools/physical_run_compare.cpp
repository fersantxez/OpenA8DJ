#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class Direction {
  GreaterOrEqual,
  LessOrEqual,
};

struct CpuStats {
  double avg = std::numeric_limits<double>::quiet_NaN();
  double p95 = std::numeric_limits<double>::quiet_NaN();
  double max = std::numeric_limits<double>::quiet_NaN();
  std::size_t samples = 0;
};

struct RunStats {
  std::filesystem::path path;
  bool metrics_present = false;
  bool cpu_present = false;
  bool reference_wav_present = false;
  bool captured_wav_present = false;
  double quality = std::numeric_limits<double>::quiet_NaN();
  double snr = std::numeric_limits<double>::quiet_NaN();
  double mid = std::numeric_limits<double>::quiet_NaN();
  double high = std::numeric_limits<double>::quiet_NaN();
  double quiet_mid = std::numeric_limits<double>::quiet_NaN();
  double lag_jumps = std::numeric_limits<double>::quiet_NaN();
  double click_outliers = std::numeric_limits<double>::quiet_NaN();
  double clipping = std::numeric_limits<double>::quiet_NaN();
  CpuStats driver;
  CpuStats coreaudiod;
};

struct FixedBaseline {
  double quality = 0.98;
  double snr = 35.0;
  double mid = 1.36;
  double high = 1.35;
  double quiet_mid = -58.0;
  double lag_jumps = 0.0;
  double click_outliers = 0.0;
  double clipping = 0.0;
  double driver_p95 = 6.5;
  double coreaudiod_p95 = 1.7;
};

struct GateResult {
  std::string name;
  Direction direction = Direction::GreaterOrEqual;
  double candidate = std::numeric_limits<double>::quiet_NaN();
  double required = std::numeric_limits<double>::quiet_NaN();
  bool pass = false;
};

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

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
    if (consumed == 0 || !std::isfinite(parsed)) {
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

std::optional<double> max_present(std::optional<double> lhs, std::optional<double> rhs) {
  if (lhs && rhs) {
    return std::max(*lhs, *rhs);
  }
  return lhs ? lhs : rhs;
}

RunStats read_run(const std::filesystem::path& path) {
  RunStats run;
  run.path = path;
  const std::filesystem::path metrics_path = path / "metrics.json";
  const std::filesystem::path cpu_path = path / "cpu-profile.tsv";
  const std::string metrics = read_file(metrics_path);
  run.metrics_present = !metrics.empty();
  run.cpu_present = std::filesystem::is_regular_file(cpu_path);
  run.reference_wav_present = std::filesystem::is_regular_file(path / "fixture/reference.wav");
  run.captured_wav_present = std::filesystem::is_regular_file(path / "captured.wav");
  run.quality = number_or_nan(json_number(metrics, "quality_alignment_score"));
  run.snr = number_or_nan(min_present(json_number(metrics, "left_snr_db"),
                                      json_number(metrics, "right_snr_db")));
  run.mid = number_or_nan(json_number(metrics, "mid_band_residual_ratio"));
  run.high = number_or_nan(json_number(metrics, "high_band_residual_ratio"));
  run.quiet_mid = number_or_nan(json_number(metrics, "quiet_mid_band_noise_dbfs"));
  run.lag_jumps = number_or_nan(json_number(metrics, "lag_jumps_gt_2_frames"));
  run.click_outliers = number_or_nan(max_present(
      max_present(json_number(metrics, "left_click_outliers"),
                  json_number(metrics, "right_click_outliers")),
      json_number(metrics, "window_click_outliers_max")));
  run.clipping = number_or_nan(json_number(metrics, "capture_clipped_frames"));
  run.driver = read_cpu_column(cpu_path, "opena8dj_driver");
  run.coreaudiod = read_cpu_column(cpu_path, "coreaudiod");
  return run;
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
    if (!std::filesystem::is_regular_file(metrics) || !std::filesystem::is_regular_file(cpu)) {
      continue;
    }
    const auto newest = std::max(std::filesystem::last_write_time(metrics),
                                 std::filesystem::last_write_time(cpu));
    if (!best || newest > best_time || (newest == best_time && dir.string() > best->string())) {
      best = dir;
      best_time = newest;
    }
  }
  return best;
}

bool finite_le(double value, double limit) {
  return std::isfinite(value) && value <= limit;
}

bool finite_ge(double value, double limit) {
  return std::isfinite(value) && value >= limit;
}

bool practical_pass(const RunStats& run) {
  return run.metrics_present && run.cpu_present && finite_ge(run.quality, 0.925) &&
         finite_le(run.mid, 1.45) && finite_le(run.high, 1.355) &&
         finite_le(run.quiet_mid, -32.5) && finite_le(run.lag_jumps, 45.0) &&
         finite_le(run.clipping, 0.0) && run.driver.samples > 0 &&
         finite_le(run.driver.p95, 12.0) && run.coreaudiod.samples > 0 &&
         finite_le(run.coreaudiod.p95, 8.0);
}

bool compare_value(double candidate, Direction direction, double required) {
  if (!std::isfinite(candidate) || !std::isfinite(required)) {
    return false;
  }
  if (direction == Direction::GreaterOrEqual) {
    return candidate >= required;
  }
  return candidate <= required;
}

std::vector<GateResult> fixed_baseline_gates(const RunStats& candidate,
                                             const FixedBaseline& baseline) {
  std::vector<GateResult> gates = {
      {"music_quality_alignment", Direction::GreaterOrEqual, candidate.quality, baseline.quality},
      {"music_snr_floor_db", Direction::GreaterOrEqual, candidate.snr, baseline.snr},
      {"music_mid_residual_ratio", Direction::LessOrEqual, candidate.mid, baseline.mid},
      {"music_high_residual_ratio", Direction::LessOrEqual, candidate.high, baseline.high},
      {"music_quiet_mid_noise_dbfs", Direction::LessOrEqual, candidate.quiet_mid,
       baseline.quiet_mid},
      {"music_lag_jumps_gt_2_frames", Direction::LessOrEqual, candidate.lag_jumps,
       baseline.lag_jumps},
      {"music_click_outliers", Direction::LessOrEqual, candidate.click_outliers,
       baseline.click_outliers},
      {"music_capture_clipped_frames", Direction::LessOrEqual, candidate.clipping,
       baseline.clipping},
      {"driver_cpu_p95", Direction::LessOrEqual, candidate.driver.p95, baseline.driver_p95},
      {"coreaudiod_cpu_p95", Direction::LessOrEqual, candidate.coreaudiod.p95,
       baseline.coreaudiod_p95},
  };
  for (auto& gate : gates) {
    gate.pass = compare_value(gate.candidate, gate.direction, gate.required);
  }
  return gates;
}

std::vector<GateResult> run_to_run_gates(const RunStats& candidate, const RunStats& baseline) {
  std::vector<GateResult> gates = {
      {"music_quality_alignment", Direction::GreaterOrEqual, candidate.quality, baseline.quality},
      {"music_snr_floor_db", Direction::GreaterOrEqual, candidate.snr, baseline.snr},
      {"music_mid_residual_ratio", Direction::LessOrEqual, candidate.mid, baseline.mid},
      {"music_high_residual_ratio", Direction::LessOrEqual, candidate.high, baseline.high},
      {"music_quiet_mid_noise_dbfs", Direction::LessOrEqual, candidate.quiet_mid,
       baseline.quiet_mid},
      {"music_lag_jumps_gt_2_frames", Direction::LessOrEqual, candidate.lag_jumps,
       baseline.lag_jumps},
      {"music_click_outliers", Direction::LessOrEqual, candidate.click_outliers,
       baseline.click_outliers},
      {"music_capture_clipped_frames", Direction::LessOrEqual, candidate.clipping,
       baseline.clipping},
      {"driver_cpu_p95", Direction::LessOrEqual, candidate.driver.p95, baseline.driver.p95},
      {"coreaudiod_cpu_p95", Direction::LessOrEqual, candidate.coreaudiod.p95,
       baseline.coreaudiod.p95},
  };
  for (auto& gate : gates) {
    gate.pass = compare_value(gate.candidate, gate.direction, gate.required);
  }
  return gates;
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

void print_run(const RunStats& run, const std::string& indent) {
  std::cout << indent << "{\n";
  std::cout << indent << "  \"run\": ";
  print_json_string(run_label(run.path));
  std::cout << ",\n";
  std::cout << indent << "  \"path\": ";
  print_json_string(run.path.string());
  std::cout << ",\n";
  std::cout << indent << "  \"metrics_present\": " << (run.metrics_present ? "true" : "false")
            << ",\n";
  std::cout << indent << "  \"cpu_profile_present\": " << (run.cpu_present ? "true" : "false")
            << ",\n";
  std::cout << indent << "  \"reference_wav_present\": "
            << (run.reference_wav_present ? "true" : "false") << ",\n";
  std::cout << indent << "  \"captured_wav_present\": "
            << (run.captured_wav_present ? "true" : "false") << ",\n";
  std::cout << indent << "  \"native_wav_reanalysis\": \""
            << (run.reference_wav_present && run.captured_wav_present ? "AVAILABLE_NOT_YET_USED"
                                                                       : "BLOCKED_MISSING_WAV")
            << "\",\n";
  std::cout << indent << "  \"practical_pass\": " << (practical_pass(run) ? "true" : "false")
            << ",\n";
  std::cout << indent << "  \"quality_alignment_score\": ";
  print_json_number(run.quality);
  std::cout << ",\n";
  std::cout << indent << "  \"snr_floor_db\": ";
  print_json_number(run.snr);
  std::cout << ",\n";
  std::cout << indent << "  \"mid_band_residual_ratio\": ";
  print_json_number(run.mid);
  std::cout << ",\n";
  std::cout << indent << "  \"high_band_residual_ratio\": ";
  print_json_number(run.high);
  std::cout << ",\n";
  std::cout << indent << "  \"quiet_mid_band_dbfs\": ";
  print_json_number(run.quiet_mid);
  std::cout << ",\n";
  std::cout << indent << "  \"lag_jumps_gt_2_frames\": ";
  print_json_number(run.lag_jumps);
  std::cout << ",\n";
  std::cout << indent << "  \"click_outliers\": ";
  print_json_number(run.click_outliers);
  std::cout << ",\n";
  std::cout << indent << "  \"capture_clipped_frames\": ";
  print_json_number(run.clipping);
  std::cout << ",\n";
  std::cout << indent << "  \"driver_cpu_p95\": ";
  print_json_number(run.driver.p95);
  std::cout << ",\n";
  std::cout << indent << "  \"driver_cpu_avg\": ";
  print_json_number(run.driver.avg);
  std::cout << ",\n";
  std::cout << indent << "  \"driver_cpu_max\": ";
  print_json_number(run.driver.max);
  std::cout << ",\n";
  std::cout << indent << "  \"coreaudiod_cpu_p95\": ";
  print_json_number(run.coreaudiod.p95);
  std::cout << ",\n";
  std::cout << indent << "  \"cpu_samples\": " << run.driver.samples << "\n";
  std::cout << indent << "}";
}

void print_gate(const GateResult& gate, bool last) {
  std::cout << "    {\n";
  std::cout << "      \"name\": ";
  print_json_string(gate.name);
  std::cout << ",\n";
  std::cout << "      \"result\": \"" << (gate.pass ? "PASS" : "FAIL") << "\",\n";
  std::cout << "      \"candidate\": ";
  print_json_number(gate.candidate);
  std::cout << ",\n";
  std::cout << "      \"required\": ";
  print_json_number(gate.required);
  std::cout << ",\n";
  std::cout << "      \"direction\": \""
            << (gate.direction == Direction::GreaterOrEqual ? ">=" : "<=") << "\"\n";
  std::cout << "    }" << (last ? "\n" : ",\n");
}

bool all_pass(const std::vector<GateResult>& gates) {
  return std::all_of(gates.begin(), gates.end(), [](const GateResult& gate) {
    return gate.pass;
  });
}

void print_fixed_baseline(const FixedBaseline& baseline) {
  std::cout << "  \"baseline\": {\n"
            << "    \"kind\": \"mainline_reference\",\n"
            << "    \"cpu_digital_stability\": \"0.3.135\",\n"
            << "    \"functional_timecode_topology\": \"0.3.25\",\n"
            << "    \"physical_tone_music_floor\": \"0.3.24\",\n"
            << "    \"quality_alignment_score_min\": " << baseline.quality << ",\n"
            << "    \"snr_floor_db_min\": " << baseline.snr << ",\n"
            << "    \"mid_band_residual_ratio_max\": " << baseline.mid << ",\n"
            << "    \"high_band_residual_ratio_max\": " << baseline.high << ",\n"
            << "    \"quiet_mid_band_dbfs_max\": " << baseline.quiet_mid << ",\n"
            << "    \"lag_jumps_gt_2_frames_max\": " << baseline.lag_jumps << ",\n"
            << "    \"click_outliers_max\": " << baseline.click_outliers << ",\n"
            << "    \"capture_clipped_frames_max\": " << baseline.clipping << ",\n"
            << "    \"driver_cpu_p95_max\": " << baseline.driver_p95 << ",\n"
            << "    \"coreaudiod_cpu_p95_max\": " << baseline.coreaudiod_p95 << "\n"
            << "  },\n";
}

void print_superiority_report(const RunStats& candidate,
                              const std::vector<GateResult>& gates,
                              const std::string& mode,
                              const std::optional<RunStats>& baseline_run = std::nullopt) {
  const bool candidate_evidence_present = candidate.metrics_present && candidate.cpu_present;
  const bool result = candidate_evidence_present && all_pass(gates);
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n";
  std::cout << "  \"schema\": \"opena8djcpp.physical-run-compare.v2\",\n";
  std::cout << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n";
  std::cout << "  \"mode\": ";
  print_json_string(mode);
  std::cout << ",\n";
  std::cout << "  \"result\": \"" << (result ? "PASS" : "FAIL") << "\",\n";
  std::cout << "  \"branch_promotion_supported\": " << (result ? "true" : "false") << ",\n";
  std::cout << "  \"candidate_evidence_present\": "
            << (candidate_evidence_present ? "true" : "false") << ",\n";
  std::cout << "  \"analysis_source\": \"metrics_json_and_cpu_profile_tsv\",\n";
  std::cout << "  \"native_wav_reanalysis_required_before_promotion\": true,\n";
  if (baseline_run) {
    std::cout << "  \"baseline\": ";
    print_run(*baseline_run, "  ");
    std::cout << ",\n";
  } else {
    print_fixed_baseline(FixedBaseline{});
  }
  std::cout << "  \"candidate\": ";
  print_run(candidate, "  ");
  std::cout << ",\n";
  std::cout << "  \"gates\": [\n";
  for (std::size_t i = 0; i < gates.size(); ++i) {
    print_gate(gates[i], i + 1 == gates.size());
  }
  std::cout << "  ],\n";
  std::cout << "  \"readiness_claim\": \"";
  std::cout << (result ? "OBJECTIVE_PRODUCT_EVIDENCE_PASSES_THIS_COMPARATOR"
                       : "BLOCKED_NOT_BETTER_THAN_MAINLINE_REFERENCE");
  std::cout << "\"\n";
  std::cout << "}\n";
}

void print_summary_report(const std::vector<RunStats>& runs) {
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n";
  std::cout << "  \"schema\": \"opena8djcpp.physical-run-compare.v2\",\n";
  std::cout << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n";
  std::cout << "  \"mode\": \"summary\",\n";
  std::cout << "  \"result\": \"PASS\",\n";
  std::cout << "  \"runs\": [\n";
  for (std::size_t i = 0; i < runs.size(); ++i) {
    print_run(runs[i], "    ");
    std::cout << (i + 1 == runs.size() ? "\n" : ",\n");
  }
  std::cout << "  ]\n";
  std::cout << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  std::optional<std::filesystem::path> candidate_path;
  std::optional<std::filesystem::path> baseline_path;
  bool compare_to_mainline_reference = true;
  std::vector<std::filesystem::path> positional;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--candidate" && i + 1 < argc) {
      candidate_path = argv[++i];
    } else if (arg == "--baseline" && i + 1 < argc) {
      baseline_path = argv[++i];
      compare_to_mainline_reference = false;
    } else if (arg == "--mainline-reference") {
      compare_to_mainline_reference = true;
      baseline_path = std::nullopt;
    } else if (arg == "--latest-candidate") {
      candidate_path = latest_complete_soundcheck(root);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "usage: physical-run-compare [--latest-candidate|--candidate RUN_DIR] "
                   "[--mainline-reference|--baseline RUN_DIR]\n"
                   "       physical-run-compare RUN_DIR [RUN_DIR ...]\n";
      return 2;
    } else {
      positional.emplace_back(arg);
    }
  }

  if (!positional.empty()) {
    std::vector<RunStats> runs;
    runs.reserve(positional.size());
    for (const auto& path : positional) {
      runs.push_back(read_run(path));
    }
    print_summary_report(runs);
    return 0;
  }

  if (!candidate_path) {
    candidate_path = latest_complete_soundcheck(root);
  }

  if (!candidate_path) {
    RunStats empty_candidate;
    empty_candidate.path = root / "local-analysis/soundcheck";
    print_superiority_report(empty_candidate, fixed_baseline_gates(empty_candidate, FixedBaseline{}),
                             "latest_candidate_vs_mainline_reference");
    return 0;
  }

  const RunStats candidate = read_run(*candidate_path);
  if (compare_to_mainline_reference) {
    print_superiority_report(candidate, fixed_baseline_gates(candidate, FixedBaseline{}),
                             "candidate_vs_mainline_reference");
    return 0;
  }

  if (!baseline_path) {
    std::cerr << "--baseline requires a RUN_DIR\n";
    return 2;
  }
  const RunStats baseline = read_run(*baseline_path);
  print_superiority_report(candidate, run_to_run_gates(candidate, baseline),
                           "candidate_vs_baseline_run", baseline);
  return 0;
}
