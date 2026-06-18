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

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << values[index] << "\"";
  }
  std::cout << "]";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto hal_source = read_file(root / "src/hal/OpenA8DJUSB.m");
  const auto makefile = read_file(root / "Makefile");

  const bool source_present = !hal_source.empty();
  const bool makefile_present = !makefile.empty();
  const bool build_exposes_capture_iso =
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(makefile, "-DOPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER=$(HAL_CAPTURE_ISO_FRAMES)");
  const bool default_preserves_legacy_logical_size =
      contains(makefile, "HAL_ISO_FRAMES ?= 8") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)");
  const bool source_has_capture_macro =
      contains(hal_source, "#ifndef OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER") &&
      contains(hal_source,
               "#define OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER OPENA8DJ_ISO_FRAMES_PER_TRANSFER");
  const bool source_has_capture_constant =
      contains(hal_source,
               "kCaptureIsoFramesPerTransfer = OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER");
  const bool capture_pool_uses_physical_size =
      contains(hal_source, "CreateIsoTransferWithCapacity(kCaptureIsoFramesPerTransfer") &&
      contains(hal_source, "(NSUInteger)kCaptureIsoFramesPerTransfer * kIsoBytesPerFrame");
  const bool capture_queue_uses_physical_size =
      contains(hal_source, "uint32_t requests[kCaptureIsoFramesPerTransfer]") &&
      contains(hal_source, "frame < kCaptureIsoFramesPerTransfer") &&
      contains(hal_source, "count:kCaptureIsoFramesPerTransfer");
  const bool capture_clock_uses_physical_size =
      contains(hal_source, "ExpectedIsoTransferTicksForFrames(kCaptureIsoFramesPerTransfer)");
  const bool capture_paced_playback_accepts_full_batch =
      contains(hal_source, "uint32_t playbackRequests[kCaptureIsoFramesPerTransfer]") &&
      contains(hal_source, "playbackRequestCount < kCaptureIsoFramesPerTransfer");
  const bool playback_logical_batcher_still_chunks =
      contains(hal_source, "offset + kPlaybackIsoFramesPerTransfer <= count") &&
      contains(hal_source, "queuePlaybackWithRequests:&requests[offset] count:kPlaybackIsoFramesPerTransfer") &&
      contains(hal_source,
               "NSUInteger room = kPlaybackIsoFramesPerTransfer - _pendingPlaybackRequestCount") &&
      contains(hal_source, "NSUInteger take = count < room ? count : room") &&
      contains(hal_source,
               "[self queuePlaybackWithRequests:_pendingPlaybackRequests\n"
               "                                                    count:kPlaybackIsoFramesPerTransfer]") &&
      !contains(hal_source,
                "if (kPlaybackCoalesceTransfers <= 1) {\n"
                "        return [self queuePlaybackWithRequests:requests count:count];\n"
                "    }") &&
      !contains(hal_source,
                "_pendingPlaybackRequestCount + count > kPlaybackIsoFramesPerTransfer");
  const bool makefile_exposes_capture_batch_diagnostic =
      contains(makefile, "hal-capture-batch-diagnostic:") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES=64") &&
      contains(makefile, "HAL_PLAYBACK_ISO_FRAMES=8") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS=1") &&
      contains(makefile, "HAL_PREPARED_USB_SUBMIT_RUNTIME=0");

  std::vector<std::string> failures;
  if (!source_present) failures.push_back("hal_source_missing");
  if (!makefile_present) failures.push_back("makefile_missing");
  if (!build_exposes_capture_iso) failures.push_back("build_capture_iso_flag_missing");
  if (!default_preserves_legacy_logical_size) failures.push_back("default_capture_iso_not_legacy_safe");
  if (!source_has_capture_macro) failures.push_back("capture_iso_macro_missing");
  if (!source_has_capture_constant) failures.push_back("capture_iso_constant_missing");
  if (!capture_pool_uses_physical_size) failures.push_back("capture_pool_not_physical_sized");
  if (!capture_queue_uses_physical_size) failures.push_back("capture_queue_not_physical_sized");
  if (!capture_clock_uses_physical_size) failures.push_back("capture_clock_not_physical_sized");
  if (!capture_paced_playback_accepts_full_batch) {
    failures.push_back("capture_paced_playback_truncates_batched_capture");
  }
  if (!playback_logical_batcher_still_chunks) {
    failures.push_back("playback_logical_chunker_missing");
  }
  if (!makefile_exposes_capture_batch_diagnostic) {
    failures.push_back("capture_batch_diagnostic_target_missing");
  }

  const bool pass = failures.empty();

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-logical-capture-batching-contract.v1\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means capture may batch physical USB transfers without changing the default logical ISO8 audio cadence\",\n"
      << "  \"source_present\": " << (source_present ? "true" : "false") << ",\n"
      << "  \"makefile_present\": " << (makefile_present ? "true" : "false") << ",\n"
      << "  \"build_exposes_capture_iso\": " << (build_exposes_capture_iso ? "true" : "false")
      << ",\n"
      << "  \"default_preserves_legacy_logical_size\": "
      << (default_preserves_legacy_logical_size ? "true" : "false") << ",\n"
      << "  \"capture_pool_uses_physical_size\": "
      << (capture_pool_uses_physical_size ? "true" : "false") << ",\n"
      << "  \"capture_queue_uses_physical_size\": "
      << (capture_queue_uses_physical_size ? "true" : "false") << ",\n"
      << "  \"capture_clock_uses_physical_size\": "
      << (capture_clock_uses_physical_size ? "true" : "false") << ",\n"
      << "  \"capture_paced_playback_accepts_full_batch\": "
      << (capture_paced_playback_accepts_full_batch ? "true" : "false") << ",\n"
      << "  \"playback_logical_batcher_still_chunks\": "
      << (playback_logical_batcher_still_chunks ? "true" : "false") << ",\n";
  std::cout
      << "  \"makefile_exposes_capture_batch_diagnostic\": "
      << (makefile_exposes_capture_batch_diagnostic ? "true" : "false") << ",\n";
  print_string_array("failures", failures);
  std::cout
      << ",\n"
      << "  \"candidate_build_flags\": "
         "\"HAL_ISO_FRAMES=8 HAL_CAPTURE_ISO_FRAMES=64 HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_ISO_FRAMES=8\",\n"
      << "  \"blocked_claim\": "
         "\"NO_RUNTIME_CPU_SUPERIORITY_CLAIM_UNTIL_OPT_IN_CAPTURE_BATCHING_HAS_SAME_WINDOW_PHYSICAL_AB_METRICS\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
