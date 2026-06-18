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

bool string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

bool number_field_is(std::string_view json, std::string_view key, double expected) {
  return opena8djcpp::evidence_json::json_number(json, key).value_or(expected + 1.0) == expected;
}

bool object_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_object(json, key).has_value();
}

bool string_array_has(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_array_contains(json, key, expected);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto executable = std::filesystem::absolute(argv[0]);
  std::filesystem::path root = executable.parent_path();
  while (!root.empty() && !std::filesystem::is_regular_file(root / "CMakeLists.txt")) {
    root = root.parent_path();
  }
  if (root.empty() || root.filename() != "audio8djcpp") {
    root = "/Users/fer/dev/audio8djcpp";
  }

  const std::vector<std::filesystem::path> required = {
      root / "docs/CANDIDATE_MANIFEST.json",
      root / "local-analysis/cpp-offline/current-offline-gates.json",
      root / "local-analysis/cpp-offline/ctest-default.txt",
      root / "local-analysis/cpp-offline/ctest-release.txt",
      root / "local-analysis/cpp-offline/packet-matrix.json",
      root / "local-analysis/cpp-offline/protocol-contract.json",
      root / "local-analysis/cpp-offline/simulated-output-matrix.json",
      root / "local-analysis/cpp-offline/mode2-python-oracle.txt",
      root / "local-analysis/cpp-offline/mode2-cross-oracle-parity.json",
      root / "local-analysis/cpp-offline/timecode-matrix.json",
      root / "local-analysis/cpp-offline/timecode-signal-analysis.json",
      root / "local-analysis/cpp-offline/timecode-readiness-gate.json",
      root / "local-analysis/cpp-offline/dvs-signal-smoke.json",
      root / "local-analysis/cpp-offline/dvs-packet-input-decode.json",
      root / "local-analysis/cpp-offline/realtime-audit.json",
      root / "local-analysis/cpp-offline/driverkit-surface-model.json",
      root / "local-analysis/cpp-offline/driverkit-shell-contract.json",
      root / "local-analysis/cpp-offline/driverkit-runtime-contract.json",
      root / "local-analysis/cpp-offline/driverkit-extension-scaffold-contract.json",
      root / "local-analysis/cpp-offline/driverkit-prepared-hotpath-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-submit-binding-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-request-lifecycle-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-request-shutdown-contract.json",
      root / "local-analysis/cpp-offline/prepared-slot-scheduler-contract.json",
      root / "local-analysis/cpp-offline/runtime-adapter-contract.json",
      root / "local-analysis/cpp-offline/usb-submit-plan-contract.json",
      root / "local-analysis/cpp-offline/usb-submit-payload-contract.json",
      root / "local-analysis/cpp-offline/prepared-transport-pressure-gate.json",
      root / "local-analysis/cpp-offline/prepared-transport-migration-gate.json",
      root / "local-analysis/cpp-offline/jitter-model.json",
      root / "local-analysis/cpp-offline/loopback-quality-analysis.json",
      root / "local-analysis/cpp-offline/capture-matrix-quality-analysis.json",
      root / "local-analysis/cpp-offline/capture-route-health-gate.json",
      root / "local-analysis/cpp-offline/hot-path-timing-analysis.json",
      root / "local-analysis/cpp-offline/quality-root-cause-analysis.json",
      root / "local-analysis/cpp-offline/soundcheck-wav-quality.json",
      root / "local-analysis/cpp-offline/channel-leakage-tone-contract.json",
      root / "local-analysis/cpp-offline/audiophile-tone-gate.json",
      root / "local-analysis/cpp-offline/physical-run-product-superiority.json",
      root / "local-analysis/cpp-offline/physical-evidence-frontier.json",
      root / "local-analysis/cpp-offline/physical-capture-forensics.json",
      root / "local-analysis/cpp-offline/direct-usb-path-attribution.json",
      root / "local-analysis/cpp-offline/irig-idle-capture-gate.json",
      root / "local-analysis/cpp-offline/historical-route-reference-gate.json",
      root / "local-analysis/cpp-offline/hal-candidate-safety-gate.json",
      root / "local-analysis/cpp-offline/physical-window-readiness-gate.json",
      root / "local-analysis/cpp-offline/evidence-json-contract.json",
      root / "local-analysis/cpp-offline/diagnostic-pass-semantics-gate.json",
      root / "local-analysis/cpp-offline/static-policy.json",
      root / "local-analysis/cpp-offline/hardware-lock-policy.json",
      root / "local-analysis/cpp-offline/promotion-readiness-offline-check.json",
      root / "local-analysis/cpp-offline/offline-bench-release.json",
      root / "docs/PHYSICAL_TEST_WINDOW_PLAN.md",
      root / "docs/OFFLINE_READINESS_REPORT.md",
  };

  std::uint32_t missing = 0;
  for (const auto& path : required) {
    if (!std::filesystem::is_regular_file(path)) {
      missing += 1;
    }
  }

  const auto summary = read_file(root / "local-analysis/cpp-offline/current-offline-gates.json");
  const auto runtime_adapter_contract =
      opena8djcpp::evidence_json::json_object(summary, "runtime_adapter_contract").value_or("");
  const auto usb_submit_plan_contract =
      opena8djcpp::evidence_json::json_object(summary, "usb_submit_plan_contract").value_or("");
  const auto usb_submit_payload_contract =
      opena8djcpp::evidence_json::json_object(summary, "usb_submit_payload_contract").value_or("");
  const auto driverkit_usb_submit_binding_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_usb_submit_binding_contract")
          .value_or("");
  const auto driverkit_usb_request_lifecycle_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_usb_request_lifecycle_contract")
          .value_or("");
  const auto driverkit_usb_request_shutdown_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_usb_request_shutdown_contract")
          .value_or("");
  const auto physical_window_readiness_gate =
      opena8djcpp::evidence_json::json_object(summary, "physical_window_readiness_gate").value_or("");
  const bool summary_pass =
      string_field_is(summary, "status", "PASS") &&
      string_field_is(summary, "diagnostic_status", "PASS") &&
      bool_field_is(summary, "branch_promotion_allowed", false) &&
      bool_field_is(summary, "physical_measurement_valid_for_promotion", false) &&
      string_array_has(summary, "promotion_hard_blockers",
                       "single_physical_promotion_evidence_bundle_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "same_session_mainline_cpp_physical_ab_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "traktor_timecode_vinyl_physical_gate_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "runtime_cpu_superiority_over_mainline_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "post_reboot_autologin_codex_resume_unfixed") &&
      object_present(summary, "runtime_adapter_contract") &&
      number_field_is(runtime_adapter_contract, "stable_usb_submit_reduction_ratio", 8.0) &&
      object_present(summary, "usb_submit_plan_contract") &&
      number_field_is(usb_submit_plan_contract, "stable_logical_slots", 528.0) &&
      number_field_is(usb_submit_plan_contract, "stable_total_frames", 5808.0) &&
      number_field_is(usb_submit_plan_contract, "stable_usb_submit_calls", 66.0) &&
      object_present(summary, "usb_submit_payload_contract") &&
      number_field_is(usb_submit_payload_contract, "descriptors", 66.0) &&
      number_field_is(usb_submit_payload_contract, "total_frames", 5808.0) &&
      object_present(summary, "driverkit_usb_submit_binding_contract") &&
      number_field_is(driverkit_usb_submit_binding_contract, "usb_submit_calls", 66.0) &&
      number_field_is(driverkit_usb_submit_binding_contract, "total_frames", 5808.0) &&
      object_present(summary, "driverkit_usb_request_lifecycle_contract") &&
      number_field_is(driverkit_usb_request_lifecycle_contract, "stable_submit_calls", 66.0) &&
      number_field_is(driverkit_usb_request_lifecycle_contract, "stable_completed_frames", 5808.0) &&
      object_present(summary, "driverkit_usb_request_shutdown_contract") &&
      number_field_is(driverkit_usb_request_shutdown_contract, "cancelled_requests", 3.0) &&
      number_field_is(driverkit_usb_request_shutdown_contract, "live_requests_after_stop", 0.0) &&
      object_present(summary, "physical_window_readiness_gate") &&
      bool_field_is(physical_window_readiness_gate, "ready_for_route_revalidation_window", true) &&
      bool_field_is(physical_window_readiness_gate, "ready_for_product_physical_ab", false) &&
      bool_field_is(physical_window_readiness_gate, "ready_for_branch_promotion", false) &&
      string_array_has(physical_window_readiness_gate, "allowed_window_types",
                       "ROUTE_REVALIDATION_ONLY") &&
      string_array_has(physical_window_readiness_gate, "allowed_window_types",
                       "NO_PROMOTION_AB_UNTIL_ROUTE_PASS") &&
      string_field_is(
          physical_window_readiness_gate, "blocked_claim",
          "NO_PRODUCT_AB_OR_BRANCH_PROMOTION_UNTIL_ROUTE_REVALIDATION_AND_SAME_SESSION_MAINLINE_CPP_PHYSICAL_COMPARE_PASS") &&
      object_present(summary, "diagnostic_pass_semantics_gate") &&
      bool_field_is(summary, "hardware_touched", false) &&
      bool_field_is(summary, "coreaudio_touched", false) &&
      bool_field_is(summary, "usb_touched", false);

  const auto manifest = read_file(root / "docs/CANDIDATE_MANIFEST.json");
  const bool manifest_pass =
      string_field_is(manifest, "worktree", "/Users/fer/dev/audio8djcpp") &&
      string_field_is(manifest, "scope", "offline_only");

  const bool pass = missing == 0 && summary_pass && manifest_pass;
  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"required_files\": " << required.size() << ",\n"
            << "  \"missing_files\": " << missing << ",\n"
            << "  \"summary_pass\": " << (summary_pass ? "true" : "false") << ",\n"
            << "  \"manifest_pass\": " << (manifest_pass ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
