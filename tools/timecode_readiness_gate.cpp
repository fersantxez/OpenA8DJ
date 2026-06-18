#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
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

std::optional<unsigned long long> json_u64(const std::string& json, const std::string& key) {
  const auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  const auto colon = json.find(':', pos);
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  std::size_t end = start;
  while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
    ++end;
  }
  if (end == start) {
    return std::nullopt;
  }
  try {
    return std::stoull(json.substr(start, end - start));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool result_pass(const std::string& json) {
  return json.find("\"result\": \"PASS\"") != std::string::npos &&
         json.find("\"result\": \"FAIL\"") == std::string::npos;
}

void print_bool(const char* name, bool value) {
  std::cout << "  \"" << name << "\": " << (value ? "true" : "false") << ",\n";
}

void print_string_array(const char* name, const std::vector<const char*>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << values[index] << "\"";
  }
  std::cout << "],\n";
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto base = root / "local-analysis/cpp-offline";
  const auto matrix = read_file(base / "timecode-matrix.json");
  const auto signal = read_file(base / "timecode-signal-analysis.json");
  const auto dvs_packet = read_file(base / "dvs-packet-input-decode.json");
  const auto dvs_stress = read_file(base / "dvs-timecode-stress-margin.json");
  const auto prepared = read_file(base / "prepared-transport-routing-timecode-contract.json");
  const auto promotion = read_file(base / "promotion-readiness-offline-check.json");

  const bool matrix_pass = result_pass(matrix);
  const bool signal_pass = result_pass(signal);
  const bool dvs_packet_pass = result_pass(dvs_packet);
  const bool dvs_stress_pass = result_pass(dvs_stress);
  const bool prepared_pass = result_pass(prepared);
  const bool promotion_allowed =
      promotion.find("\"branch_promotion_allowed\": true") != std::string::npos;
  const bool physical_blocked = !promotion_allowed ||
                                promotion.find("BLOCKED_UNVALIDATED_DVS") != std::string::npos ||
                                promotion.find("traktor_timecode_physical") != std::string::npos;
  const bool offline_pass =
      matrix_pass && signal_pass && dvs_packet_pass && dvs_stress_pass && prepared_pass;
  const bool product_timecode_ready = offline_pass && !physical_blocked;

  const auto matrix_failures = json_u64(matrix, "failures").value_or(9999);
  const auto matrix_rows = json_u64(matrix, "row_count").value_or(0);
  const auto signal_rows = json_u64(signal, "row_count").value_or(0);
  const auto dvs_rows = json_u64(dvs_packet, "row_count").value_or(0);
  const auto dvs_stress_rows = json_u64(dvs_stress, "row_count").value_or(0);
  const auto dvs_stress_failures = json_u64(dvs_stress, "failures").value_or(9999);
  const auto dvs_stress_false_accepts = json_u64(dvs_stress, "false_accepts").value_or(9999);
  const auto dvs_stress_deck_swaps = json_u64(dvs_stress, "deck_swaps").value_or(9999);
  const auto prepared_rows = json_u64(prepared, "row_count").value_or(0);
  std::vector<const char*> physical_requirements_remaining;
  if (physical_blocked) {
    physical_requirements_remaining.push_back("real_traktor_scope_lock");
    physical_requirements_remaining.push_back("physical_timecode_vinyl_decks");
    physical_requirements_remaining.push_back("same_session_mainline_cpp_dvs_comparison");
    physical_requirements_remaining.push_back("validated_capture_route");
  }

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.timecode-readiness-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_traktor_or_hardware_touch\",\n"
            << "  \"result\": \"" << (offline_pass ? "PASS" : "FAIL") << "\",\n";
  print_bool("offline_timecode_pass", offline_pass);
  print_bool("timecode_matrix_pass", matrix_pass);
  print_bool("timecode_signal_pass", signal_pass);
  print_bool("dvs_packet_input_decode_pass", dvs_packet_pass);
  print_bool("dvs_timecode_stress_margin_pass", dvs_stress_pass);
  print_bool("prepared_transport_timecode_pass", prepared_pass);
  print_bool("physical_traktor_timecode_blocked", physical_blocked);
  print_bool("product_timecode_ready", product_timecode_ready);
  std::cout << "  \"offline_coverage\": {\"profiles\": [\"timecode-vinyl\", "
               "\"timecode-cd-line\", \"phono\", \"disabled\"], "
               "\"decks\": [\"A\", \"B\", \"C\", \"D\"], "
               "\"sample_rates\": [44100, 48000], "
               "\"mode2_packet_decode\": true, "
               "\"synthetic_stress_margin\": true, "
               "\"prepared_transport_path\": true},\n";
  print_string_array("physical_requirements_remaining", physical_requirements_remaining);
  std::cout << "  \"metrics\": {\"timecode_matrix_rows\": " << matrix_rows
            << ", \"timecode_matrix_failures\": " << matrix_failures
            << ", \"timecode_signal_rows\": " << signal_rows
            << ", \"dvs_packet_rows\": " << dvs_rows
            << ", \"dvs_stress_rows\": " << dvs_stress_rows
            << ", \"dvs_stress_failures\": " << dvs_stress_failures
            << ", \"dvs_stress_false_accepts\": " << dvs_stress_false_accepts
            << ", \"dvs_stress_deck_swaps\": " << dvs_stress_deck_swaps
            << ", \"prepared_transport_profile_deck_rows\": " << prepared_rows << "},\n"
            << "  \"physical_status\": \""
            << (physical_blocked ? "BLOCKED_UNVALIDATED_DVS" : "PHYSICAL_EVIDENCE_PRESENT")
            << "\",\n"
            << "  \"readiness_claim\": \"OFFLINE_DVS_ONLY_NOT_TRAKTOR_VINYL_READINESS\"\n"
            << "}\n";
  return offline_pass ? 0 : 1;
}
