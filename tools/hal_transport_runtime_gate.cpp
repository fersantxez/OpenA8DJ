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

  const bool evidence_present = !hal_source.empty() && !control_source.empty() &&
                                !run_soundcheck.empty() && !stream_stats_analyzer.empty() &&
                                !makefile.empty() && !migration.empty() &&
                                !product_quality.empty() && !physical_window.empty() &&
                                !hal_safety.empty() && !prepared_runtime_source.empty();

  const bool hal_has_direct_usb_enqueue =
      contains(hal_source, "enqueueIORequestWithData:transfer.data") &&
      contains(hal_source, "- (BOOL)queuePlaybackWithRequests:") &&
      contains(hal_source, "[self queueCaptureTransfer]");
  const bool hal_default_capture_paced =
      contains(makefile, "HAL_PLAYBACK_CAPTURE_PACED ?= 1") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1");
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
  const bool capture_submit_counter_success_only =
      contains(hal_source,
               "return;\n    }\n    atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic, 1, memory_order_relaxed);") &&
      appears_before(hal_source,
                     "BOOL queued = [_capturePipe enqueueIORequestWithData:transfer.data",
                     "atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool playback_submit_counter_success_only =
      appears_before(hal_source,
                     "BOOL queued = [_playbackPipe enqueueIORequestWithData:transfer.data",
                     "atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);") &&
      appears_before(hal_source,
                     "return NO;\n    }\n    atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);",
                     "atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool runtime_submit_observability_present =
      hal_has_capture_submit_counter && hal_has_playback_submit_counter &&
      control_exposes_submit_counters && soundcheck_tsv_captures_submit_counters &&
      analyzer_summarizes_submit_counters && capture_submit_counter_success_only &&
      playback_submit_counter_success_only;

  const bool offline_prepared_model_supported =
      string_field_is(migration, "result", "PASS") &&
      bool_field_is(migration, "migration_candidate_supported", true) &&
      bool_field_is(migration, "product_ready", false) &&
      bool_field_is(migration, "branch_promotion_supported", false) &&
      number_or(migration, "runtime_adapter_stable_usb_submit_reduction_ratio", 0.0) >= 8.0 &&
      gate_array_has_name(migration, "driverkit_usb_request_lifecycle_safe") &&
      gate_array_has_name(migration, "driverkit_usb_request_shutdown_safe");

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

  const bool runtime_reduction_missing =
      hal_has_direct_usb_enqueue && hal_default_capture_paced &&
      !hal_prepared_runtime_physical_evidence_present;
  const bool product_claim_blocked =
      runtime_reduction_missing && offline_prepared_model_supported &&
      hal_prepared_runtime_source_contract_pass && current_quality_blocked && physical_ab_blocked &&
      hal_load_is_only_safety;

  std::vector<std::string> blockers;
  if (!evidence_present) {
    blockers.push_back("required_runtime_evidence_missing");
  }
  if (!runtime_submit_observability_present) {
    blockers.push_back("runtime_submit_observability_missing");
  }
  if (runtime_reduction_missing) {
    blockers.push_back("hal_runtime_still_direct_usb_enqueue");
    blockers.push_back("hal_prepared_runtime_not_physically_validated");
  }
  if (!hal_has_runtime_prepared_submit_guard) {
    blockers.push_back("hal_prepared_runtime_source_guard_missing");
  }
  if (!hal_prepared_runtime_source_contract_pass) {
    blockers.push_back("hal_prepared_runtime_source_contract_missing_or_failing");
  }
  if (!offline_prepared_model_supported) {
    blockers.push_back("offline_prepared_transport_model_not_supported");
  }
  if (current_quality_blocked) {
    blockers.push_back("physical_quality_claim_blocked");
  }
  if (physical_ab_blocked) {
    blockers.push_back("same_session_product_ab_blocked");
  }
  if (hal_load_is_only_safety) {
    blockers.push_back("hal_load_evidence_is_safety_only");
  }

  const bool pass = evidence_present && runtime_submit_observability_present && product_claim_blocked;

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-transport-runtime-gate.v1\",\n"
      << "  \"safety\": \"offline_source_and_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means the guard correctly blocks HAL runtime superiority claims until real USB enqueue reduction exists\",\n"
      << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
      << "  \"hal_has_direct_usb_enqueue\": " << (hal_has_direct_usb_enqueue ? "true" : "false")
      << ",\n"
      << "  \"hal_default_capture_paced\": " << (hal_default_capture_paced ? "true" : "false")
      << ",\n"
      << "  \"hal_has_runtime_prepared_submit_guard\": "
      << (hal_has_runtime_prepared_submit_guard ? "true" : "false") << ",\n"
      << "  \"hal_prepared_runtime_source_contract_pass\": "
      << (hal_prepared_runtime_source_contract_pass ? "true" : "false") << ",\n"
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
      << "  \"runtime_reduction_missing\": " << (runtime_reduction_missing ? "true" : "false")
      << ",\n"
      << "  \"offline_prepared_model_supported\": "
      << (offline_prepared_model_supported ? "true" : "false") << ",\n"
      << "  \"offline_usb_submit_reduction_ratio\": "
      << number_or(migration, "runtime_adapter_stable_usb_submit_reduction_ratio", -1.0)
      << ",\n"
      << "  \"current_quality_blocked\": " << (current_quality_blocked ? "true" : "false")
      << ",\n"
      << "  \"physical_ab_blocked\": " << (physical_ab_blocked ? "true" : "false") << ",\n"
      << "  \"hal_load_is_only_safety\": " << (hal_load_is_only_safety ? "true" : "false")
      << ",\n"
      << "  \"product_claim_blocked\": " << (product_claim_blocked ? "true" : "false")
      << ",\n";
  print_string_array("runtime_claim_blockers", blockers);
  std::cout
      << "  \"next_required_action\": "
         "\"COMPILE_OPT_IN_HAL_PREPARED_RUNTIME_THEN_LOCK_GATED_ROUTE_REVALIDATION_BEFORE_PHYSICAL_AB\",\n"
      << "  \"blocked_claim\": "
         "\"NO_CPU_OR_AUDIOPHILE_SUPERIORITY_CLAIM_UNTIL_PREPARED_RUNTIME_HAS_LOCK_GATED_SAME_SESSION_PHYSICAL_EVIDENCE\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
