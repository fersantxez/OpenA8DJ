#include "evidence_json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Thresholds {
  double max_top_abs_correlation_delta = 0.03;
  double max_residual_median_delta = 0.005;
  double max_lag_jump_p95_delta_frames = 4.0;
  double max_snr_median_delta_db = 0.25;
};

struct Comparison {
  bool python_present = false;
  bool cpp_executed = false;
  std::uint32_t python_run_count = 0;
  std::uint32_t cpp_run_count = 0;
  double max_top_abs_correlation_delta = std::numeric_limits<double>::infinity();
  double max_residual_median_delta = std::numeric_limits<double>::infinity();
  double max_lag_jump_p95_delta_frames = std::numeric_limits<double>::infinity();
  double max_snr_median_delta_db = std::numeric_limits<double>::infinity();
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

std::vector<double> json_numbers(std::string_view json, std::string_view key) {
  std::vector<double> values;
  std::size_t from = 0;
  while (from < json.size()) {
    const auto start = opena8djcpp::evidence_json::find_value_start(json, key, from);
    if (!start) {
      break;
    }
    std::size_t end = *start;
    while (end < json.size()) {
      const char c = json[end];
      if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' ||
            c == '.' || c == 'e' || c == 'E')) {
        break;
      }
      ++end;
    }
    if (end > *start) {
      try {
        const double value = std::stod(std::string(json.substr(*start, end - *start)));
        if (std::isfinite(value)) {
          values.push_back(value);
        }
      } catch (const std::exception&) {
      }
    }
    from = end + 1U;
  }
  return values;
}

std::vector<std::string> json_strings(std::string_view json, std::string_view key) {
  std::vector<std::string> values;
  std::size_t from = 0;
  while (from < json.size()) {
    const auto start = opena8djcpp::evidence_json::find_value_start(json, key, from);
    if (!start) {
      break;
    }
    const auto value = opena8djcpp::evidence_json::parse_string_at(json, *start);
    if (value) {
      values.push_back(*value);
    }
    from = start.value() + 1U;
  }
  return values;
}

double max_pairwise_delta(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.empty() || b.empty() || a.size() != b.size()) {
    return std::numeric_limits<double>::infinity();
  }
  double out = 0.0;
  for (std::size_t index = 0; index < a.size(); ++index) {
    out = std::max(out, std::abs(a[index] - b[index]));
  }
  return out;
}

bool run_cpp_analyzer(const std::filesystem::path& analyzer,
                      const std::vector<std::filesystem::path>& run_dirs,
                      const std::filesystem::path& json_out) {
  std::string command = shell_quote(analyzer) + " --json-out " + shell_quote(json_out);
  for (const auto& run_dir : run_dirs) {
    command += " " + shell_quote(run_dir);
  }
  command += " >/dev/null 2>/dev/null";
  return std::system(command.c_str()) == 0;
}

Comparison compare(const std::filesystem::path& analyzer,
                   const std::filesystem::path& python_json_path,
                   const std::filesystem::path& tmp_json,
                   const Thresholds& thresholds) {
  Comparison out{};
  const auto python = read_file(python_json_path);
  out.python_present = !python.empty();
  if (!out.python_present) {
    return out;
  }
  const auto run_dir_strings = json_strings(python, "run_dir");
  std::vector<std::filesystem::path> run_dirs;
  run_dirs.reserve(run_dir_strings.size());
  for (const auto& run_dir : run_dir_strings) {
    run_dirs.emplace_back(run_dir);
  }
  out.cpp_executed = run_cpp_analyzer(analyzer, run_dirs, tmp_json);
  const auto cpp = out.cpp_executed ? read_file(tmp_json) : std::string{};
  if (cpp.empty()) {
    return out;
  }

  out.python_run_count = static_cast<std::uint32_t>(run_dir_strings.size());
  out.cpp_run_count = static_cast<std::uint32_t>(json_strings(cpp, "run_dir").size());
  out.max_top_abs_correlation_delta =
      max_pairwise_delta(json_numbers(python, "abs_correlation"), json_numbers(cpp, "abs_correlation"));
  out.max_residual_median_delta =
      max_pairwise_delta(json_numbers(python, "residual_rms_median"),
                         json_numbers(cpp, "residual_rms_median"));
  out.max_lag_jump_p95_delta_frames =
      max_pairwise_delta(json_numbers(python, "lag_jump_p95_frames"),
                         json_numbers(cpp, "lag_jump_p95_frames"));
  out.max_snr_median_delta_db =
      max_pairwise_delta(json_numbers(python, "scalar_snr_db_median"),
                         json_numbers(cpp, "scalar_snr_db_median"));
  out.parity_pass = out.python_run_count == out.cpp_run_count && out.cpp_run_count > 0U &&
                    out.max_top_abs_correlation_delta <=
                        thresholds.max_top_abs_correlation_delta &&
                    out.max_residual_median_delta <= thresholds.max_residual_median_delta &&
                    out.max_lag_jump_p95_delta_frames <=
                        thresholds.max_lag_jump_p95_delta_frames &&
                    out.max_snr_median_delta_db <= thresholds.max_snr_median_delta_db;
  return out;
}

void print_number(const char* key, double value) {
  std::cout << "  \"" << key << "\": ";
  if (std::isfinite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
  std::cout << ",\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto analyzer =
      std::filesystem::absolute(argv[0]).parent_path() / "opena8djcpp_runtime_discontinuity_analysis";
  const auto python_json =
      root / "local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/runtime-discontinuities.json";
  const auto tmp_json =
      std::filesystem::temp_directory_path() / "opena8djcpp-runtime-discontinuity-parity.json";
  const Thresholds thresholds{};
  const auto comparison = compare(analyzer, python_json, tmp_json, thresholds);

  std::vector<std::string> blockers;
  if (!comparison.python_present || !comparison.cpp_executed) {
    blockers.push_back("runtime_discontinuity_python_or_cpp_evidence_missing");
  }
  if (!comparison.parity_pass) {
    blockers.push_back("cpp_runtime_discontinuity_not_yet_numerically_equivalent_to_python_oracle");
  }

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.runtime-discontinuity-parity-gate.v1\",\n"
            << "  \"safety\": \"offline_saved_wav_tsv_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"PASS\",\n"
            << "  \"meaning\": \"PASS means the parity guard ran; runtime_discontinuity_parity_pass controls whether C++ runtime-discontinuity metrics can replace Python for claims\",\n"
            << "  \"python_evidence\": \"" << python_json.string() << "\",\n"
            << "  \"evidence_present\": " << (comparison.python_present ? "true" : "false") << ",\n"
            << "  \"cpp_executed\": " << (comparison.cpp_executed ? "true" : "false") << ",\n"
            << "  \"runtime_discontinuity_parity_pass\": "
            << (comparison.parity_pass ? "true" : "false") << ",\n"
            << "  \"cpp_runtime_discontinuity_claim_allowed\": "
            << (comparison.parity_pass ? "true" : "false") << ",\n"
            << "  \"python_run_count\": " << comparison.python_run_count << ",\n"
            << "  \"cpp_run_count\": " << comparison.cpp_run_count << ",\n";
  print_number("max_top_abs_correlation_delta", comparison.max_top_abs_correlation_delta);
  print_number("max_residual_median_delta", comparison.max_residual_median_delta);
  print_number("max_lag_jump_p95_delta_frames", comparison.max_lag_jump_p95_delta_frames);
  print_number("max_snr_median_delta_db", comparison.max_snr_median_delta_db);
  std::cout << "  \"blockers\": [";
  for (std::size_t index = 0; index < blockers.size(); ++index) {
    std::cout << (index == 0U ? "" : ", ") << "\"" << blockers[index] << "\"";
  }
  std::cout << "],\n"
            << "  \"blocked_claim\": \""
            << (comparison.parity_pass
                    ? "CPP_RUNTIME_DISCONTINUITY_PARITY_PASSED_FOR_SAVED_EVIDENCE_ONLY_NO_PRODUCT_CLAIM"
                    : "NO_CPP_RUNTIME_DISCONTINUITY_CLAIM_UNTIL_PARITY_WITH_PYTHON_ORACLE_PASSES_ON_SAVED_PHYSICAL_EVIDENCE")
            << "\"\n"
            << "}\n";
  return 0;
}
