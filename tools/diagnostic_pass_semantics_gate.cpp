#include "evidence_json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

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

bool string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or("") == expected;
}

bool last_string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_last(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

bool has_readiness_claim(std::string_view json, std::string_view expected) {
  return string_field_is(json, "readiness_claim", expected);
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
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
  const auto evidence = repo_root(argv) / "local-analysis/cpp-offline";

  const auto soundcheck = read_file(evidence / "soundcheck-wav-quality.json");
  const auto physical_compare = read_file(evidence / "physical-run-product-superiority.json");
  const auto timecode = read_file(evidence / "timecode-readiness-gate.json");
  const auto migration = read_file(evidence / "prepared-transport-migration-gate.json");
  const auto physical_window = read_file(evidence / "physical-window-readiness-gate.json");
  const auto promotion = read_file(evidence / "promotion-readiness-offline-check.json");

  const bool evidence_present =
      !soundcheck.empty() && !physical_compare.empty() && !timecode.empty() &&
      !migration.empty() && !physical_window.empty() && !promotion.empty();

  const bool soundcheck_analyzer_only =
      string_field_is(soundcheck, "result", "PASS") &&
      has_readiness_claim(soundcheck, "ANALYZER_ONLY_NOT_PRODUCT_READINESS");
  const bool physical_compare_not_promotion =
      (string_field_is(physical_compare, "result", "FAIL") ||
       bool_field_is(physical_compare, "branch_promotion_supported", false)) &&
      !bool_field_is(physical_compare, "branch_promotion_supported", true);
  const bool timecode_offline_only =
      string_field_is(timecode, "result", "PASS") &&
      bool_field_is(timecode, "product_timecode_ready", false) &&
      bool_field_is(timecode, "physical_traktor_timecode_blocked", true) &&
      has_readiness_claim(timecode, "OFFLINE_DVS_ONLY_NOT_TRAKTOR_VINYL_READINESS");
  const bool migration_not_product =
      string_field_is(migration, "result", "PASS") &&
      bool_field_is(migration, "product_ready", false) &&
      bool_field_is(migration, "branch_promotion_supported", false);
  const bool physical_window_not_product =
      string_field_is(physical_window, "result", "PASS") &&
      bool_field_is(physical_window, "ready_for_product_physical_ab", false) &&
      bool_field_is(physical_window, "ready_for_branch_promotion", false);
  const bool promotion_not_allowed =
      last_string_field_is(promotion, "result", "FAIL") &&
      bool_field_is(promotion, "branch_promotion_allowed", false);

  std::vector<std::string> protected_diagnostic_passes;
  if (soundcheck_analyzer_only) {
    protected_diagnostic_passes.push_back("soundcheck_wav_quality_analyzer_only");
  }
  if (physical_compare_not_promotion) {
    protected_diagnostic_passes.push_back("physical_run_compare_not_branch_promotion");
  }
  if (timecode_offline_only) {
    protected_diagnostic_passes.push_back("timecode_offline_only");
  }
  if (migration_not_product) {
    protected_diagnostic_passes.push_back("prepared_transport_migration_not_product");
  }
  if (physical_window_not_product) {
    protected_diagnostic_passes.push_back("physical_window_not_product");
  }
  if (promotion_not_allowed) {
    protected_diagnostic_passes.push_back("promotion_not_allowed");
  }

  const bool pass = evidence_present && soundcheck_analyzer_only &&
                    physical_compare_not_promotion && timecode_offline_only &&
                    migration_not_product && physical_window_not_product &&
                    promotion_not_allowed;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.diagnostic-pass-semantics-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means diagnostic PASS artifacts carry explicit non-product-readiness semantics\",\n"
            << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
            << "  \"soundcheck_analyzer_only\": " << (soundcheck_analyzer_only ? "true" : "false")
            << ",\n"
            << "  \"physical_compare_not_promotion\": "
            << (physical_compare_not_promotion ? "true" : "false") << ",\n"
            << "  \"timecode_offline_only\": " << (timecode_offline_only ? "true" : "false")
            << ",\n"
            << "  \"migration_not_product\": " << (migration_not_product ? "true" : "false")
            << ",\n"
            << "  \"physical_window_not_product\": "
            << (physical_window_not_product ? "true" : "false") << ",\n"
            << "  \"promotion_not_allowed\": " << (promotion_not_allowed ? "true" : "false")
            << ",\n";
  print_string_array("protected_diagnostic_passes", protected_diagnostic_passes);
  std::cout << "  \"blocked_claim\": \"DIAGNOSTIC_PASS_DOES_NOT_MEAN_PRODUCT_READINESS_OR_BRANCH_PROMOTION\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
