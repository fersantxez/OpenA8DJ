#include "evidence_json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Thresholds {
  double max_scalar_improvement_delta_db = 0.75;
  double max_matrix_improvement_delta_db = 0.75;
  double max_delay_p95_delta_frames = 4.0;
  double max_delay_median_delta_frames = 4.0;
  double max_window_median_delta_db = 1.5;
  double max_classification_mismatch = 0.0;
};

struct RunComparison {
  std::string label;
  std::filesystem::path run_dir;
  bool python_present = false;
  bool cpp_executed = false;
  bool classification_match = false;
  double scalar_improvement_delta_db = std::numeric_limits<double>::infinity();
  double matrix_improvement_delta_db = std::numeric_limits<double>::infinity();
  double delay_p95_delta_frames = std::numeric_limits<double>::infinity();
  double delay_median_delta_frames = std::numeric_limits<double>::infinity();
  double window_median_delta_db = std::numeric_limits<double>::infinity();
  bool parity_pass = false;
};

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
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

std::string shell_quote(const std::filesystem::path& path) {
  std::string out = "'";
  for (const char c : path.string()) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

double abs_delta(double a, double b) {
  if (!std::isfinite(a) || !std::isfinite(b)) {
    return std::numeric_limits<double>::infinity();
  }
  return std::abs(a - b);
}

std::string object_for_key(std::string_view json, std::string_view key) {
  const auto obj = opena8djcpp::evidence_json::json_object(json, key);
  return obj ? std::string(*obj) : std::string{};
}

double number_in_object(std::string_view json, std::string_view object_key, std::string_view key) {
  const auto object = object_for_key(json, object_key);
  if (object.empty()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return opena8djcpp::evidence_json::json_number(object, key)
      .value_or(std::numeric_limits<double>::quiet_NaN());
}

std::string first_classification(std::string_view json) {
  return opena8djcpp::evidence_json::json_string(json, "classification").value_or("");
}

bool run_cpp_analyzer(const std::filesystem::path& analyzer,
                      const std::filesystem::path& run_dir,
                      const std::filesystem::path& json_out) {
  const std::string command = shell_quote(analyzer) + " --json-out " + shell_quote(json_out) +
                              " " + shell_quote(run_dir) + " >/dev/null 2>/dev/null";
  return std::system(command.c_str()) == 0;
}

RunComparison compare_run(const std::string& label,
                          const std::filesystem::path& analyzer,
                          const std::filesystem::path& run_dir,
                          const std::filesystem::path& tmp_json,
                          const Thresholds& thresholds) {
  RunComparison out{};
  out.label = label;
  out.run_dir = run_dir;
  const auto python_json = read_file(run_dir / "fractional-time-warp.json");
  out.python_present = !python_json.empty();
  out.cpp_executed = out.python_present && run_cpp_analyzer(analyzer, run_dir, tmp_json);
  const auto cpp_json = out.cpp_executed ? read_file(tmp_json) : std::string{};
  if (!out.cpp_executed || cpp_json.empty()) {
    return out;
  }

  out.scalar_improvement_delta_db =
      abs_delta(number_in_object(python_json, "global_scalar", "improvement_db"),
                number_in_object(cpp_json, "global_scalar", "improvement_db"));
  out.matrix_improvement_delta_db =
      abs_delta(number_in_object(python_json, "global_matrix", "improvement_db"),
                number_in_object(cpp_json, "global_matrix", "improvement_db"));
  out.delay_p95_delta_frames =
      abs_delta(number_in_object(python_json, "delay", "p95_abs_frames"),
                number_in_object(cpp_json, "delay", "p95_abs_frames"));
  out.delay_median_delta_frames =
      abs_delta(number_in_object(python_json, "delay", "median_frames"),
                number_in_object(cpp_json, "delay", "median_frames"));
  out.window_median_delta_db =
      abs_delta(number_in_object(python_json, "window_snr", "median_delta_db"),
                number_in_object(cpp_json, "window_snr", "median_delta_db"));
  out.classification_match = first_classification(python_json) == first_classification(cpp_json);
  out.parity_pass =
      out.classification_match &&
      out.scalar_improvement_delta_db <= thresholds.max_scalar_improvement_delta_db &&
      out.matrix_improvement_delta_db <= thresholds.max_matrix_improvement_delta_db &&
      out.delay_p95_delta_frames <= thresholds.max_delay_p95_delta_frames &&
      out.delay_median_delta_frames <= thresholds.max_delay_median_delta_frames &&
      out.window_median_delta_db <= thresholds.max_window_median_delta_db;
  return out;
}

void print_number(const char* key, double value) {
  std::cout << "      \"" << key << "\": ";
  if (std::isfinite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

void print_run(const RunComparison& run, bool trailing) {
  std::cout << "    {\n"
            << "      \"label\": \"" << run.label << "\",\n"
            << "      \"run_dir\": \"" << run.run_dir.string() << "\",\n"
            << "      \"python_present\": " << (run.python_present ? "true" : "false") << ",\n"
            << "      \"cpp_executed\": " << (run.cpp_executed ? "true" : "false") << ",\n"
            << "      \"classification_match\": "
            << (run.classification_match ? "true" : "false") << ",\n";
  print_number("scalar_improvement_delta_db", run.scalar_improvement_delta_db);
  std::cout << ",\n";
  print_number("matrix_improvement_delta_db", run.matrix_improvement_delta_db);
  std::cout << ",\n";
  print_number("delay_p95_delta_frames", run.delay_p95_delta_frames);
  std::cout << ",\n";
  print_number("delay_median_delta_frames", run.delay_median_delta_frames);
  std::cout << ",\n";
  print_number("window_median_delta_db", run.window_median_delta_db);
  std::cout << ",\n"
            << "      \"parity_pass\": " << (run.parity_pass ? "true" : "false") << "\n"
            << "    }";
  if (trailing) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto binary_dir = std::filesystem::absolute(argv[0]).parent_path();
  const auto analyzer = binary_dir / "opena8djcpp_fractional_time_warp";
  const auto tmp_root = std::filesystem::temp_directory_path();
  const Thresholds thresholds{};

  const std::array<std::pair<std::string, std::filesystem::path>, 2> runs{{
      {"candidate",
       root /
           "local-analysis/physical-superiority-window/20260617T212050Z-mainline-vs-cpp-raw-reuse-irig/cpp-soundcheck"},
      {"baseline",
       root /
           "local-analysis/physical-superiority-window/20260617T212050Z-mainline-vs-cpp-raw-reuse-irig/mainline-soundcheck"},
  }};

  std::vector<RunComparison> comparisons;
  comparisons.reserve(runs.size());
  for (std::size_t index = 0; index < runs.size(); ++index) {
    comparisons.push_back(compare_run(runs[index].first, analyzer, runs[index].second,
                                      tmp_root / ("opena8djcpp-fractional-time-warp-parity-" +
                                                  std::to_string(index) + ".json"),
                                      thresholds));
  }

  const bool evidence_present = std::all_of(comparisons.begin(), comparisons.end(),
                                            [](const RunComparison& run) {
                                              return run.python_present && run.cpp_executed;
                                            });
  const bool parity_pass =
      evidence_present && std::all_of(comparisons.begin(), comparisons.end(),
                                      [](const RunComparison& run) { return run.parity_pass; });

  std::vector<std::string> blockers;
  if (!evidence_present) {
    blockers.push_back("fractional_time_warp_python_or_cpp_evidence_missing");
  }
  if (!parity_pass) {
    blockers.push_back("cpp_fractional_time_warp_not_yet_numerically_equivalent_to_python_oracle");
  }

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.fractional-time-warp-parity-gate.v1\",\n"
            << "  \"safety\": \"offline_saved_wav_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"PASS\",\n"
            << "  \"meaning\": \"PASS means the parity guard ran; timewarp_parity_pass controls whether C++ time-warp can replace Python for claims\",\n"
            << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
            << "  \"timewarp_parity_pass\": " << (parity_pass ? "true" : "false") << ",\n"
            << "  \"cpp_timewarp_claim_allowed\": " << (parity_pass ? "true" : "false")
            << ",\n"
            << "  \"thresholds\": {\n"
            << "    \"max_scalar_improvement_delta_db\": "
            << thresholds.max_scalar_improvement_delta_db << ",\n"
            << "    \"max_matrix_improvement_delta_db\": "
            << thresholds.max_matrix_improvement_delta_db << ",\n"
            << "    \"max_delay_p95_delta_frames\": " << thresholds.max_delay_p95_delta_frames
            << ",\n"
            << "    \"max_delay_median_delta_frames\": "
            << thresholds.max_delay_median_delta_frames << ",\n"
            << "    \"max_window_median_delta_db\": " << thresholds.max_window_median_delta_db
            << "\n"
            << "  },\n"
            << "  \"comparisons\": [\n";
  for (std::size_t index = 0; index < comparisons.size(); ++index) {
    print_run(comparisons[index], index + 1U != comparisons.size());
  }
  std::cout << "  ],\n"
            << "  \"blockers\": [";
  for (std::size_t index = 0; index < blockers.size(); ++index) {
    std::cout << (index == 0U ? "" : ", ") << "\"" << blockers[index] << "\"";
  }
  std::cout << "],\n"
            << "  \"blocked_claim\": \""
            << (parity_pass
                    ? "CPP_FRACTIONAL_TIME_WARP_PARITY_PASSED_FOR_SAVED_EVIDENCE_ONLY_NO_PRODUCT_CLAIM"
                    : "NO_CPP_FRACTIONAL_TIME_WARP_CLAIM_UNTIL_PARITY_WITH_PYTHON_ORACLE_PASSES_ON_SAVED_PHYSICAL_EVIDENCE")
            << "\"\n"
            << "}\n";
  return 0;
}
