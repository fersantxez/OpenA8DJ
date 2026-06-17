#include <algorithm>
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

std::optional<double> json_number_from(const std::string& json,
                                       const std::string& key,
                                       std::size_t search_from = 0) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle, search_from);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  return parse_number_at(json, colon + 1U);
}

std::optional<double> json_number_after_anchor(const std::string& json,
                                               const std::string& anchor,
                                               const std::string& key) {
  const std::size_t anchor_pos = json.find(anchor);
  if (anchor_pos == std::string::npos) {
    return std::nullopt;
  }
  return json_number_from(json, key, anchor_pos);
}

std::optional<double> kv_number(const std::string& text, const std::string& key) {
  const std::string needle = key + "=";
  const std::size_t key_pos = text.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  return parse_number_at(text, key_pos + needle.size());
}

double value_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

bool finite(double value) {
  return std::isfinite(value);
}

void print_json_number(double value) {
  if (finite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

void print_bool(const char* name, bool value, bool trailing_comma = true) {
  std::cout << "  \"" << name << "\": " << (value ? "true" : "false");
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << values[index] << "\"";
  }
  std::cout << "],\n";
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto route_summary = read_file(
      root / "local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/"
             "metrics-summary.json");
  const auto timebase_family = read_file(
      root / "local-analysis/timebase-window-comparison/20260617-current-family/"
             "timebase-family.json");
  const auto packed_usb = read_file(
      root / "local-analysis/driver-capture-analysis/"
             "diag-pack-big-start4-output-packed-usb-auto.txt");
  const auto candidate_metrics = read_file(
      root / "local-analysis/soundcheck/20260617-iso12q8-irig-pairA-12s-cpp-hal/"
             "metrics.json");
  const auto hot_path = read_file(root / "local-analysis/cpp-offline/hot-path-timing-analysis.json");

  const double mainline_route_quality =
      value_or_nan(json_number_after_anchor(route_summary, "mainline-0.3.135",
                                            "quality_alignment_score"));
  const double mainline_route_snr =
      value_or_nan(json_number_after_anchor(route_summary, "mainline-0.3.135", "snr_floor_db"));
  const double iso64_route_quality =
      value_or_nan(json_number_after_anchor(route_summary, "cpp-iso64q8-stopisoc",
                                            "quality_alignment_score"));
  const double iso64_route_snr =
      value_or_nan(json_number_after_anchor(route_summary, "cpp-iso64q8-stopisoc", "snr_floor_db"));

  const double candidate_quality =
      value_or_nan(json_number_from(candidate_metrics, "quality_alignment_score"));
  const double candidate_snr = value_or_nan(json_number_from(candidate_metrics, "right_snr_db"));
  const double candidate_mid =
      value_or_nan(json_number_from(candidate_metrics, "mid_band_residual_ratio"));
  const double candidate_high =
      value_or_nan(json_number_from(candidate_metrics, "high_band_residual_ratio"));
  const double candidate_lag_jumps =
      value_or_nan(json_number_from(candidate_metrics, "lag_jumps_gt_2_frames"));

  const double usb_alignment = value_or_nan(kv_number(packed_usb, "usb_alignment_score"));
  const double usb_check_errors = value_or_nan(kv_number(packed_usb, "usb_check_errors"));
  const double usb_panic_flags = value_or_nan(kv_number(packed_usb, "usb_panic_flags"));
  const double usb_left_snr = value_or_nan(kv_number(packed_usb, "usb_left_snr_db"));
  const double usb_right_snr = value_or_nan(kv_number(packed_usb, "usb_right_snr_db"));

  const double max_lag_jumps =
      value_or_nan(json_number_from(timebase_family, "max_lag_jump_count_gt_2_frames"));
  const double lag_improvement =
      value_or_nan(json_number_from(timebase_family,
                                    "median_lag_correction_mid_ratio_improvement"));
  const double max_abs_drift_ppm =
      value_or_nan(json_number_from(timebase_family, "max_abs_drift_ppm"));

  const double fixed_to_fill =
      value_or_nan(json_number_from(hot_path, "fixed_queue_to_playback_fill_ratio"));

  const bool payload_clean = finite(usb_alignment) && usb_alignment >= 0.999999 &&
                             finite(usb_check_errors) && usb_check_errors == 0.0 &&
                             finite(usb_panic_flags) && usb_panic_flags == 0.0 &&
                             finite(usb_left_snr) && usb_left_snr >= 120.0 &&
                             finite(usb_right_snr) && usb_right_snr >= 120.0;
  const bool shared_route_degraded =
      finite(mainline_route_quality) && finite(iso64_route_quality) &&
      finite(mainline_route_snr) && finite(iso64_route_snr) &&
      mainline_route_quality < 0.90 && iso64_route_quality < 0.90 &&
      std::fabs(mainline_route_quality - iso64_route_quality) < 0.03 &&
      mainline_route_snr < 20.0 && iso64_route_snr < 20.0;
  const bool candidate_quality_fails =
      finite(candidate_quality) && finite(candidate_snr) && finite(candidate_mid) &&
      finite(candidate_high) && finite(candidate_lag_jumps) &&
      (candidate_quality < 0.98 || candidate_snr < 35.0 || candidate_mid > 1.36 ||
       candidate_high > 1.35 || candidate_lag_jumps > 0.0);
  const bool timebase_not_sufficient =
      finite(max_lag_jumps) && finite(lag_improvement) && finite(max_abs_drift_ppm) &&
      max_lag_jumps > 0.0 && std::fabs(max_abs_drift_ppm) < 100.0 && lag_improvement < 0.05;
  const bool fixed_transport_cpu_suspect = finite(fixed_to_fill) && fixed_to_fill > 6.0;

  std::vector<std::string> classifications;
  if (payload_clean) {
    classifications.push_back("digital_payload_clean");
  }
  if (shared_route_degraded) {
    classifications.push_back("shared_fixture_or_capture_path_unhealthy");
  }
  if (candidate_quality_fails) {
    classifications.push_back("candidate_physical_quality_fails");
  }
  if (timebase_not_sufficient) {
    classifications.push_back("lag_present_but_not_sufficient_explanation");
  }
  if (fixed_transport_cpu_suspect) {
    classifications.push_back("fixed_transport_queue_requeue_enqueue_cpu_suspect");
  }

  const bool promotion_allowed = payload_clean && !shared_route_degraded &&
                                 !candidate_quality_fails && !timebase_not_sufficient &&
                                 !fixed_transport_cpu_suspect;

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n";
  std::cout << "  \"schema\": \"opena8djcpp.quality-root-cause-analysis.v1\",\n";
  std::cout << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n";
  std::cout << "  \"result\": \"PASS\",\n";
  std::cout << "  \"product_readiness_allowed\": " << (promotion_allowed ? "true" : "false") << ",\n";
  print_string_array("classification", classifications);
  print_bool("digital_payload_clean", payload_clean);
  print_bool("shared_fixture_or_capture_path_unhealthy", shared_route_degraded);
  print_bool("candidate_physical_quality_fails", candidate_quality_fails);
  print_bool("timebase_lag_not_sufficient_explanation", timebase_not_sufficient);
  print_bool("fixed_transport_cpu_suspect", fixed_transport_cpu_suspect);
  std::cout << "  \"metrics\": {\n";
  std::cout << "    \"mainline_route_quality\": ";
  print_json_number(mainline_route_quality);
  std::cout << ",\n    \"mainline_route_snr_db\": ";
  print_json_number(mainline_route_snr);
  std::cout << ",\n    \"iso64_route_quality\": ";
  print_json_number(iso64_route_quality);
  std::cout << ",\n    \"iso64_route_snr_db\": ";
  print_json_number(iso64_route_snr);
  std::cout << ",\n    \"candidate_quality\": ";
  print_json_number(candidate_quality);
  std::cout << ",\n    \"candidate_snr_floor_db\": ";
  print_json_number(candidate_snr);
  std::cout << ",\n    \"candidate_mid_residual_ratio\": ";
  print_json_number(candidate_mid);
  std::cout << ",\n    \"candidate_high_residual_ratio\": ";
  print_json_number(candidate_high);
  std::cout << ",\n    \"candidate_lag_jumps_gt_2_frames\": ";
  print_json_number(candidate_lag_jumps);
  std::cout << ",\n    \"usb_alignment_score\": ";
  print_json_number(usb_alignment);
  std::cout << ",\n    \"usb_check_errors\": ";
  print_json_number(usb_check_errors);
  std::cout << ",\n    \"usb_panic_flags\": ";
  print_json_number(usb_panic_flags);
  std::cout << ",\n    \"timebase_max_lag_jumps_gt_2_frames\": ";
  print_json_number(max_lag_jumps);
  std::cout << ",\n    \"timebase_lag_correction_mid_ratio_improvement\": ";
  print_json_number(lag_improvement);
  std::cout << ",\n    \"timebase_max_abs_drift_ppm\": ";
  print_json_number(max_abs_drift_ppm);
  std::cout << ",\n    \"fixed_queue_to_playback_fill_ratio\": ";
  print_json_number(fixed_to_fill);
  std::cout << "\n  },\n";
  std::cout << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_BLOCKS_PROMOTION_UNTIL_ROUTE_QUALITY_CPU_AND_TIMECODE_PASS\"\n";
  std::cout << "}\n";
  return 0;
}
