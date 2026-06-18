#include <cctype>
#include <cmath>
#include <cstdint>
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

std::optional<double> json_number_after_anchor(const std::string& json,
                                               const std::string& anchor,
                                               const std::string& key) {
  const std::size_t anchor_pos = json.find(anchor);
  if (anchor_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle, anchor_pos);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  return parse_number_at(json, colon + 1U);
}

std::optional<bool> json_bool(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
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

std::optional<std::string> json_object(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (start >= json.size() || json[start] != '{') {
    return std::nullopt;
  }

  std::uint32_t depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t index = start; index < json.size(); ++index) {
    const char c = json[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (in_string && c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      if (depth == 0) {
        return std::nullopt;
      }
      --depth;
      if (depth == 0) {
        return json.substr(start, index - start + 1U);
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> json_string(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (start >= json.size() || json[start] != '"') {
    return std::nullopt;
  }
  ++start;
  std::string out;
  bool escaped = false;
  for (std::size_t index = start; index < json.size(); ++index) {
    const char c = json[index];
    if (escaped) {
      out.push_back(c);
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '"') {
      return out;
    } else {
      out.push_back(c);
    }
  }
  return std::nullopt;
}

double number_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

bool finite(double value) {
  return std::isfinite(value);
}

void print_number(const char* name, double value) {
  std::cout << "  \"" << name << "\": ";
  if (finite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
  std::cout << ",\n";
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << values[index] << "\"";
  }
  std::cout << "],\n";
}

void print_string_value(const char* name, const std::string& value) {
  std::cout << "  \"" << name << "\": \"" << value << "\",\n";
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto route_summary = read_file(
      root / "local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/"
             "metrics-summary.json");
  const auto quality_root = read_file(root / "local-analysis/cpp-offline/quality-root-cause-analysis.json");
  const auto soundcheck = read_file(root / "local-analysis/cpp-offline/soundcheck-wav-quality.json");
  const auto physical = read_file(root / "local-analysis/cpp-offline/physical-run-product-superiority.json");
  const auto direct_usb =
      read_file(root / "local-analysis/cpp-offline/direct-usb-path-attribution.json");
  const std::string direct_usb_latest = json_object(direct_usb, "latest_run").value_or(direct_usb);

  const double mainline_quality =
      number_or_nan(json_number_after_anchor(route_summary, "mainline-0.3.135",
                                             "quality_alignment_score"));
  const double mainline_snr =
      number_or_nan(json_number_after_anchor(route_summary, "mainline-0.3.135", "snr_floor_db"));
  const double cpp_route_quality =
      number_or_nan(json_number_after_anchor(route_summary, "cpp-iso64q8-stopisoc",
                                             "quality_alignment_score"));
  const double cpp_route_snr =
      number_or_nan(json_number_after_anchor(route_summary, "cpp-iso64q8-stopisoc", "snr_floor_db"));
  const double soundcheck_quality = number_or_nan(json_number(soundcheck, "quality_alignment_score"));
  const double soundcheck_snr =
      std::min(number_or_nan(json_number(soundcheck, "left_snr_db")),
               number_or_nan(json_number(soundcheck, "right_snr_db")));
  const double soundcheck_quiet = number_or_nan(json_number(soundcheck, "quiet_mid_band_noise_dbfs"));
  const double soundcheck_lag_jumps = number_or_nan(json_number(soundcheck, "lag_jumps_gt_2_frames"));
  const double physical_snr = number_or_nan(json_number(physical, "snr_floor_db"));
  const double physical_quality = number_or_nan(json_number(physical, "quality_alignment_score"));
  const double direct_usb_capture_quality =
      number_or_nan(json_number(direct_usb_latest, "capture_quality_alignment_score"));
  const double direct_usb_capture_snr =
      number_or_nan(json_number(direct_usb_latest, "capture_snr_floor_db"));
  const double direct_usb_mid_ratio =
      number_or_nan(json_number(direct_usb_latest, "capture_mid_band_residual_ratio"));
  const double direct_usb_high_ratio =
      number_or_nan(json_number(direct_usb_latest, "capture_high_band_residual_ratio"));
  const double direct_usb_drift_ppm =
      number_or_nan(json_number(direct_usb_latest, "failure_drift_ppm"));
  const double direct_usb_mid_coherence =
      number_or_nan(json_number(direct_usb_latest, "lti_mid_coherence"));
  const double direct_usb_usb_alignment =
      number_or_nan(json_number(direct_usb_latest, "usb_alignment_score"));
  const double direct_usb_usb_snr =
      number_or_nan(json_number(direct_usb_latest, "usb_snr_floor_db"));
  const double direct_usb_usb_check_errors =
      number_or_nan(json_number(direct_usb_latest, "usb_check_errors"));
  const double direct_usb_usb_panic_flags =
      number_or_nan(json_number(direct_usb_latest, "usb_panic_flags"));
  const auto direct_usb_attribution =
      json_string(direct_usb_latest, "attribution").value_or("missing");
  const bool shared_route_unhealthy =
      json_bool(quality_root, "shared_fixture_or_capture_path_unhealthy").value_or(false);
  const bool digital_payload_clean = json_bool(quality_root, "digital_payload_clean").value_or(false);
  const bool direct_usb_internal_clean =
      json_bool(direct_usb_latest, "internal_clean").value_or(false);
  const bool direct_usb_capture_failed =
      json_bool(direct_usb_latest, "capture_failed").value_or(false);

  const bool common_route_low =
      finite(mainline_quality) && finite(cpp_route_quality) && finite(mainline_snr) &&
      finite(cpp_route_snr) && mainline_quality < 0.90 && cpp_route_quality < 0.90 &&
      std::fabs(mainline_quality - cpp_route_quality) <= 0.03 && mainline_snr < 20.0 &&
      cpp_route_snr < 20.0;
  const bool candidate_capture_low =
      finite(soundcheck_quality) && finite(soundcheck_snr) && finite(soundcheck_quiet) &&
      (soundcheck_quality < 0.98 || soundcheck_snr < 35.0 || soundcheck_quiet > -58.0);
  const bool timing_unstable = finite(soundcheck_lag_jumps) && soundcheck_lag_jumps > 0.0;
  const bool product_compare_fails =
      finite(physical_quality) && finite(physical_snr) &&
      (physical_quality < 0.98 || physical_snr < 35.0);
  const bool direct_usb_capture_failed_after_clean_payload =
      direct_usb_internal_clean && direct_usb_capture_failed &&
      finite(direct_usb_usb_alignment) && direct_usb_usb_alignment >= 0.999999 &&
      direct_usb_usb_check_errors == 0.0 && direct_usb_usb_panic_flags == 0.0;

  std::vector<std::string> blockers;
  if (shared_route_unhealthy || common_route_low) {
    blockers.push_back("shared_capture_route_unhealthy");
  }
  if (candidate_capture_low) {
    blockers.push_back("candidate_capture_quality_below_reference_threshold");
  }
  if (timing_unstable) {
    blockers.push_back("capture_lag_not_stable_enough_for_promotion");
  }
  if (product_compare_fails) {
    blockers.push_back("candidate_not_better_than_mainline_reference");
  }
  if (!digital_payload_clean) {
    blockers.push_back("digital_payload_not_proven_clean");
  }
  if (direct_usb_capture_failed_after_clean_payload) {
    blockers.push_back("direct_usb_capture_failed_after_clean_payload");
  }

  std::vector<std::string> required_experiments;
  required_experiments.push_back("source_reference_file_or_tone_into_audio8_to_irig_capture_route");
  required_experiments.push_back("audio8_direct_to_irig_without_mixer_or_eq_if_physically_possible");
  required_experiments.push_back("same_session_mainline_cpp_physical_ab_against_source_reference");
  required_experiments.push_back("traktor_timecode_vinyl_scope_against_source_reference_route");

  const bool measurement_valid_for_promotion = blockers.empty();
  const bool product_claim_allowed = measurement_valid_for_promotion;
  const bool branch_promotion_allowed = measurement_valid_for_promotion;
  const char* route_measurement_status =
      measurement_valid_for_promotion ? "VALID_FOR_PROMOTION" : "BLOCKED_FOR_PROMOTION";
  const bool pass = true;

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.capture-route-health-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"diagnostic_result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"diagnostic_pass_not_product_readiness\": true,\n"
            << "  \"route_measurement_status\": \"" << route_measurement_status << "\",\n"
            << "  \"measurement_valid_for_promotion\": "
            << (measurement_valid_for_promotion ? "true" : "false") << ",\n"
            << "  \"product_claim_allowed\": "
            << (product_claim_allowed ? "true" : "false") << ",\n"
            << "  \"branch_promotion_allowed\": "
            << (branch_promotion_allowed ? "true" : "false") << ",\n"
            << "  \"digital_payload_clean\": " << (digital_payload_clean ? "true" : "false") << ",\n"
            << "  \"shared_route_unhealthy\": "
            << ((shared_route_unhealthy || common_route_low) ? "true" : "false") << ",\n"
            << "  \"candidate_capture_low\": " << (candidate_capture_low ? "true" : "false") << ",\n"
            << "  \"timing_unstable\": " << (timing_unstable ? "true" : "false") << ",\n"
            << "  \"product_compare_fails\": " << (product_compare_fails ? "true" : "false")
            << ",\n";
  std::cout << "  \"direct_usb_internal_clean\": "
            << (direct_usb_internal_clean ? "true" : "false") << ",\n"
            << "  \"direct_usb_capture_failed\": "
            << (direct_usb_capture_failed ? "true" : "false") << ",\n"
            << "  \"direct_usb_capture_failed_after_clean_payload\": "
            << (direct_usb_capture_failed_after_clean_payload ? "true" : "false") << ",\n";
  print_string_value("direct_usb_attribution", direct_usb_attribution);
  print_string_array("promotion_blockers", blockers);
  print_string_array("required_physical_experiments", required_experiments);
  print_number("mainline_route_quality", mainline_quality);
  print_number("mainline_route_snr_db", mainline_snr);
  print_number("cpp_route_quality", cpp_route_quality);
  print_number("cpp_route_snr_db", cpp_route_snr);
  print_number("soundcheck_quality_alignment_score", soundcheck_quality);
  print_number("soundcheck_snr_floor_db", soundcheck_snr);
  print_number("soundcheck_quiet_mid_band_noise_dbfs", soundcheck_quiet);
  print_number("soundcheck_lag_jumps_gt_2_frames", soundcheck_lag_jumps);
  print_number("direct_usb_capture_quality_alignment_score", direct_usb_capture_quality);
  print_number("direct_usb_capture_snr_floor_db", direct_usb_capture_snr);
  print_number("direct_usb_capture_mid_band_residual_ratio", direct_usb_mid_ratio);
  print_number("direct_usb_capture_high_band_residual_ratio", direct_usb_high_ratio);
  print_number("direct_usb_failure_drift_ppm", direct_usb_drift_ppm);
  print_number("direct_usb_lti_mid_coherence", direct_usb_mid_coherence);
  print_number("direct_usb_usb_alignment_score", direct_usb_usb_alignment);
  print_number("direct_usb_usb_snr_floor_db", direct_usb_usb_snr);
  print_number("direct_usb_usb_check_errors", direct_usb_usb_check_errors);
  print_number("direct_usb_usb_panic_flags", direct_usb_usb_panic_flags);
  std::cout << "  \"next_required_action\": \"LOCK_GATED_CAPTURE_ROUTE_REVALIDATION_BEFORE_QUALITY_PROMOTION\",\n"
            << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_CAPTURE_ROUTE_NOT_VALID_FOR_PROMOTION\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
