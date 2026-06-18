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

std::size_t count_occurrences(std::string_view text, std::string_view needle) {
  if (needle.empty()) {
    return 0;
  }
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string_view::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

std::string function_body(std::string_view source, std::string_view signature) {
  const auto start = source.find(signature);
  if (start == std::string_view::npos) {
    return {};
  }
  const auto next_method = source.find("\n- (", start + signature.size());
  const auto end = next_method == std::string_view::npos ? source.size() : next_method;
  return std::string(source.substr(start, end - start));
}

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
}

void print_number(const char* key, std::uint32_t value) {
  std::cout << "  \"" << key << "\": " << value << ",\n";
}

void print_string_array(const char* key, const std::vector<std::string>& values) {
  std::cout << "  \"" << key << "\": [";
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
  const auto hal_source = read_file(root / "src/hal/OpenA8DJUSB.m");
  const auto bridge_header = read_file(root / "src/hal/OpenA8DJPreparedRuntimeBridge.h");
  const auto bridge_source = read_file(root / "src/hal/OpenA8DJPreparedRuntimeBridge.mm");
  const auto control_source = read_file(root / "src/tools/opena8dj-control.c");
  const auto makefile = read_file(root / "Makefile");
  const auto run_soundcheck = read_file(root / "scripts/run-soundcheck");
  const auto stream_stats_analyzer = read_file(root / "scripts/analyze-stream-stats.py");

  const auto capture_queue = function_body(hal_source, "- (void)queueCaptureTransfer\n{");
  const auto playback_queue = function_body(hal_source, "- (BOOL)queuePlaybackTransfer\n{");
  const auto capture_paced_playback =
      function_body(hal_source,
                    "- (BOOL)queueCapturePacedPlaybackWithRequests:(const uint32_t *)requests "
                    "count:(NSUInteger)count\n{");
  const auto playback_with_requests =
      function_body(hal_source,
                    "- (BOOL)queuePlaybackWithRequests:(const uint32_t *)requests "
                    "count:(NSUInteger)count\n{");
  const auto submit_capture =
      function_body(hal_source,
                    "- (BOOL)submitCaptureTransfer:(OpenA8DJIsoTransfer *)transfer\n"
                    "                  transactions:(IOUSB" "HostIsochronousTransaction *)transactions\n"
                    "             completionHandler:(OpenA8DJIsoCompletionHandler)completionHandler\n"
                    "                          error:(NSError **)error\n{");
  const auto prepared_capture =
      function_body(hal_source,
                    "- (BOOL)enqueuePreparedCaptureSubmitWithTransfer:(OpenA8DJIsoTransfer *)transfer\n"
                    "                                    transactions:(IOUSB" "HostIsochronousTransaction *)transactions\n"
                    "                               completionHandler:(OpenA8DJIsoCompletionHandler)completionHandler\n"
                    "                                            error:(NSError **)error\n{");
  const auto submit_playback =
      function_body(hal_source,
                    "- (BOOL)submitPlaybackTransfer:(OpenA8DJIsoTransfer *)transfer\n"
                    "                   transactions:(IOUSB" "HostIsochronousTransaction *)transactions\n"
                    "               firstFrameNumber:(uint64_t)firstFrameNumber\n"
                    "              completionHandler:(OpenA8DJIsoCompletionHandler)completionHandler\n"
                    "                           error:(NSError **)error\n{");
  const auto prepared_playback =
      function_body(hal_source,
                    "- (BOOL)enqueuePreparedPlaybackSubmitWithTransfer:(OpenA8DJIsoTransfer *)transfer\n"
                    "                                     transactions:(IOUSB" "HostIsochronousTransaction *)transactions\n"
                    "                                 firstFrameNumber:(uint64_t)firstFrameNumber\n"
                    "                                completionHandler:(OpenA8DJIsoCompletionHandler)completionHandler\n"
                    "                                             error:(NSError **)error\n{");
  const auto capture_completion =
      function_body(hal_source,
                    "- (void)handleCaptureTransfer:(OpenA8DJIsoTransfer *)transfer\n"
                    "                        status:(IOReturn)status\n"
                    "                  transactions:(IOUSB" "HostIsochronousTransaction *)transactions\n{");
  const auto playback_completion =
      function_body(hal_source,
                    "- (void)handlePlaybackTransfer:(OpenA8DJIsoTransfer *)transfer\n"
                    "                         status:(IOReturn)status\n"
                    "                   transactions:(IOUSB" "HostIsochronousTransaction *)transactions\n{");

  const bool sources_present = !hal_source.empty() && !bridge_header.empty() &&
                               !bridge_source.empty() && !control_source.empty() &&
                               !makefile.empty() && !run_soundcheck.empty() &&
                               !stream_stats_analyzer.empty();
  const bool opt_in_profile_binds_64_transaction_geometry =
      contains(makefile, "HAL_PREPARED_USB_SUBMIT_RUNTIME=1") &&
      contains(makefile, "HAL_PREPARED_USB_SLOTS_PER_SUBMIT=8") &&
      contains(makefile, "HAL_ISO_FRAMES=8") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES=64") &&
      contains(makefile, "HAL_PLAYBACK_ISO_FRAMES=8") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS=8");
  const bool default_runtime_preserved =
      contains(makefile, "HAL_PREPARED_USB_SUBMIT_RUNTIME ?= 0") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1") &&
      contains(makefile, "HAL_SRC := $(HAL_BASE_SRC)");
  const bool prepared_bridge_opt_in_build_only =
      contains(makefile, "HAL_PREPARED_RUNTIME_SRC :=") &&
      contains(makefile, "src/hal/OpenA8DJPreparedRuntimeBridge.mm") &&
      contains(makefile, "core/src/prepared_usb_async_runtime.cpp") &&
      contains(makefile, "ifeq ($(HAL_PREPARED_USB_SUBMIT_RUNTIME),1)") &&
      contains(makefile, "HAL_SRC := $(HAL_BASE_SRC) $(HAL_PREPARED_RUNTIME_SRC)") &&
      contains(makefile, "HAL_LD := xcrun clang++") &&
      contains(makefile, "HAL_CFLAGS += -Icore/include -Isrc/hal");
  const bool compile_time_geometry_guard_present =
      contains(hal_source, "#if OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME") &&
      contains(hal_source, "kPreparedRuntimeGeometrySupported");
  const bool prepared_bridge_api_present =
      contains(bridge_header, "OpenA8DJPreparedRuntimeBridgeCreate") &&
      contains(bridge_header, "OpenA8DJPreparedRuntimeBridgeQueueSubmit") &&
      contains(bridge_header, "OpenA8DJPreparedRuntimeBridgeComplete") &&
      contains(bridge_header, "OpenA8DJPreparedRuntimeBridgeCancelAll") &&
      contains(bridge_header, "OpenA8DJPreparedRuntimeBridgeSnapshotCounters") &&
      contains(bridge_header, "captureBytesPerSlot") &&
      contains(bridge_header, "playbackBytesPerSlot") &&
      contains(bridge_source, "opena8djcpp::PreparedUsbAsyncRuntime") &&
      contains(bridge_source, ".capture_bytes_per_slot = config->captureBytesPerSlot") &&
      contains(bridge_source, ".playback_bytes_per_slot = config->playbackBytesPerSlot") &&
      contains(bridge_source, "bridge->runtime.submit") &&
      contains(bridge_source, "bridge->runtime.complete") &&
      contains(bridge_source, "bridge->runtime.cancel_all");
  const bool hal_uses_directional_prepared_byte_geometry =
      contains(hal_source, ".captureBytesPerSlot = kIsoBytesPerFrame * kIsoFramesPerTransfer") &&
      contains(hal_source, ".playbackBytesPerSlot = bytesPerPacket * kIsoFramesPerTransfer") &&
      !contains(hal_source, ".bytesPerSlot = bytesPerPacket * kIsoFramesPerTransfer");

  const bool capture_pool_uses_prepared_geometry =
      contains(hal_source, "CreateIsoTransferWithCapacity(kCaptureIsoFramesPerTransfer,") &&
      contains(hal_source,
               "(NSUInteger)kCaptureIsoFramesPerTransfer * kIsoBytesPerFrame");
  const bool playback_pool_uses_prepared_geometry =
      contains(hal_source, "CreateIsoTransferWithCapacity(kPlaybackIsoFramesPerTransfer,") &&
      contains(hal_source,
               "(NSUInteger)kPlaybackIsoFramesPerTransfer * kIsoBytesPerFrame");
  const bool prepared_runtime_dispatch_path_present =
      contains(capture_queue, "[self submitCaptureTransfer:transfer") &&
      contains(playback_with_requests, "[self submitPlaybackTransfer:transfer") &&
      contains(submit_capture, "#if OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME") &&
      contains(submit_capture, "if (kPreparedRuntimeGeometrySupported)") &&
      contains(submit_capture, "enqueuePreparedCaptureSubmitWithTransfer:transfer") &&
      contains(submit_playback, "#if OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME") &&
      contains(submit_playback, "if (kPreparedRuntimeGeometrySupported)") &&
      contains(submit_playback, "enqueuePreparedPlaybackSubmitWithTransfer:transfer") &&
      contains(prepared_capture, "OpenA8DJPreparedRuntimeBridgeQueueSubmit") &&
      appears_before(prepared_capture, "OpenA8DJPreparedRuntimeBridgeQueueSubmit",
                     "[_capturePipe enqueueIORequestWithData:transfer.data") &&
      contains(prepared_capture, "[_capturePipe enqueueIORequestWithData:transfer.data") &&
      contains(prepared_playback, "OpenA8DJPreparedRuntimeBridgeQueueSubmit") &&
      appears_before(prepared_playback, "OpenA8DJPreparedRuntimeBridgeQueueSubmit",
                     "[_playbackPipe enqueueIORequestWithData:transfer.data") &&
      contains(prepared_playback, "[_playbackPipe enqueueIORequestWithData:transfer.data");
  const bool transfer_pool_lifetime_completion_owned =
      count_occurrences(hal_source, "captureCompletionHandler =") >= 1 &&
      count_occurrences(hal_source, "playbackCompletionHandler =") >= 1 &&
      contains(capture_completion, "OpenA8DJPreparedRuntimeBridgeComplete") &&
      contains(playback_completion, "OpenA8DJPreparedRuntimeBridgeComplete") &&
      contains(capture_completion, "[self releasePooledTransfer:transfer];") &&
      contains(playback_completion, "[self releasePooledTransfer:transfer];") &&
      !contains(capture_queue, "[self releasePooledTransfer:transfer];\n    atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic") &&
      !contains(playback_queue, "[self releasePooledTransfer:transfer];\n    atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic");

  const bool capture_enqueue_uses_prepared_geometry =
      contains(capture_queue, "uint32_t requests[kCaptureIsoFramesPerTransfer];") &&
      contains(capture_queue, "frame < kCaptureIsoFramesPerTransfer") &&
      contains(capture_queue, "count:kCaptureIsoFramesPerTransfer") &&
      contains(capture_queue, "[self submitCaptureTransfer:transfer") &&
      contains(prepared_capture, "transfer.preparedRuntimeHandleValid = YES") &&
      contains(prepared_capture, "transfer.preparedRuntimeHandleValid = NO") &&
      contains(prepared_capture, "transactionListCount:transfer.transactionCount") &&
      contains(prepared_capture, "firstFrameNumber:0");
  const bool playback_enqueue_uses_prepared_geometry =
      contains(playback_queue, "uint32_t requests[kPlaybackIsoFramesPerTransfer];") &&
      contains(playback_queue, "frame < kPlaybackIsoFramesPerTransfer") &&
      contains(playback_queue, "count:kPlaybackIsoFramesPerTransfer") &&
      contains(playback_with_requests, "count:count") &&
      contains(playback_with_requests, "[self submitPlaybackTransfer:transfer") &&
      contains(prepared_playback, "transfer.preparedRuntimeHandleValid = YES") &&
      contains(prepared_playback, "transfer.preparedRuntimeHandleValid = NO") &&
      contains(prepared_playback, "transactionListCount:transfer.transactionCount") &&
      contains(prepared_playback, "firstFrameNumber:firstFrameNumber");
  const bool capture_paced_playback_batches_to_prepared_geometry =
      contains(capture_paced_playback, "while (offset + kPlaybackIsoFramesPerTransfer <= count)") &&
      contains(capture_paced_playback,
               "[self queuePlaybackWithRequests:&requests[offset] count:kPlaybackIsoFramesPerTransfer]") &&
      contains(capture_paced_playback,
               "_pendingPlaybackRequestCount + count > kPlaybackIsoFramesPerTransfer") &&
      contains(capture_paced_playback,
               "_pendingPlaybackRequestCount >= kPlaybackIsoFramesPerTransfer");

  const bool capture_submit_counter_success_only =
      appears_before(capture_queue,
                     "atomic_fetch_add_explicit(&_captureSubmitAttemptsAtomic, 1, memory_order_relaxed);",
                     "BOOL queued = [self submitCaptureTransfer:transfer") &&
      appears_before(capture_queue,
                     "if (!queued) {",
                     "atomic_fetch_add_explicit(&_captureTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool playback_submit_counter_success_only =
      appears_before(playback_with_requests,
                     "atomic_fetch_add_explicit(&_playbackSubmitAttemptsAtomic, 1, memory_order_relaxed);",
                     "BOOL queued = [self submitPlaybackTransfer:transfer") &&
      appears_before(playback_with_requests,
                     "if (!queued) {",
                     "atomic_fetch_add_explicit(&_playbackTransfersSubmittedAtomic, 1, memory_order_relaxed);");
  const bool completion_counters_completion_owned =
      contains(capture_completion, "_captureTransfersCompletedAtomic") &&
      contains(playback_completion, "_playbackTransfersCompletedAtomic");
  const bool timestamps_use_physical_counts =
      contains(hal_source, "ExpectedCaptureIsoTransferTicks()") &&
      contains(hal_source, "ExpectedPlaybackIsoTransferTicks()") &&
      contains(playback_with_requests, "[self nextPlaybackFirstFrameNumberForCount:count]") &&
      contains(capture_completion, "captureCompletionTime = mach_absolute_time()") &&
      contains(playback_completion, "playbackCompletionTime = mach_absolute_time()");
  const bool no_partial_submit_path =
      !contains(capture_queue, "partial") && !contains(playback_with_requests, "partial");

  const bool runtime_geometry_observable =
      contains(hal_source, "stats.logicalIsoFramesPerTransfer = kIsoFramesPerTransfer;") &&
      contains(hal_source, "stats.captureIsoFramesPerTransfer = kCaptureIsoFramesPerTransfer;") &&
      contains(hal_source, "stats.playbackIsoFramesPerTransfer = kPlaybackIsoFramesPerTransfer;") &&
      contains(control_source, "logicalIsoFramesPerTransfer=%u") &&
      contains(control_source, "captureIsoFramesPerTransfer=%u") &&
      contains(control_source, "playbackIsoFramesPerTransfer=%u") &&
      contains(run_soundcheck, "\"logicalIsoFramesPerTransfer\"") &&
      contains(stream_stats_analyzer, "\"runtime_geometry\"");
  const bool submit_cadence_observable =
      contains(control_source, "captureSubmitAttempts=%llu") &&
      contains(control_source, "captureTransfersSubmitted=%llu") &&
      contains(control_source, "playbackSubmitAttempts=%llu") &&
      contains(control_source, "playbackTransfersSubmitted=%llu") &&
      contains(run_soundcheck, "\"captureSubmitAttempts\"") &&
      contains(run_soundcheck, "\"playbackSubmitAttempts\"") &&
      contains(stream_stats_analyzer, "\"capture_submit_reduction_ratio_vs_logical\"") &&
      contains(stream_stats_analyzer, "\"playback_submit_reduction_ratio_vs_base\"");

  constexpr std::uint32_t kLogicalIsoFrames = 8;
  constexpr std::uint32_t kPreparedSlotsPerSubmit = 8;
  constexpr std::uint32_t kExpectedPreparedTransactions = kLogicalIsoFrames * kPreparedSlotsPerSubmit;
  constexpr std::uint32_t kExpectedSubmitReductionRatio = kPreparedSlotsPerSubmit;
  constexpr bool physical_evidence_present = false;
  constexpr bool product_claim_allowed = false;

  std::vector<std::string> blockers;
  if (!sources_present) blockers.push_back("required_sources_missing");
  if (!opt_in_profile_binds_64_transaction_geometry) {
    blockers.push_back("hal_prepared_runtime_opt_in_geometry_not_bound");
  }
  if (!default_runtime_preserved) blockers.push_back("default_hal_runtime_geometry_drifted");
  if (!prepared_bridge_opt_in_build_only) blockers.push_back("prepared_bridge_not_opt_in_only");
  if (!compile_time_geometry_guard_present) blockers.push_back("compile_time_geometry_guard_missing");
  if (!prepared_bridge_api_present) blockers.push_back("prepared_bridge_api_missing");
  if (!hal_uses_directional_prepared_byte_geometry) {
    blockers.push_back("hal_directional_prepared_byte_geometry_missing");
  }
  if (!capture_pool_uses_prepared_geometry) blockers.push_back("capture_pool_not_prepared_geometry");
  if (!playback_pool_uses_prepared_geometry) blockers.push_back("playback_pool_not_prepared_geometry");
  if (!prepared_runtime_dispatch_path_present) {
    blockers.push_back("prepared_runtime_dispatch_path_missing");
  }
  if (!transfer_pool_lifetime_completion_owned) {
    blockers.push_back("transfer_lifetime_not_completion_owned");
  }
  if (!capture_enqueue_uses_prepared_geometry) {
    blockers.push_back("capture_enqueue_not_prepared_geometry");
  }
  if (!playback_enqueue_uses_prepared_geometry) {
    blockers.push_back("playback_enqueue_not_prepared_geometry");
  }
  if (!capture_paced_playback_batches_to_prepared_geometry) {
    blockers.push_back("capture_paced_playback_not_prepared_batching");
  }
  if (!capture_submit_counter_success_only) blockers.push_back("capture_submit_counter_not_safe");
  if (!playback_submit_counter_success_only) blockers.push_back("playback_submit_counter_not_safe");
  if (!completion_counters_completion_owned) blockers.push_back("completion_counters_missing");
  if (!timestamps_use_physical_counts) blockers.push_back("timestamp_physical_count_binding_missing");
  if (!no_partial_submit_path) blockers.push_back("partial_submit_path_present");
  if (!runtime_geometry_observable) blockers.push_back("runtime_geometry_not_observable");
  if (!submit_cadence_observable) blockers.push_back("submit_cadence_not_observable");

  const bool pass = blockers.empty();

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.hal-prepared-runtime-binding-contract.v1\",\n"
            << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"meaning\": \"PASS means the opt-in HAL prepared-runtime profile is bound to actual enqueue geometry and observability; it is not physical CPU or audio superiority\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  print_bool("sources_present", sources_present);
  print_bool("opt_in_profile_binds_64_transaction_geometry",
             opt_in_profile_binds_64_transaction_geometry);
  print_bool("default_runtime_preserved", default_runtime_preserved);
  print_bool("prepared_bridge_opt_in_build_only", prepared_bridge_opt_in_build_only);
  print_bool("compile_time_geometry_guard_present", compile_time_geometry_guard_present);
  print_bool("prepared_bridge_api_present", prepared_bridge_api_present);
  print_bool("hal_uses_directional_prepared_byte_geometry",
             hal_uses_directional_prepared_byte_geometry);
  print_bool("capture_pool_uses_prepared_geometry", capture_pool_uses_prepared_geometry);
  print_bool("playback_pool_uses_prepared_geometry", playback_pool_uses_prepared_geometry);
  print_bool("prepared_runtime_dispatch_path_present", prepared_runtime_dispatch_path_present);
  print_bool("transfer_pool_lifetime_completion_owned", transfer_pool_lifetime_completion_owned);
  print_bool("capture_enqueue_uses_prepared_geometry", capture_enqueue_uses_prepared_geometry);
  print_bool("playback_enqueue_uses_prepared_geometry", playback_enqueue_uses_prepared_geometry);
  print_bool("capture_paced_playback_batches_to_prepared_geometry",
             capture_paced_playback_batches_to_prepared_geometry);
  print_bool("capture_submit_counter_success_only", capture_submit_counter_success_only);
  print_bool("playback_submit_counter_success_only", playback_submit_counter_success_only);
  print_bool("completion_counters_completion_owned", completion_counters_completion_owned);
  print_bool("timestamps_use_physical_counts", timestamps_use_physical_counts);
  print_bool("no_partial_submit_path", no_partial_submit_path);
  print_bool("runtime_geometry_observable", runtime_geometry_observable);
  print_bool("submit_cadence_observable", submit_cadence_observable);
  print_number("expected_logical_iso_frames", kLogicalIsoFrames);
  print_number("expected_slots_per_submit", kPreparedSlotsPerSubmit);
  print_number("expected_capture_transactions_per_submit", kExpectedPreparedTransactions);
  print_number("expected_playback_transactions_per_submit", kExpectedPreparedTransactions);
  print_number("expected_submit_reduction_ratio", kExpectedSubmitReductionRatio);
  print_bool("physical_evidence_present", physical_evidence_present);
  print_bool("product_claim_allowed", product_claim_allowed);
  print_string_array("blockers", blockers);
  std::cout
      << "  \"next_required_action\": \"BUILD_HAL_PREPARED_RUNTIME_THEN_LOCK_GATED_PHYSICAL_SUBMIT_COUNTER_AB\",\n"
      << "  \"blocked_claim\": \"NO_CPU_AUDIOPHILE_OR_TIMECODE_SUPERIORITY_CLAIM_UNTIL_PREPARED_RUNTIME_COUNTERS_AND_CAPTURE_QUALITY_PASS_ON_HARDWARE\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
