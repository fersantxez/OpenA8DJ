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

std::string json_escape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8U);
  for (const char c : value) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  return escaped;
}

void print_string(const char* key, std::string_view value, bool trailing = true) {
  std::cout << "  \"" << key << "\": \"" << json_escape(value) << "\"";
  if (trailing) {
    std::cout << ",";
  }
  std::cout << "\n";
}

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
}

void print_array(const char* key, const std::vector<std::string>& values) {
  std::cout << "  \"" << key << "\": [";
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0U) {
      std::cout << ", ";
    }
    std::cout << "\"" << json_escape(values[i]) << "\"";
  }
  std::cout << "],\n";
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

bool string_array_has(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_array_contains(json, key, expected);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto evidence = root / "local-analysis/cpp-offline";

  const auto hal_bundle = read_file(evidence / "hal-bundle-complete.json");
  const auto hal_safety = read_file(evidence / "hal-candidate-safety-gate.json");
  const auto hardware_lock_policy = read_file(evidence / "hardware-lock-policy.json");
  const auto physical_window = read_file(evidence / "physical-window-readiness-gate.json");
  const auto capture_readiness = read_file(evidence / "capture-readiness-contract.json");
  const auto known_good = read_file(evidence / "known-good-route-selector.json");
  const auto route_matrix = read_file(evidence / "physical-route-matrix-contract.json");
  const auto product_quality = read_file(evidence / "product-quality-claim-gate.json");
  const auto hal_runtime = read_file(evidence / "hal-transport-runtime-gate.json");
  const auto timecode = read_file(evidence / "timecode-readiness-gate.json");
  const auto promotion = read_file(evidence / "promotion-readiness-offline-check.json");
  const auto prepared_window = read_file(evidence / "prepared-runtime-physical-window-contract.json");

  const auto hal_path = root / "build/OpenA8DJ.driver";
  const auto hal_exec = hal_path / "Contents/MacOS/OpenA8DJHAL";
  const auto pkg_path = root / "build/OpenA8DJ-0.3.25.pkg";
  const auto dmg_path = root / "build/OpenA8DJ-0.3.25.dmg";
  const auto checksums_path = root / "build/OpenA8DJ-0.3.25-checksums.txt";

  const bool evidence_present =
      !hal_bundle.empty() && !hal_safety.empty() && !hardware_lock_policy.empty() &&
      !physical_window.empty() && !capture_readiness.empty() && !known_good.empty() &&
      !route_matrix.empty() && !product_quality.empty() && !hal_runtime.empty() &&
      !timecode.empty() && !promotion.empty() && !prepared_window.empty();
  const bool bundle_ready =
      string_field_is(hal_bundle, "result", "PASS") && std::filesystem::is_directory(hal_path) &&
      std::filesystem::is_regular_file(hal_exec) && std::filesystem::file_size(hal_exec) > 0U;
  const bool package_present =
      std::filesystem::is_regular_file(pkg_path) && std::filesystem::is_regular_file(dmg_path) &&
      std::filesystem::is_regular_file(checksums_path);
  const bool lock_policy_ready =
      string_field_is(hardware_lock_policy, "result", "PASS") &&
      string_array_has(hardware_lock_policy, "sensitive_paths",
                       "scripts/run-physical-superiority-window") &&
      string_array_has(hardware_lock_policy, "sensitive_paths",
                       "scripts/run-known-good-route-soundcheck");
  const bool safety_smoke_required =
      !string_field_is(hal_safety, "safety_window_status", "PASS");
  const bool safety_external_recovery_seen =
      string_array_has(hal_safety, "promotion_blockers", "safety_run_required_external_recovery");
  const bool capture_visible =
      string_field_is(capture_readiness, "capture_status", "VISIBLE") &&
      bool_field_is(capture_readiness, "irig_coreaudio_capture_visible", true);
  const bool route_ready = bool_field_is(known_good, "route_revalidation_ready", true);
  const bool route_known_blocked =
      bool_field_is(known_good, "route_revalidation_ready", false) &&
      string_array_has(known_good, "blockers",
                       "non_audio8_non_builtin_known_good_output_not_visible");
  const bool route_matrix_blocks_product =
      string_field_is(route_matrix, "result", "PASS") &&
      bool_field_is(route_matrix, "human_product_test_allowed", false) &&
      bool_field_is(route_matrix, "all_pairs_no_useful_correlated_capture", true);
  const bool product_quality_blocked =
      string_field_is(product_quality, "result", "PASS") &&
      bool_field_is(product_quality, "quality_claim_allowed", false);
  const bool cpu_superiority_blocked =
      string_field_is(hal_runtime, "result", "PASS") &&
      bool_field_is(hal_runtime, "product_claim_blocked", true) &&
      bool_field_is(hal_runtime, "hal_prepared_runtime_physical_evidence_present", false);
  const bool timecode_offline_pass_physical_blocked =
      string_field_is(timecode, "result", "PASS") &&
      bool_field_is(timecode, "offline_timecode_pass", true) &&
      bool_field_is(timecode, "product_timecode_ready", false) &&
      bool_field_is(timecode, "physical_traktor_timecode_blocked", true);
  const bool promotion_blocked =
      last_string_field_is(promotion, "result", "FAIL") &&
      bool_field_is(promotion, "branch_promotion_allowed", false);
  const bool physical_window_blocks_product =
      string_field_is(physical_window, "result", "PASS") &&
      bool_field_is(physical_window, "ready_for_product_physical_ab", false) &&
      bool_field_is(physical_window, "ready_for_branch_promotion", false);
  const bool prepared_window_ready_for_controlled_experiment =
      string_field_is(prepared_window, "result", "PASS") &&
      bool_field_is(prepared_window, "preflight_hashes_candidate", true) &&
      bool_field_is(prepared_window, "preflight_preserves_claim_block", true);

  const bool diagnostic_rc_artifacts_ready =
      evidence_present && bundle_ready && package_present && lock_policy_ready &&
      capture_visible && product_quality_blocked && cpu_superiority_blocked &&
      timecode_offline_pass_physical_blocked && promotion_blocked &&
      prepared_window_ready_for_controlled_experiment;
  const bool diagnostic_install_smoke_allowed_after_lock =
      diagnostic_rc_artifacts_ready && safety_smoke_required;
  const bool route_revalidation_allowed_after_lock =
      diagnostic_rc_artifacts_ready && route_ready && !safety_smoke_required;
  const bool product_human_test_allowed =
      false;  // Requires route revalidation plus same-session C++/mainline A/B.
  const bool timecode_vinyl_human_test_allowed =
      false;  // Requires physical Traktor/vinyl evidence on a validated route.
  const bool branch_promotion_allowed = false;

  std::vector<std::string> blockers;
  if (!package_present) {
    blockers.push_back("pkg_dmg_checksums_not_generated");
  }
  if (safety_smoke_required) {
    blockers.push_back("fresh_lock_gated_hal_safety_smoke_required");
  }
  if (safety_external_recovery_seen) {
    blockers.push_back("latest_hal_safety_window_needed_external_recovery");
  }
  if (route_known_blocked) {
    blockers.push_back("non_audio8_non_builtin_known_good_output_not_visible");
  }
  if (route_matrix_blocks_product) {
    blockers.push_back("latest_audio8_route_matrix_has_no_useful_correlated_capture");
  }
  if (product_quality_blocked) {
    blockers.push_back("same_session_product_quality_not_proven");
  }
  if (cpu_superiority_blocked) {
    blockers.push_back("same_session_cpu_resource_superiority_not_proven");
  }
  if (timecode_offline_pass_physical_blocked) {
    blockers.push_back("traktor_timecode_vinyl_physical_gate_missing");
  }
  if (promotion_blocked) {
    blockers.push_back("branch_promotion_forbidden");
  }

  const bool classifier_safe =
      evidence_present && bundle_ready && lock_policy_ready && capture_visible &&
      product_quality_blocked && cpu_superiority_blocked &&
      timecode_offline_pass_physical_blocked && promotion_blocked &&
      physical_window_blocks_product && prepared_window_ready_for_controlled_experiment &&
      !product_human_test_allowed && !timecode_vinyl_human_test_allowed &&
      !branch_promotion_allowed;
  const bool pass = classifier_safe;

  std::cout << "{\n";
  print_string("schema", "opena8djcpp.human-test-rc-gate.v1");
  print_string("safety", "offline_existing_evidence_and_package_files_only_no_audio_coreaudio_usb_or_hardware_touch");
  print_string("result", pass ? "PASS" : "FAIL");
  print_string("meaning",
               "PASS means the RC state is explicitly classified; it is not product readiness");
  print_bool("classifier_safe", classifier_safe);
  print_bool("evidence_present", evidence_present);
  print_bool("bundle_ready", bundle_ready);
  print_bool("package_present", package_present);
  print_string("hal_bundle_path", hal_path.string());
  print_string("pkg_path", pkg_path.string());
  print_string("dmg_path", dmg_path.string());
  print_bool("lock_policy_ready", lock_policy_ready);
  print_bool("capture_visible", capture_visible);
  print_bool("product_quality_blocked", product_quality_blocked);
  print_bool("cpu_superiority_blocked", cpu_superiority_blocked);
  print_bool("timecode_offline_pass_physical_blocked", timecode_offline_pass_physical_blocked);
  print_bool("promotion_blocked", promotion_blocked);
  print_bool("physical_window_blocks_product", physical_window_blocks_product);
  print_bool("prepared_window_ready_for_controlled_experiment",
             prepared_window_ready_for_controlled_experiment);
  print_bool("diagnostic_rc_artifacts_ready", diagnostic_rc_artifacts_ready);
  print_bool("diagnostic_install_smoke_allowed_after_lock",
             diagnostic_install_smoke_allowed_after_lock);
  print_bool("fresh_hal_safety_smoke_required", safety_smoke_required);
  print_bool("route_revalidation_allowed_after_lock", route_revalidation_allowed_after_lock);
  print_bool("route_revalidation_ready", route_ready);
  print_bool("product_human_test_allowed", product_human_test_allowed);
  print_bool("timecode_vinyl_human_test_allowed", timecode_vinyl_human_test_allowed);
  print_bool("cpu_superiority_claim_allowed", false);
  print_bool("branch_promotion_allowed", branch_promotion_allowed);
  print_bool("no_more_transport_tuning_until_route_validated", true);
  print_array("blockers", blockers);
  print_array("next_lock_gated_actions",
              {"fresh_hal_safety_smoke_if_installing",
               "known_good_non_audio8_route_revalidation",
               "same_session_mainline_cpp_physical_ab",
               "runtime_cpu_submit_counter_comparison",
               "traktor_timecode_vinyl_physical_gate"});
  print_string("next_required_action",
               safety_smoke_required
                   ? "LOCK_GATED_FRESH_HAL_SAFETY_SMOKE_BEFORE_HUMAN_DIAGNOSTIC_INSTALL"
                   : "LOCK_GATED_KNOWN_GOOD_ROUTE_REVALIDATION",
               false);
  std::cout << "}\n";
  return pass ? 0 : 1;
}
