#include "evidence_json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Thresholds {
  double max_alignment_lag_delta_frames = 8.0;
  double max_scalar_snr_delta_db = 0.5;
  double max_mid_coherence_delta = 0.15;
  double max_lti_snr_delta_db = 1.0;
  double max_lti_mid_ratio_delta = 0.25;
  double max_lti_high_ratio_delta = 0.50;
};

struct RunComparison {
  std::string label;
  std::filesystem::path run_dir;
  bool python_present = false;
  bool cpp_executed = false;
  double alignment_lag_delta_frames = 0.0;
  double min_mid_coherence_delta = 0.0;
  double scalar_snr_delta_db = 0.0;
  double lti_snr_delta_db = 0.0;
  double lti_mid_ratio_delta = 0.0;
  double lti_high_ratio_delta = 0.0;
  bool parity_pass = false;
};

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
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

double first_number(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_number(json, key)
      .value_or(std::numeric_limits<double>::quiet_NaN());
}

double abs_delta(double a, double b) {
  if (!std::isfinite(a) || !std::isfinite(b)) {
    return std::numeric_limits<double>::infinity();
  }
  return std::abs(a - b);
}

double max_pairwise_delta(std::string_view python, std::string_view cpp, std::string_view key) {
  const auto py = json_numbers(python, key);
  const auto cc = json_numbers(cpp, key);
  if (py.empty() || cc.empty() || py.size() != cc.size()) {
    return std::numeric_limits<double>::infinity();
  }
  double delta = 0.0;
  for (std::size_t index = 0; index < py.size(); ++index) {
    delta = std::max(delta, abs_delta(py[index], cc[index]));
  }
  return delta;
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
  const auto python_json = read_file(run_dir / "lti-transfer-quality.json");
  out.python_present = !python_json.empty();
  out.cpp_executed = out.python_present && run_cpp_analyzer(analyzer, run_dir, tmp_json);
  const auto cpp_json = out.cpp_executed ? read_file(tmp_json) : std::string{};
  if (!out.cpp_executed || cpp_json.empty()) {
    return out;
  }

  out.alignment_lag_delta_frames =
      abs_delta(first_number(python_json, "alignment_lag"), first_number(cpp_json, "alignment_lag"));
  out.min_mid_coherence_delta = abs_delta(first_number(python_json, "min_mid_coherence"),
                                          first_number(cpp_json, "min_mid_coherence"));
  out.scalar_snr_delta_db = max_pairwise_delta(python_json, cpp_json, "scalar_snr_db");
  out.lti_snr_delta_db = max_pairwise_delta(python_json, cpp_json, "lti_snr_db");
  out.lti_mid_ratio_delta = max_pairwise_delta(python_json, cpp_json, "lti_mid_ratio");
  out.lti_high_ratio_delta = max_pairwise_delta(python_json, cpp_json, "lti_high_ratio");
  out.parity_pass =
      out.alignment_lag_delta_frames <= thresholds.max_alignment_lag_delta_frames &&
      out.scalar_snr_delta_db <= thresholds.max_scalar_snr_delta_db &&
      out.min_mid_coherence_delta <= thresholds.max_mid_coherence_delta &&
      out.lti_snr_delta_db <= thresholds.max_lti_snr_delta_db &&
      out.lti_mid_ratio_delta <= thresholds.max_lti_mid_ratio_delta &&
      out.lti_high_ratio_delta <= thresholds.max_lti_high_ratio_delta;
  return out;
}

void print_number(const char* key, double value, const char* indent) {
  std::cout << indent << "\"" << key << "\": ";
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
            << "      \"cpp_executed\": " << (run.cpp_executed ? "true" : "false") << ",\n";
  print_number("alignment_lag_delta_frames", run.alignment_lag_delta_frames, "      ");
  std::cout << ",\n";
  print_number("min_mid_coherence_delta", run.min_mid_coherence_delta, "      ");
  std::cout << ",\n";
  print_number("scalar_snr_delta_db", run.scalar_snr_delta_db, "      ");
  std::cout << ",\n";
  print_number("lti_snr_delta_db", run.lti_snr_delta_db, "      ");
  std::cout << ",\n";
  print_number("lti_mid_ratio_delta", run.lti_mid_ratio_delta, "      ");
  std::cout << ",\n";
  print_number("lti_high_ratio_delta", run.lti_high_ratio_delta, "      ");
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
  const auto analyzer = binary_dir / "opena8djcpp_lti_transfer_quality";
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
                                      tmp_root / ("opena8djcpp-lti-parity-" +
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
    blockers.push_back("lti_python_or_cpp_evidence_missing");
  }
  if (!parity_pass) {
    blockers.push_back("cpp_lti_transfer_quality_not_yet_numerically_equivalent_to_python_oracle");
  }

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.lti-transfer-quality-parity-gate.v1\",\n"
            << "  \"safety\": \"offline_saved_wav_analysis_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"PASS\",\n"
            << "  \"meaning\": \"PASS means the parity guard ran; lti_parity_pass controls whether C++ LTI can replace Python for claims\",\n"
            << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
            << "  \"lti_parity_pass\": " << (parity_pass ? "true" : "false") << ",\n"
            << "  \"cpp_lti_claim_allowed\": " << (parity_pass ? "true" : "false") << ",\n"
            << "  \"thresholds\": {\n";
  print_number("max_alignment_lag_delta_frames", thresholds.max_alignment_lag_delta_frames,
               "    ");
  std::cout << ",\n";
  print_number("max_scalar_snr_delta_db", thresholds.max_scalar_snr_delta_db, "    ");
  std::cout << ",\n";
  print_number("max_mid_coherence_delta", thresholds.max_mid_coherence_delta, "    ");
  std::cout << ",\n";
  print_number("max_lti_snr_delta_db", thresholds.max_lti_snr_delta_db, "    ");
  std::cout << ",\n";
  print_number("max_lti_mid_ratio_delta", thresholds.max_lti_mid_ratio_delta, "    ");
  std::cout << ",\n";
  print_number("max_lti_high_ratio_delta", thresholds.max_lti_high_ratio_delta, "    ");
  std::cout << "\n  },\n"
            << "  \"runs\": [\n";
  for (std::size_t index = 0; index < comparisons.size(); ++index) {
    print_run(comparisons[index], index + 1U < comparisons.size());
  }
  std::cout << "  ],\n"
            << "  \"blockers\": [";
  for (std::size_t index = 0; index < blockers.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << blockers[index] << "\"";
  }
  std::cout << "],\n";
  if (parity_pass) {
    std::cout << "  \"blocked_claim\": \"CPP_LTI_TRANSFER_QUALITY_PARITY_PASSED_FOR_SAVED_EVIDENCE_ONLY_NO_PRODUCT_CLAIM\"\n";
  } else {
    std::cout << "  \"blocked_claim\": \"NO_CPP_LTI_TRANSFER_QUALITY_CLAIM_UNTIL_PARITY_WITH_PYTHON_ORACLE_PASSES_ON_SAVED_PHYSICAL_EVIDENCE\"\n";
  }
  std::cout << "}\n";

  return 0;
}
