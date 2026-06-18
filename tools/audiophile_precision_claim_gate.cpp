#include "evidence_json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Thresholds {
  double min_mid_coherence = 0.80;
  double min_high_coherence = 0.65;
  double min_lti_snr_delta_db = 6.0;
  double max_lti_mid_ratio = 1.15;
  double max_lti_high_ratio = 1.15;
  double max_delay_p95_frames = 2.0;
  double max_delay_range_frames = 8.0;
  double max_timewarp_improvement_db = 3.0;
  double max_runtime_residual_correlation = 0.16;
  std::uint32_t min_same_window_runs = 3;
};

struct PrecisionMetrics {
  bool evidence_present = false;
  double min_mid_coherence = std::numeric_limits<double>::quiet_NaN();
  double min_high_coherence = std::numeric_limits<double>::quiet_NaN();
  double min_lti_snr_delta_db = std::numeric_limits<double>::quiet_NaN();
  double max_lti_mid_ratio = std::numeric_limits<double>::quiet_NaN();
  double max_lti_high_ratio = std::numeric_limits<double>::quiet_NaN();
  double delay_p95_frames = std::numeric_limits<double>::quiet_NaN();
  double delay_range_frames = std::numeric_limits<double>::quiet_NaN();
  double timewarp_improvement_db = std::numeric_limits<double>::quiet_NaN();
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
    from = end + 1;
  }
  return values;
}

double min_value(const std::vector<double>& values) {
  if (values.empty()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return *std::min_element(values.begin(), values.end());
}

double max_value(const std::vector<double>& values) {
  if (values.empty()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return *std::max_element(values.begin(), values.end());
}

double number_or_nan(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_number(json, key)
      .value_or(std::numeric_limits<double>::quiet_NaN());
}

bool finite_le(double value, double limit) {
  return std::isfinite(value) && value <= limit;
}

bool finite_ge(double value, double limit) {
  return std::isfinite(value) && value >= limit;
}

std::optional<std::filesystem::path> latest_bundle_with(
    const std::filesystem::path& parent,
    const std::vector<std::filesystem::path>& required) {
  if (!std::filesystem::is_directory(parent)) {
    return std::nullopt;
  }
  std::vector<std::filesystem::path> candidates;
  for (const auto& entry : std::filesystem::directory_iterator(parent)) {
    if (!entry.is_directory()) {
      continue;
    }
    bool ok = true;
    for (const auto& rel : required) {
      ok = ok && std::filesystem::is_regular_file(entry.path() / rel);
    }
    if (ok) {
      candidates.push_back(entry.path());
    }
  }
  if (candidates.empty()) {
    return std::nullopt;
  }
  std::sort(candidates.begin(), candidates.end());
  return candidates.back();
}

PrecisionMetrics read_precision_metrics(const std::filesystem::path& run_dir) {
  PrecisionMetrics metrics{};
  const auto lti = read_file(run_dir / "lti-transfer-quality.json");
  const auto warp = read_file(run_dir / "fractional-time-warp.json");
  if (lti.empty() || warp.empty()) {
    return metrics;
  }

  const auto high_coherence = json_numbers(lti, "coherence_high_mean");
  const auto mid_ratio = json_numbers(lti, "lti_mid_ratio");
  const auto high_ratio = json_numbers(lti, "lti_high_ratio");
  const double delay_min = number_or_nan(warp, "min_frames");
  const double delay_max = number_or_nan(warp, "max_frames");

  metrics.evidence_present = true;
  metrics.min_mid_coherence = number_or_nan(lti, "min_mid_coherence");
  metrics.min_high_coherence = min_value(high_coherence);
  metrics.min_lti_snr_delta_db = number_or_nan(lti, "min_lti_snr_delta_db");
  metrics.max_lti_mid_ratio = max_value(mid_ratio);
  metrics.max_lti_high_ratio = max_value(high_ratio);
  metrics.delay_p95_frames = number_or_nan(warp, "p95_abs_frames");
  metrics.delay_range_frames =
      (std::isfinite(delay_min) && std::isfinite(delay_max)) ? (delay_max - delay_min)
                                                             : std::numeric_limits<double>::quiet_NaN();
  metrics.timewarp_improvement_db =
      max_value(json_numbers(warp, "improvement_db"));
  return metrics;
}

bool lti_pass(const PrecisionMetrics& metrics, const Thresholds& thresholds) {
  return finite_ge(metrics.min_mid_coherence, thresholds.min_mid_coherence) &&
         finite_ge(metrics.min_high_coherence, thresholds.min_high_coherence) &&
         finite_ge(metrics.min_lti_snr_delta_db, thresholds.min_lti_snr_delta_db) &&
         finite_le(metrics.max_lti_mid_ratio, thresholds.max_lti_mid_ratio) &&
         finite_le(metrics.max_lti_high_ratio, thresholds.max_lti_high_ratio);
}

bool warp_pass(const PrecisionMetrics& metrics, const Thresholds& thresholds) {
  return finite_le(metrics.delay_p95_frames, thresholds.max_delay_p95_frames) &&
         finite_le(metrics.delay_range_frames, thresholds.max_delay_range_frames) &&
         finite_le(metrics.timewarp_improvement_db, thresholds.max_timewarp_improvement_db);
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

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
}

void print_string_array(const char* key, const std::vector<std::string>& values) {
  std::cout << "  \"" << key << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << values[index] << "\"";
  }
  std::cout << "],\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const Thresholds thresholds{};
  const auto local = root / "local-analysis";

  const auto physical_bundle = latest_bundle_with(
      local / "physical-superiority-window",
      {
          "cpp-soundcheck/lti-transfer-quality.json",
          "cpp-soundcheck/fractional-time-warp.json",
          "mainline-soundcheck/lti-transfer-quality.json",
          "mainline-soundcheck/fractional-time-warp.json",
      });
  const auto runtime_bundle = latest_bundle_with(
      local / "route-validation-offline",
      {
          "runtime-discontinuities.json",
          "lti-transfer-quality.json",
      });

  const PrecisionMetrics cpp =
      physical_bundle ? read_precision_metrics(*physical_bundle / "cpp-soundcheck")
                      : PrecisionMetrics{};
  const PrecisionMetrics mainline =
      physical_bundle ? read_precision_metrics(*physical_bundle / "mainline-soundcheck")
                      : PrecisionMetrics{};
  const auto runtime_json =
      runtime_bundle ? read_file(*runtime_bundle / "runtime-discontinuities.json") : std::string{};
  const auto runtime_correlations = json_numbers(runtime_json, "abs_correlation");
  const double max_runtime_correlation = max_value(runtime_correlations);

  const bool physical_bundle_present = physical_bundle.has_value();
  const bool runtime_bundle_present = runtime_bundle.has_value() && !runtime_json.empty();
  const bool cpp_lti_ok = cpp.evidence_present && lti_pass(cpp, thresholds);
  const bool cpp_warp_ok = cpp.evidence_present && warp_pass(cpp, thresholds);
  const bool mainline_lti_ok = mainline.evidence_present && lti_pass(mainline, thresholds);
  const bool mainline_warp_ok = mainline.evidence_present && warp_pass(mainline, thresholds);
  const bool runtime_correlation_ok =
      runtime_bundle_present &&
      (runtime_correlations.empty() ||
       finite_le(max_runtime_correlation, thresholds.max_runtime_residual_correlation));
  const std::uint32_t same_window_runs = physical_bundle_present ? 1U : 0U;
  const bool enough_same_window_runs = same_window_runs >= thresholds.min_same_window_runs;

  const bool precision_claim_allowed = physical_bundle_present && runtime_bundle_present &&
                                       cpp_lti_ok && cpp_warp_ok && mainline_lti_ok &&
                                       mainline_warp_ok && runtime_correlation_ok &&
                                       enough_same_window_runs;

  std::vector<std::string> blockers;
  if (!physical_bundle_present) {
    blockers.push_back("same_window_lti_timewarp_bundle_missing");
  }
  if (!runtime_bundle_present) {
    blockers.push_back("runtime_discontinuity_evidence_missing");
  }
  if (!cpp_lti_ok) {
    blockers.push_back("candidate_lti_precision_thresholds_not_met");
  }
  if (!cpp_warp_ok) {
    blockers.push_back("candidate_timewarp_stability_thresholds_not_met");
  }
  if (!mainline_lti_ok) {
    blockers.push_back("mainline_lti_precision_reference_thresholds_not_met");
  }
  if (!mainline_warp_ok) {
    blockers.push_back("mainline_timewarp_reference_thresholds_not_met");
  }
  if (!runtime_correlation_ok) {
    blockers.push_back("runtime_residual_correlation_above_threshold_or_missing");
  }
  if (!enough_same_window_runs) {
    blockers.push_back("same_window_statistical_sample_too_small");
  }

  const bool guard_pass = physical_bundle_present && runtime_bundle_present &&
                          !precision_claim_allowed && blockers.size() >= 4U;

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.audiophile-precision-claim-gate.v1\",\n"
      << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (guard_pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means precision-analysis evidence is enforced and audiophile superiority remains blocked\",\n";
  print_bool("physical_bundle_present", physical_bundle_present);
  print_bool("runtime_bundle_present", runtime_bundle_present);
  std::cout << "  \"physical_bundle\": \""
            << (physical_bundle ? physical_bundle->string() : std::string{}) << "\",\n";
  std::cout << "  \"runtime_bundle\": \""
            << (runtime_bundle ? runtime_bundle->string() : std::string{}) << "\",\n";
  std::cout << "  \"thresholds\": {"
            << "\"min_mid_coherence\": " << thresholds.min_mid_coherence
            << ", \"min_high_coherence\": " << thresholds.min_high_coherence
            << ", \"min_lti_snr_delta_db\": " << thresholds.min_lti_snr_delta_db
            << ", \"max_lti_mid_ratio\": " << thresholds.max_lti_mid_ratio
            << ", \"max_lti_high_ratio\": " << thresholds.max_lti_high_ratio
            << ", \"max_delay_p95_frames\": " << thresholds.max_delay_p95_frames
            << ", \"max_delay_range_frames\": " << thresholds.max_delay_range_frames
            << ", \"max_timewarp_improvement_db\": "
            << thresholds.max_timewarp_improvement_db
            << ", \"max_runtime_residual_correlation\": "
            << thresholds.max_runtime_residual_correlation
            << ", \"min_same_window_runs\": " << thresholds.min_same_window_runs
            << "},\n";
  print_number("candidate_min_mid_coherence", cpp.min_mid_coherence);
  print_number("candidate_min_high_coherence", cpp.min_high_coherence);
  print_number("candidate_min_lti_snr_delta_db", cpp.min_lti_snr_delta_db);
  print_number("candidate_max_lti_mid_ratio", cpp.max_lti_mid_ratio);
  print_number("candidate_max_lti_high_ratio", cpp.max_lti_high_ratio);
  print_number("candidate_delay_p95_frames", cpp.delay_p95_frames);
  print_number("candidate_delay_range_frames", cpp.delay_range_frames);
  print_number("candidate_timewarp_improvement_db", cpp.timewarp_improvement_db);
  print_number("mainline_min_mid_coherence", mainline.min_mid_coherence);
  print_number("mainline_min_high_coherence", mainline.min_high_coherence);
  print_number("mainline_delay_p95_frames", mainline.delay_p95_frames);
  print_number("max_runtime_residual_correlation", max_runtime_correlation);
  print_bool("candidate_lti_pass", cpp_lti_ok);
  print_bool("candidate_timewarp_pass", cpp_warp_ok);
  print_bool("mainline_lti_reference_pass", mainline_lti_ok);
  print_bool("mainline_timewarp_reference_pass", mainline_warp_ok);
  print_bool("runtime_correlation_pass", runtime_correlation_ok);
  std::cout << "  \"same_window_runs\": " << same_window_runs << ",\n";
  print_bool("enough_same_window_runs", enough_same_window_runs);
  print_bool("audiophile_precision_claim_allowed", precision_claim_allowed);
  print_string_array("precision_claim_blockers", blockers);
  std::cout
      << "  \"blocked_claim\": "
         "\"NO_AUDIOPHILE_PRECISION_OR_SUPERIORITY_CLAIM_WITHOUT_LTI_TIMEWARP_RUNTIME_AND_STATISTICAL_SAME_WINDOW_PASS\"\n"
      << "}\n";

  return guard_pass ? 0 : 1;
}
