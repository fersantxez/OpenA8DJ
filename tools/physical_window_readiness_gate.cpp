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

bool string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or("") == expected;
}

bool last_string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_last(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

bool number_field_is(std::string_view json, std::string_view key, double expected) {
  return opena8djcpp::evidence_json::json_number(json, key).value_or(expected + 1.0) == expected;
}

bool string_array_has(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_array_contains(json, key, expected);
}

bool gate_array_has_name(std::string_view json, std::string_view expected) {
  return opena8djcpp::evidence_json::json_object_array_contains_string_field(json, "gates", "name",
                                                                            expected);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto evidence = root / "local-analysis/cpp-offline";

  const auto capture_route = read_file(evidence / "capture-route-health-gate.json");
  const auto direct_usb = read_file(evidence / "direct-usb-path-attribution.json");
  const auto historical_route = read_file(evidence / "historical-route-reference-gate.json");
  const auto hal_safety = read_file(evidence / "hal-candidate-safety-gate.json");
  const auto physical_frontier = read_file(evidence / "physical-evidence-frontier.json");
  const auto promotion = read_file(evidence / "promotion-readiness-offline-check.json");
  const auto migration = read_file(evidence / "prepared-transport-migration-gate.json");
  const auto hardware_policy = read_file(evidence / "hardware-lock-policy.json");
  const auto route_inventory = read_file(evidence / "physical-route-inventory.json");

  const bool evidence_present =
      !capture_route.empty() && !direct_usb.empty() && !historical_route.empty() &&
      !hal_safety.empty() && !physical_frontier.empty() && !promotion.empty() &&
      !migration.empty() && !hardware_policy.empty() && !route_inventory.empty();

  const bool offline_clean =
      string_field_is(capture_route, "safety",
                      "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch") &&
      string_field_is(direct_usb, "safety",
                      "offline_existing_direct_usb_evidence_only_no_audio_coreaudio_usb_or_hardware_touch") &&
      string_field_is(historical_route, "safety",
                      "offline_readonly_mainline_reference_and_cpp_evidence_only_no_audio_coreaudio_usb_or_hardware_touch") &&
      string_field_is(hal_safety, "safety",
                      "offline_existing_hal_safety_evidence_only_no_audio_coreaudio_usb_or_hardware_touch") &&
      string_field_is(physical_frontier, "safety",
                      "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch") &&
      string_field_is(migration, "safety",
                      "offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch");
  const bool route_inventory_clean =
      string_field_is(route_inventory, "schema", "opena8djcpp.physical-route-inventory.v1") &&
      last_string_field_is(route_inventory, "result", "PASS") &&
      bool_field_is(route_inventory, "hardware_touched", false) &&
      bool_field_is(route_inventory, "audio_played", false) &&
      bool_field_is(route_inventory, "audio_recorded", false) &&
      bool_field_is(route_inventory, "driver_installed_or_activated", false);
  const bool current_promotion_route_ready =
      bool_field_is(route_inventory, "promotion_route_ready", true);
  const bool current_known_good_output_missing =
      !current_promotion_route_ready &&
      string_array_has(route_inventory, "blockers",
                       "non_audio8_non_builtin_known_good_output_not_visible");
  const bool product_blocked =
      last_string_field_is(promotion, "result", "FAIL") &&
      bool_field_is(promotion, "branch_promotion_allowed", false) &&
      gate_array_has_name(promotion, "capture_route_measurement_valid_for_promotion") &&
      gate_array_has_name(promotion, "same_session_mainline_cpp_physical_compare") &&
      gate_array_has_name(promotion, "runtime_cpu_beats_mainline") &&
      gate_array_has_name(promotion, "traktor_timecode_physical");
  const bool route_not_valid_for_promotion =
      string_field_is(capture_route, "result", "PASS") &&
      bool_field_is(capture_route, "measurement_valid_for_promotion", false) &&
      bool_field_is(capture_route, "digital_payload_clean", true) &&
      bool_field_is(capture_route, "shared_route_unhealthy", true) &&
      string_array_has(capture_route, "promotion_blockers", "shared_capture_route_unhealthy") &&
      string_array_has(capture_route, "required_physical_experiments",
                       "same_session_mainline_cpp_physical_ab_on_validated_route");
  const auto latest_direct_usb = opena8djcpp::evidence_json::json_object(direct_usb, "latest_run");
  const bool direct_usb_blocks_route_claim =
      string_field_is(direct_usb, "result", "PASS") && latest_direct_usb.has_value() &&
      bool_field_is(*latest_direct_usb, "internal_clean", true) &&
      bool_field_is(*latest_direct_usb, "capture_failed", true) &&
      bool_field_is(*latest_direct_usb, "has_timewarp_evidence", true) &&
      string_field_is(*latest_direct_usb, "timewarp_classification",
                      "fractional_time_warp_rejected") &&
      bool_field_is(*latest_direct_usb,
                    "capture_failed_after_clean_not_timewarp_explained", true) &&
      string_field_is(direct_usb, "readiness_claim",
                      "DIAGNOSTIC_ONLY_DIRECT_USB_PHYSICAL_CAPTURE_STILL_FAILS_AND_TIMEWARP_DOES_NOT_EXPLAIN_IT");
  const bool historical_route_not_current =
      string_field_is(historical_route, "result", "PASS") &&
      bool_field_is(historical_route, "historical_reference_currently_valid_for_promotion", false) &&
      string_array_has(historical_route, "promotion_blockers",
                       "historical_reference_is_not_current_same_session_evidence");
  const bool hal_safety_is_only_precondition =
      string_field_is(hal_safety, "result", "PASS") &&
      bool_field_is(hal_safety, "driver_installed_or_activated_now", false) &&
      string_field_is(hal_safety, "readiness_claim",
                      "DIAGNOSTIC_ONLY_HAL_ENUMERATION_SAFE_NOT_SOUND_QUALITY_READY");
  const bool diagnostic_hal_active_precondition =
      string_field_is(hal_safety, "result", "PASS") &&
      bool_field_is(hal_safety, "diagnostic_install_active", true) &&
      bool_field_is(hal_safety, "active_hal_installed_now", true) &&
      bool_field_is(hal_safety, "installed_hash_matches_candidate", true) &&
      bool_field_is(hal_safety, "driver_installed_or_activated_now", true) &&
      bool_field_is(hal_safety, "product_claim_allowed", false) &&
      bool_field_is(hal_safety, "branch_promotion_allowed", false) &&
      string_field_is(hal_safety, "readiness_claim",
                      "DIAGNOSTIC_ONLY_HAL_ENUMERATION_SAFE_NOT_SOUND_QUALITY_READY");
  const bool hal_safety_precondition_ready =
      hal_safety_is_only_precondition || diagnostic_hal_active_precondition;
  const bool no_product_candidate_runs =
      string_field_is(physical_frontier, "result", "PASS") &&
      number_field_is(physical_frontier, "product_candidate_runs", 0.0) &&
      string_field_is(physical_frontier, "readiness_claim",
                      "DIAGNOSTIC_ONLY_NO_EXISTING_PHYSICAL_RUN_PROVES_SUPERIORITY");
  const bool migration_ready_only_offline =
      string_field_is(migration, "result", "PASS") &&
      bool_field_is(migration, "migration_candidate_supported", true) &&
      bool_field_is(migration, "product_ready", false) &&
      bool_field_is(migration, "branch_promotion_supported", false) &&
      gate_array_has_name(migration, "driverkit_usb_request_shutdown_safe");
  const bool scripts_present =
      std::filesystem::is_regular_file(root / "scripts/physical-window-preflight") &&
      std::filesystem::is_regular_file(root / "scripts/run-physical-superiority-window") &&
      std::filesystem::is_regular_file(root / "scripts/run-known-good-route-soundcheck");
  const bool lock_policy_covers_window =
      string_field_is(hardware_policy, "result", "PASS") &&
      string_array_has(hardware_policy, "sensitive_paths",
                       "scripts/run-physical-superiority-window") &&
      string_array_has(hardware_policy, "sensitive_paths",
                       "scripts/run-known-good-route-soundcheck");

  std::vector<std::string> required_next_actions;
  if (current_known_good_output_missing) {
    required_next_actions.push_back("provision_wired_non_audio8_known_good_output");
  }
  required_next_actions.push_back("lock_gated_known_good_non_audio8_route_revalidation");
  required_next_actions.push_back("same_session_mainline_cpp_physical_ab_on_validated_route");
  required_next_actions.push_back("runtime_cpu_superiority_against_mainline");
  required_next_actions.push_back("traktor_timecode_vinyl_physical_gate");
  required_next_actions.push_back("post_reboot_autologin_codex_resume_fix");

  std::vector<std::string> allowed_window_types;
  if (evidence_present && offline_clean && route_inventory_clean && scripts_present &&
      lock_policy_covers_window && current_promotion_route_ready) {
    allowed_window_types.push_back("ROUTE_REVALIDATION_ONLY");
  }
  if (evidence_present && offline_clean && route_inventory_clean && scripts_present &&
      lock_policy_covers_window && !current_promotion_route_ready) {
    allowed_window_types.push_back("NO_ROUTE_REVALIDATION_UNTIL_NON_AUDIO8_SOURCE_VISIBLE");
  }
  if (route_not_valid_for_promotion) {
    allowed_window_types.push_back("NO_PROMOTION_AB_UNTIL_ROUTE_PASS");
  }

  const bool route_revalidation_plan_ready =
      evidence_present && offline_clean && scripts_present && lock_policy_covers_window &&
      route_inventory_clean && migration_ready_only_offline && hal_safety_precondition_ready &&
      product_blocked && route_not_valid_for_promotion && direct_usb_blocks_route_claim &&
      historical_route_not_current && no_product_candidate_runs;
  const bool ready_for_route_revalidation_window =
      route_revalidation_plan_ready && current_promotion_route_ready;
  const bool ready_for_product_physical_ab =
      false;  // Product A/B remains blocked until the route is revalidated under lock.
  const bool ready_for_branch_promotion = false;

  const bool pass =
      evidence_present && offline_clean && product_blocked && route_not_valid_for_promotion &&
      direct_usb_blocks_route_claim && historical_route_not_current &&
      hal_safety_precondition_ready && no_product_candidate_runs &&
      migration_ready_only_offline && route_inventory_clean && scripts_present &&
      lock_policy_covers_window && route_revalidation_plan_ready &&
      !ready_for_product_physical_ab &&
      !ready_for_branch_promotion;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.physical-window-readiness-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means physical-window blockers are explicit and route revalidation can be planned; it is not product readiness\",\n"
            << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
            << "  \"offline_clean\": " << (offline_clean ? "true" : "false") << ",\n"
            << "  \"product_blocked\": " << (product_blocked ? "true" : "false") << ",\n"
            << "  \"route_not_valid_for_promotion\": "
            << (route_not_valid_for_promotion ? "true" : "false") << ",\n"
            << "  \"direct_usb_blocks_route_claim\": "
            << (direct_usb_blocks_route_claim ? "true" : "false") << ",\n"
            << "  \"historical_route_not_current\": "
            << (historical_route_not_current ? "true" : "false") << ",\n"
            << "  \"hal_safety_is_only_precondition\": "
            << (hal_safety_is_only_precondition ? "true" : "false") << ",\n"
            << "  \"diagnostic_hal_active_precondition\": "
            << (diagnostic_hal_active_precondition ? "true" : "false") << ",\n"
            << "  \"hal_safety_precondition_ready\": "
            << (hal_safety_precondition_ready ? "true" : "false") << ",\n"
            << "  \"no_product_candidate_runs\": "
            << (no_product_candidate_runs ? "true" : "false") << ",\n"
            << "  \"migration_ready_only_offline\": "
            << (migration_ready_only_offline ? "true" : "false") << ",\n"
            << "  \"route_inventory_clean\": "
            << (route_inventory_clean ? "true" : "false") << ",\n"
            << "  \"current_promotion_route_ready\": "
            << (current_promotion_route_ready ? "true" : "false") << ",\n"
            << "  \"current_known_good_output_missing\": "
            << (current_known_good_output_missing ? "true" : "false") << ",\n"
            << "  \"route_revalidation_plan_ready\": "
            << (route_revalidation_plan_ready ? "true" : "false") << ",\n"
            << "  \"scripts_present\": " << (scripts_present ? "true" : "false") << ",\n"
            << "  \"lock_policy_covers_window\": "
            << (lock_policy_covers_window ? "true" : "false") << ",\n"
            << "  \"ready_for_route_revalidation_window\": "
            << (ready_for_route_revalidation_window ? "true" : "false") << ",\n"
            << "  \"ready_for_product_physical_ab\": "
            << (ready_for_product_physical_ab ? "true" : "false") << ",\n"
            << "  \"ready_for_branch_promotion\": "
            << (ready_for_branch_promotion ? "true" : "false") << ",\n";
  print_string_array("allowed_window_types", allowed_window_types);
  print_string_array("required_next_actions", required_next_actions);
  std::cout << "  \"next_required_action\": \""
            << (current_known_good_output_missing
                    ? "PROVISION_WIRED_NON_AUDIO8_KNOWN_GOOD_OUTPUT_THEN_LOCK_GATED_ROUTE_REVALIDATION"
                    : "LOCK_GATED_KNOWN_GOOD_NON_AUDIO8_ROUTE_REVALIDATION")
            << "\",\n"
            << "  \"blocked_claim\": \"NO_PRODUCT_AB_OR_BRANCH_PROMOTION_UNTIL_ROUTE_REVALIDATION_AND_SAME_SESSION_MAINLINE_CPP_PHYSICAL_COMPARE_PASS\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
