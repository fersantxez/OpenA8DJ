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

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

bool bool_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_bool(json, key).has_value();
}

bool string_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_string(json, key).has_value();
}

bool string_array_has(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_array_contains(json, key, expected);
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0U) {
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
  const auto evidence = root / "local-analysis/cpp-offline";
  const auto inventory_json = read_file(evidence / "physical-route-inventory.json");

  const auto decision = opena8djcpp::evidence_json::json_object(inventory_json, "decision");
  const auto lock = opena8djcpp::evidence_json::json_object(inventory_json, "lock");
  const auto latest_route =
      opena8djcpp::evidence_json::json_object(inventory_json,
                                              "latest_known_good_route_diagnostic");

  const bool evidence_present = !inventory_json.empty() && decision.has_value() &&
                                lock.has_value() &&
                                latest_route.has_value();
  const bool inventory_clean =
      evidence_present &&
      string_field_is(inventory_json, "schema", "opena8djcpp.physical-route-inventory.v1") &&
      inventory_json.find("\"result\": \"PASS\"") != std::string::npos &&
      bool_field_is(inventory_json, "hardware_touched", false) &&
      bool_field_is(inventory_json, "audio_played", false) &&
      bool_field_is(inventory_json, "audio_recorded", false) &&
      bool_field_is(inventory_json, "coreaudio_configuration_changed", false) &&
      bool_field_is(inventory_json, "usb_reset_or_configuration_changed", false) &&
      bool_field_is(inventory_json, "driver_installed_or_activated", false);

  const bool irig_usb_visible = bool_field_is(inventory_json, "irig_usb_visible", true);
  const bool irig_coreaudio_capture_visible =
      decision && bool_field_is(*decision, "irig_capture_visible", true);
  const bool audio8_usb_visible = decision && bool_field_is(*decision, "audio8_usb_visible", true);
  const bool audio8_coreaudio_visible =
      decision && bool_field_is(*decision, "audio8_coreaudio_visible", true);
  const bool lock_available = lock && bool_field_is(*lock, "available_for_current_window", true);
  const bool promotion_route_ready =
      decision && bool_field_is(*decision, "promotion_route_ready", true);
  const bool same_device_diagnostic_possible =
      decision && bool_field_is(*decision, "same_device_irig_diagnostic_possible", true);
  const bool known_good_missing =
      decision &&
      string_array_has(*decision, "blockers", "non_audio8_non_builtin_known_good_output_not_visible");
  const bool product_promotion_measurement_possible =
      decision && bool_field_is(*decision, "product_promotion_measurement_possible_now", true);
  const bool latest_route_present = latest_route && bool_field_is(*latest_route, "present", true);
  const bool latest_route_valid_for_promotion =
      latest_route && bool_field_is(*latest_route, "valid_for_promotion", true);
  const bool latest_route_diagnostic_only =
      latest_route && bool_field_is(*latest_route, "diagnostic_only", true);
  const bool latest_route_same_device =
      latest_route && bool_field_is(*latest_route, "same_device_loopback_diagnostic", true);
  const bool latest_route_classified =
      latest_route && string_field_present(*latest_route, "failure_classification");
  const bool latest_route_correlated_field_present =
      latest_route && bool_field_present(*latest_route, "correlated_loopback_signal_detected");
  const bool latest_no_correlated_signal =
      latest_route && string_field_is(*latest_route, "failure_classification",
                                      "no_correlated_loopback_signal_detected");
  const bool next_action_same_device_only =
      decision && string_field_is(*decision, "next_lock_gated_action",
                                  "LOCK_GATED_SAME_DEVICE_IRIG_DIAGNOSTIC_ONLY");
  const bool next_action_known_good_route =
      decision && string_field_is(*decision, "next_lock_gated_action",
                                  "LOCK_GATED_KNOWN_GOOD_ROUTE_REVALIDATION");
  const std::string latest_failure_classification =
      latest_route
          ? opena8djcpp::evidence_json::json_string(*latest_route, "failure_classification")
                .value_or("")
          : std::string{};

  std::vector<std::string> blockers;
  if (!irig_usb_visible) {
    blockers.push_back("irig_usb_not_visible");
  }
  if (!irig_coreaudio_capture_visible) {
    blockers.push_back("irig_coreaudio_capture_not_visible");
  }
  if (!audio8_usb_visible) {
    blockers.push_back("audio8_usb_not_visible");
  }
  if (!audio8_coreaudio_visible) {
    blockers.push_back("audio8_coreaudio_not_visible");
  }
  if (!lock_available) {
    blockers.push_back("hardware_lock_not_available");
  }
  if (known_good_missing) {
    blockers.push_back("non_audio8_non_builtin_known_good_output_missing");
  }
  if (latest_route_present && latest_route_same_device && latest_route_diagnostic_only &&
      !latest_route_valid_for_promotion) {
    blockers.push_back("latest_known_good_route_is_same_device_diagnostic_only");
  }
  if (latest_no_correlated_signal) {
    blockers.push_back("latest_same_device_irig_diagnostic_has_no_correlated_signal");
  }

  const std::string capture_status =
      irig_usb_visible && irig_coreaudio_capture_visible ? "VISIBLE" : "BLOCKED_CAPTURE_MISSING";
  const std::string route_status =
      promotion_route_ready
          ? "READY_FOR_LOCK_GATED_ROUTE_REVALIDATION"
          : (known_good_missing ? "BLOCKED_KNOWN_GOOD_OUTPUT_MISSING" : "BLOCKED_ROUTE_UNCLEAR");
  const std::string diagnostic_status =
      latest_no_correlated_signal ? "DIAGNOSTIC_NO_CORRELATED_LOOPBACK_SIGNAL"
                                  : (same_device_diagnostic_possible
                                         ? "DIAGNOSTIC_ONLY_AVAILABLE"
                                         : "DIAGNOSTIC_BLOCKED");

  const bool blocked_state_explicit =
      !promotion_route_ready && !product_promotion_measurement_possible &&
      known_good_missing && next_action_same_device_only && latest_route_present &&
      latest_route_diagnostic_only && !latest_route_valid_for_promotion &&
      latest_route_classified && latest_route_correlated_field_present;
  const bool future_ready_state_explicit =
      promotion_route_ready && next_action_known_good_route && !known_good_missing &&
      !product_promotion_measurement_possible;
  const bool pass = inventory_clean && irig_usb_visible && irig_coreaudio_capture_visible &&
                    audio8_usb_visible && lock_available &&
                    (blocked_state_explicit || future_ready_state_explicit);

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.capture-readiness-contract.v1\",\n"
            << "  \"safety\": \"offline_existing_route_inventory_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means capture and route readiness are explicitly classified; it is not product readiness\",\n"
            << "  \"capture_status\": \"" << capture_status << "\",\n"
            << "  \"route_status\": \"" << route_status << "\",\n"
            << "  \"diagnostic_status\": \"" << diagnostic_status << "\",\n"
            << "  \"inventory_clean\": " << (inventory_clean ? "true" : "false") << ",\n"
            << "  \"irig_usb_visible\": " << (irig_usb_visible ? "true" : "false") << ",\n"
            << "  \"irig_coreaudio_capture_visible\": "
            << (irig_coreaudio_capture_visible ? "true" : "false") << ",\n"
            << "  \"audio8_usb_visible\": " << (audio8_usb_visible ? "true" : "false")
            << ",\n"
            << "  \"audio8_coreaudio_visible\": "
            << (audio8_coreaudio_visible ? "true" : "false") << ",\n"
            << "  \"hardware_lock_available_for_current_window\": "
            << (lock_available ? "true" : "false") << ",\n"
            << "  \"promotion_route_ready\": " << (promotion_route_ready ? "true" : "false")
            << ",\n"
            << "  \"same_device_irig_diagnostic_possible\": "
            << (same_device_diagnostic_possible ? "true" : "false") << ",\n"
            << "  \"known_good_output_missing\": " << (known_good_missing ? "true" : "false")
            << ",\n"
            << "  \"product_promotion_measurement_possible_now\": "
            << (product_promotion_measurement_possible ? "true" : "false") << ",\n"
            << "  \"latest_route_present\": " << (latest_route_present ? "true" : "false")
            << ",\n"
            << "  \"latest_route_valid_for_promotion\": "
            << (latest_route_valid_for_promotion ? "true" : "false") << ",\n"
            << "  \"latest_route_diagnostic_only\": "
            << (latest_route_diagnostic_only ? "true" : "false") << ",\n"
            << "  \"latest_route_same_device_loopback\": "
            << (latest_route_same_device ? "true" : "false") << ",\n"
            << "  \"latest_route_failure_classification\": \"" << latest_failure_classification
            << "\",\n"
            << "  \"latest_route_correlated_loopback_signal_detected\": "
            << (latest_route && bool_field_is(*latest_route, "correlated_loopback_signal_detected",
                                              true)
                    ? "true"
                    : "false")
            << ",\n"
            << "  \"ready_streak\": " << (irig_usb_visible && irig_coreaudio_capture_visible ? 1 : 0)
            << ",\n"
            << "  \"promotion_ready_streak\": " << (promotion_route_ready ? 1 : 0) << ",\n"
            << "  \"failed_usb_ports_known\": false,\n"
            << "  \"next_recovery_action\": \""
            << (known_good_missing
                    ? "PROVISION_WIRED_NON_AUDIO8_NON_BUILTIN_OUTPUT_FOR_SAME_WINDOW_ROUTE_VALIDATION"
                    : (latest_no_correlated_signal ? "RECOVER_OR_REWIRE_IRIG_ROUTE_BEFORE_PROMOTION"
                                                   : "LOCK_GATED_ROUTE_REVALIDATION"))
            << "\",\n";
  print_string_array("readiness_blockers", blockers);
  std::cout
      << "  \"product_claim_allowed\": false,\n"
      << "  \"branch_promotion_allowed\": false,\n"
      << "  \"blocked_claim\": \"NO_CAPTURE_ROUTE_OR_PRODUCT_CLAIM_UNTIL_IRIG_CAPTURE_AND_NON_AUDIO8_KNOWN_GOOD_ROUTE_ARE_VALIDATED_UNDER_LOCK\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
