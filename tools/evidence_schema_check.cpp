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

bool bool_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_bool(json, key).has_value();
}

bool number_field_present(std::string_view json, std::string_view key) {
  return opena8djcpp::evidence_json::json_number(json, key).has_value();
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
      root / "local-analysis/cpp-offline/audiophile-precision-claim-gate.json",
      root / "local-analysis/cpp-offline/audiophile-wav-analysis-self-test.json",
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
      root / "local-analysis/cpp-offline/product-quality-claim-gate.json",
      root / "local-analysis/cpp-offline/hal-transport-runtime-gate.json",
      root / "local-analysis/cpp-offline/hal-logical-capture-batching-contract.json",
      root / "local-analysis/cpp-offline/hal-runtime-geometry-observability-contract.json",
      root / "local-analysis/cpp-offline/physical-submit-comparison-contract.json",
      root / "local-analysis/cpp-offline/evidence-provenance-freshness-gate.json",
      root / "local-analysis/cpp-offline/static-policy.json",
      root / "local-analysis/cpp-offline/hardware-lock-policy.json",
      root / "local-analysis/cpp-offline/promotion-readiness-offline-check.json",
      root / "local-analysis/cpp-offline/promotion-window-contract.txt",
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
  const auto audiophile_precision_claim_gate =
      opena8djcpp::evidence_json::json_object(summary, "audiophile_precision_claim_gate")
          .value_or("");
  const auto audiophile_wav_analysis_self_test =
      opena8djcpp::evidence_json::json_object(summary, "audiophile_wav_analysis_self_test")
          .value_or("");
  const auto dvs_timecode_stress_margin =
      opena8djcpp::evidence_json::json_object(summary, "dvs_timecode_stress_margin")
          .value_or("");
  const auto physical_window_readiness_gate =
      opena8djcpp::evidence_json::json_object(summary, "physical_window_readiness_gate").value_or("");
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
  const auto promotion_window_contract =
      opena8djcpp::evidence_json::json_object(summary, "promotion_window_contract").value_or("");
  const bool summary_pass =
      string_field_is(summary, "status", "PASS") &&
      string_field_is(summary, "diagnostic_status", "PASS") &&
      bool_field_is(summary, "branch_promotion_allowed", false) &&
      bool_field_is(summary, "physical_measurement_valid_for_promotion", false) &&
      string_array_has(summary, "promotion_hard_blockers",
                       "single_physical_promotion_evidence_bundle_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "same_window_known_good_route_revalidation_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "diagnostic_physical_window_not_promotable") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "same_session_mainline_cpp_physical_ab_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "traktor_timecode_vinyl_physical_gate_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "runtime_cpu_superiority_over_mainline_missing") &&
      string_array_has(summary, "promotion_hard_blockers",
                       "real_driverkit_sdk_and_selected_xcode_missing") &&
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
      object_present(summary, "driverkit_sdk_preflight_gate") &&
      bool_field_is(driverkit_sdk_preflight_gate, "product_driverkit_build_allowed", false) &&
      bool_field_is(driverkit_sdk_preflight_gate, "real_driverkit_claim_blocked", true) &&
      number_field_present(driverkit_sdk_preflight_gate, "applications_free_gib") &&
      number_field_present(driverkit_sdk_preflight_gate, "xcode_install_minimum_free_gib") &&
      bool_field_present(driverkit_sdk_preflight_gate, "xcode_install_disk_space_ok") &&
      bool_field_present(driverkit_sdk_preflight_gate,
                         "noninteractive_xcode_install_prerequisites_met") &&
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
      object_present(summary, "audiophile_wav_analysis_self_test") &&
      string_field_is(audiophile_wav_analysis_self_test, "status", "PASS") &&
      string_field_is(audiophile_wav_analysis_self_test, "schema",
                      "opena8djcpp.audiophile-wav-analysis.v1") &&
      bool_field_is(audiophile_wav_analysis_self_test, "leakage_evaluable", true) &&
      bool_field_is(audiophile_wav_analysis_self_test, "product_claim_allowed", false) &&
      number_field_present(audiophile_wav_analysis_self_test, "alignment_score") &&
      number_field_present(audiophile_wav_analysis_self_test, "left_snr_db") &&
      number_field_present(audiophile_wav_analysis_self_test, "right_snr_db") &&
      number_field_present(audiophile_wav_analysis_self_test, "left_mid_active_coherence") &&
      number_field_present(audiophile_wav_analysis_self_test, "right_mid_active_coherence") &&
      number_field_present(audiophile_wav_analysis_self_test, "delay_p95_frames") &&
      number_field_present(audiophile_wav_analysis_self_test,
                           "worst_offdiag_db_relative") &&
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
      object_present(summary, "product_quality_claim_gate") &&
      object_present(summary, "hal_transport_runtime_gate") &&
      bool_field_is(hal_transport_runtime_gate, "runtime_reduction_missing", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_direct_usb_enqueue", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_no_runtime_prepared_submit", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_logical_physical_capture_split", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_capture_submit_counter", true) &&
      bool_field_is(hal_transport_runtime_gate, "hal_has_playback_submit_counter", true) &&
      bool_field_is(hal_transport_runtime_gate, "control_exposes_submit_counters", true) &&
      bool_field_is(hal_transport_runtime_gate, "soundcheck_tsv_captures_submit_counters", true) &&
      bool_field_is(hal_transport_runtime_gate, "analyzer_summarizes_submit_counters", true) &&
      bool_field_is(hal_transport_runtime_gate, "capture_submit_counter_success_only", true) &&
      bool_field_is(hal_transport_runtime_gate, "playback_submit_counter_success_only", true) &&
      bool_field_is(hal_transport_runtime_gate, "runtime_submit_observability_present", true) &&
      bool_field_is(hal_transport_runtime_gate, "product_claim_blocked", true) &&
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
      string_field_is(
          hal_logical_capture_batching_contract, "blocked_claim",
          "NO_RUNTIME_CPU_SUPERIORITY_CLAIM_UNTIL_OPT_IN_CAPTURE_BATCHING_HAS_SAME_WINDOW_PHYSICAL_AB_METRICS") &&
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
      object_present(summary, "promotion_window_contract") &&
      string_field_is(promotion_window_contract, "status", "PASS") &&
      bool_field_is(promotion_window_contract, "same_window_known_good_route_required", true) &&
      bool_field_is(promotion_window_contract, "diagnostic_window_blocked", true) &&
      object_present(summary, "evidence_provenance_freshness_gate") &&
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
