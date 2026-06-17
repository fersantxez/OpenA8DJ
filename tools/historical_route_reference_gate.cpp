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

constexpr double kMinHistoricalDurationSec = 45.0;
constexpr double kMinHistoricalActiveSeconds = 40.0;
constexpr double kExpectedRate = 48000.0;

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

bool finite(double value) {
  return std::isfinite(value);
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
    return finite(value) ? std::optional<double>(value) : std::nullopt;
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

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto historical_summary =
      std::filesystem::path("/Users/fer/dev/opena8dj/local-analysis/irig-stream-capture/"
                            "vlc-long-route-proof-20260612-163849/capture-summary.json");
  const auto historical_capture =
      std::filesystem::path("/Users/fer/dev/opena8dj/local-analysis/irig-stream-capture/"
                            "vlc-long-route-proof-20260612-163849/capture.wav");
  const auto route_health_path = root / "local-analysis/cpp-offline/capture-route-health-gate.json";
  const auto direct_usb_path = root / "local-analysis/cpp-offline/direct-usb-path-attribution.json";
  const auto idle_gate_path = root / "local-analysis/cpp-offline/irig-idle-capture-gate.json";

  const auto historical = read_file(historical_summary);
  const auto route_health = read_file(route_health_path);
  const auto direct_usb = read_file(direct_usb_path);
  const auto idle_gate = read_file(idle_gate_path);

  const bool historical_summary_present = !historical.empty();
  const bool historical_capture_present = std::filesystem::is_regular_file(historical_capture);
  const double rate = number_or_nan(json_number(historical, "rate"));
  const double duration = number_or_nan(json_number(historical, "duration_sec"));
  const double active_seconds = number_or_nan(json_number(historical, "active_seconds_count"));
  const double clipped = number_or_nan(json_number(historical, "clipped_frames"));
  const double rms_ch1 = number_or_nan(json_number(historical, "overall_rms_dbfs_ch1"));
  const double rms_ch2 = number_or_nan(json_number(historical, "overall_rms_dbfs_ch2"));
  const double peak_ch1 = number_or_nan(json_number(historical, "overall_peak_dbfs_ch1"));
  const double peak_ch2 = number_or_nan(json_number(historical, "overall_peak_dbfs_ch2"));

  const bool historical_reference_pass =
      historical_summary_present && historical_capture_present && finite(rate) &&
      std::fabs(rate - kExpectedRate) < 0.5 && finite(duration) &&
      duration >= kMinHistoricalDurationSec && finite(active_seconds) &&
      active_seconds >= kMinHistoricalActiveSeconds && finite(clipped) && clipped == 0.0 &&
      finite(rms_ch1) && finite(rms_ch2) && rms_ch1 > -45.0 && rms_ch2 > -45.0 &&
      rms_ch1 < -10.0 && rms_ch2 < -10.0 && finite(peak_ch1) && finite(peak_ch2) &&
      peak_ch1 < -3.0 && peak_ch2 < -3.0 && peak_ch1 > -25.0 && peak_ch2 > -25.0;

  const bool current_measurement_valid =
      json_bool(route_health, "measurement_valid_for_promotion").value_or(false);
  const bool current_direct_usb_internal_clean =
      json_bool(direct_usb, "direct_usb_internal_clean").value_or(
          json_bool(route_health, "direct_usb_internal_clean").value_or(false));
  const bool current_direct_usb_capture_failed_after_clean =
      json_bool(route_health, "direct_usb_capture_failed_after_clean_payload").value_or(false);
  const bool current_irig_idle_gate_pass = idle_gate.find("\"result\": \"PASS\"") != std::string::npos;

  std::vector<std::string> blockers;
  if (!historical_reference_pass) {
    blockers.push_back("historical_reference_missing_or_below_guardrails");
  }
  if (!current_measurement_valid) {
    blockers.push_back("current_capture_route_not_valid_for_promotion");
  }
  if (current_direct_usb_capture_failed_after_clean) {
    blockers.push_back("current_direct_usb_capture_failed_after_clean_payload");
  }
  blockers.push_back("historical_reference_is_not_current_same_session_evidence");

  const bool pass = historical_reference_pass && !route_health.empty() && !direct_usb.empty();

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.historical-route-reference-gate.v1\",\n"
            << "  \"safety\": \"offline_readonly_mainline_reference_and_cpp_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means the historical iRig route proof is readable and sane; it never authorizes promotion without current same-session route validation\",\n"
            << "  \"historical_summary\": \"" << json_escape(historical_summary.string()) << "\",\n"
            << "  \"historical_capture\": \"" << json_escape(historical_capture.string()) << "\",\n"
            << "  \"historical_summary_present\": "
            << (historical_summary_present ? "true" : "false") << ",\n"
            << "  \"historical_capture_present\": "
            << (historical_capture_present ? "true" : "false") << ",\n"
            << "  \"historical_reference_pass\": "
            << (historical_reference_pass ? "true" : "false") << ",\n";
  print_number("historical_rate", rate);
  print_number("historical_duration_sec", duration);
  print_number("historical_active_seconds_count", active_seconds);
  print_number("historical_clipped_frames", clipped);
  print_number("historical_overall_rms_dbfs_ch1", rms_ch1);
  print_number("historical_overall_rms_dbfs_ch2", rms_ch2);
  print_number("historical_overall_peak_dbfs_ch1", peak_ch1);
  print_number("historical_overall_peak_dbfs_ch2", peak_ch2);
  std::cout << "  \"current_measurement_valid_for_promotion\": "
            << (current_measurement_valid ? "true" : "false") << ",\n"
            << "  \"current_direct_usb_internal_clean\": "
            << (current_direct_usb_internal_clean ? "true" : "false") << ",\n"
            << "  \"current_direct_usb_capture_failed_after_clean_payload\": "
            << (current_direct_usb_capture_failed_after_clean ? "true" : "false") << ",\n"
            << "  \"current_irig_idle_gate_pass\": "
            << (current_irig_idle_gate_pass ? "true" : "false") << ",\n"
            << "  \"current_route_revalidation_required\": true,\n"
            << "  \"historical_reference_currently_valid_for_promotion\": false,\n"
            << "  \"regression_state\": \"historical_route_reference_exists_current_route_not_valid_for_promotion\",\n";
  print_string_array("promotion_blockers", blockers);
  std::cout << "  \"next_required_action\": \"LOCK_GATED_KNOWN_GOOD_NON_AUDIO8_ROUTE_CAPTURE_THEN_SAME_SESSION_MAINLINE_CPP_COMPARE\",\n"
            << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_HISTORICAL_ROUTE_REFERENCE_NOT_CURRENT_PROMOTION_EVIDENCE\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
