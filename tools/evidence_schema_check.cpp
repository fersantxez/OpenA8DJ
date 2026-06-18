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

bool string_field_is_last(std::string_view json,
                          std::string_view key,
                          std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_last(json, key).value_or("") == expected;
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

bool bool_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_bool(json, key).has_value();
}

bool number_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_number(json, key).has_value();
}

bool string_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_string(json, key).has_value();
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
      root / "local-analysis/cpp-offline/hal-prepared-runtime-candidate.json",
      root / "local-analysis/cpp-offline/hal-prepared-runtime-bundle-complete.json",
      root / "local-analysis/cpp-offline/hal-prepared-lite-candidate.json",
      root / "local-analysis/cpp-offline/hal-prepared-lite-bundle-complete.json",
      root / "local-analysis/physical-evidence-window/"
             "20260618T213212Z-goal-continuation-prepared-lite-source-reference/"
             "source-reference-ab/same-session-physical-compare.json",
      root / "local-analysis/cpp-offline/hal-playback-scheduler-candidate.json",
      root / "local-analysis/cpp-offline/hal-playback-scheduler-bundle-complete.json",
      root / "local-analysis/cpp-offline/hal-hotpath-diagnostic-candidate.json",
      root / "local-analysis/cpp-offline/hal-hotpath-diagnostic-bundle-complete.json",
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
      root / "local-analysis/cpp-offline/dvs-timecode-stress-margin.json",
      root / "local-analysis/cpp-offline/realtime-audit.json",
      root / "local-analysis/cpp-offline/driverkit-surface-model.json",
      root / "local-analysis/cpp-offline/driverkit-shell-contract.json",
      root / "local-analysis/cpp-offline/driverkit-runtime-contract.json",
      root / "local-analysis/cpp-offline/driverkit-extension-scaffold-contract.json",
      root / "local-analysis/cpp-offline/driverkit-runtime-binding-gap-gate.json",
      root / "local-analysis/cpp-offline/driverkit-device-binding-contract.json",
      root / "local-analysis/cpp-offline/driverkit-sdk-preflight-gate.json",
      root / "local-analysis/cpp-offline/driverkit-prepared-hotpath-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-submit-binding-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-request-lifecycle-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-request-shutdown-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-isochronous-schedule-contract.json",
      root / "local-analysis/cpp-offline/prepared-slot-scheduler-contract.json",
      root / "local-analysis/cpp-offline/playback-scheduler-contract.json",
      root / "local-analysis/cpp-offline/playback-scheduler-runtime-contract.json",
      root / "local-analysis/cpp-offline/runtime-adapter-contract.json",
      root / "local-analysis/cpp-offline/usb-submit-plan-contract.json",
      root / "local-analysis/cpp-offline/usb-submit-payload-contract.json",
      root / "local-analysis/cpp-offline/prepared-usb-runtime-submit-contract.json",
      root / "local-analysis/cpp-offline/prepared-usb-async-runtime-contract.json",
      root / "local-analysis/cpp-offline/persistent-usb-transport-contract.json",
      root / "local-analysis/cpp-offline/hal-prepared-submit-adapter-contract.json",
      root / "local-analysis/cpp-offline/hal-prepared-runtime-source-contract.json",
      root / "local-analysis/cpp-offline/hal-prepared-runtime-binding-contract.json",
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
      root / "local-analysis/cpp-offline/audiophile-precision-claim-gate.json",
      root / "local-analysis/cpp-offline/audiophile-wav-analysis-cpp-self-test.json",
      root / "local-analysis/cpp-offline/audiophile-wav-analysis-self-test.json",
      root / "local-analysis/cpp-offline/audiophile-analysis-stack-contract.json",
      root / "local-analysis/cpp-offline/lti-transfer-quality-cpp-self-test.json",
      root / "local-analysis/cpp-offline/lti-transfer-quality-parity-gate.json",
      root / "local-analysis/cpp-offline/fractional-time-warp-cpp-self-test.json",
      root / "local-analysis/cpp-offline/fractional-time-warp-parity-gate.json",
      root / "local-analysis/cpp-offline/runtime-discontinuity-analysis-cpp-self-test.json",
      root / "local-analysis/cpp-offline/runtime-discontinuity-parity-gate.json",
      root / "local-analysis/cpp-offline/physical-run-product-superiority.json",
      root / "local-analysis/cpp-offline/physical-evidence-frontier.json",
      root / "local-analysis/cpp-offline/physical-capture-forensics.json",
      root / "local-analysis/cpp-offline/direct-usb-path-attribution.json",
      root / "local-analysis/cpp-offline/irig-idle-capture-gate.json",
      root / "local-analysis/cpp-offline/route-contamination-analysis.json",
      root / "local-analysis/cpp-offline/route-contamination-analysis-test.txt",
      root / "local-analysis/cpp-offline/historical-route-reference-gate.json",
      root / "local-analysis/cpp-offline/hal-candidate-safety-gate.json",
      root / "local-analysis/cpp-offline/physical-window-readiness-gate.json",
      root / "local-analysis/cpp-offline/physical-route-inventory.json",
      root / "local-analysis/cpp-offline/known-good-route-selector.json",
      root / "local-analysis/cpp-offline/watch-known-good-route.json",
      root / "local-analysis/cpp-offline/watch-known-good-route-test.txt",
      root / "local-analysis/cpp-offline/physical-evidence-window-plan.json",
      root / "local-analysis/cpp-offline/physical-evidence-window-plan-test.txt",
      root / "local-analysis/cpp-offline/timecode-physical-window-plan.json",
      root / "local-analysis/cpp-offline/timecode-physical-window-plan-test.txt",
      root / "local-analysis/cpp-offline/capture-readiness-contract.json",
      root / "local-analysis/cpp-offline/evidence-json-contract.json",
      root / "local-analysis/cpp-offline/diagnostic-pass-semantics-gate.json",
      root / "local-analysis/cpp-offline/product-quality-claim-gate.json",
      root / "local-analysis/cpp-offline/hal-transport-runtime-gate.json",
      root / "local-analysis/cpp-offline/hal-logical-capture-batching-contract.json",
      root / "local-analysis/cpp-offline/hal-runtime-geometry-observability-contract.json",
      root / "local-analysis/cpp-offline/physical-submit-comparison-contract.json",
      root / "local-analysis/cpp-offline/prepared-runtime-physical-window-contract.json",
      root / "local-analysis/cpp-offline/human-test-rc-gate.json",
      root / "local-analysis/cpp-offline/evidence-provenance-freshness-gate.json",
      root / "local-analysis/cpp-offline/static-policy.json",
      root / "local-analysis/cpp-offline/hardware-lock-policy.json",
      root / "local-analysis/cpp-offline/promotion-readiness-offline-check.json",
      root / "local-analysis/cpp-offline/promotion-window-contract.txt",
      root / "local-analysis/cpp-offline/final-objective-readiness.json",
      root / "local-analysis/cpp-offline/final-objective-readiness-test.txt",
      root / "local-analysis/cpp-offline/human-test-rc-packet.json",
      root / "local-analysis/cpp-offline/human-test-rc-packet.md",
      root / "local-analysis/cpp-offline/human-test-rc-packet-test.txt",
      root / "local-analysis/cpp-offline/objective-external-readiness.json",
      root / "local-analysis/cpp-offline/objective-external-readiness-test.txt",
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
  const auto prepared_usb_runtime_submit_contract =
      opena8djcpp::evidence_json::json_object(summary, "prepared_usb_runtime_submit_contract")
          .value_or("");
  const auto persistent_usb_transport_contract =
      opena8djcpp::evidence_json::json_object(summary, "persistent_usb_transport_contract")
          .value_or("");
  const auto hal_prepared_submit_adapter_contract =
      opena8djcpp::evidence_json::json_object(summary, "hal_prepared_submit_adapter_contract")
          .value_or("");
  const auto hal_prepared_runtime_source_contract =
      opena8djcpp::evidence_json::json_object(summary, "hal_prepared_runtime_source_contract")
          .value_or("");
  const auto hal_prepared_runtime_binding_contract =
      opena8djcpp::evidence_json::json_object(summary, "hal_prepared_runtime_binding_contract")
          .value_or("");
  const auto hal_prepared_runtime_candidate =
      opena8djcpp::evidence_json::json_object(summary, "hal_prepared_runtime_candidate")
          .value_or("");
  const auto hal_prepared_runtime_bundle_complete =
      opena8djcpp::evidence_json::json_object(summary, "hal_prepared_runtime_bundle_complete")
          .value_or("");
  const auto hal_playback_scheduler_candidate =
      opena8djcpp::evidence_json::json_object(summary, "hal_playback_scheduler_candidate")
          .value_or("");
  const auto hal_prepared_lite_candidate =
      opena8djcpp::evidence_json::json_object(summary, "hal_prepared_lite_candidate")
          .value_or("");
  const auto hal_prepared_lite_bundle_complete =
      opena8djcpp::evidence_json::json_object(summary,
                                              "hal_prepared_lite_bundle_complete")
          .value_or("");
  const auto hal_playback_scheduler_bundle_complete =
      opena8djcpp::evidence_json::json_object(summary,
                                              "hal_playback_scheduler_bundle_complete")
          .value_or("");
  const auto hal_hotpath_diagnostic_candidate =
      opena8djcpp::evidence_json::json_object(summary, "hal_hotpath_diagnostic_candidate")
          .value_or("");
  const auto hal_hotpath_diagnostic_bundle_complete =
      opena8djcpp::evidence_json::json_object(summary,
                                              "hal_hotpath_diagnostic_bundle_complete")
          .value_or("");
  const auto driverkit_usb_submit_binding_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_usb_submit_binding_contract")
          .value_or("");
  const auto driverkit_sdk_preflight_gate =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_sdk_preflight_gate").value_or("");
  const auto driverkit_runtime_binding_gap_gate =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_runtime_binding_gap_gate")
          .value_or("");
  const auto driverkit_device_binding_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_device_binding_contract")
          .value_or("");
  const auto driverkit_usb_request_lifecycle_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_usb_request_lifecycle_contract")
          .value_or("");
  const auto driverkit_usb_request_shutdown_contract =
      opena8djcpp::evidence_json::json_object(summary, "driverkit_usb_request_shutdown_contract")
          .value_or("");
  const auto driverkit_usb_isochronous_schedule_contract =
      opena8djcpp::evidence_json::json_object(summary,
                                              "driverkit_usb_isochronous_schedule_contract")
          .value_or("");
  const auto audiophile_precision_claim_gate =
      opena8djcpp::evidence_json::json_object(summary, "audiophile_precision_claim_gate")
          .value_or("");
  const auto audiophile_wav_analysis_cpp_self_test =
      opena8djcpp::evidence_json::json_object(summary, "audiophile_wav_analysis_cpp_self_test")
          .value_or("");
  const auto audiophile_wav_analysis_python_self_test =
      opena8djcpp::evidence_json::json_object(summary, "audiophile_wav_analysis_python_self_test")
          .value_or("");
  const auto audiophile_analysis_stack_contract =
      opena8djcpp::evidence_json::json_object(summary, "audiophile_analysis_stack_contract")
          .value_or("");
  const auto lti_transfer_quality_cpp_self_test =
      opena8djcpp::evidence_json::json_object(summary, "lti_transfer_quality_cpp_self_test")
          .value_or("");
  const auto lti_transfer_quality_parity_gate =
      opena8djcpp::evidence_json::json_object(summary, "lti_transfer_quality_parity_gate")
          .value_or("");
  const auto fractional_time_warp_cpp_self_test =
      opena8djcpp::evidence_json::json_object(summary, "fractional_time_warp_cpp_self_test")
          .value_or("");
  const auto fractional_time_warp_parity_gate =
      opena8djcpp::evidence_json::json_object(summary, "fractional_time_warp_parity_gate")
          .value_or("");
  const auto runtime_discontinuity_analysis_cpp_self_test =
      opena8djcpp::evidence_json::json_object(summary,
                                              "runtime_discontinuity_analysis_cpp_self_test")
          .value_or("");
  const auto runtime_discontinuity_parity_gate =
      opena8djcpp::evidence_json::json_object(summary, "runtime_discontinuity_parity_gate")
          .value_or("");
  const auto dvs_timecode_stress_margin =
      opena8djcpp::evidence_json::json_object(summary, "dvs_timecode_stress_margin")
          .value_or("");
  const auto physical_window_readiness_gate =
      opena8djcpp::evidence_json::json_object(summary, "physical_window_readiness_gate").value_or("");
  const auto capture_route_health_gate =
      opena8djcpp::evidence_json::json_object(summary, "capture_route_health_gate").value_or("");
  const auto direct_usb_path_attribution =
      opena8djcpp::evidence_json::json_object(summary, "direct_usb_path_attribution").value_or("");
  const auto direct_usb_latest_run =
      opena8djcpp::evidence_json::json_object(direct_usb_path_attribution, "latest_run")
          .value_or("");
  const auto route_contamination_analysis =
      opena8djcpp::evidence_json::json_object(summary, "route_contamination_analysis")
          .value_or("");
  const auto hal_candidate_safety_gate =
      opena8djcpp::evidence_json::json_object(summary, "hal_candidate_safety_gate").value_or("");
  const auto physical_route_inventory =
      opena8djcpp::evidence_json::json_object(summary, "physical_route_inventory").value_or("");
  const auto known_good_route_selector =
      opena8djcpp::evidence_json::json_object(summary, "known_good_route_selector").value_or("");
  const auto watch_known_good_route =
      opena8djcpp::evidence_json::json_object(summary, "watch_known_good_route").value_or("");
  const auto physical_evidence_window_plan =
      opena8djcpp::evidence_json::json_object(summary, "physical_evidence_window_plan")
          .value_or("");
  const auto timecode_physical_window_plan =
      opena8djcpp::evidence_json::json_object(summary, "timecode_physical_window_plan")
          .value_or("");
  const auto capture_readiness_contract =
      opena8djcpp::evidence_json::json_object(summary, "capture_readiness_contract").value_or("");
  const auto transport_budget_model =
      opena8djcpp::evidence_json::json_object(summary, "transport_budget_model").value_or("");
  const auto hot_path_timing_analysis =
      opena8djcpp::evidence_json::json_object(summary, "hot_path_timing_analysis").value_or("");
  const auto playback_scheduler_contract =
      opena8djcpp::evidence_json::json_object(summary, "playback_scheduler_contract")
          .value_or("");
  const auto playback_scheduler_runtime_contract =
      opena8djcpp::evidence_json::json_object(summary, "playback_scheduler_runtime_contract")
          .value_or("");
  const auto hal_transport_runtime_gate =
      opena8djcpp::evidence_json::json_object(summary, "hal_transport_runtime_gate").value_or("");
  const auto hal_logical_capture_batching_contract =
      opena8djcpp::evidence_json::json_object(summary, "hal_logical_capture_batching_contract")
          .value_or("");
  const auto hal_runtime_geometry_observability_contract =
      opena8djcpp::evidence_json::json_object(
          summary, "hal_runtime_geometry_observability_contract")
          .value_or("");
  const auto physical_submit_comparison_contract =
      opena8djcpp::evidence_json::json_object(summary, "physical_submit_comparison_contract")
          .value_or("");
  const auto prepared_runtime_physical_window_contract =
      opena8djcpp::evidence_json::json_object(summary,
                                              "prepared_runtime_physical_window_contract")
          .value_or("");
  const auto human_test_rc_gate =
      opena8djcpp::evidence_json::json_object(summary, "human_test_rc_gate").value_or("");
  const auto human_test_rc_status_report =
      opena8djcpp::evidence_json::json_object(summary, "human_test_rc_status_report")
          .value_or("");
  const auto promotion_window_contract =
      opena8djcpp::evidence_json::json_object(summary, "promotion_window_contract").value_or("");
  const auto product_quality_claim_gate =
      opena8djcpp::evidence_json::json_object(summary, "product_quality_claim_gate").value_or("");
  const auto final_objective_readiness =
      opena8djcpp::evidence_json::json_object(summary, "final_objective_readiness").value_or("");
  const auto human_test_rc_packet =
      opena8djcpp::evidence_json::json_object(summary, "human_test_rc_packet").value_or("");
  const auto objective_external_readiness =
      opena8djcpp::evidence_json::json_object(summary, "objective_external_readiness")
          .value_or("");
  const bool summary_pass =
      string_field_is(summary, "status", "PASS") &&
      string_field_is_last(summary, "diagnostic_status", "PASS") &&
      bool_field_is(summary, "branch_promotion_allowed", false) &&
      bool_field_is(summary, "route_revalidation_plan_ready", true) &&
      bool_field_present(summary, "current_promotion_route_ready") &&
      bool_field_present(summary, "current_known_good_output_missing") &&
      bool_field_is(summary, "non_audio8_known_good_route_required", false) &&
      bool_field_is(summary, "source_reference_policy_ready", true) &&
      bool_field_is(summary, "ready_for_source_reference_ab_window", true) &&
      bool_field_present(summary, "ready_for_route_revalidation_window") &&
      bool_field_is(summary, "ready_for_product_physical_ab", false) &&
      bool_field_is(summary, "ready_for_branch_promotion", false) &&
      string_field_present(summary, "current_route_next_lock_gated_action") &&
      string_field_present(summary, "current_route_latest_diagnostic_classification") &&
      bool_field_present(summary,
                         "current_route_latest_correlated_loopback_signal_detected") &&
      string_field_present(summary, "capture_readiness_status") &&
      string_field_present(summary, "capture_route_status") &&
      string_field_present(summary, "capture_diagnostic_status") &&
      string_field_present(summary, "capture_next_recovery_action") &&
      bool_field_present(summary, "known_good_route_selector_ready") &&
      string_field_present(summary, "known_good_route_selector_next_action") &&
      string_field_present(summary, "watch_known_good_route_status") &&
      bool_field_present(summary, "watch_known_good_route_ready") &&
      string_field_present(summary, "watch_known_good_route_next_action") &&
      string_field_present(summary, "physical_evidence_window_plan_status") &&
      bool_field_present(summary, "physical_evidence_window_plan_route_only_ready") &&
      bool_field_present(summary, "physical_evidence_window_plan_full_ab_ready") &&
      string_field_present(summary, "physical_evidence_window_plan_next_action") &&
      string_field_present(summary, "timecode_physical_window_plan_status") &&
      bool_field_present(summary, "timecode_physical_window_plan_ready") &&
      string_field_present(summary, "timecode_physical_window_plan_next_action") &&
      object_present(summary, "timecode_physical_window_plan") &&
      string_field_is(timecode_physical_window_plan, "schema",
                      "opena8djcpp.timecode-physical-window-plan.v1") &&
      bool_field_is(timecode_physical_window_plan, "offline_timecode_pass", true) &&
      bool_field_is(timecode_physical_window_plan,
                    "ready_for_lock_gated_timecode_window", false) &&
      bool_field_is(timecode_physical_window_plan,
                    "route_validated_for_timecode_window", false) &&
      bool_field_is(timecode_physical_window_plan, "same_session_physical_ab_ready", false) &&
      bool_field_is(timecode_physical_window_plan, "product_claim_allowed", false) &&
      bool_field_is(timecode_physical_window_plan, "branch_promotion_allowed", false) &&
      bool_field_is(timecode_physical_window_plan,
                    "timecode_vinyl_certification_allowed", false) &&
      string_array_has(timecode_physical_window_plan, "blockers",
                       "same_session_physical_ab_not_ready") &&
      string_array_has(timecode_physical_window_plan, "blockers",
                       "validated_route_and_full_ab_window_not_ready") &&
      string_field_is(summary, "human_test_rc_status", "PASS") &&
      bool_field_present(summary, "human_test_diagnostic_rc_artifacts_ready") &&
      string_field_is(summary, "human_test_rc_status_live",
                      "DIAGNOSTIC_RC_ARTIFACTS_READY_SOURCE_REFERENCE_AB_REQUIRED") &&
      string_array_has(summary, "human_test_rc_allowed_window_types",
                       "LOCK_GATED_SOURCE_REFERENCE_AB") &&
      string_array_has(summary, "human_test_rc_allowed_window_types",
                       "NO_PRODUCT_CLAIM_WINDOW") &&
      string_array_has(summary, "human_test_rc_disallowed_claims",
                       "audiophile_quality_superiority") &&
      string_array_has(summary, "human_test_rc_disallowed_claims",
                       "timecode_vinyl_certification") &&
      string_array_has(summary, "human_test_rc_required_before_product_human_test",
                       "same_session_mainline_cpp_physical_ab_pass") &&
      string_field_is(summary, "human_test_rc_route_contamination_classification",
                      "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB") &&
      bool_field_is(summary, "human_test_rc_timecode_physical_window_ready", false) &&
      bool_field_is(summary, "human_test_product_allowed", false) &&
      string_field_present(summary, "human_test_next_required_action") &&
      string_field_is(summary, "final_objective_status", "NOT_READY") &&
      bool_field_is(summary, "final_objective_achieved", false) &&
      bool_field_is(summary, "final_objective_branch_promotion_allowed", false) &&
      string_field_present(summary, "final_objective_next_required_action") &&
      string_field_is(summary, "human_test_rc_packet_status", "DIAGNOSTIC_RC_PACKET_READY") &&
      bool_field_is(summary, "human_test_rc_packet_product_human_test_allowed", false) &&
      bool_field_is(summary, "human_test_rc_packet_objective_achieved", false) &&
      string_field_is(summary, "human_test_rc_packet_external_readiness_status",
                      "BLOCKED") &&
      bool_field_is(summary, "human_test_rc_packet_external_objective_ready", false) &&
      bool_field_is(summary, "human_test_rc_packet_external_promotion_allowed", false) &&
      string_array_has(summary, "human_test_rc_packet_next_commands",
                       "lock_gated_source_reference_mainline_cpp_ab") &&
      string_field_is(summary, "objective_external_readiness_status", "BLOCKED") &&
      bool_field_is(summary, "objective_external_ready", false) &&
      bool_field_is(summary, "objective_external_promotion_allowed", false) &&
      bool_field_is(summary,
                    "objective_external_driverkit_install_or_build_attempt_allowed_now",
                    false) &&
      bool_field_is(summary, "objective_external_route_revalidation_allowed_now", true) &&
      string_array_has(summary, "objective_external_next_required_actions",
                       "RUN_LOCK_GATED_SOURCE_REFERENCE_MAINLINE_CPP_AB_AUDIO8_TO_IRIG") &&
      string_array_has(summary, "objective_external_next_required_actions",
                       "INSTALL_SELECT_FULL_XCODE_WITH_DRIVERKIT_SDK") &&
      string_array_has(summary, "objective_external_next_required_actions",
                       "PREPARE_CLEAN_MAINLINE_REFERENCE_BEFORE_PROMOTION") &&
      string_field_is(summary, "route_contamination_status", "PASS") &&
      string_field_is(summary, "route_contamination_classification",
                      "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB") &&
      bool_field_is(summary, "route_contamination_internal_usb_clean", true) &&
      bool_field_is(summary, "route_contamination_idle_capture_non_silent", true) &&
      bool_field_is(summary, "route_contamination_human_product_test_allowed", false) &&
      string_field_present(summary, "route_contamination_next_required_action") &&
      string_array_has(summary, "current_route_inventory_blockers",
                       "non_audio8_non_builtin_known_good_output_not_visible") &&
      bool_field_is(summary, "physical_measurement_valid_for_promotion", false) &&
      string_array_has(summary, "promotion_hard_blockers",
                       "single_physical_promotion_evidence_bundle_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "same_window_source_reference_ab_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "diagnostic_physical_window_not_promotable") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "same_session_mainline_cpp_physical_ab_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "traktor_timecode_vinyl_physical_gate_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "runtime_cpu_superiority_over_mainline_missing") &&
      object_present(summary, "transport_budget_model") &&
      string_field_is(transport_budget_model, "status", "PASS") &&
      bool_field_is(transport_budget_model,
                    "prepared_runtime_offline_resource_superiority_model_pass", true) &&
      bool_field_is(transport_budget_model, "prepared_runtime_resource_model_inputs_complete",
                    true) &&
      number_field_is(transport_budget_model, "prepared_runtime_submit_reduction_ratio", 8.0) &&
      number_field_is(transport_budget_model, "prepared_runtime_logical_slots_per_usb_submit",
                      8.0) &&
      number_field_present(transport_budget_model,
                           "prepared_runtime_observed_fixed_queue_ticks_per_second") &&
      number_field_present(transport_budget_model,
                           "prepared_runtime_predicted_fixed_queue_ticks_per_second") &&
      number_field_is(transport_budget_model,
                      "prepared_runtime_predicted_fixed_queue_work_reduction_ratio", 8.0) &&
      bool_field_is(transport_budget_model, "playback_scheduler_model_pass", true) &&
      number_field_is(transport_budget_model,
                      "playback_scheduler_stable_playback_submit_reduction_ratio", 8.0) &&
      number_field_present(transport_budget_model,
                           "playback_scheduler_stable_total_submit_reduction_ratio") &&
      bool_field_is(transport_budget_model, "persistent_usb_transport_model_pass", true) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_steady_live_requests_before_drain", 8.0) &&
      number_field_is(transport_budget_model, "persistent_usb_transport_max_live_requests",
                      8.0) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_max_capture_lead_frames", 256.0) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_max_playback_lead_frames", 256.0) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_slot_identity_mismatches", 0.0) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_persistent_slot_reuses", 256.0) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_sequence_gap_errors", 0.0) &&
      number_field_is(transport_budget_model,
                      "persistent_usb_transport_timestamp_gap_errors", 0.0) &&
      bool_field_is(transport_budget_model,
                    "persistent_usb_transport_physical_evidence_present", false) &&
      bool_field_is(transport_budget_model,
                    "persistent_usb_transport_hal_binding_present", false) &&
      bool_field_is(transport_budget_model,
                    "persistent_usb_transport_product_claim_allowed", false) &&
      bool_field_is(transport_budget_model, "runtime_cpu_superiority_claim_allowed", false) &&
      string_array_has(transport_budget_model, "resource_claim_blockers",
                       "same_session_physical_cpu_ab_missing") &&
      object_present(summary, "hot_path_timing_analysis") &&
      string_field_is(hot_path_timing_analysis, "status", "PASS") &&
      string_field_is(hot_path_timing_analysis, "mode", "stored_hot_path_timing_evidence") &&
      string_field_present(hot_path_timing_analysis, "selected_run") &&
      number_field_present(hot_path_timing_analysis, "capture_transfers_per_second") &&
      number_field_present(hot_path_timing_analysis, "playback_transfers_per_second") &&
      number_field_present(hot_path_timing_analysis,
                           "capture_zero_complete_per_capture_transfer") &&
      number_field_present(hot_path_timing_analysis,
                           "capture_transaction_errors_per_capture_transfer") &&
      string_field_is(hot_path_timing_analysis, "attribution",
                      "fixed_queue_requeue_enqueue_dominant") &&
      number_field_present(hot_path_timing_analysis,
                           "fixed_queue_to_playback_fill_ratio") &&
      number_field_present(hot_path_timing_analysis,
                           "nested_sum_to_capture_handler_ratio") &&
      string_field_is(hot_path_timing_analysis, "readiness_claim",
                      "DIAGNOSTIC_ONLY_NOT_PRODUCT_READINESS") &&
      object_present(summary, "playback_scheduler_contract") &&
      string_field_is(playback_scheduler_contract, "status", "PASS") &&
      number_field_is(playback_scheduler_contract,
                      "stable_capture_request_submit_calls", 256.0) &&
      number_field_is(playback_scheduler_contract,
                      "stable_playback_request_submit_calls", 33.0) &&
      number_field_is(playback_scheduler_contract,
                      "stable_playback_submit_reduction_ratio", 8.0) &&
      bool_field_is(playback_scheduler_contract, "physical_evidence_present", false) &&
      bool_field_is(playback_scheduler_contract, "product_claim_allowed", false) &&
      object_present(summary, "playback_scheduler_runtime_contract") &&
      string_field_is(playback_scheduler_runtime_contract, "status", "PASS") &&
      number_field_is(playback_scheduler_runtime_contract,
                      "stable_capture_runtime_submit_calls", 256.0) &&
      number_field_is(playback_scheduler_runtime_contract,
                      "stable_playback_runtime_submit_calls", 33.0) &&
      number_field_is(playback_scheduler_runtime_contract,
                      "stable_playback_logical_slots_submitted", 264.0) &&
      number_field_is(playback_scheduler_runtime_contract,
                      "stable_playback_submit_reduction_ratio", 8.0) &&
      bool_field_is(playback_scheduler_runtime_contract, "physical_evidence_present", false) &&
      bool_field_is(playback_scheduler_runtime_contract, "product_claim_allowed", false) &&
      string_array_has(summary, "promotion_hard_blockers",
                       "real_driverkit_sdk_and_selected_xcode_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                      "post_reboot_autologin_codex_resume_unfixed") &&
      object_present(summary, "hal_candidate_safety_gate") &&
      string_field_is(hal_candidate_safety_gate, "status", "PASS") &&
      string_field_present(hal_candidate_safety_gate, "safety_window_status") &&
      bool_field_present(hal_candidate_safety_gate, "guard_health_pass") &&
      bool_field_present(hal_candidate_safety_gate, "guard_coreaudio_enumeration_pass") &&
      bool_field_present(hal_candidate_safety_gate, "recovery_present") &&
      bool_field_present(hal_candidate_safety_gate, "recovery_unloaded") &&
      bool_field_present(hal_candidate_safety_gate, "recovery_irig_visible") &&
      bool_field_present(hal_candidate_safety_gate, "active_hal_left_loaded") &&
      bool_field_present(hal_candidate_safety_gate,
                         "active_installed_hash_matches_current_candidate") &&
      string_field_present(hal_candidate_safety_gate, "current_candidate_hash") &&
      string_field_present(hal_candidate_safety_gate, "active_installed_hash") &&
      number_field_present(hal_candidate_safety_gate, "guard_coreaudiod_cpu_pct") &&
      number_field_present(hal_candidate_safety_gate, "guard_opena8dj_driver_cpu_pct") &&
      string_field_present(hal_candidate_safety_gate, "guard_max_label") &&
      (string_field_is(hal_candidate_safety_gate, "safety_window_status", "PASS") ||
       string_array_has(summary, "promotion_hard_blockers",
                        "latest_hal_candidate_safety_window_not_passing")) &&
      object_present(summary, "hal_prepared_runtime_candidate") &&
      string_field_is(hal_prepared_runtime_candidate, "status", "PASS") &&
      bool_field_is(hal_prepared_runtime_candidate, "prepared_runtime_enabled", true) &&
      number_field_is(hal_prepared_runtime_candidate, "logical_iso_frames", 8.0) &&
      number_field_is(hal_prepared_runtime_candidate, "capture_iso_frames", 64.0) &&
      number_field_is(hal_prepared_runtime_candidate, "playback_base_iso_frames", 8.0) &&
      number_field_is(hal_prepared_runtime_candidate, "playback_coalesce_transfers", 8.0) &&
      number_field_is(hal_prepared_runtime_candidate, "expected_submit_reduction_ratio", 8.0) &&
      bool_field_is(hal_prepared_runtime_candidate, "default_hal_restored", true) &&
      bool_field_is(hal_prepared_runtime_candidate, "prepared_hash_differs_from_default", true) &&
      bool_field_is(hal_prepared_runtime_candidate, "physical_evidence_present", false) &&
      bool_field_is(hal_prepared_runtime_candidate, "product_claim_allowed", false) &&
      object_present(summary, "hal_prepared_runtime_bundle_complete") &&
      string_field_is(hal_prepared_runtime_bundle_complete, "status", "PASS") &&
      object_present(summary, "hal_prepared_lite_candidate") &&
      string_field_is(hal_prepared_lite_candidate, "status", "PASS") &&
      bool_field_is(hal_prepared_lite_candidate, "prepared_runtime_enabled", true) &&
      string_field_is(hal_prepared_lite_candidate, "prepared_runtime_mode",
                      "capture_and_playback") &&
      bool_field_is(hal_prepared_lite_candidate, "playback_only_runtime", false) &&
      bool_field_is(hal_prepared_lite_candidate, "capture_runtime_enabled", true) &&
      bool_field_is(hal_prepared_lite_candidate, "playback_runtime_enabled", true) &&
      number_field_is(hal_prepared_lite_candidate, "logical_iso_frames", 8.0) &&
      number_field_is(hal_prepared_lite_candidate, "prepared_submit_frames", 16.0) &&
      number_field_is(hal_prepared_lite_candidate, "capture_iso_frames", 16.0) &&
      number_field_is(hal_prepared_lite_candidate, "playback_base_iso_frames", 8.0) &&
      number_field_is(hal_prepared_lite_candidate, "playback_coalesce_transfers", 2.0) &&
      number_field_is(hal_prepared_lite_candidate, "expected_submit_reduction_ratio", 2.0) &&
      bool_field_is(hal_prepared_lite_candidate, "default_hal_restored", true) &&
      bool_field_is(hal_prepared_lite_candidate, "prepared_hash_differs_from_default", true) &&
      bool_field_is(hal_prepared_lite_candidate, "physical_evidence_present", false) &&
      bool_field_is(hal_prepared_lite_candidate, "product_claim_allowed", false) &&
      object_present(summary, "hal_prepared_lite_bundle_complete") &&
      string_field_is(hal_prepared_lite_bundle_complete, "status", "PASS") &&
      object_present(summary, "hal_playback_scheduler_candidate") &&
      string_field_is(hal_playback_scheduler_candidate, "status", "PASS") &&
      bool_field_is(hal_playback_scheduler_candidate, "prepared_runtime_enabled", true) &&
      string_field_is(hal_playback_scheduler_candidate, "prepared_runtime_mode", "playback_only") &&
      bool_field_is(hal_playback_scheduler_candidate, "playback_only_runtime", true) &&
      bool_field_is(hal_playback_scheduler_candidate, "capture_runtime_enabled", false) &&
      bool_field_is(hal_playback_scheduler_candidate, "playback_runtime_enabled", true) &&
      number_field_is(hal_playback_scheduler_candidate, "logical_iso_frames", 8.0) &&
      number_field_is(hal_playback_scheduler_candidate, "prepared_submit_frames", 64.0) &&
      number_field_is(hal_playback_scheduler_candidate, "capture_iso_frames", 8.0) &&
      number_field_is(hal_playback_scheduler_candidate, "playback_base_iso_frames", 8.0) &&
      number_field_is(hal_playback_scheduler_candidate, "playback_coalesce_transfers", 8.0) &&
      number_field_is(hal_playback_scheduler_candidate, "expected_submit_reduction_ratio", 8.0) &&
      bool_field_is(hal_playback_scheduler_candidate, "default_hal_restored", true) &&
      bool_field_is(hal_playback_scheduler_candidate, "prepared_hash_differs_from_default", true) &&
      bool_field_is(hal_playback_scheduler_candidate, "physical_evidence_present", false) &&
      bool_field_is(hal_playback_scheduler_candidate, "product_claim_allowed", false) &&
      object_present(summary, "hal_playback_scheduler_bundle_complete") &&
      string_field_is(hal_playback_scheduler_bundle_complete, "status", "PASS") &&
      object_present(summary, "hal_hotpath_diagnostic_candidate") &&
      string_field_is(hal_hotpath_diagnostic_candidate, "status", "PASS") &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "hot_path_timing_enabled", true) &&
      number_field_is(hal_hotpath_diagnostic_candidate, "hot_stream_stats_interval", 1.0) &&
      bool_field_is(hal_hotpath_diagnostic_candidate,
                    "stream_stats_atomic_accumulators_enabled",
                    true) &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "transfer_ledger_enabled", false) &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "playback_payload_guard_enabled", false) &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "cadence_diagnostic_enabled", false) &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "default_hal_restored", true) &&
      bool_field_is(hal_hotpath_diagnostic_candidate,
                    "candidate_hash_differs_from_default",
                    true) &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "physical_evidence_present", false) &&
      bool_field_is(hal_hotpath_diagnostic_candidate, "product_claim_allowed", false) &&
      string_field_is(hal_hotpath_diagnostic_candidate,
                      "blocked_claim",
                      "NO_AUDIOPHILE_TIMING_OR_CPU_CLAIM_FROM_HOTPATH_DIAGNOSTIC_BUILD_WITHOUT_LOCK_GATED_PHYSICAL_EVIDENCE") &&
      object_present(summary, "hal_hotpath_diagnostic_bundle_complete") &&
      string_field_is(hal_hotpath_diagnostic_bundle_complete, "status", "PASS") &&
      object_present(summary, "runtime_adapter_contract") &&
      number_field_is(runtime_adapter_contract, "stable_usb_submit_reduction_ratio", 8.0) &&
      object_present(summary, "usb_submit_plan_contract") &&
      number_field_is(usb_submit_plan_contract, "stable_logical_slots", 528.0) &&
      number_field_is(usb_submit_plan_contract, "stable_total_frames", 5808.0) &&
      number_field_is(usb_submit_plan_contract, "stable_usb_submit_calls", 66.0) &&
      object_present(summary, "usb_submit_payload_contract") &&
      number_field_is(usb_submit_payload_contract, "descriptors", 66.0) &&
      number_field_is(usb_submit_payload_contract, "total_frames", 5808.0) &&
      object_present(summary, "prepared_usb_runtime_submit_contract") &&
      string_field_is(prepared_usb_runtime_submit_contract, "status", "PASS") &&
      number_field_is(prepared_usb_runtime_submit_contract, "logical_slots", 528.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "capture_logical_slots", 264.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "playback_logical_slots", 264.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "usb_submit_calls", 66.0) &&
      number_field_is(prepared_usb_runtime_submit_contract,
                      "first_descriptor_snapshot_logical_slots", 8.0) &&
      number_field_is(prepared_usb_runtime_submit_contract,
                      "first_descriptor_snapshot_usb_submit_calls", 1.0) &&
      number_field_is(prepared_usb_runtime_submit_contract,
                      "first_descriptor_snapshot_request_submit_calls", 1.0) &&
      number_field_is(prepared_usb_runtime_submit_contract,
                      "first_descriptor_snapshot_max_live_requests", 1.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "partial_submit_calls", 0.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "descriptors", 66.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "capture_descriptors", 33.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "playback_descriptors", 33.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "total_bytes", 185856.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "total_frames", 5808.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "request_submit_calls", 66.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "request_completion_calls", 66.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "request_recycle_calls", 66.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "max_live_requests", 4.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "fallback_allocations", 0.0) &&
      number_field_is(prepared_usb_runtime_submit_contract, "submit_failures", 0.0) &&
      number_field_is(prepared_usb_runtime_submit_contract,
                      "retained_descriptor_overflows", 0.0) &&
      bool_field_is(prepared_usb_runtime_submit_contract, "runtime_safe", true) &&
      bool_field_is(prepared_usb_runtime_submit_contract, "payload_equivalent", true) &&
      object_present(summary, "persistent_usb_transport_contract") &&
      string_field_is(persistent_usb_transport_contract, "status", "PASS") &&
      string_field_is(persistent_usb_transport_contract, "schema",
                      "opena8djcpp.persistent-usb-transport-contract.v1") &&
      number_field_is(persistent_usb_transport_contract, "slots_per_submit", 8.0) &&
      number_field_is(persistent_usb_transport_contract, "frames_per_slot", 8.0) &&
      number_field_is(persistent_usb_transport_contract, "capture_queue_depth", 4.0) &&
      number_field_is(persistent_usb_transport_contract, "playback_queue_depth", 4.0) &&
      number_field_is(persistent_usb_transport_contract, "request_slots", 8.0) &&
      number_field_is(persistent_usb_transport_contract,
                      "steady_completions_per_direction", 128.0) &&
      number_field_is(persistent_usb_transport_contract, "prime_submit_calls", 8.0) &&
      number_field_is(persistent_usb_transport_contract, "steady_submit_calls", 256.0) &&
      number_field_is(persistent_usb_transport_contract, "completion_calls", 256.0) &&
      number_field_is(persistent_usb_transport_contract,
                      "steady_live_requests_before_drain", 8.0) &&
      number_field_is(persistent_usb_transport_contract, "max_live_requests", 8.0) &&
      number_field_is(persistent_usb_transport_contract,
                      "max_capture_live_requests", 4.0) &&
      number_field_is(persistent_usb_transport_contract,
                      "max_playback_live_requests", 4.0) &&
      number_field_is(persistent_usb_transport_contract,
                      "max_capture_lead_frames", 256.0) &&
      number_field_is(persistent_usb_transport_contract,
                      "max_playback_lead_frames", 256.0) &&
      number_field_is(persistent_usb_transport_contract, "sequence_gap_errors", 0.0) &&
      number_field_is(persistent_usb_transport_contract, "timestamp_gap_errors", 0.0) &&
      number_field_is(persistent_usb_transport_contract, "depth_drift_errors", 0.0) &&
      number_field_is(persistent_usb_transport_contract, "slot_identity_mismatches", 0.0) &&
      number_field_is(persistent_usb_transport_contract, "persistent_slot_reuses", 256.0) &&
      number_field_is(persistent_usb_transport_contract, "fallback_allocations", 0.0) &&
      number_field_is(persistent_usb_transport_contract, "cancelled_requests", 8.0) &&
      bool_field_is(persistent_usb_transport_contract, "preallocated_only", true) &&
      bool_field_is(persistent_usb_transport_contract, "bounded_live_requests", true) &&
      bool_field_is(persistent_usb_transport_contract, "stable_queue_depth", true) &&
      bool_field_is(persistent_usb_transport_contract, "continuous_sequences", true) &&
      bool_field_is(persistent_usb_transport_contract, "timestamp_continuity", true) &&
      bool_field_is(persistent_usb_transport_contract, "descriptor_shape_safe", true) &&
      bool_field_is(persistent_usb_transport_contract, "persistent_slot_identity", true) &&
      bool_field_is(persistent_usb_transport_contract, "completion_owned_lifecycle", true) &&
      bool_field_is(persistent_usb_transport_contract, "drained", true) &&
      bool_field_is(persistent_usb_transport_contract, "product_safe", true) &&
      bool_field_is(persistent_usb_transport_contract, "physical_evidence_present", false) &&
      bool_field_is(persistent_usb_transport_contract, "hal_binding_present", false) &&
      bool_field_is(persistent_usb_transport_contract, "product_claim_allowed", false) &&
      string_field_is(
          persistent_usb_transport_contract, "blocked_claim",
          "NO_CPU_AUDIOPHILE_TIMECODE_OR_MAINLINE_SUPERIORITY_CLAIM_FROM_OFFLINE_PERSISTENT_TRANSPORT_MODEL") &&
      object_present(summary, "hal_prepared_submit_adapter_contract") &&
      number_field_is(hal_prepared_submit_adapter_contract, "logical_slots", 528.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "capture_logical_slots", 264.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "playback_logical_slots", 264.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "usb_submit_calls", 66.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "partial_submit_calls", 0.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "total_bytes", 185856.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "total_frames", 5808.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "request_submit_calls", 66.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "request_completion_calls", 66.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "request_recycle_calls", 66.0) &&
      number_field_is(hal_prepared_submit_adapter_contract, "fallback_allocations", 0.0) &&
      bool_field_is(hal_prepared_submit_adapter_contract, "planner_safe", true) &&
      bool_field_is(hal_prepared_submit_adapter_contract, "request_pool_safe", true) &&
      bool_field_is(hal_prepared_submit_adapter_contract, "hal_geometry_preserved", true) &&
      bool_field_is(hal_prepared_submit_adapter_contract, "payload_equivalent", true) &&
      object_present(summary, "hal_prepared_runtime_source_contract") &&
      string_field_is(hal_prepared_runtime_source_contract, "status", "PASS") &&
      bool_field_is(hal_prepared_runtime_source_contract, "prepared_runtime_default_off", true) &&
      bool_field_is(hal_prepared_runtime_source_contract, "prepared_runtime_cflags_exposed", true) &&
      bool_field_is(hal_prepared_runtime_source_contract,
                    "prepared_runtime_opt_in_target_present", true) &&
      bool_field_is(hal_prepared_runtime_source_contract,
                    "prepared_runtime_opt_in_target_build_only", true) &&
      bool_field_is(hal_prepared_runtime_source_contract,
                    "source_has_compile_time_geometry_guards", true) &&
      bool_field_is(hal_prepared_runtime_source_contract,
                    "source_exposes_runtime_geometry_constants", true) &&
      bool_field_is(hal_prepared_runtime_source_contract, "default_geometry_preserved", true) &&
      bool_field_is(hal_prepared_runtime_source_contract, "runtime_claim_still_blocked", true) &&
      object_present(summary, "hal_prepared_runtime_binding_contract") &&
      string_field_is(hal_prepared_runtime_binding_contract, "status", "PASS") &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "opt_in_profile_binds_64_transaction_geometry", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract, "default_runtime_preserved", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "capture_pool_uses_prepared_geometry", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "playback_pool_uses_prepared_geometry", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "prepared_runtime_dispatch_path_present", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "transfer_pool_lifetime_completion_owned", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "capture_enqueue_uses_prepared_geometry", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "playback_enqueue_uses_prepared_geometry", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "prepared_playback_sequence_monotonic_without_explicit_schedule", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "capture_paced_playback_batches_to_prepared_geometry", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "capture_submit_counter_success_only", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "playback_submit_counter_success_only", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "completion_counters_completion_owned", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract,
                    "timestamps_use_physical_counts", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract, "runtime_geometry_observable", true) &&
      bool_field_is(hal_prepared_runtime_binding_contract, "submit_cadence_observable", true) &&
      number_field_is(hal_prepared_runtime_binding_contract, "expected_submit_reduction_ratio",
                      8.0) &&
      bool_field_is(hal_prepared_runtime_binding_contract, "physical_evidence_present", false) &&
      bool_field_is(hal_prepared_runtime_binding_contract, "product_claim_allowed", false) &&
      object_present(summary, "driverkit_usb_submit_binding_contract") &&
      number_field_is(driverkit_usb_submit_binding_contract, "usb_submit_calls", 66.0) &&
      number_field_is(driverkit_usb_submit_binding_contract, "total_frames", 5808.0) &&
      object_present(summary, "driverkit_sdk_preflight_gate") &&
      string_field_is(driverkit_sdk_preflight_gate, "schema",
                      "opena8djcpp.driverkit-sdk-preflight-gate.v2") &&
      bool_field_is(driverkit_sdk_preflight_gate, "product_driverkit_build_allowed", false) &&
      bool_field_is(driverkit_sdk_preflight_gate, "real_driverkit_claim_blocked", true) &&
      string_field_present(driverkit_sdk_preflight_gate, "driverkit_sdk_path") &&
      bool_field_present(driverkit_sdk_preflight_gate, "driverkit_sdk_path_available") &&
      bool_field_present(driverkit_sdk_preflight_gate, "driverkit_sdk_path_exists") &&
      string_field_present(driverkit_sdk_preflight_gate, "driverkit_sdk_version") &&
      bool_field_present(driverkit_sdk_preflight_gate, "driverkit_sdk_version_available") &&
      string_field_present(driverkit_sdk_preflight_gate, "developer_dir_effective") &&
      bool_field_present(driverkit_sdk_preflight_gate, "xcodebuild_driverkit_sdk_visible") &&
      string_field_present(driverkit_sdk_preflight_gate, "driverkit_clang_path") &&
      bool_field_present(driverkit_sdk_preflight_gate, "clang_available") &&
      string_field_present(driverkit_sdk_preflight_gate, "driverkit_iig_path") &&
      bool_field_present(driverkit_sdk_preflight_gate, "iig_available") &&
      string_field_present(driverkit_sdk_preflight_gate, "codesign_path") &&
      bool_field_present(driverkit_sdk_preflight_gate, "codesign_available") &&
      number_field_present(driverkit_sdk_preflight_gate, "applications_free_gib") &&
      number_field_present(driverkit_sdk_preflight_gate, "xcode_install_minimum_free_gib") &&
      bool_field_present(driverkit_sdk_preflight_gate, "xcode_install_disk_space_ok") &&
      bool_field_present(driverkit_sdk_preflight_gate,
                         "noninteractive_xcode_install_prerequisites_met") &&
      bool_field_present(driverkit_sdk_preflight_gate, "build_only_probe_allowed") &&
      string_field_is(
          driverkit_sdk_preflight_gate, "blocked_claim",
          "NO_REAL_DRIVERKIT_DEXT_BUILD_OR_READINESS_CLAIM_WITHOUT_DRIVERKIT_SDK_AND_SELECTED_FULL_XCODE") &&
      object_present(summary, "driverkit_runtime_binding_gap_gate") &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "device_start_io_passthrough", false) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "device_stop_io_passthrough", false) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "device_configuration_change_unsupported", false) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "device_abort_configuration_change_stub", false) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "stream_memory_binding_missing", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "zero_timestamp_binding_missing", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "driver_start_device_uses_default_config", false) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "source_binding_present", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "driver_start_device_configures_binding", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "driver_stop_device_bridged", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "device_start_io_bridged", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "device_stop_io_bridged", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "configuration_change_bridged", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "configuration_abort_bridged", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "source_stream_memory_model_present", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "source_zero_timestamp_model_present", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "placeholder_zero_timestamp_model", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "prepared_backend_available", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "source_binding_complete", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "runtime_binding_blocked", false) &&
      bool_field_is(driverkit_runtime_binding_gap_gate,
                    "real_driverkit_sdk_runtime_blocked", true) &&
      bool_field_is(driverkit_runtime_binding_gap_gate, "product_driverkit_runtime_ready", false) &&
      string_field_is(
          driverkit_runtime_binding_gap_gate, "blocked_claim",
          "NO_REAL_DRIVERKIT_RUNTIME_OR_HARDWARE_READINESS_UNTIL_SOURCE_BINDING_BUILDS_WITH_DRIVERKIT_SDK_AND_PHYSICAL_VALIDATION") &&
      object_present(summary, "driverkit_device_binding_contract") &&
      number_field_is(driverkit_device_binding_contract, "lifecycle_failures", 0.0) &&
      number_field_is(driverkit_device_binding_contract, "memory_failures", 0.0) &&
      number_field_is(driverkit_device_binding_contract, "timestamp_failures", 0.0) &&
      number_field_is(driverkit_device_binding_contract, "config_failures", 0.0) &&
      number_field_is(driverkit_device_binding_contract, "shutdown_failures", 0.0) &&
      number_field_is(driverkit_device_binding_contract, "initial_io_memory_descriptors", 5.0) &&
      number_field_is(driverkit_device_binding_contract, "initial_io_memory_total_bytes", 4096.0) &&
      number_field_is(driverkit_device_binding_contract, "changed_io_memory_total_bytes", 8192.0) &&
      number_field_is(driverkit_device_binding_contract, "stream_memory_publications", 2.0) &&
      number_field_is(driverkit_device_binding_contract, "zero_timestamp_publications", 2.0) &&
      number_field_is(driverkit_device_binding_contract, "configuration_change_accepts", 1.0) &&
      number_field_is(driverkit_device_binding_contract, "configuration_change_rejects", 1.0) &&
      number_field_is(driverkit_device_binding_contract, "stop_io_idempotent_noops", 1.0) &&
      bool_field_is(driverkit_device_binding_contract, "product_driverkit_runtime_ready", false) &&
      string_field_is(
          driverkit_device_binding_contract, "blocked_claim",
          "NO_DRIVERKIT_RUNTIME_READY_CLAIM_UNTIL_BINDING_IS_COMPILED_IN_REAL_DEXT_AND_PHYSICALLY_VALIDATED") &&
      object_present(summary, "driverkit_usb_request_lifecycle_contract") &&
      number_field_is(driverkit_usb_request_lifecycle_contract, "stable_submit_calls", 66.0) &&
      number_field_is(driverkit_usb_request_lifecycle_contract, "stable_completed_frames", 5808.0) &&
      object_present(summary, "driverkit_usb_request_shutdown_contract") &&
      number_field_is(driverkit_usb_request_shutdown_contract, "cancelled_requests", 3.0) &&
      number_field_is(driverkit_usb_request_shutdown_contract, "live_requests_after_stop", 0.0) &&
      object_present(summary, "driverkit_usb_isochronous_schedule_contract") &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_scheduled_descriptors", 66.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_capture_descriptors", 33.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_playback_descriptors", 33.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_late_submits", 0.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_descriptor_shape_errors", 0.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_timestamp_regressions", 0.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_sequence_regressions", 0.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_total_frames", 5808.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_total_bytes", 185856.0) &&
      number_field_is(driverkit_usb_isochronous_schedule_contract,
                      "stable_min_submit_lead_frames", 8.0) &&
      bool_field_is(driverkit_usb_isochronous_schedule_contract,
                    "stable_product_safe", true) &&
      string_field_is(driverkit_usb_isochronous_schedule_contract,
                      "late_submit_rejected_result", "PASS") &&
      string_field_is(driverkit_usb_isochronous_schedule_contract,
                      "timestamp_regression_rejected_result", "PASS") &&
      string_field_is(driverkit_usb_isochronous_schedule_contract,
                      "bad_shape_rejected_result", "PASS") &&
      object_present(summary, "audiophile_precision_claim_gate") &&
      string_field_is(audiophile_precision_claim_gate, "status", "PASS") &&
      bool_field_is(audiophile_precision_claim_gate,
                    "audiophile_precision_claim_allowed", false) &&
      bool_field_is(audiophile_precision_claim_gate, "candidate_lti_pass", false) &&
      bool_field_is(audiophile_precision_claim_gate, "candidate_timewarp_pass", false) &&
      bool_field_is(audiophile_precision_claim_gate, "runtime_correlation_pass", false) &&
      bool_field_is(audiophile_precision_claim_gate, "enough_same_window_runs", false) &&
      number_field_is(audiophile_precision_claim_gate, "same_window_runs", 1.0) &&
      string_array_has(audiophile_precision_claim_gate, "precision_claim_blockers",
                       "candidate_lti_precision_thresholds_not_met") &&
      string_array_has(audiophile_precision_claim_gate, "precision_claim_blockers",
                       "candidate_timewarp_stability_thresholds_not_met") &&
      string_array_has(audiophile_precision_claim_gate, "precision_claim_blockers",
                       "runtime_residual_correlation_above_threshold_or_missing") &&
      string_array_has(audiophile_precision_claim_gate, "precision_claim_blockers",
                       "same_window_statistical_sample_too_small") &&
      string_field_is(
          audiophile_precision_claim_gate, "blocked_claim",
          "NO_AUDIOPHILE_PRECISION_OR_SUPERIORITY_CLAIM_WITHOUT_LTI_TIMEWARP_RUNTIME_AND_STATISTICAL_SAME_WINDOW_PASS") &&
      object_present(summary, "audiophile_wav_analysis_cpp_self_test") &&
      string_field_is(audiophile_wav_analysis_cpp_self_test, "status", "PASS") &&
      string_field_is(audiophile_wav_analysis_cpp_self_test, "schema",
                      "opena8djcpp.audiophile-wav-analysis-cpp.v1") &&
      bool_field_is(audiophile_wav_analysis_cpp_self_test, "leakage_evaluable", true) &&
      bool_field_is(audiophile_wav_analysis_cpp_self_test, "product_claim_allowed", false) &&
      number_field_present(audiophile_wav_analysis_cpp_self_test, "alignment_score") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test, "left_snr_db") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test, "right_snr_db") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test, "left_mid_active_coherence") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test, "right_mid_active_coherence") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "left_residual_burst_p95_to_median_db") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "right_residual_burst_p95_to_median_db") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "left_residual_signal_abs_correlation") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "right_residual_signal_abs_correlation") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "left_residual_peak_to_rms_db") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "right_residual_peak_to_rms_db") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test, "delay_p95_frames") &&
      number_field_present(audiophile_wav_analysis_cpp_self_test,
                           "worst_offdiag_db_relative") &&
      object_present(summary, "audiophile_wav_analysis_python_self_test") &&
      string_field_is(audiophile_wav_analysis_python_self_test, "status", "PASS") &&
      string_field_is(audiophile_wav_analysis_python_self_test, "schema",
                      "opena8djcpp.audiophile-wav-analysis.v1") &&
      bool_field_is(audiophile_wav_analysis_python_self_test, "leakage_evaluable", true) &&
      bool_field_is(audiophile_wav_analysis_python_self_test, "product_claim_allowed", false) &&
      number_field_present(audiophile_wav_analysis_python_self_test, "alignment_score") &&
      number_field_present(audiophile_wav_analysis_python_self_test, "left_snr_db") &&
      number_field_present(audiophile_wav_analysis_python_self_test, "right_snr_db") &&
      number_field_present(audiophile_wav_analysis_python_self_test, "left_mid_active_coherence") &&
      number_field_present(audiophile_wav_analysis_python_self_test, "right_mid_active_coherence") &&
      number_field_present(audiophile_wav_analysis_python_self_test, "delay_p95_frames") &&
      number_field_present(audiophile_wav_analysis_python_self_test,
                           "worst_offdiag_db_relative") &&
      object_present(summary, "audiophile_analysis_stack_contract") &&
      string_field_is(audiophile_analysis_stack_contract, "status", "PASS") &&
      bool_field_is(audiophile_analysis_stack_contract, "dependencies_pinned", true) &&
      bool_field_is(audiophile_analysis_stack_contract, "cpp_analyzer_native", true) &&
      bool_field_is(audiophile_analysis_stack_contract, "python_oracle_retained", true) &&
      bool_field_is(audiophile_analysis_stack_contract, "physical_window_runs_both", true) &&
      bool_field_is(audiophile_analysis_stack_contract, "comparator_requires_both", true) &&
      bool_field_is(audiophile_analysis_stack_contract,
                    "product_claim_blocks_without_both", true) &&
      bool_field_is(audiophile_analysis_stack_contract, "offline_runner_self_tests_both",
                    true) &&
      bool_field_is(audiophile_analysis_stack_contract, "product_claim_allowed", false) &&
      string_array_has(audiophile_analysis_stack_contract, "required_same_session_artifacts",
                       "candidate/audiophile-wav-analysis-cpp.json") &&
      string_array_has(audiophile_analysis_stack_contract, "required_same_session_artifacts",
                       "baseline/audiophile-wav-analysis.json") &&
      string_field_is(
          audiophile_analysis_stack_contract, "blocked_claim",
          "NO_AUDIOPHILE_OR_BRANCH_PROMOTION_CLAIM_WITHOUT_DUAL_CPP_AND_PYTHON_WAV_ANALYZERS_PASSING_ON_SAME_SESSION_MAINLINE_AND_CPP_CAPTURE") &&
      object_present(summary, "lti_transfer_quality_cpp_self_test") &&
      string_field_is(lti_transfer_quality_cpp_self_test, "status", "PASS_DIAGNOSTIC") &&
      string_field_is(lti_transfer_quality_cpp_self_test, "schema",
                      "opena8djcpp.lti-transfer-quality-cpp.v1") &&
      bool_field_is(lti_transfer_quality_cpp_self_test, "self_test", true) &&
      bool_field_is(lti_transfer_quality_cpp_self_test, "product_claim_allowed", false) &&
      number_field_is(lti_transfer_quality_cpp_self_test, "row_count", 1.0) &&
      number_field_present(lti_transfer_quality_cpp_self_test, "min_mid_coherence") &&
      number_field_present(lti_transfer_quality_cpp_self_test, "min_lti_snr_delta_db") &&
      string_field_is(
          lti_transfer_quality_cpp_self_test, "blocked_claim",
          "CPP_LTI_TRANSFER_ANALYSIS_IS_DIAGNOSTIC_UNTIL_PARITY_WITH_PYTHON_AND_SAME_SESSION_PHYSICAL_EVIDENCE_PASS") &&
      object_present(summary, "lti_transfer_quality_parity_gate") &&
      string_field_is(lti_transfer_quality_parity_gate, "status", "PASS") &&
      string_field_is(lti_transfer_quality_parity_gate, "schema",
                      "opena8djcpp.lti-transfer-quality-parity-gate.v1") &&
      bool_field_is(lti_transfer_quality_parity_gate, "evidence_present", true) &&
      bool_field_is(lti_transfer_quality_parity_gate, "lti_parity_pass", true) &&
      bool_field_is(lti_transfer_quality_parity_gate, "cpp_lti_claim_allowed", true) &&
      number_field_is(lti_transfer_quality_parity_gate, "run_count", 2.0) &&
      number_field_present(lti_transfer_quality_parity_gate, "max_lti_snr_delta_db") &&
      number_field_present(lti_transfer_quality_parity_gate, "max_lti_mid_ratio_delta") &&
      string_field_is(
          lti_transfer_quality_parity_gate, "blocked_claim",
          "CPP_LTI_TRANSFER_QUALITY_PARITY_PASSED_FOR_SAVED_EVIDENCE_ONLY_NO_PRODUCT_CLAIM") &&
      object_present(summary, "fractional_time_warp_cpp_self_test") &&
      string_field_is(fractional_time_warp_cpp_self_test, "status", "PASS") &&
      string_field_is(fractional_time_warp_cpp_self_test, "schema",
                      "opena8djcpp.fractional-time-warp-cpp-self-test.v1") &&
      number_field_present(fractional_time_warp_cpp_self_test, "estimated_delay_frames") &&
      number_field_present(fractional_time_warp_cpp_self_test, "score") &&
      bool_field_is(fractional_time_warp_cpp_self_test, "product_claim_allowed", false) &&
      object_present(summary, "fractional_time_warp_parity_gate") &&
      string_field_is(fractional_time_warp_parity_gate, "status", "PASS") &&
      string_field_is(fractional_time_warp_parity_gate, "schema",
                      "opena8djcpp.fractional-time-warp-parity-gate.v1") &&
      bool_field_is(fractional_time_warp_parity_gate, "evidence_present", true) &&
      bool_field_is(fractional_time_warp_parity_gate, "timewarp_parity_pass", true) &&
      bool_field_is(fractional_time_warp_parity_gate, "cpp_timewarp_claim_allowed", true) &&
      number_field_is(fractional_time_warp_parity_gate, "run_count", 2.0) &&
      number_field_present(fractional_time_warp_parity_gate,
                           "max_scalar_improvement_delta_db") &&
      number_field_present(fractional_time_warp_parity_gate,
                           "max_matrix_improvement_delta_db") &&
      string_field_is(
          fractional_time_warp_parity_gate, "blocked_claim",
          "CPP_FRACTIONAL_TIME_WARP_PARITY_PASSED_FOR_SAVED_EVIDENCE_ONLY_NO_PRODUCT_CLAIM") &&
      object_present(summary, "runtime_discontinuity_analysis_cpp_self_test") &&
      string_field_is(runtime_discontinuity_analysis_cpp_self_test, "status", "PASS") &&
      string_field_is(runtime_discontinuity_analysis_cpp_self_test, "schema",
                      "opena8djcpp.runtime-discontinuity-analysis-cpp-self-test.v1") &&
      number_field_present(runtime_discontinuity_analysis_cpp_self_test, "correlation") &&
      bool_field_is(runtime_discontinuity_analysis_cpp_self_test, "product_claim_allowed",
                    false) &&
      object_present(summary, "runtime_discontinuity_parity_gate") &&
      string_field_is(runtime_discontinuity_parity_gate, "status", "PASS") &&
      string_field_is(runtime_discontinuity_parity_gate, "schema",
                      "opena8djcpp.runtime-discontinuity-parity-gate.v1") &&
      bool_field_is(runtime_discontinuity_parity_gate, "evidence_present", true) &&
      bool_field_is(runtime_discontinuity_parity_gate,
                    "runtime_discontinuity_parity_pass", true) &&
      bool_field_is(runtime_discontinuity_parity_gate,
                    "cpp_runtime_discontinuity_claim_allowed", true) &&
      number_field_is(runtime_discontinuity_parity_gate, "python_run_count", 6.0) &&
      number_field_is(runtime_discontinuity_parity_gate, "cpp_run_count", 6.0) &&
      number_field_present(runtime_discontinuity_parity_gate,
                           "max_top_abs_correlation_delta") &&
      number_field_present(runtime_discontinuity_parity_gate,
                           "max_residual_median_delta") &&
      string_field_is(
          runtime_discontinuity_parity_gate, "blocked_claim",
          "CPP_RUNTIME_DISCONTINUITY_PARITY_PASSED_FOR_SAVED_EVIDENCE_ONLY_NO_PRODUCT_CLAIM") &&
      object_present(summary, "dvs_timecode_stress_margin") &&
      string_field_is(dvs_timecode_stress_margin, "status", "PASS") &&
      number_field_is(dvs_timecode_stress_margin, "rows", 48.0) &&
      number_field_is(dvs_timecode_stress_margin, "failures", 0.0) &&
      number_field_is(dvs_timecode_stress_margin, "false_accepts", 0.0) &&
      number_field_is(dvs_timecode_stress_margin, "deck_swaps", 0.0) &&
      number_field_present(dvs_timecode_stress_margin, "min_abs_correlation") &&
      number_field_present(dvs_timecode_stress_margin, "max_frequency_error_ppm") &&
      number_field_present(dvs_timecode_stress_margin, "max_jitter_p95_frames") &&
      number_field_present(dvs_timecode_stress_margin, "max_inactive_leakage_dbfs") &&
      number_field_present(dvs_timecode_stress_margin,
                           "min_inactive_to_active_tone_gap_db") &&
      object_present(summary, "physical_window_readiness_gate") &&
      bool_field_present(physical_window_readiness_gate,
                         "ready_for_route_revalidation_window") &&
      bool_field_is(physical_window_readiness_gate, "route_revalidation_plan_ready", true) &&
      bool_field_present(physical_window_readiness_gate,
                         "current_promotion_route_ready") &&
      bool_field_present(physical_window_readiness_gate,
                         "current_known_good_output_missing") &&
      bool_field_is(physical_window_readiness_gate,
                    "non_audio8_known_good_route_required", false) &&
      bool_field_is(physical_window_readiness_gate, "source_reference_policy_ready", true) &&
      bool_field_is(physical_window_readiness_gate,
                    "ready_for_source_reference_ab_window", true) &&
      bool_field_is(physical_window_readiness_gate, "ready_for_product_physical_ab", false) &&
      bool_field_is(physical_window_readiness_gate, "ready_for_branch_promotion", false) &&
      string_array_has(physical_window_readiness_gate, "allowed_window_types",
                       "SOURCE_REFERENCE_MAINLINE_CPP_AB_UNDER_LOCK") &&
      string_field_is(
          physical_window_readiness_gate, "blocked_claim",
          "NO_PRODUCT_AB_OR_BRANCH_PROMOTION_UNTIL_SOURCE_REFERENCE_MAINLINE_CPP_PHYSICAL_COMPARE_CPU_AND_TIMECODE_PASS") &&
      object_present(summary, "direct_usb_path_attribution") &&
      string_field_is(direct_usb_path_attribution, "status", "PASS") &&
      number_field_present(direct_usb_path_attribution, "run_count") &&
      number_field_present(direct_usb_path_attribution, "capture_failed_after_clean_runs") &&
      object_present(direct_usb_path_attribution, "latest_run") &&
      bool_field_is(direct_usb_latest_run, "internal_clean", true) &&
      bool_field_is(direct_usb_latest_run, "capture_failed", true) &&
      bool_field_is(direct_usb_latest_run, "has_timewarp_evidence", true) &&
      string_field_is(direct_usb_latest_run, "timewarp_classification",
                      "fractional_time_warp_rejected") &&
      bool_field_is(direct_usb_latest_run,
                    "capture_failed_after_clean_not_timewarp_explained", true) &&
      number_field_present(direct_usb_latest_run, "timewarp_scalar_improvement_db") &&
      number_field_present(direct_usb_latest_run, "timewarp_matrix_improvement_db") &&
      number_field_present(direct_usb_latest_run, "usb_alignment_score") &&
      number_field_present(direct_usb_latest_run, "usb_snr_floor_db") &&
      string_field_is(direct_usb_latest_run, "audiophile_wav_analysis_result", "FAIL") &&
      number_field_present(direct_usb_latest_run, "audiophile_alignment_score") &&
      number_field_present(direct_usb_latest_run, "audiophile_snr_floor_db") &&
      number_field_present(direct_usb_latest_run, "audiophile_mid_coherence_floor") &&
      number_field_present(direct_usb_latest_run, "audiophile_delay_p95_frames") &&
      object_present(summary, "route_contamination_analysis") &&
      string_field_is(route_contamination_analysis, "schema",
                      "opena8djcpp.route-contamination-analysis.v1") &&
      string_field_is(route_contamination_analysis, "status", "PASS") &&
      string_field_is(route_contamination_analysis, "classification",
                      "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB") &&
      bool_field_is(route_contamination_analysis, "internal_usb_clean", true) &&
      bool_field_is(route_contamination_analysis, "physical_capture_failed", true) &&
      bool_field_is(route_contamination_analysis, "timewarp_explains_failure", false) &&
      bool_field_is(route_contamination_analysis, "idle_capture_non_silent", true) &&
      bool_field_is(route_contamination_analysis, "contamination_classified", true) &&
      bool_field_is(route_contamination_analysis, "product_claim_allowed", false) &&
      bool_field_is(route_contamination_analysis, "branch_promotion_allowed", false) &&
      bool_field_is(route_contamination_analysis, "timecode_vinyl_claim_allowed", false) &&
      bool_field_is(route_contamination_analysis, "human_product_test_allowed", false) &&
      bool_field_is(route_contamination_analysis, "diagnostic_rc_allowed", true) &&
      string_field_is(route_contamination_analysis, "next_required_action",
                      "RUN_SOURCE_REFERENCE_AUDIO8_TO_IRIG_AB_OR_FIX_IRIG_MIXER_MONITORING") &&
      object_present(summary, "physical_route_inventory") &&
      string_field_is(physical_route_inventory, "status", "PASS") &&
      string_field_is(physical_route_inventory, "schema",
                      "opena8djcpp.physical-route-inventory.v1") &&
      bool_field_present(physical_route_inventory, "irig_capture_visible") &&
      bool_field_present(physical_route_inventory, "irig_output_visible") &&
      bool_field_present(physical_route_inventory, "audio8_usb_visible") &&
      bool_field_present(physical_route_inventory, "audio8_coreaudio_visible") &&
      bool_field_present(physical_route_inventory, "active_hal_installed") &&
      bool_field_present(physical_route_inventory, "promotion_route_ready") &&
      bool_field_present(physical_route_inventory,
                         "same_device_irig_diagnostic_possible") &&
      string_field_present(physical_route_inventory,
                           "latest_same_device_irig_diagnostic_result") &&
      bool_field_present(physical_route_inventory,
                         "latest_known_good_route_valid_for_promotion") &&
      bool_field_present(physical_route_inventory,
                         "latest_known_good_route_diagnostic_only") &&
      string_field_present(physical_route_inventory,
                           "latest_known_good_route_failure_classification") &&
      bool_field_present(physical_route_inventory,
                         "latest_known_good_route_correlated_loopback_signal_detected") &&
      number_field_present(physical_route_inventory,
                           "latest_known_good_route_click_outliers") &&
      number_field_present(physical_route_inventory,
                           "latest_known_good_route_analysis_rc") &&
      number_field_present(physical_route_inventory,
                           "latest_known_good_route_native_rc") &&
      string_field_present(physical_route_inventory,
                           "latest_known_good_route_native_readiness_claim") &&
      bool_field_present(physical_route_inventory,
                         "candidate_hal_window_possible_after_lock") &&
      bool_field_is(physical_route_inventory, "product_promotion_measurement_possible_now",
                    false) &&
      bool_field_is(physical_route_inventory, "hardware_touched", false) &&
      bool_field_is(physical_route_inventory, "audio_played", false) &&
      bool_field_is(physical_route_inventory, "audio_recorded", false) &&
      bool_field_is(physical_route_inventory, "driver_installed_or_activated", false) &&
      object_present(summary, "known_good_route_selector") &&
      string_field_is(known_good_route_selector, "status", "PASS") &&
      string_field_is(known_good_route_selector, "schema",
                      "opena8djcpp.known-good-route-selector.v1") &&
      bool_field_present(known_good_route_selector, "route_revalidation_ready") &&
      number_field_present(known_good_route_selector, "valid_known_good_output_count") &&
      number_field_present(known_good_route_selector, "irig_capture_count") &&
      object_present(known_good_route_selector, "selected_known_good_output") &&
      object_present(known_good_route_selector, "selected_irig_capture") &&
      string_field_present(known_good_route_selector, "next_action") &&
      string_field_present(known_good_route_selector, "evidence") &&
      bool_field_is(known_good_route_selector, "lock_required_before_command", true) &&
      bool_field_is(known_good_route_selector, "product_claim_allowed", false) &&
      bool_field_is(known_good_route_selector, "branch_promotion_allowed", false) &&
      object_present(summary, "watch_known_good_route") &&
      string_field_is(watch_known_good_route, "status", "PASS") &&
      string_field_is(watch_known_good_route, "schema",
                      "opena8djcpp.watch-known-good-route.v1") &&
      string_field_present(watch_known_good_route, "watch_status") &&
      bool_field_present(watch_known_good_route, "route_revalidation_ready") &&
      number_field_present(watch_known_good_route, "attempt_count") &&
      number_field_present(watch_known_good_route, "selector_return_code") &&
      number_field_present(watch_known_good_route, "valid_known_good_output_count") &&
      number_field_present(watch_known_good_route, "irig_capture_count") &&
      string_field_present(watch_known_good_route, "next_action") &&
      string_field_present(watch_known_good_route, "evidence") &&
      bool_field_is(watch_known_good_route, "lock_required_before_command", true) &&
      bool_field_is(watch_known_good_route, "product_claim_allowed", false) &&
      bool_field_is(watch_known_good_route, "branch_promotion_allowed", false) &&
      object_present(summary, "physical_evidence_window_plan") &&
      string_field_is(physical_evidence_window_plan, "status", "PASS") &&
      string_field_is(physical_evidence_window_plan, "schema",
                      "opena8djcpp.physical-evidence-window-plan.v1") &&
      string_field_present(physical_evidence_window_plan, "plan_status") &&
      bool_field_present(physical_evidence_window_plan, "route_revalidation_ready") &&
      bool_field_is(physical_evidence_window_plan, "source_reference_policy_ready", true) &&
      bool_field_is(physical_evidence_window_plan,
                    "non_audio8_known_good_route_required", false) &&
      bool_field_is(physical_evidence_window_plan,
                    "ready_for_source_reference_ab_window", true) &&
      bool_field_present(physical_evidence_window_plan, "route_only_ready") &&
      bool_field_present(physical_evidence_window_plan, "full_ab_ready") &&
      object_present(physical_evidence_window_plan, "candidate") &&
      object_present(physical_evidence_window_plan, "mainline_candidate") &&
      string_field_present(physical_evidence_window_plan, "next_action") &&
      string_field_present(physical_evidence_window_plan, "evidence") &&
      bool_field_is(physical_evidence_window_plan, "lock_required_before_command", true) &&
      bool_field_is(physical_evidence_window_plan, "product_claim_allowed", false) &&
      bool_field_is(physical_evidence_window_plan, "branch_promotion_allowed", false) &&
      object_present(summary, "capture_readiness_contract") &&
      string_field_is(capture_readiness_contract, "status", "PASS") &&
      string_field_is(capture_readiness_contract, "capture_status", "VISIBLE") &&
      string_field_present(capture_readiness_contract, "route_status") &&
      string_field_present(capture_readiness_contract, "diagnostic_status") &&
      bool_field_is(capture_readiness_contract, "inventory_clean", true) &&
      bool_field_is(capture_readiness_contract, "irig_usb_visible", true) &&
      bool_field_is(capture_readiness_contract, "irig_coreaudio_capture_visible", true) &&
      bool_field_is(capture_readiness_contract, "audio8_usb_visible", true) &&
      bool_field_present(capture_readiness_contract, "audio8_coreaudio_visible") &&
      bool_field_is(capture_readiness_contract,
                    "hardware_lock_available_for_current_window", true) &&
      bool_field_present(capture_readiness_contract, "promotion_route_ready") &&
      bool_field_present(capture_readiness_contract,
                         "same_device_irig_diagnostic_possible") &&
      bool_field_present(capture_readiness_contract, "known_good_output_missing") &&
      bool_field_is(capture_readiness_contract,
                    "product_promotion_measurement_possible_now", false) &&
      string_field_present(capture_readiness_contract,
                           "latest_route_failure_classification") &&
      bool_field_present(capture_readiness_contract,
                         "latest_route_correlated_loopback_signal_detected") &&
      number_field_present(capture_readiness_contract, "ready_streak") &&
      number_field_present(capture_readiness_contract, "promotion_ready_streak") &&
      bool_field_is(capture_readiness_contract, "failed_usb_ports_known", false) &&
      string_field_present(capture_readiness_contract, "next_recovery_action") &&
      string_array_has(capture_readiness_contract, "readiness_blockers",
                       "non_audio8_non_builtin_known_good_output_missing") &&
      bool_field_is(capture_readiness_contract, "product_claim_allowed", false) &&
      bool_field_is(capture_readiness_contract, "branch_promotion_allowed", false) &&
      string_field_is(
          capture_readiness_contract, "blocked_claim",
          "NO_CAPTURE_ROUTE_OR_PRODUCT_CLAIM_UNTIL_SOURCE_REFERENCE_AUDIO8_TO_IRIG_AND_MAINLINE_CPP_AB_ARE_VALIDATED_UNDER_LOCK") &&
      object_present(summary, "capture_route_health_gate") &&
      string_field_is(capture_route_health_gate, "status", "PASS") &&
      string_field_is(capture_route_health_gate, "diagnostic_result", "PASS") &&
      bool_field_is(capture_route_health_gate, "diagnostic_pass_not_product_readiness", true) &&
      string_field_is(capture_route_health_gate, "route_measurement_status",
                      "BLOCKED_FOR_PROMOTION") &&
      bool_field_is(capture_route_health_gate, "measurement_valid_for_promotion", false) &&
      bool_field_is(capture_route_health_gate, "product_claim_allowed", false) &&
      bool_field_is(capture_route_health_gate, "branch_promotion_allowed", false) &&
      bool_field_is(capture_route_health_gate,
                    "direct_usb_capture_failed_after_clean_payload", true) &&
      object_present(summary, "diagnostic_pass_semantics_gate") &&
      object_present(summary, "product_quality_claim_gate") &&
      bool_field_is(product_quality_claim_gate, "quality_claim_allowed", false) &&
      bool_field_is(product_quality_claim_gate,
                    "direct_usb_capture_failed_after_clean_payload", true) &&
      bool_field_is(product_quality_claim_gate,
                    "same_session_audiophile_wav_analyzers_pass", false) &&
      bool_field_is(product_quality_claim_gate,
                    "timing_instability_blocks_quality_claim", true) &&
      string_array_has(product_quality_claim_gate, "quality_claim_blockers",
                       "same_session_audiophile_wav_analyzers_missing_or_failing") &&
      string_array_has(product_quality_claim_gate, "quality_claim_blockers",
                       "direct_usb_capture_failed_after_clean_payload") &&
      string_array_has(product_quality_claim_gate, "quality_claim_blockers",
                       "timing_instability_dominant_or_delay_unstable") &&
      string_field_is(
          product_quality_claim_gate, "blocked_claim",
          "NO_AUDIOPHILE_QUALITY_CLAIM_UNTIL_REAL_MUSIC_ANALYZERS_TONE_ROUTE_AND_SAME_SESSION_PROMOTION_PASS") &&
      object_present(summary, "hal_transport_runtime_gate") &&
      bool_field_is(hal_transport_runtime_gate, "runtime_reduction_missing", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_direct_usb_enqueue", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_runtime_prepared_submit_guard", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_prepared_runtime_source_contract_pass",
                    true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_prepared_runtime_binding_contract_pass",
                    true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_prepared_runtime_dispatch_path_present",
                    true) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_prepared_runtime_expected_submit_reduction_ratio", 8.0) &&
      bool_field_is(hal_transport_runtime_gate, "hal_prepared_runtime_default_off", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_prepared_runtime_opt_in_target_present",
                    true) &&
      bool_field_is(hal_transport_runtime_gate,
                    "hal_prepared_runtime_physical_evidence_present", false) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_logical_physical_capture_split", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_capture_submit_counter", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_playback_submit_counter", true) &&
      bool_field_is(hal_transport_runtime_gate, "control_exposes_submit_counters", true) &&
      bool_field_is(hal_transport_runtime_gate, "soundcheck_tsv_captures_submit_counters", true) &&
      bool_field_is(hal_transport_runtime_gate, "analyzer_summarizes_submit_counters", true) &&
      bool_field_is(hal_transport_runtime_gate, "capture_submit_counter_success_only", true) &&
      bool_field_is(hal_transport_runtime_gate, "playback_submit_counter_success_only", true) &&
      bool_field_is(hal_transport_runtime_gate, "runtime_submit_observability_present", true) &&
      bool_field_is(hal_transport_runtime_gate, "usb_enqueue_timing_observability_present",
                    true) &&
      bool_field_is(hal_transport_runtime_gate, "stable_default_load_preserved", true) &&
      bool_field_is(hal_transport_runtime_gate,
                    "rejected_transport_variants_default_off", true) &&
      bool_field_is(hal_transport_runtime_gate, "observability_defaults_preserved", true) &&
      bool_field_is(hal_transport_runtime_gate, "prepared_runtime_not_next_default", true) &&
      bool_field_is(hal_transport_runtime_gate,
                    "playback_scheduler_runtime_contract_pass", true) &&
      number_field_is(hal_transport_runtime_gate,
                      "playback_scheduler_runtime_capture_submits", 256.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "playback_scheduler_runtime_playback_submits", 33.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "playback_scheduler_runtime_reduction_ratio", 8.0) &&
      bool_field_is(hal_transport_runtime_gate, "hal_playback_scheduler_candidate_pass", true) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_playback_scheduler_candidate_capture_iso_frames", 8.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_playback_scheduler_candidate_prepared_submit_frames", 64.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_playback_scheduler_candidate_playback_coalesce_transfers", 8.0) &&
      bool_field_is(hal_transport_runtime_gate, "hal_prepared_lite_candidate_pass", true) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_prepared_lite_candidate_capture_iso_frames", 16.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_prepared_lite_candidate_prepared_submit_frames", 16.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "hal_prepared_lite_candidate_expected_submit_reduction_ratio", 2.0) &&
      bool_field_is(hal_transport_runtime_gate,
                    "hal_prepared_lite_candidate_physical_evidence_present", false) &&
      bool_field_is(hal_transport_runtime_gate, "prepared_lite_physically_rejected", true) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_baseline_quality_alignment_score", 0.063492) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_candidate_quality_alignment_score", 0.739469) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_baseline_capture_submit_calls_per_second",
                      4313.935681) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_candidate_capture_submit_calls_per_second",
                      500.210669) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_baseline_driver_cpu_p95", 5.2) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_candidate_driver_cpu_p95", 10.3) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_baseline_coreaudiod_cpu_p95", 6.0) &&
      number_field_is(hal_transport_runtime_gate,
                      "prepared_lite_physical_candidate_coreaudiod_cpu_p95", 74.4) &&
      bool_field_is(hal_transport_runtime_gate, "playback_scheduler_physically_rejected",
                    true) &&
      bool_field_is(hal_transport_runtime_gate,
                    "default_postclose_physically_rejected_for_product", true) &&
      bool_field_is(hal_transport_runtime_gate,
                    "postclose_cpu_sample_points_to_usbhost_enqueue", true) &&
      bool_field_is(hal_transport_runtime_gate, "product_claim_blocked", true) &&
      string_field_is(
          hal_transport_runtime_gate, "next_cpu_direction",
          "DESIGN_NEW_TRANSPORT_REDUCING_IOUSBHOST_ENQUEUE_OR_DRIVERKIT_USB_RUNTIME") &&
      string_field_is(
          hal_transport_runtime_gate, "next_required_action",
          "KEEP_DEFAULT_STABLE_LOAD_DO_NOT_REPEAT_REJECTED_PLAYBACK_SCHEDULER_IMPLEMENT_NEW_TRANSPORT_CANDIDATE_OFFLINE_FIRST") &&
      string_field_is(
          hal_transport_runtime_gate, "blocked_claim",
          "NO_CPU_OR_AUDIOPHILE_SUPERIORITY_CLAIM_UNTIL_A_NEW_TRANSPORT_CANDIDATE_BEATS_MAINLINE_IN_LOCK_GATED_SAME_SESSION_AB") &&
      object_present(summary, "hal_logical_capture_batching_contract") &&
      bool_field_is(hal_logical_capture_batching_contract, "build_exposes_capture_iso", true) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "default_preserves_legacy_logical_size", true) &&
      bool_field_is(hal_logical_capture_batching_contract, "capture_queue_uses_physical_size",
                    true) &&
      bool_field_is(hal_logical_capture_batching_contract, "capture_clock_uses_physical_size",
                    true) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "capture_paced_playback_accepts_full_batch", true) &&
      bool_field_is(hal_logical_capture_batching_contract, "playback_logical_batcher_still_chunks",
                    true) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "input_decode_batch_capacity_present", false) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "input_decode_batches_before_ring_write", false) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "input_decode_preserves_overflow_fallback", true) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "input_decode_preserves_per_frame_diagnostic", true) &&
      string_field_is(hal_logical_capture_batching_contract,
                      "input_decode_ring_write_reduction_model",
                      "REJECTED_PHYSICAL_CANDIDATE_NOT_ACTIVE_IN_DEFAULT_HAL") &&
      string_field_is(hal_logical_capture_batching_contract,
                      "input_decode_batch_physical_status", "REJECTED") &&
      number_field_is(hal_logical_capture_batching_contract,
                      "input_decode_batch_rejected_quality_alignment_score", 0.112023) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "input_decode_batch_rejected_analog_snr_db", -20.50) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "input_decode_batch_rejected_lag_jumps_gt_2_frames", 45.0) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "input_decode_batch_rejected_driver_cpu_p95_pct", 18.8) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "input_decode_batch_product_candidate_allowed", false) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "input_decode_batch_active_in_default_hal", false) &&
      string_field_present(hal_logical_capture_batching_contract,
                           "input_decode_batch_rejection_evidence") &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "makefile_exposes_capture_batch_v2_diagnostic", true) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_capture_iso_frames", 16.0) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_playback_iso_frames", 8.0) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_playback_coalesce_transfers", 1.0) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "capture_batch_v2_preserves_one_stream_output_surface", true) &&
      string_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_physical_status", "REJECTED") &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_rejected_quality_alignment_score", 0.115437) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_rejected_analog_snr_db", -18.27) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_rejected_lag_jumps_gt_2_frames", 45.0) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_rejected_capture_zero_complete_transactions",
                      43172.0) &&
      number_field_is(hal_logical_capture_batching_contract,
                      "capture_batch_v2_rejected_playback_completion_delta_outliers",
                      2505.0) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "capture_batch_v2_product_candidate_allowed", false) &&
      bool_field_is(hal_logical_capture_batching_contract,
                    "capture_batching_above_iso8_product_blocked", true) &&
      string_field_present(hal_logical_capture_batching_contract,
                           "capture_batch_v2_rejection_evidence") &&
      string_field_is(
          hal_logical_capture_batching_contract, "blocked_claim",
          "NO_RUNTIME_CPU_SUPERIORITY_CLAIM_FROM_CAPTURE_BATCHING_ABOVE_ISO8_AFTER_PHYSICAL_REJECTION_WITH_ZERO_COMPLETE_STORM") &&
      object_present(summary, "hal_runtime_geometry_observability_contract") &&
      bool_field_is(hal_runtime_geometry_observability_contract, "build_exposes_capture_iso",
                    true) &&
      bool_field_is(hal_runtime_geometry_observability_contract,
                    "hal_payload_exposes_runtime_geometry", true) &&
      bool_field_is(hal_runtime_geometry_observability_contract,
                    "control_payload_exposes_runtime_geometry", true) &&
      bool_field_is(hal_runtime_geometry_observability_contract,
                    "snapshot_populates_runtime_geometry", true) &&
      bool_field_is(hal_runtime_geometry_observability_contract,
                    "control_prints_runtime_geometry", true) &&
      string_field_is(
          hal_runtime_geometry_observability_contract, "blocked_claim",
          "NO_PHYSICAL_HAL_QUALITY_OR_PERFORMANCE_CLAIM_WITHOUT_ACTIVE_RUNTIME_GEOMETRY_IN_EVIDENCE") &&
      object_present(summary, "physical_submit_comparison_contract") &&
      bool_field_is(physical_submit_comparison_contract, "analyzer_outputs_submit_rates", true) &&
      bool_field_is(physical_submit_comparison_contract, "soundcheck_records_submit_counters",
                    true) &&
      bool_field_is(physical_submit_comparison_contract,
                    "physical_window_generates_stream_summary", true) &&
      bool_field_is(physical_submit_comparison_contract,
                    "physical_window_generates_transfer_ledger_analysis", true) &&
      bool_field_is(physical_submit_comparison_contract, "compare_reads_submit_rates", true) &&
      bool_field_is(physical_submit_comparison_contract, "compare_has_legacy_fallback", true) &&
      bool_field_is(physical_submit_comparison_contract,
                    "same_session_gates_include_submit_rates", true) &&
      bool_field_is(physical_submit_comparison_contract, "compare_prints_submit_rates", true) &&
      bool_field_is(physical_submit_comparison_contract,
                    "promotion_depends_on_same_session_compare", true) &&
      string_field_is(
          physical_submit_comparison_contract, "blocked_claim",
          "NO_RUNTIME_CPU_OR_RESOURCE_SUPERIORITY_CLAIM_WITHOUT_SAME_SESSION_SUBMIT_CADENCE_COMPARISON") &&
      object_present(summary, "prepared_runtime_physical_window_contract") &&
      string_field_is(prepared_runtime_physical_window_contract, "status", "PASS") &&
      bool_field_is(prepared_runtime_physical_window_contract, "preflight_has_prepared_flag",
                    true) &&
      bool_field_is(prepared_runtime_physical_window_contract, "preflight_hashes_candidate",
                    true) &&
      bool_field_is(prepared_runtime_physical_window_contract,
                    "preflight_binds_to_offline_candidate", true) &&
      bool_field_is(prepared_runtime_physical_window_contract,
                    "preflight_requires_dispatch_contract", true) &&
      bool_field_is(prepared_runtime_physical_window_contract,
                    "preflight_preserves_claim_block", true) &&
      bool_field_is(prepared_runtime_physical_window_contract,
                    "runner_records_manifest_identity", true) &&
      bool_field_is(prepared_runtime_physical_window_contract,
                    "runner_forwards_prepared_flag_to_preflight", true) &&
      string_field_is(
          prepared_runtime_physical_window_contract, "blocked_claim",
          "NO_PREPARED_RUNTIME_PHYSICAL_WINDOW_WITHOUT_OFFLINE_HASH_AND_DISPATCH_EVIDENCE") &&
      object_present(summary, "human_test_rc_gate") &&
      string_field_is(human_test_rc_gate, "status", "PASS") &&
      string_field_is(human_test_rc_gate, "schema", "opena8djcpp.human-test-rc-gate.v1") &&
      bool_field_is(human_test_rc_gate, "bundle_ready", true) &&
      bool_field_present(human_test_rc_gate, "package_present") &&
      bool_field_is(human_test_rc_gate, "lock_policy_ready", true) &&
      bool_field_is(human_test_rc_gate, "capture_visible", true) &&
      bool_field_present(human_test_rc_gate, "diagnostic_rc_artifacts_ready") &&
      bool_field_present(human_test_rc_gate, "diagnostic_hal_installed_now") &&
      bool_field_present(human_test_rc_gate, "diagnostic_install_smoke_allowed_after_lock") &&
      bool_field_present(human_test_rc_gate, "diagnostic_install_allowed_after_lock") &&
      bool_field_present(human_test_rc_gate, "fresh_hal_safety_smoke_required") &&
      bool_field_present(human_test_rc_gate, "route_revalidation_allowed_after_lock") &&
      bool_field_present(human_test_rc_gate, "route_revalidation_ready") &&
      bool_field_is(human_test_rc_gate, "product_human_test_allowed", false) &&
      bool_field_is(human_test_rc_gate, "timecode_vinyl_human_test_allowed", false) &&
      bool_field_is(human_test_rc_gate, "cpu_superiority_claim_allowed", false) &&
      bool_field_is(human_test_rc_gate, "branch_promotion_allowed", false) &&
      bool_field_is(human_test_rc_gate, "no_more_transport_tuning_until_route_validated",
                    true) &&
      string_field_present(human_test_rc_gate, "next_required_action") &&
      object_present(summary, "human_test_rc_status_report") &&
      string_field_is(human_test_rc_status_report, "status",
                      "DIAGNOSTIC_RC_ARTIFACTS_READY_SOURCE_REFERENCE_AB_REQUIRED") &&
      string_field_present(human_test_rc_status_report, "next_action") &&
      string_array_has(human_test_rc_status_report, "allowed_window_types",
                       "LOCK_GATED_SOURCE_REFERENCE_AB") &&
      string_array_has(human_test_rc_status_report, "allowed_window_types",
                       "DIAGNOSTIC_PACKAGE_REVIEW_ONLY") &&
      string_array_has(human_test_rc_status_report, "allowed_window_types",
                       "NO_PRODUCT_CLAIM_WINDOW") &&
      string_array_has(human_test_rc_status_report, "disallowed_claims",
                       "mainline_superiority") &&
      string_array_has(human_test_rc_status_report, "required_before_product_human_test",
                       "same_session_mainline_cpp_physical_ab_pass") &&
      string_field_is(human_test_rc_status_report, "route_contamination_classification",
                      "DOWNSTREAM_ROUTE_CONTAMINATION_OR_MONITORING_AFTER_CLEAN_USB") &&
      bool_field_is(human_test_rc_status_report,
                    "route_contamination_human_product_test_allowed", false) &&
      string_field_is(human_test_rc_status_report, "timecode_physical_window_status",
                      "BLOCKED") &&
      bool_field_is(human_test_rc_status_report, "timecode_physical_window_ready", false) &&
      object_present(summary, "promotion_window_contract") &&
      string_field_is(promotion_window_contract, "status", "PASS") &&
      bool_field_is(promotion_window_contract, "same_window_known_good_route_required", true) &&
      bool_field_is(promotion_window_contract, "diagnostic_window_blocked", true) &&
      bool_field_is(promotion_window_contract, "missing_known_good_route_blocked", true) &&
      bool_field_is(promotion_window_contract, "skip_known_good_window_blocked", true) &&
      bool_field_is(promotion_window_contract, "same_device_diagnostic_window_blocked", true) &&
      bool_field_is(promotion_window_contract, "built_in_acoustic_diagnostic_window_blocked",
                    true) &&
      bool_field_is(promotion_window_contract, "audio8_known_good_output_rejected", true) &&
      bool_field_is(promotion_window_contract, "ambiguous_known_good_output_rejected", true) &&
      bool_field_is(promotion_window_contract, "virtual_capture_window_blocked", true) &&
      bool_field_is(promotion_window_contract, "non_irig_capture_window_blocked", true) &&
      object_present(summary, "final_objective_readiness") &&
      string_field_is(final_objective_readiness, "status", "PASS") &&
      string_field_is(final_objective_readiness, "schema",
                      "opena8djcpp.final-objective-readiness.v1") &&
      string_field_is(final_objective_readiness, "objective_status", "NOT_READY") &&
      bool_field_is(final_objective_readiness, "objective_achieved", false) &&
      bool_field_is(final_objective_readiness, "quality_superiority_proven", false) &&
      bool_field_is(final_objective_readiness, "functionality_superiority_or_parity_proven",
                    false) &&
      bool_field_is(final_objective_readiness, "performance_superiority_proven", false) &&
      bool_field_is(final_objective_readiness, "timecode_vinyl_physical_proven", false) &&
      bool_field_is(final_objective_readiness, "legacy_main_promotion_plan_allowed", false) &&
      bool_field_is(final_objective_readiness, "branch_promotion_allowed", false) &&
      string_array_has(final_objective_readiness, "blockers",
                       "real DriverKit/dext build and runtime readiness are not proven on this host") &&
      string_array_has(final_objective_readiness, "blockers",
                       "source-reference Audio8-to-iRig physical comparison is not validated") &&
      string_array_has(final_objective_readiness, "blockers",
                       "same-session physical quality has not beaten mainline") &&
      string_array_has(final_objective_readiness, "blockers",
                       "runtime CPU/resource superiority over mainline is not physically proven") &&
      string_array_has(final_objective_readiness, "blockers",
                       "physical Traktor/Timecode Vinyl window is blocked or uncertified") &&
      string_array_has(final_objective_readiness, "blockers",
                       "Legacy/main promotion remains forbidden until all objective evidence passes") &&
      string_field_is(final_objective_readiness, "next_required_action",
                      "RUN_SOURCE_REFERENCE_MAINLINE_CPP_AB_CPU_TIMECODE") &&
      object_present(summary, "human_test_rc_packet") &&
      string_field_is(human_test_rc_packet, "status", "PASS") &&
      string_field_is(human_test_rc_packet, "schema",
                      "opena8djcpp.human-test-rc-packet.v1") &&
      string_field_is(human_test_rc_packet, "packet_status",
                      "DIAGNOSTIC_RC_PACKET_READY") &&
      bool_field_is(human_test_rc_packet, "artifacts_ready", true) &&
      bool_field_is(human_test_rc_packet, "objective_achieved", false) &&
      bool_field_is(human_test_rc_packet, "product_human_test_allowed", false) &&
      bool_field_is(human_test_rc_packet, "route_only_ready", true) &&
      bool_field_is(human_test_rc_packet, "full_ab_ready", true) &&
      bool_field_is(human_test_rc_packet, "timecode_physical_window_ready", false) &&
      bool_field_is(human_test_rc_packet, "driverkit_build_allowed", false) &&
      string_field_is(human_test_rc_packet, "external_readiness_status", "BLOCKED") &&
      bool_field_is(human_test_rc_packet, "external_objective_ready", false) &&
      bool_field_is(human_test_rc_packet, "external_promotion_allowed", false) &&
      bool_field_is(human_test_rc_packet, "external_product_human_audio_allowed",
                    false) &&
      bool_field_is(human_test_rc_packet,
                    "external_route_revalidation_allowed_now", true) &&
      bool_field_is(human_test_rc_packet, "legacy_main_promotion_allowed", false) &&
      string_array_has(human_test_rc_packet, "next_commands",
                       "lock_gated_source_reference_mainline_cpp_ab") &&
      string_field_is(human_test_rc_packet, "evidence",
                      "local-analysis/cpp-offline/human-test-rc-packet.json") &&
      object_present(summary, "objective_external_readiness") &&
      string_field_is(objective_external_readiness, "status", "PASS") &&
      string_field_is(objective_external_readiness, "schema",
                      "opena8djcpp.objective-external-readiness.v1") &&
      string_field_is(objective_external_readiness, "external_readiness_status", "BLOCKED") &&
      bool_field_is(objective_external_readiness, "objective_ready", false) &&
      bool_field_is(objective_external_readiness, "promotion_allowed", false) &&
      bool_field_is(objective_external_readiness, "product_human_audio_allowed", false) &&
      bool_field_is(objective_external_readiness,
                    "driverkit_install_or_build_attempt_allowed_now", false) &&
      bool_field_is(objective_external_readiness, "source_reference_policy_ready", true) &&
      bool_field_is(objective_external_readiness,
                    "non_audio8_known_good_route_required", false) &&
      bool_field_is(objective_external_readiness, "route_revalidation_allowed_now", true) &&
      string_array_has(objective_external_readiness, "blockers",
                       "mainline worktree is dirty; use a clean reference before Legacy/main promotion") &&
      string_array_has(objective_external_readiness, "blockers",
                       "full Xcode/DriverKit SDK install is not feasible now; free disk and select full Xcode") &&
      string_array_has(objective_external_readiness, "next_required_actions",
                       "RUN_LOCK_GATED_SOURCE_REFERENCE_MAINLINE_CPP_AB_AUDIO8_TO_IRIG") &&
      string_array_has(objective_external_readiness, "next_required_actions",
                       "INSTALL_SELECT_FULL_XCODE_WITH_DRIVERKIT_SDK") &&
      string_array_has(objective_external_readiness, "next_required_actions",
                       "PREPARE_CLEAN_MAINLINE_REFERENCE_BEFORE_PROMOTION") &&
      object_present(summary, "evidence_provenance_freshness_gate") &&
      bool_field_is(summary, "hardware_touched", false) &&
      bool_field_is(summary, "coreaudio_touched", false) &&
      bool_field_is(summary, "usb_touched", false);

  const bool source_reference_summary_pass =
      string_field_is(summary, "status", "PASS") &&
      string_field_is_last(summary, "diagnostic_status", "PASS") &&
      string_field_is(summary, "product_readiness_status", "FAIL") &&
      bool_field_is(summary, "branch_promotion_allowed", false) &&
      bool_field_is(summary, "source_reference_policy_ready", true) &&
      bool_field_is(summary, "non_audio8_known_good_route_required", false) &&
      bool_field_is(summary, "ready_for_source_reference_ab_window", true) &&
      string_field_is(summary, "physical_evidence_window_plan_status",
                      "SOURCE_REFERENCE_AB_READY") &&
      bool_field_is(summary, "physical_evidence_window_plan_full_ab_ready", true) &&
      string_field_is(summary, "physical_evidence_window_plan_next_action",
                      "ACQUIRE_LOCK_AND_RUN_SOURCE_REFERENCE_AB_COMMAND") &&
      string_field_is(summary, "human_test_rc_status_live",
                      "DIAGNOSTIC_RC_ARTIFACTS_READY_SOURCE_REFERENCE_AB_REQUIRED") &&
      string_array_has(summary, "human_test_rc_allowed_window_types",
                       "LOCK_GATED_SOURCE_REFERENCE_AB") &&
      string_array_has(summary, "human_test_rc_packet_next_commands",
                       "lock_gated_source_reference_mainline_cpp_ab") &&
      string_field_is(summary, "final_objective_status", "NOT_READY") &&
      bool_field_is(summary, "final_objective_achieved", false) &&
      string_array_has(summary, "objective_external_next_required_actions",
                       "RUN_LOCK_GATED_SOURCE_REFERENCE_MAINLINE_CPP_AB_AUDIO8_TO_IRIG") &&
      bool_field_is(summary, "objective_external_route_revalidation_allowed_now", true) &&
      bool_field_is(summary, "hardware_touched", false) &&
      bool_field_is(summary, "coreaudio_touched", false) &&
      bool_field_is(summary, "usb_touched", false) &&
      object_present(summary, "evidence_provenance_freshness_gate");
  const bool post_source_reference_baseline_blocked_summary_pass =
      string_field_is(summary, "status", "PASS") &&
      string_field_is_last(summary, "diagnostic_status", "PASS") &&
      string_field_is(summary, "product_readiness_status", "FAIL") &&
      bool_field_is(summary, "branch_promotion_allowed", false) &&
      bool_field_is(summary, "quality_claim_allowed", false) &&
      bool_field_is(summary, "source_reference_policy_ready", true) &&
      bool_field_is(summary, "non_audio8_known_good_route_required", false) &&
      bool_field_is(summary, "ready_for_source_reference_ab_window", true) &&
      string_field_is(summary, "physical_evidence_window_plan_status",
                      "SOURCE_REFERENCE_AB_READY") &&
      string_field_is(summary, "human_test_rc_status_live", "BLOCKED") &&
      string_field_is(summary, "human_test_rc_packet_status", "BLOCKED") &&
      string_field_is(summary, "human_test_next_required_action",
                      "LOCK_GATED_FRESH_HAL_SAFETY_SMOKE_BEFORE_HUMAN_DIAGNOSTIC_INSTALL") &&
      string_field_is(summary, "final_objective_status", "NOT_READY") &&
      bool_field_is(summary, "final_objective_achieved", false) &&
      string_array_has(summary, "objective_external_next_required_actions",
                       "RUN_LOCK_GATED_SOURCE_REFERENCE_MAINLINE_CPP_AB_AUDIO8_TO_IRIG") &&
      bool_field_is(summary, "objective_external_route_revalidation_allowed_now", true) &&
      bool_field_is(summary, "hardware_touched", false) &&
      bool_field_is(summary, "coreaudio_touched", false) &&
      bool_field_is(summary, "usb_touched", false) &&
      object_present(summary, "evidence_provenance_freshness_gate");
  const bool effective_summary_pass =
      summary_pass || source_reference_summary_pass ||
      post_source_reference_baseline_blocked_summary_pass;

  const auto manifest = read_file(root / "docs/CANDIDATE_MANIFEST.json");
  const bool manifest_pass =
      string_field_is(manifest, "worktree", "/Users/fer/dev/audio8djcpp") &&
      string_field_is(manifest, "scope", "offline_only");

  const bool pass = missing == 0 && effective_summary_pass && manifest_pass;
  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"required_files\": " << required.size() << ",\n"
            << "  \"missing_files\": " << missing << ",\n"
            << "  \"summary_pass\": " << (effective_summary_pass ? "true" : "false") << ",\n"
            << "  \"legacy_summary_pass\": " << (summary_pass ? "true" : "false") << ",\n"
            << "  \"source_reference_summary_pass\": "
            << (source_reference_summary_pass ? "true" : "false") << ",\n"
            << "  \"post_source_reference_baseline_blocked_summary_pass\": "
            << (post_source_reference_baseline_blocked_summary_pass ? "true" : "false")
            << ",\n"
            << "  \"manifest_pass\": " << (manifest_pass ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
