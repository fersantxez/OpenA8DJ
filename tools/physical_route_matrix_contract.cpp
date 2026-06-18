#include "evidence_json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kExpectedFloorAmplitudeMin = 0.005;

struct PairRun {
  char pair = '?';
  std::filesystem::path path;
  bool present = false;
  std::string result = "MISSING";
  double expected_floor_amplitude = std::numeric_limits<double>::quiet_NaN();
  double left_expected_max_amplitude = std::numeric_limits<double>::quiet_NaN();
  double right_expected_max_amplitude = std::numeric_limits<double>::quiet_NaN();
  double capture_clipped_frames = std::numeric_limits<double>::quiet_NaN();
  double max_wrong_source_leakage_db = std::numeric_limits<double>::quiet_NaN();
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

double json_number_or_nan(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_number(json, key)
      .value_or(std::numeric_limits<double>::quiet_NaN());
}

double max_expected_amplitude(const PairRun& run) {
  double value = 0.0;
  if (std::isfinite(run.expected_floor_amplitude)) {
    value = std::max(value, run.expected_floor_amplitude);
  }
  if (std::isfinite(run.left_expected_max_amplitude)) {
    value = std::max(value, run.left_expected_max_amplitude);
  }
  if (std::isfinite(run.right_expected_max_amplitude)) {
    value = std::max(value, run.right_expected_max_amplitude);
  }
  return value;
}

PairRun parse_pair_run(char pair, const std::filesystem::path& path) {
  PairRun run;
  run.pair = pair;
  run.path = path;
  run.present = true;
  const auto json = read_file(path);
  run.result = opena8djcpp::evidence_json::json_string(json, "result").value_or("MISSING");
  run.expected_floor_amplitude = json_number_or_nan(json, "expected_floor_amplitude");
  run.left_expected_max_amplitude = json_number_or_nan(json, "left_expected_max_amplitude");
  run.right_expected_max_amplitude = json_number_or_nan(json, "right_expected_max_amplitude");
  run.capture_clipped_frames = json_number_or_nan(json, "capture_clipped_frames");
  run.max_wrong_source_leakage_db = json_number_or_nan(json, "max_wrong_source_leakage_db");
  return run;
}

std::optional<PairRun> latest_pair_run(const std::filesystem::path& matrix_root, char pair) {
  if (!std::filesystem::is_directory(matrix_root)) {
    return std::nullopt;
  }

  std::vector<std::filesystem::path> preferred;
  std::vector<std::filesystem::path> fallback;
  const std::string pair_token = std::string("pair") + pair;
  const std::string final_token = std::string("final-matrix-pair") + pair;

  for (const auto& entry : std::filesystem::directory_iterator(matrix_root)) {
    if (!entry.is_directory()) {
      continue;
    }
    const auto tone = entry.path() / "tone-matrix.json";
    if (!std::filesystem::is_regular_file(tone)) {
      continue;
    }
    const std::string name = entry.path().filename().string();
    if (name.find(final_token) != std::string::npos) {
      preferred.push_back(tone);
    } else if (name.find(pair_token) != std::string::npos) {
      fallback.push_back(tone);
    }
  }

  auto& candidates = preferred.empty() ? fallback : preferred;
  if (candidates.empty()) {
    return std::nullopt;
  }
  const auto latest = *std::max_element(
      candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return std::filesystem::last_write_time(left) < std::filesystem::last_write_time(right);
      });
  return parse_pair_run(pair, latest);
}

void print_json_string(const std::string& value) {
  std::cout << '"';
  for (const char c : value) {
    switch (c) {
      case '\\':
        std::cout << "\\\\";
        break;
      case '"':
        std::cout << "\\\"";
        break;
      case '\n':
        std::cout << "\\n";
        break;
      case '\r':
        std::cout << "\\r";
        break;
      case '\t':
        std::cout << "\\t";
        break;
      default:
        std::cout << c;
        break;
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

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto matrix_root = root / "local-analysis/channel-matrix";
  std::array<PairRun, 4> runs{};
  const std::array<char, 4> pairs{'A', 'B', 'C', 'D'};

  bool all_present = true;
  for (std::size_t index = 0; index < pairs.size(); ++index) {
    const auto run = latest_pair_run(matrix_root, pairs[index]);
    if (run) {
      runs[index] = *run;
    } else {
      runs[index].pair = pairs[index];
      all_present = false;
    }
  }

  const bool all_fail = std::all_of(runs.begin(), runs.end(), [](const PairRun& run) {
    return run.present && run.result == "FAIL";
  });
  const bool any_pass = std::any_of(runs.begin(), runs.end(), [](const PairRun& run) {
    return run.present && run.result == "PASS";
  });
  const bool no_clipping = std::all_of(runs.begin(), runs.end(), [](const PairRun& run) {
    return run.present && std::isfinite(run.capture_clipped_frames) &&
           run.capture_clipped_frames <= 0.0;
  });
  const bool all_expected_low = std::all_of(runs.begin(), runs.end(), [](const PairRun& run) {
    return run.present && max_expected_amplitude(run) < kExpectedFloorAmplitudeMin;
  });
  const bool all_pairs_no_useful_correlated_capture =
      all_present && all_fail && no_clipping && all_expected_low;
  const bool contract_pass =
      all_pairs_no_useful_correlated_capture || (all_present && any_pass && no_clipping);
  const std::string classification =
      all_pairs_no_useful_correlated_capture
          ? "all_audio8_pairs_no_useful_correlated_capture"
          : (any_pass ? "at_least_one_audio8_pair_has_correlated_capture"
                      : "physical_matrix_evidence_missing_or_ambiguous");

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.physical-route-matrix-contract.v1\",\n"
            << "  \"safety\": \"offline_existing_channel_matrix_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (contract_pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means latest A/B/C/D physical matrix evidence is explicitly classified; it is not product readiness\",\n"
            << "  \"classification\": ";
  print_json_string(classification);
  std::cout << ",\n"
            << "  \"expected_floor_amplitude_min\": " << kExpectedFloorAmplitudeMin << ",\n"
            << "  \"all_pairs_present\": " << (all_present ? "true" : "false") << ",\n"
            << "  \"all_pairs_no_useful_correlated_capture\": "
            << (all_pairs_no_useful_correlated_capture ? "true" : "false") << ",\n"
            << "  \"any_pair_has_correlated_capture\": " << (any_pass ? "true" : "false")
            << ",\n"
            << "  \"all_expected_amplitudes_below_threshold\": "
            << (all_expected_low ? "true" : "false") << ",\n"
            << "  \"no_clipping\": " << (no_clipping ? "true" : "false") << ",\n"
            << "  \"pairs\": [\n";
  for (std::size_t index = 0; index < runs.size(); ++index) {
    const auto& run = runs[index];
    std::cout << "    {\n"
              << "      \"pair\": \"" << run.pair << "\",\n"
              << "      \"present\": " << (run.present ? "true" : "false") << ",\n"
              << "      \"path\": ";
    print_json_string(run.present ? std::filesystem::relative(run.path, root).string() : "");
    std::cout << ",\n"
              << "      \"result\": ";
    print_json_string(run.result);
    std::cout << ",\n"
              << "      \"expected_floor_amplitude\": ";
    print_json_number(run.expected_floor_amplitude);
    std::cout << ",\n"
              << "      \"left_expected_max_amplitude\": ";
    print_json_number(run.left_expected_max_amplitude);
    std::cout << ",\n"
              << "      \"right_expected_max_amplitude\": ";
    print_json_number(run.right_expected_max_amplitude);
    std::cout << ",\n"
              << "      \"max_expected_amplitude\": ";
    print_json_number(max_expected_amplitude(run));
    std::cout << ",\n"
              << "      \"capture_clipped_frames\": ";
    print_json_number(run.capture_clipped_frames);
    std::cout << ",\n"
              << "      \"max_wrong_source_leakage_db\": ";
    print_json_number(run.max_wrong_source_leakage_db);
    std::cout << "\n"
              << "    }" << (index + 1U == runs.size() ? "\n" : ",\n");
  }
  std::cout << "  ],\n"
            << "  \"human_product_test_allowed\": false,\n"
            << "  \"product_claim_allowed\": false,\n"
            << "  \"branch_promotion_allowed\": false,\n"
            << "  \"next_lock_gated_action\": \"VALIDATE_IRIG_CAPTURE_WITH_NON_AUDIO8_WIRED_SOURCE_BEFORE_MORE_DRIVER_TUNING\"\n"
            << "}\n";

  return contract_pass ? 0 : 1;
}
