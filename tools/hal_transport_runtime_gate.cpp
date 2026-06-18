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

bool contains(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

bool appears_before(std::string_view text, std::string_view first, std::string_view second) {
  const auto first_pos = text.find(first);
  const auto second_pos = text.find(second);
  return first_pos != std::string_view::npos && second_pos != std::string_view::npos &&
         first_pos < second_pos;
}

bool string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

bool bool_last_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool_last(json, key).value_or(!expected) == expected;
}

bool gate_array_has_name(std::string_view json, std::string_view expected) {
  return opena8djcpp::evidence_json::json_object_array_contains_string_field(json, "gates", "name",
                                                                            expected);
}

double number_or(std::string_view json, std::string_view key, double fallback) {
  return opena8djcpp::evidence_json::json_number(json, key).value_or(fallback);
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
  const auto root = repo_root(argv);
  const auto evidence = root / "local-analysis/cpp-offline";

  const auto hal_source = read_file(root / "src/hal/OpenA8DJUSB.m");
  const auto control_source = read_file(root / "src/tools/opena8dj-control.c");
  const auto run_soundcheck = read_file(root / "scripts/run-soundcheck");
  const auto stream_stats_analyzer = read_file(root / "scripts/analyze-stream-stats.py");
  const auto makefile = read_file(root / "Makefile");
  const auto migration = read_file(evidence / "prepared-transport-migration-gate.json");
  const auto product_quality = read_file(evidence / "product-quality-claim-gate.json");
  const auto physical_window = read_file(evidence / "physical-window-readiness-gate.json");
  const auto hal_safety = read_file(evidence / "hal-candidate-safety-gate.json");
  const auto prepared_runtime_source =
      read_file(evidence / "hal-prepared-runtime-source-contract.json");
  const auto prepared_runtime_binding =
      read_file(evidence / "hal-prepared-runtime-binding-contract.json");
  const auto playback_scheduler_runtime =
      read_file(evidence / "playback-scheduler-runtime-contract.json");
  const auto hal_playback_scheduler_candidate =
      read_file(evidence / "hal-playback-scheduler-candidate.json");
  const auto hal_prepared_lite_candidate =
      read_file(evidence / "hal-prepared-lite-candidate.json");
  const auto playback_scheduler_physical_compare =
      read_file(root / "local-analysis/physical-evidence-window/"
                       "20260618T2002Z-playback-scheduler-source-reference-ab-8s/"
                       "same-session-physical-compare.recomputed.json");
  const auto default_postclose_physical_compare =
      read_file(root / "local-analysis/physical-evidence-window/"
                       "20260618T2020Z-default-source-reference-ab-8s-postclose/"
                       "same-session-physical-compare.json");
  const auto prepared_lite_physical_compare =
      read_file(root / "local-analysis/physical-evidence-window/"
                       "20260618T213212Z-goal-continuation-prepared-lite-source-reference/"
                       "source-reference-ab/same-session-physical-compare.json");
  const auto postclose_driver_sample =
      read_file(root / "local-analysis/cpu-sample/"
                       "20260618T2024Z-default-cpp-postclose-driver-sample/"
                       "driver-sample/analysis.json");

  const bool evidence_present = !hal_source.empty() && !control_source.empty() &&
                                !run_soundcheck.empty() && !stream_stats_analyzer.empty() &&
                                !makefile.empty() && !migration.empty() &&
                                !product_quality.empty() && !physical_window.empty() &&
                                !hal_safety.empty() && !prepared_runtime_source.empty() &&
                                !prepared_runtime_binding.empty() &&
                                !playback_scheduler_runtime.empty() &&
                                !hal_playback_scheduler_candidate.empty() &&
                                !hal_prepared_lite_candidate.empty() &&
                                !playback_scheduler_physical_compare.empty() &&
                                !default_postclose_physical_compare.empty() &&
                                !prepared_lite_physical_compare.empty() &&
                                !postclose_driver_sample.empty();

  const bool hal_has_direct_usb_enqueue =
      contains(hal_source, "enqueueIORequestWithData:transfer.data") &&
      contains(hal_source, "- (BOOL)queuePlaybackWithRequests:") &&
      contains(hal_source, "[self queueCaptureTransfer]");
  const bool hal_default_capture_paced =
      contains(makefile, "HAL_PLAYBACK_CAPTURE_PACED ?= 1") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1");
  const bool rejected_transport_variants_default_off =
      contains(makefile, "HAL_USB_CLOCK_ANCHOR ?= 0") &&
      contains(makefile, "HAL_PREPARED_USB_SUBMIT_RUNTIME ?= 0") &&
      contains(makefile, "HAL_REUSE_ISOC_COMPLETIONS ?= 0") &&
      contains(makefile, "HAL_RAW_ISOC_COMPLETIONS ?= 0") &&
      contains(makefile, "HAL_FAST_ISO_TRANSFER_CONFIG ?= 0") &&
      contains(makefile, "HAL_IGNORE_OUTPUT_SAMPLE_TIME ?= 0") &&
      contains(makefile, "HAL_FLUSH_OUTPUT_IN_WRITE_MIX ?= 0");
  const bool stable_default_load_preserved =
      contains(makefile, "HAL_ISO_FRAMES ?= 8") &&
      contains(makefile, "HAL_PLAYBACK_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(makefile, "HAL_PLAYBACK_CAPTURE_PACED ?= 1") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1") &&
      rejected_transport_variants_default_off;
  const bool observability_defaults_preserved =
      contains(makefile, "HAL_OUTPUT_WRITE_STATS ?= 1") &&
      contains(makefile, "HAL_HOT_STREAM_STATS ?= 1") &&
      contains(makefile, "HAL_HOT_STREAM_STATS_INTERVAL ?= 16");
  const bool hal_has_runtime_prepared_submit_guard =
      !contains(hal_source, "PreparedUsbSubmitPlanner") &&
      contains(hal_source, "OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME") &&
      contains(hal_source, "kPreparedRuntimeGeometrySupported");
  const bool hal_prepared_runtime_source_contract_pass =
      string_field_is(prepared_runtime_source, "result", "PASS") &&
      bool_field_is(prepared_runtime_source, "prepared_runtime_default_off", true) &&
      bool_field_is(prepared_runtime_source, "prepared_runtime_opt_in_target_present", true) &&
      bool_field_is(prepared_runtime_source, "prepared_runtime_opt_in_target_build_only", true) &&
      bool_field_is(prepared_runtime_source, "source_has_compile_time_geometry_guards", true) &&
      bool_field_is(prepared_runtime_source, "runtime_claim_still_blocked", true);
  const bool hal_prepared_runtime_binding_contract_pass =
      string_field_is(prepared_runtime_binding, "result", "PASS") &&
      bool_field_is(prepared_runtime_binding, "opt_in_profile_binds_64_transaction_geometry",
                    true) &&
      bool_field_is(prepared_runtime_binding, "default_runtime_preserved", true) &&
      bool_field_is(prepared_runtime_binding, "capture_pool_uses_prepared_geometry", true) &&
      bool_field_is(prepared_runtime_binding, "playback_pool_uses_prepared_geometry", true) &&
      bool_field_is(prepared_runtime_binding, "transfer_pool_lifetime_completion_owned", true) &&
      bool_field_is(prepared_runtime_binding, "capture_enqueue_uses_prepared_geometry", true) &&
      bool_field_is(prepared_runtime_binding, "playback_enqueue_uses_prepared_geometry", true) &&
      bool_field_is(prepared_runtime_binding,
                    "capture_paced_playback_batches_to_prepared_geometry", true) &&
      bool_field_is(prepared_runtime_binding, "capture_submit_counter_success_only", true) &&
      bool_field_is(prepared_runtime_binding, "playback_submit_counter_success_only", true) &&
      bool_field_is(prepared_runtime_binding, "completion_counters_completion_owned", true) &&
      bool_field_is(prepared_runtime_binding, "timestamps_use_physical_counts", true) &&
      bool_field_is(prepared_runtime_binding, "runtime_geometry_observable", true) &&
      bool_field_is(prepared_runtime_binding, "submit_cadence_observable", true) &&
      number_or(prepared_runtime_binding, "expected_submit_reduction_ratio", 0.0) >= 8.0 &&
      bool_field_is(prepared_runtime_binding, "physical_evidence_present", false) &&
      bool_field_is(prepared_runtime_binding, "product_claim_allowed", false);
  const bool hal_prepared_runtime_dispatch_path_present =
      bool_field_is(prepared_runtime_binding, "prepared_runtime_dispatch_path_present", true);
  const bool hal_prepared_runtime_physical_evidence_present = false;
  const bool hal_has_logical_physical_capture_split =
      contains(hal_source, "OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER") &&
      contains(hal_source,
               "kCaptureIsoFramesPerTransfer = OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(hal_source, "ExpectedIsoTransferTicksForFrames(kCaptureIsoFramesPerTransfer)");
  const bool hal_has_capture_submit_counter =
      contains(hal_source, "uint64_t captureTransfersSubmitted;") &&
      contains(hal_source, "uint64_t captureSubmitAttempts;") &&
      contains(hal_source, "atomic_uint_fast64_t _captureSubmitAttemptsAtomic;") &&
      contains(hal_source, "atomic_init(&_captureSubmitAttemptsAtomic, 0);") &&
      contains(hal_source, "atomic_store(&_captureSubmitAttemptsAtomic, 0);") &&
      contains(hal_source,
               "stats.captureSubmitAttempts = atomic_load(&_captureSubmitAttemptsAtomic);") &&
      contains(hal_source,
               "atomic_fetch_add_explicit(&_captureSubmitAttemptsAtomic, 1, memory_order_relaxed);") &&
      contains(hal_source, "atomic_uint_fast64_t _captureTransfersSubmittedAtomic;") &&
      contains(hal_source, "atomic_init(&_captureTransfersSubmittedAtomic, 0);") &&
      contains(hal_source, "atomic_store(&_captureTransfersSubmittedAtomic, 0);") &&
      contains(hal_source,
               "stats.captureTransfersSubmitted = atomic_load(&_captureTransfersSubmittedAtomic);") &&
      contains(hal_source,
               "atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool hal_has_playback_submit_counter =
      contains(hal_source, "uint64_t playbackTransfersSubmitted;") &&
      contains(hal_source, "uint64_t playbackSubmitAttempts;") &&
      contains(hal_source, "atomic_uint_fast64_t _playbackSubmitAttemptsAtomic;") &&
      contains(hal_source, "atomic_init(&_playbackSubmitAttemptsAtomic, 0);") &&
      contains(hal_source, "atomic_store(&_playbackSubmitAttemptsAtomic, 0);") &&
      contains(hal_source,
               "stats.playbackSubmitAttempts = atomic_load(&_playbackSubmitAttemptsAtomic);") &&
      contains(hal_source,
               "atomic_fetch_add_explicit(&_playbackSubmitAttemptsAtomic, 1, memory_order_relaxed);") &&
      contains(hal_source, "atomic_uint_fast64_t _playbackTransfersSubmittedAtomic;") &&
      contains(hal_source,
               "stats.playbackTransfersSubmitted = atomic_load(&_playbackTransfersSubmittedAtomic);") &&
      contains(hal_source,
               "atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool control_exposes_submit_counters =
      contains(control_source, "captureSubmitAttempts=%llu") &&
      contains(control_source, "captureTransfersSubmitted=%llu") &&
      contains(control_source, "playbackSubmitAttempts=%llu") &&
      contains(control_source, "playbackTransfersSubmitted=%llu");
  const bool soundcheck_tsv_captures_submit_counters =
      contains(run_soundcheck, "\"captureSubmitAttempts\",") &&
      contains(run_soundcheck, "\"captureTransfersSubmitted\",") &&
      contains(run_soundcheck, "\"playbackSubmitAttempts\",") &&
      contains(run_soundcheck, "\"playbackTransfersSubmitted\",");
  const bool analyzer_summarizes_submit_counters =
      contains(stream_stats_analyzer, "\"captureSubmitAttempts\",") &&
      contains(stream_stats_analyzer, "\"captureTransfersSubmitted\",") &&
      contains(stream_stats_analyzer, "\"playbackSubmitAttempts\",") &&
      contains(stream_stats_analyzer, "\"playbackTransfersSubmitted\",") &&
      contains(stream_stats_analyzer, "\"capture_submit_attempts_per_second\"") &&
      contains(stream_stats_analyzer, "\"capture_transfers_submitted_per_second\"") &&
      contains(stream_stats_analyzer, "\"capture_submit_failures\"") &&
      contains(stream_stats_analyzer, "\"playback_submit_attempts_per_second\"") &&
      contains(stream_stats_analyzer, "\"playback_submit_failures\"") &&
      contains(stream_stats_analyzer, "\"capture_submit_reduction_ratio_vs_logical\"") &&
      contains(stream_stats_analyzer, "\"playback_submit_reduction_ratio_vs_base\"");
  const bool prepared_runtime_rejection_observability_present =
      contains(hal_source, "preparedRuntimeSubmitFailures") &&
      contains(hal_source, "preparedRuntimeDescriptorMismatches") &&
      contains(hal_source, "OpenA8DJPreparedRuntimeBridgeSnapshotCounters") &&
      contains(control_source, "preparedRuntimeSubmitFailures=%llu") &&
      contains(control_source, "preparedRuntimeDescriptorMismatches=%llu") &&
      contains(control_source, "preparedPlaybackRejectTransactionCount=%llu") &&
      contains(run_soundcheck, "\"preparedRuntimeSubmitFailures\"") &&
      contains(run_soundcheck, "\"preparedRuntimeDescriptorMismatches\"") &&
      contains(run_soundcheck, "\"preparedPlaybackRejectTransactionCount\"") &&
      contains(stream_stats_analyzer, "\"preparedRuntimeSubmitFailures\",") &&
      contains(stream_stats_analyzer, "\"preparedPlaybackRejectTransactionCount\",") &&
      contains(stream_stats_analyzer, "\"prepared_runtime\"");
  const bool usb_enqueue_timing_observability_present =
      contains(hal_source, "hotPathCaptureEnqueueTicksMin") &&
      contains(hal_source, "hotPathCaptureEnqueueStartTime") &&
      contains(hal_source, "submitCaptureTransfer:transfer") &&
      contains(control_source, "hotPathCaptureEnqueueTicksSamples") &&
      contains(control_source, "\"capture-enqueue\"") &&
      contains(run_soundcheck, "\"hotPathCaptureEnqueueTicksSamples\"") &&
      contains(stream_stats_analyzer, "\"hotPathCaptureEnqueueTicksSamples\"") &&
      contains(stream_stats_analyzer, "\"capture_enqueue\"");
  const bool capture_submit_counter_success_only =
      contains(hal_source,
               "return;\n    }\n    atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic, 1, memory_order_relaxed);") &&
      appears_before(hal_source,
                     "BOOL queued = [self submitCaptureTransfer:transfer",
                     "atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool playback_submit_counter_success_only =
      appears_before(hal_source,
                     "BOOL queued = [self submitPlaybackTransfer:transfer",
                     "atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);") &&
      appears_before(hal_source,
                     "return NO;\n    }\n    atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);",
                     "atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool runtime_submit_observability_present =
      hal_has_capture_submit_counter && hal_has_playback_submit_counter &&
      control_exposes_submit_counters && soundcheck_tsv_captures_submit_counters &&
      analyzer_summarizes_submit_counters && capture_submit_counter_success_only &&
      playback_submit_counter_success_only &&
      prepared_runtime_rejection_observability_present && usb_enqueue_timing_observability_present;

  const bool offline_prepared_model_supported =
      string_field_is(migration, "result", "PASS") &&
      bool_field_is(migration, "migration_candidate_supported", true) &&
      bool_field_is(migration, "product_ready", false) &&
      bool_field_is(migration, "branch_promotion_supported", false) &&
      bool_field_is(migration, "hal_prepared_runtime_binding_safe", true) &&
      number_or(migration, "runtime_adapter_stable_usb_submit_reduction_ratio", 0.0) >= 8.0 &&
      gate_array_has_name(migration, "driverkit_usb_request_lifecycle_safe") &&
      gate_array_has_name(migration, "driverkit_usb_request_shutdown_safe");
  const bool playback_scheduler_runtime_contract_pass =
      string_field_is(playback_scheduler_runtime, "result", "PASS") &&
      number_or(playback_scheduler_runtime, "stable_capture_runtime_submit_calls", 0.0) ==
          256.0 &&
      number_or(playback_scheduler_runtime, "stable_playback_runtime_submit_calls", 999.0) <=
          33.0 &&
      number_or(playback_scheduler_runtime, "stable_playback_logical_slots_submitted", 0.0) ==
          264.0 &&
      number_or(playback_scheduler_runtime, "stable_playback_submit_reduction_ratio", 0.0) >=
          8.0 &&
      number_or(playback_scheduler_runtime, "stable_total_submit_reduction_ratio", 0.0) >
          1.5 &&
      bool_field_is(playback_scheduler_runtime, "physical_evidence_present", false) &&
      bool_field_is(playback_scheduler_runtime, "product_claim_allowed", false);
  const bool hal_playback_scheduler_candidate_pass =
      string_field_is(hal_playback_scheduler_candidate, "result", "PASS") &&
      string_field_is(hal_playback_scheduler_candidate, "prepared_runtime_mode", "playback_only") &&
      bool_field_is(hal_playback_scheduler_candidate, "playback_only_runtime", true) &&
      bool_field_is(hal_playback_scheduler_candidate, "capture_runtime_enabled", false) &&
      bool_field_is(hal_playback_scheduler_candidate, "playback_runtime_enabled", true) &&
      number_or(hal_playback_scheduler_candidate, "logical_iso_frames", 0.0) == 8.0 &&
      number_or(hal_playback_scheduler_candidate, "capture_iso_frames", 0.0) == 8.0 &&
      number_or(hal_playback_scheduler_candidate, "prepared_submit_frames", 0.0) == 64.0 &&
      number_or(hal_playback_scheduler_candidate, "playback_coalesce_transfers", 0.0) == 8.0 &&
      bool_field_is(hal_playback_scheduler_candidate, "default_hal_restored", true) &&
      bool_field_is(hal_playback_scheduler_candidate, "physical_evidence_present", false) &&
      bool_field_is(hal_playback_scheduler_candidate, "product_claim_allowed", false);
  const bool hal_prepared_lite_candidate_pass =
      string_field_is(hal_prepared_lite_candidate, "result", "PASS") &&
      string_field_is(hal_prepared_lite_candidate, "prepared_runtime_mode",
                      "capture_and_playback") &&
      bool_field_is(hal_prepared_lite_candidate, "playback_only_runtime", false) &&
      bool_field_is(hal_prepared_lite_candidate, "capture_runtime_enabled", true) &&
      bool_field_is(hal_prepared_lite_candidate, "playback_runtime_enabled", true) &&
      number_or(hal_prepared_lite_candidate, "logical_iso_frames", 0.0) == 8.0 &&
      number_or(hal_prepared_lite_candidate, "capture_iso_frames", 0.0) == 16.0 &&
      number_or(hal_prepared_lite_candidate, "prepared_submit_frames", 0.0) == 16.0 &&
      number_or(hal_prepared_lite_candidate, "playback_coalesce_transfers", 0.0) == 2.0 &&
      number_or(hal_prepared_lite_candidate, "expected_submit_reduction_ratio", 0.0) == 2.0 &&
      bool_field_is(hal_prepared_lite_candidate, "default_hal_restored", true) &&
      bool_last_field_is(hal_prepared_lite_candidate, "prepared_hash_differs_from_default", true) &&
      bool_field_is(hal_prepared_lite_candidate, "physical_evidence_present", false) &&
      bool_field_is(hal_prepared_lite_candidate, "product_claim_allowed", false);
  const bool playback_scheduler_physically_rejected =
      string_field_is(playback_scheduler_physical_compare, "result", "FAIL") &&
      string_field_is(playback_scheduler_physical_compare, "readiness_claim",
                      "BLOCKED_NOT_BETTER_THAN_MAINLINE_REFERENCE") &&
      contains(playback_scheduler_physical_compare, "\"playback_submit_reduction_ratio_vs_base\": 8") &&
      contains(playback_scheduler_physical_compare, "\"quality_alignment_score\": 0.396583") &&
      contains(playback_scheduler_physical_compare, "\"driver_cpu_p95\": 14.3") &&
      contains(playback_scheduler_physical_compare, "\"coreaudiod_cpu_p95\": 22.3");
  const bool default_postclose_physically_rejected_for_product =
      string_field_is(default_postclose_physical_compare, "result", "FAIL") &&
      string_field_is(default_postclose_physical_compare, "readiness_claim",
                      "BLOCKED_NOT_BETTER_THAN_MAINLINE_REFERENCE") &&
      contains(default_postclose_physical_compare, "\"quality_alignment_score\": 0.843286") &&
      contains(default_postclose_physical_compare, "\"driver_cpu_p95\": 19.4") &&
      contains(default_postclose_physical_compare, "\"coreaudiod_cpu_p95\": 14.5");
  const auto prepared_lite_physical_baseline =
      opena8djcpp::evidence_json::json_object(prepared_lite_physical_compare, "baseline")
          .value_or(std::string_view{});
  const auto prepared_lite_physical_candidate =
      opena8djcpp::evidence_json::json_object(prepared_lite_physical_compare, "candidate")
          .value_or(std::string_view{});
  const bool prepared_lite_physically_rejected =
      string_field_is(prepared_lite_physical_compare, "result", "FAIL") &&
      string_field_is(prepared_lite_physical_compare, "readiness_claim",
                      "BLOCKED_NOT_BETTER_THAN_MAINLINE_REFERENCE") &&
      number_or(prepared_lite_physical_candidate, "quality_alignment_score", 0.0) >
          number_or(prepared_lite_physical_baseline, "quality_alignment_score", 1.0) &&
      number_or(prepared_lite_physical_candidate, "quality_alignment_score", 1.0) < 0.98 &&
      number_or(prepared_lite_physical_candidate, "capture_submit_calls_per_second", 1000000.0) <
          number_or(prepared_lite_physical_baseline, "capture_submit_calls_per_second", 0.0) &&
      number_or(prepared_lite_physical_candidate, "driver_cpu_p95", 0.0) >
          number_or(prepared_lite_physical_baseline, "driver_cpu_p95", 1000000.0) &&
      number_or(prepared_lite_physical_candidate, "coreaudiod_cpu_p95", 0.0) >
          number_or(prepared_lite_physical_baseline, "coreaudiod_cpu_p95", 1000000.0) &&
      bool_field_is(prepared_lite_physical_candidate, "audiophile_cpp_wav_analysis_pass",
                    false) &&
      bool_field_is(prepared_lite_physical_candidate, "audiophile_python_wav_analysis_pass",
                    false);
  const bool postclose_cpu_sample_points_to_usbhost_enqueue =
      contains(postclose_driver_sample,
               "\"dominant_interpretation\": \"usbhost_async_enqueue_from_capture_and_playback_paths\"") &&
      contains(postclose_driver_sample, "\"usbhost_enqueue\": 918") &&
      contains(postclose_driver_sample, "\"queue_capture\": 873");

  const bool current_quality_blocked =
      string_field_is(product_quality, "result", "PASS") &&
      bool_field_is(product_quality, "quality_claim_allowed", false) &&
      bool_field_is(product_quality, "branch_promotion_allowed", false);
  const bool physical_ab_blocked =
      string_field_is(physical_window, "result", "PASS") &&
      bool_field_is(physical_window, "ready_for_product_physical_ab", false) &&
      bool_field_is(physical_window, "ready_for_branch_promotion", false);
  const bool hal_load_is_only_safety =
      string_field_is(hal_safety, "result", "PASS") &&
      string_field_is(hal_safety, "readiness_claim",
                      "DIAGNOSTIC_ONLY_HAL_ENUMERATION_SAFE_NOT_SOUND_QUALITY_READY");
  const bool hal_safety_blocks_claims =
      !hal_safety.empty() &&
      bool_field_is(hal_safety, "product_claim_allowed", false) &&
      bool_field_is(hal_safety, "branch_promotion_allowed", false) &&
      string_field_is(hal_safety, "readiness_claim",
                      "DIAGNOSTIC_ONLY_HAL_ENUMERATION_SAFE_NOT_SOUND_QUALITY_READY");

  const bool runtime_reduction_missing =
      hal_has_direct_usb_enqueue && hal_default_capture_paced &&
      !hal_prepared_runtime_physical_evidence_present;
  const bool prepared_runtime_not_next_default =
      runtime_reduction_missing && rejected_transport_variants_default_off &&
      stable_default_load_preserved;
  const bool product_claim_blocked =
      runtime_reduction_missing && offline_prepared_model_supported &&
      hal_prepared_runtime_source_contract_pass && hal_prepared_runtime_binding_contract_pass &&
      playback_scheduler_runtime_contract_pass && hal_playback_scheduler_candidate_pass &&
      hal_prepared_lite_candidate_pass &&
      playback_scheduler_physically_rejected && default_postclose_physically_rejected_for_product &&
      prepared_lite_physically_rejected &&
      postclose_cpu_sample_points_to_usbhost_enqueue &&
      current_quality_blocked && physical_ab_blocked && hal_safety_blocks_claims &&
      stable_default_load_preserved &&
      observability_defaults_preserved && prepared_runtime_not_next_default;

  std::vector<std::string> blockers;
  if (!evidence_present) {
    blockers.push_back("required_runtime_evidence_missing");
  }
  if (!runtime_submit_observability_present) {
    blockers.push_back("runtime_submit_observability_missing");
  }
  if (!usb_enqueue_timing_observability_present) {
    blockers.push_back("usb_enqueue_timing_observability_missing");
  }
  if (!stable_default_load_preserved) {
    blockers.push_back("stable_default_load_not_preserved");
  }
  if (!rejected_transport_variants_default_off) {
    blockers.push_back("rejected_transport_variant_default_enabled");
  }
  if (!observability_defaults_preserved) {
    blockers.push_back("observability_defaults_not_preserved");
  }
  if (runtime_reduction_missing) {
    blockers.push_back("hal_prepared_runtime_not_physically_validated");
    if (!hal_prepared_runtime_dispatch_path_present) {
      blockers.push_back("hal_runtime_still_direct_usb_enqueue_without_prepared_dispatch");
    }
  }
  if (!hal_has_runtime_prepared_submit_guard) {
    blockers.push_back("hal_prepared_runtime_source_guard_missing");
  }
  if (!hal_prepared_runtime_source_contract_pass) {
    blockers.push_back("hal_prepared_runtime_source_contract_missing_or_failing");
  }
  if (!hal_prepared_runtime_binding_contract_pass) {
    blockers.push_back("hal_prepared_runtime_binding_contract_missing_or_failing");
  }
  if (!offline_prepared_model_supported) {
    blockers.push_back("offline_prepared_transport_model_not_supported");
  }
  if (!playback_scheduler_runtime_contract_pass) {
    blockers.push_back("playback_scheduler_runtime_contract_missing_or_failing");
  }
  if (!hal_playback_scheduler_candidate_pass) {
    blockers.push_back("hal_playback_scheduler_candidate_missing_or_failing");
  }
  if (!hal_prepared_lite_candidate_pass) {
    blockers.push_back("hal_prepared_lite_candidate_missing_or_failing");
  }
  if (playback_scheduler_physically_rejected) {
    blockers.push_back("playback_scheduler_physically_rejected");
  } else {
    blockers.push_back("playback_scheduler_physical_rejection_missing");
  }
  if (default_postclose_physically_rejected_for_product) {
    blockers.push_back("default_postclose_physically_rejected_for_product");
  } else {
    blockers.push_back("default_postclose_rejection_missing");
  }
  if (prepared_lite_physically_rejected) {
    blockers.push_back("prepared_lite_physically_rejected_for_product");
  } else {
    blockers.push_back("prepared_lite_physical_rejection_missing");
  }
  if (postclose_cpu_sample_points_to_usbhost_enqueue) {
    blockers.push_back("postclose_cpu_sample_points_to_usbhost_enqueue");
  } else {
    blockers.push_back("postclose_cpu_sample_missing_or_not_usbhost_enqueue");
  }
  if (current_quality_blocked) {
    blockers.push_back("physical_quality_claim_blocked");
  }
  if (physical_ab_blocked) {
    blockers.push_back("same_session_product_ab_blocked");
  }
  if (hal_safety_blocks_claims) {
    blockers.push_back("hal_safety_evidence_blocks_product_claims");
  }

  const bool pass = evidence_present && runtime_submit_observability_present && product_claim_blocked;

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-transport-runtime-gate.v1\",\n"
      << "  \"safety\": \"offline_source_and_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means the guard blocks HAL superiority claims and keeps rejected physical variants out of the stable default load\",\n"
      << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
      << "  \"hal_has_direct_usb_enqueue\": " << (hal_has_direct_usb_enqueue ? "true" : "false")
      << ",\n"
      << "  \"hal_default_capture_paced\": " << (hal_default_capture_paced ? "true" : "false")
      << ",\n"
      << "  \"stable_default_load_preserved\": "
      << (stable_default_load_preserved ? "true" : "false") << ",\n"
      << "  \"rejected_transport_variants_default_off\": "
      << (rejected_transport_variants_default_off ? "true" : "false") << ",\n"
      << "  \"observability_defaults_preserved\": "
      << (observability_defaults_preserved ? "true" : "false") << ",\n"
      << "  \"hal_has_runtime_prepared_submit_guard\": "
      << (hal_has_runtime_prepared_submit_guard ? "true" : "false") << ",\n"
      << "  \"hal_prepared_runtime_source_contract_pass\": "
      << (hal_prepared_runtime_source_contract_pass ? "true" : "false") << ",\n"
      << "  \"hal_prepared_runtime_binding_contract_pass\": "
      << (hal_prepared_runtime_binding_contract_pass ? "true" : "false") << ",\n"
      << "  \"hal_prepared_runtime_dispatch_path_present\": "
      << (hal_prepared_runtime_dispatch_path_present ? "true" : "false") << ",\n"
      << "  \"hal_prepared_runtime_expected_submit_reduction_ratio\": "
      << number_or(prepared_runtime_binding, "expected_submit_reduction_ratio", -1.0) << ",\n"
      << "  \"hal_prepared_runtime_default_off\": "
      << (bool_field_is(prepared_runtime_source, "prepared_runtime_default_off", true) ? "true"
                                                                                       : "false")
      << ",\n"
      << "  \"hal_prepared_runtime_opt_in_target_present\": "
      << (bool_field_is(prepared_runtime_source, "prepared_runtime_opt_in_target_present", true)
              ? "true"
              : "false")
      << ",\n"
      << "  \"hal_prepared_runtime_physical_evidence_present\": "
      << (hal_prepared_runtime_physical_evidence_present ? "true" : "false") << ",\n"
      << "  \"hal_has_logical_physical_capture_split\": "
      << (hal_has_logical_physical_capture_split ? "true" : "false") << ",\n"
      << "  \"hal_has_capture_submit_counter\": "
      << (hal_has_capture_submit_counter ? "true" : "false") << ",\n"
      << "  \"hal_has_playback_submit_counter\": "
      << (hal_has_playback_submit_counter ? "true" : "false") << ",\n"
      << "  \"control_exposes_submit_counters\": "
      << (control_exposes_submit_counters ? "true" : "false") << ",\n"
      << "  \"soundcheck_tsv_captures_submit_counters\": "
      << (soundcheck_tsv_captures_submit_counters ? "true" : "false") << ",\n"
      << "  \"analyzer_summarizes_submit_counters\": "
      << (analyzer_summarizes_submit_counters ? "true" : "false") << ",\n"
      << "  \"capture_submit_counter_success_only\": "
      << (capture_submit_counter_success_only ? "true" : "false") << ",\n"
      << "  \"playback_submit_counter_success_only\": "
      << (playback_submit_counter_success_only ? "true" : "false") << ",\n"
      << "  \"runtime_submit_observability_present\": "
      << (runtime_submit_observability_present ? "true" : "false") << ",\n"
      << "  \"usb_enqueue_timing_observability_present\": "
      << (usb_enqueue_timing_observability_present ? "true" : "false") << ",\n"
      << "  \"runtime_reduction_missing\": " << (runtime_reduction_missing ? "true" : "false")
      << ",\n"
      << "  \"prepared_runtime_not_next_default\": "
      << (prepared_runtime_not_next_default ? "true" : "false") << ",\n"
      << "  \"offline_prepared_model_supported\": "
      << (offline_prepared_model_supported ? "true" : "false") << ",\n"
      << "  \"offline_usb_submit_reduction_ratio\": "
      << number_or(migration, "runtime_adapter_stable_usb_submit_reduction_ratio", -1.0)
      << ",\n"
      << "  \"playback_scheduler_runtime_contract_pass\": "
      << (playback_scheduler_runtime_contract_pass ? "true" : "false") << ",\n"
      << "  \"playback_scheduler_runtime_capture_submits\": "
      << number_or(playback_scheduler_runtime, "stable_capture_runtime_submit_calls", -1.0)
      << ",\n"
      << "  \"playback_scheduler_runtime_playback_submits\": "
      << number_or(playback_scheduler_runtime, "stable_playback_runtime_submit_calls", -1.0)
      << ",\n"
      << "  \"playback_scheduler_runtime_reduction_ratio\": "
      << number_or(playback_scheduler_runtime, "stable_playback_submit_reduction_ratio", -1.0)
      << ",\n"
      << "  \"hal_playback_scheduler_candidate_pass\": "
      << (hal_playback_scheduler_candidate_pass ? "true" : "false") << ",\n"
      << "  \"hal_playback_scheduler_candidate_capture_iso_frames\": "
      << number_or(hal_playback_scheduler_candidate, "capture_iso_frames", -1.0) << ",\n"
      << "  \"hal_playback_scheduler_candidate_prepared_submit_frames\": "
      << number_or(hal_playback_scheduler_candidate, "prepared_submit_frames", -1.0) << ",\n"
      << "  \"hal_playback_scheduler_candidate_playback_coalesce_transfers\": "
      << number_or(hal_playback_scheduler_candidate, "playback_coalesce_transfers", -1.0)
      << ",\n"
      << "  \"hal_prepared_lite_candidate_pass\": "
      << (hal_prepared_lite_candidate_pass ? "true" : "false") << ",\n"
      << "  \"hal_prepared_lite_candidate_capture_iso_frames\": "
      << number_or(hal_prepared_lite_candidate, "capture_iso_frames", -1.0) << ",\n"
      << "  \"hal_prepared_lite_candidate_prepared_submit_frames\": "
      << number_or(hal_prepared_lite_candidate, "prepared_submit_frames", -1.0) << ",\n"
      << "  \"hal_prepared_lite_candidate_expected_submit_reduction_ratio\": "
      << number_or(hal_prepared_lite_candidate, "expected_submit_reduction_ratio", -1.0)
      << ",\n"
      << "  \"hal_prepared_lite_candidate_physical_evidence_present\": "
      << (bool_field_is(hal_prepared_lite_candidate, "physical_evidence_present", false)
              ? "false"
              : "true")
      << ",\n"
      << "  \"prepared_lite_physically_rejected\": "
      << (prepared_lite_physically_rejected ? "true" : "false") << ",\n"
      << "  \"prepared_lite_physical_baseline_quality_alignment_score\": "
      << number_or(prepared_lite_physical_baseline, "quality_alignment_score", -1.0)
      << ",\n"
      << "  \"prepared_lite_physical_candidate_quality_alignment_score\": "
      << number_or(prepared_lite_physical_candidate, "quality_alignment_score", -1.0)
      << ",\n"
      << "  \"prepared_lite_physical_baseline_capture_submit_calls_per_second\": "
      << number_or(prepared_lite_physical_baseline, "capture_submit_calls_per_second", -1.0)
      << ",\n"
      << "  \"prepared_lite_physical_candidate_capture_submit_calls_per_second\": "
      << number_or(prepared_lite_physical_candidate, "capture_submit_calls_per_second", -1.0)
      << ",\n"
      << "  \"prepared_lite_physical_baseline_driver_cpu_p95\": "
      << number_or(prepared_lite_physical_baseline, "driver_cpu_p95", -1.0) << ",\n"
      << "  \"prepared_lite_physical_candidate_driver_cpu_p95\": "
      << number_or(prepared_lite_physical_candidate, "driver_cpu_p95", -1.0) << ",\n"
      << "  \"prepared_lite_physical_baseline_coreaudiod_cpu_p95\": "
      << number_or(prepared_lite_physical_baseline, "coreaudiod_cpu_p95", -1.0) << ",\n"
      << "  \"prepared_lite_physical_candidate_coreaudiod_cpu_p95\": "
      << number_or(prepared_lite_physical_candidate, "coreaudiod_cpu_p95", -1.0)
      << ",\n"
      << "  \"playback_scheduler_physically_rejected\": "
      << (playback_scheduler_physically_rejected ? "true" : "false") << ",\n"
      << "  \"default_postclose_physically_rejected_for_product\": "
      << (default_postclose_physically_rejected_for_product ? "true" : "false") << ",\n"
      << "  \"postclose_cpu_sample_points_to_usbhost_enqueue\": "
      << (postclose_cpu_sample_points_to_usbhost_enqueue ? "true" : "false") << ",\n"
      << "  \"current_quality_blocked\": " << (current_quality_blocked ? "true" : "false")
      << ",\n"
      << "  \"physical_ab_blocked\": " << (physical_ab_blocked ? "true" : "false") << ",\n"
      << "  \"hal_load_is_only_safety\": " << (hal_load_is_only_safety ? "true" : "false")
      << ",\n"
      << "  \"product_claim_blocked\": " << (product_claim_blocked ? "true" : "false")
      << ",\n";
  print_string_array("runtime_claim_blockers", blockers);
  std::cout
      << "  \"next_cpu_direction\": "
         "\"DESIGN_NEW_TRANSPORT_REDUCING_IOUSBHOST_ENQUEUE_OR_DRIVERKIT_USB_RUNTIME\",\n"
      << "  \"next_required_action\": "
         "\"KEEP_DEFAULT_STABLE_LOAD_DO_NOT_REPEAT_REJECTED_PLAYBACK_SCHEDULER_IMPLEMENT_NEW_TRANSPORT_CANDIDATE_OFFLINE_FIRST\",\n"
      << "  \"blocked_claim\": "
         "\"NO_CPU_OR_AUDIOPHILE_SUPERIORITY_CLAIM_UNTIL_A_NEW_TRANSPORT_CANDIDATE_BEATS_MAINLINE_IN_LOCK_GATED_SAME_SESSION_AB\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
