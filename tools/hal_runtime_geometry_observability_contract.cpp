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
  const auto control_source = read_file(root / "src/tools/opena8dj-control.c");
  const auto run_soundcheck = read_file(root / "scripts/run-soundcheck");
  const auto stream_stats_analyzer = read_file(root / "scripts/analyze-stream-stats.py");
  const auto makefile = read_file(root / "Makefile");

  const bool hal_source_present = !hal_source.empty();
  const bool control_source_present = !control_source.empty();
  const bool run_soundcheck_present = !run_soundcheck.empty();
  const bool stream_stats_analyzer_present = !stream_stats_analyzer.empty();
  const bool makefile_present = !makefile.empty();
  const bool build_exposes_capture_iso =
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(makefile, "-DOPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER=$(HAL_CAPTURE_ISO_FRAMES)");

  const std::vector<std::string_view> fields = {
      "logicalIsoFramesPerTransfer",
      "captureIsoFramesPerTransfer",
      "playbackBaseIsoFramesPerTransfer",
      "playbackIsoFramesPerTransfer",
      "playbackCoalesceTransfers",
      "captureQueueDepth",
      "playbackQueueTarget",
  };

  bool hal_payload_exposes_runtime_geometry = true;
  bool control_payload_exposes_runtime_geometry = true;
  bool snapshot_populates_runtime_geometry = true;
  bool control_prints_runtime_geometry = true;
  bool soundcheck_tsv_captures_runtime_geometry = true;
  bool analyzer_summarizes_runtime_geometry = true;
  std::vector<std::string> failures;

  if (!hal_source_present) failures.push_back("hal_source_missing");
  if (!control_source_present) failures.push_back("control_source_missing");
  if (!run_soundcheck_present) failures.push_back("run_soundcheck_missing");
  if (!stream_stats_analyzer_present) failures.push_back("stream_stats_analyzer_missing");
  if (!makefile_present) failures.push_back("makefile_missing");
  if (!build_exposes_capture_iso) failures.push_back("build_capture_iso_flag_missing");

  for (const auto field : fields) {
    if (!contains(hal_source, std::string("uint32_t ") + std::string(field) + ";")) {
      hal_payload_exposes_runtime_geometry = false;
      failures.push_back(std::string("hal_payload_missing_") + std::string(field));
    }
    if (!contains(control_source, std::string("uint32_t ") + std::string(field) + ";")) {
      control_payload_exposes_runtime_geometry = false;
      failures.push_back(std::string("control_payload_missing_") + std::string(field));
    }
    if (!contains(control_source, std::string("stats->") + std::string(field))) {
      control_prints_runtime_geometry = false;
      failures.push_back(std::string("control_output_missing_") + std::string(field));
    }
    if (!contains(run_soundcheck, std::string("\"") + std::string(field) + "\",")) {
      soundcheck_tsv_captures_runtime_geometry = false;
      failures.push_back(std::string("soundcheck_tsv_missing_") + std::string(field));
    }
    if (!contains(stream_stats_analyzer, std::string("\"") + std::string(field) + "\",")) {
      analyzer_summarizes_runtime_geometry = false;
      failures.push_back(std::string("analyzer_missing_") + std::string(field));
    }
  }

  const std::vector<std::pair<std::string_view, std::string_view>> assignments = {
      {"stats.logicalIsoFramesPerTransfer", "kIsoFramesPerTransfer"},
      {"stats.captureIsoFramesPerTransfer", "kCaptureIsoFramesPerTransfer"},
      {"stats.playbackBaseIsoFramesPerTransfer", "kPlaybackBaseIsoFramesPerTransfer"},
      {"stats.playbackIsoFramesPerTransfer", "kPlaybackIsoFramesPerTransfer"},
      {"stats.playbackCoalesceTransfers", "kPlaybackCoalesceTransfers"},
      {"stats.captureQueueDepth", "kCaptureQueueDepth"},
      {"stats.playbackQueueTarget", "kPlaybackQueueTarget"},
  };
  for (const auto& [left, right] : assignments) {
    if (!contains(hal_source, std::string(left) + " = " + std::string(right) + ";")) {
      snapshot_populates_runtime_geometry = false;
      failures.push_back(std::string("snapshot_missing_") + std::string(left));
    }
  }

  if (!contains(control_source, "STREAM_STATS_HAS_FIELD(payloadLength, captureQueueDepth)")) {
    control_prints_runtime_geometry = false;
    failures.push_back("control_output_not_backward_compatible");
  }
  if (!contains(control_source, "transport-geometry:") ||
      !contains(control_source, "logicalIsoFramesPerTransfer=%u") ||
      !contains(control_source, "captureIsoFramesPerTransfer=%u") ||
      !contains(control_source, "playbackIsoFramesPerTransfer=%u")) {
    control_prints_runtime_geometry = false;
    failures.push_back("control_runtime_geometry_text_missing");
  }
  if (!contains(stream_stats_analyzer, "\"runtime_geometry\"") ||
      !contains(stream_stats_analyzer, "\"capture_submit_reduction_ratio_vs_logical\"") ||
      !contains(stream_stats_analyzer, "\"playback_submit_reduction_ratio_vs_base\"") ||
      !contains(stream_stats_analyzer, "\"capture_submit_rate_ratio_to_expected\"") ||
      !contains(stream_stats_analyzer, "\"playback_submit_rate_ratio_to_expected\"")) {
    analyzer_summarizes_runtime_geometry = false;
    failures.push_back("analyzer_runtime_geometry_summary_missing");
  }

  const bool pass = failures.empty();

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-runtime-geometry-observability-contract.v1\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means future HAL physical evidence can attribute active ISO geometry and queue settings from stream stats\",\n"
      << "  \"hal_source_present\": " << (hal_source_present ? "true" : "false") << ",\n"
      << "  \"control_source_present\": " << (control_source_present ? "true" : "false") << ",\n"
      << "  \"run_soundcheck_present\": " << (run_soundcheck_present ? "true" : "false") << ",\n"
      << "  \"stream_stats_analyzer_present\": "
      << (stream_stats_analyzer_present ? "true" : "false") << ",\n"
      << "  \"makefile_present\": " << (makefile_present ? "true" : "false") << ",\n"
      << "  \"build_exposes_capture_iso\": " << (build_exposes_capture_iso ? "true" : "false")
      << ",\n"
      << "  \"hal_payload_exposes_runtime_geometry\": "
      << (hal_payload_exposes_runtime_geometry ? "true" : "false") << ",\n"
      << "  \"control_payload_exposes_runtime_geometry\": "
      << (control_payload_exposes_runtime_geometry ? "true" : "false") << ",\n"
      << "  \"snapshot_populates_runtime_geometry\": "
      << (snapshot_populates_runtime_geometry ? "true" : "false") << ",\n"
      << "  \"control_prints_runtime_geometry\": "
      << (control_prints_runtime_geometry ? "true" : "false") << ",\n";
  std::cout
      << "  \"soundcheck_tsv_captures_runtime_geometry\": "
      << (soundcheck_tsv_captures_runtime_geometry ? "true" : "false") << ",\n"
      << "  \"analyzer_summarizes_runtime_geometry\": "
      << (analyzer_summarizes_runtime_geometry ? "true" : "false") << ",\n";
  print_string_array("runtime_geometry_fields",
                     {"logicalIsoFramesPerTransfer",
                      "captureIsoFramesPerTransfer",
                      "playbackBaseIsoFramesPerTransfer",
                      "playbackIsoFramesPerTransfer",
                      "playbackCoalesceTransfers",
                      "captureQueueDepth",
                      "playbackQueueTarget"});
  std::cout << ",\n";
  print_string_array("failures", failures);
  std::cout
      << ",\n"
      << "  \"blocked_claim\": "
         "\"NO_PHYSICAL_HAL_QUALITY_OR_PERFORMANCE_CLAIM_WITHOUT_ACTIVE_RUNTIME_GEOMETRY_IN_EVIDENCE\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
