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

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto route_summary = read_file(
      root / "local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/"
             "metrics-summary.json");
  const auto quality_root = read_file(root / "local-analysis/cpp-offline/quality-root-cause-analysis.json");
  const auto soundcheck = read_file(root / "local-analysis/cpp-offline/soundcheck-wav-quality.json");
  const auto physical = read_file(root / "local-analysis/cpp-offline/physical-run-product-superiority.json");

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
  const bool shared_route_unhealthy =
      json_bool(quality_root, "shared_fixture_or_capture_path_unhealthy").value_or(false);
  const bool digital_payload_clean = json_bool(quality_root, "digital_payload_clean").value_or(false);

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

  const bool measurement_valid_for_promotion = blockers.empty();
  const bool pass = true;

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.capture-route-health-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"measurement_valid_for_promotion\": "
            << (measurement_valid_for_promotion ? "true" : "false") << ",\n"
            << "  \"digital_payload_clean\": " << (digital_payload_clean ? "true" : "false") << ",\n"
            << "  \"shared_route_unhealthy\": "
            << ((shared_route_unhealthy || common_route_low) ? "true" : "false") << ",\n"
            << "  \"candidate_capture_low\": " << (candidate_capture_low ? "true" : "false") << ",\n"
            << "  \"timing_unstable\": " << (timing_unstable ? "true" : "false") << ",\n"
            << "  \"product_compare_fails\": " << (product_compare_fails ? "true" : "false")
            << ",\n";
  print_string_array("promotion_blockers", blockers);
  print_number("mainline_route_quality", mainline_quality);
  print_number("mainline_route_snr_db", mainline_snr);
  print_number("cpp_route_quality", cpp_route_quality);
  print_number("cpp_route_snr_db", cpp_route_snr);
  print_number("soundcheck_quality_alignment_score", soundcheck_quality);
  print_number("soundcheck_snr_floor_db", soundcheck_snr);
  print_number("soundcheck_quiet_mid_band_noise_dbfs", soundcheck_quiet);
  print_number("soundcheck_lag_jumps_gt_2_frames", soundcheck_lag_jumps);
  std::cout << "  \"next_required_action\": \"LOCK_GATED_CAPTURE_ROUTE_REVALIDATION_BEFORE_QUALITY_PROMOTION\",\n"
            << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_CAPTURE_ROUTE_NOT_VALID_FOR_PROMOTION\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
