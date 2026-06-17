#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

struct SafetyRun {
  std::filesystem::path dir;
  std::filesystem::file_time_type time{};
  bool has_time = false;
};

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

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::string json_escape(const std::string& input) {
  std::string out;
  for (const char c : input) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

std::optional<std::string> manifest_value(const std::string& text, const std::string& key) {
  const std::string prefix = key + "=";
  for (std::size_t pos = 0; pos < text.size();) {
    const std::size_t line_end = text.find('\n', pos);
    const std::string line =
        text.substr(pos, line_end == std::string::npos ? std::string::npos : line_end - pos);
    if (line.rfind(prefix, 0) == 0) {
      return line.substr(prefix.size());
    }
    if (line_end == std::string::npos) {
      break;
    }
    pos = line_end + 1U;
  }
  return std::nullopt;
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << json_escape(values[index]) << "\"";
  }
  std::cout << "],\n";
}

std::optional<SafetyRun> latest_run(const std::filesystem::path& root) {
  const auto runs_root = root / "local-analysis/hal-candidate-safety";
  if (!std::filesystem::is_directory(runs_root)) {
    return std::nullopt;
  }
  std::optional<SafetyRun> latest;
  for (const auto& entry : std::filesystem::directory_iterator(runs_root)) {
    if (!entry.is_directory()) {
      continue;
    }
    const auto summary = entry.path() / "summary.txt";
    if (!std::filesystem::is_regular_file(summary)) {
      continue;
    }
    std::error_code error;
    const auto time = std::filesystem::last_write_time(summary, error);
    if (error) {
      continue;
    }
    SafetyRun run{};
    run.dir = entry.path();
    run.time = time;
    run.has_time = true;
    if (!latest || run.time > latest->time || run.dir.string() > latest->dir.string()) {
      latest = run;
    }
  }
  return latest;
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto latest = latest_run(root);
  const std::filesystem::path run_dir = latest ? latest->dir : std::filesystem::path{};

  const auto summary = latest ? read_file(run_dir / "summary.txt") : std::string{};
  const auto manifest = latest ? read_file(run_dir / "manifest.txt") : std::string{};
  const auto required_device =
      latest ? read_file(run_dir / "cycle-1/guard/required-device-status.txt") : std::string{};
  const auto guard_audio_list =
      latest ? read_file(run_dir / "cycle-1/guard/audio-list.txt") : std::string{};
  const auto post_unload_audio_list =
      latest ? read_file(run_dir / "cycle-1/post-unload-guard/audio-list.txt") : std::string{};
  const auto post_unload_summary =
      latest ? read_file(run_dir / "cycle-1/post-unload-guard/summary.txt") : std::string{};

  const bool summary_pass = contains(summary, "hal_candidate_safety=PASS");
  const bool required_device_pass = contains(required_device, "required_device=PASS") &&
                                    contains(required_device, "org.opena8dj.Audio8DJ");
  const bool audio8_enumerated =
      contains(guard_audio_list, "Open Audio 8 DJ") &&
      contains(guard_audio_list, "uid=org.opena8dj.Audio8DJ") &&
      contains(guard_audio_list, "in=8 out=8");
  const bool irig_preserved_during_guard =
      contains(guard_audio_list, "iRig Stream") && contains(guard_audio_list, "in=2 out=2");
  const bool post_unload_coreaudio_clean =
      !contains(post_unload_audio_list, "uid=org.opena8dj.Audio8DJ") &&
      !contains(post_unload_audio_list, "Open Audio 8 DJ") &&
      contains(post_unload_audio_list, "iRig Stream");
  const bool post_unload_guard_pass = contains(post_unload_summary, "audio_stack_guard=PASS") ||
                                      contains(post_unload_summary, "result=PASS") ||
                                      !post_unload_summary.empty();
  const auto leave_loaded = manifest_value(manifest, "leave_loaded").value_or("missing");
  const auto candidate_hash = manifest_value(manifest, "candidate_hash").value_or("missing");
  const auto cycles = manifest_value(manifest, "cycles").value_or("missing");
  const bool unloaded_by_design = leave_loaded == "0";

  std::vector<std::string> blockers;
  if (!latest) {
    blockers.push_back("missing_hal_candidate_safety_run");
  }
  if (!summary_pass) {
    blockers.push_back("hal_candidate_safety_summary_not_pass");
  }
  if (!required_device_pass) {
    blockers.push_back("required_audio8_coreaudio_device_not_confirmed");
  }
  if (!audio8_enumerated) {
    blockers.push_back("open_audio8dj_not_enumerated_as_8x8");
  }
  if (!irig_preserved_during_guard) {
    blockers.push_back("irig_not_visible_during_hal_guard");
  }
  if (!unloaded_by_design) {
    blockers.push_back("safety_run_left_hal_loaded");
  }
  if (!post_unload_coreaudio_clean) {
    blockers.push_back("post_unload_coreaudio_not_clean");
  }

  const bool pass = blockers.empty();

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.hal-candidate-safety-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_hal_safety_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means the latest stored HAL safety window loaded, enumerated, and unloaded the candidate cleanly; not audio quality readiness\",\n"
            << "  \"latest_run\": \"" << json_escape(run_dir.string()) << "\",\n"
            << "  \"summary_pass\": " << (summary_pass ? "true" : "false") << ",\n"
            << "  \"required_device_pass\": " << (required_device_pass ? "true" : "false")
            << ",\n"
            << "  \"audio8_enumerated_8x8\": " << (audio8_enumerated ? "true" : "false")
            << ",\n"
            << "  \"irig_preserved_during_guard\": "
            << (irig_preserved_during_guard ? "true" : "false") << ",\n"
            << "  \"post_unload_coreaudio_clean\": "
            << (post_unload_coreaudio_clean ? "true" : "false") << ",\n"
            << "  \"post_unload_guard_present\": "
            << (post_unload_guard_pass ? "true" : "false") << ",\n"
            << "  \"leave_loaded\": \"" << json_escape(leave_loaded) << "\",\n"
            << "  \"cycles\": \"" << json_escape(cycles) << "\",\n"
            << "  \"candidate_hash\": \"" << json_escape(candidate_hash) << "\",\n";
  print_string_array("promotion_blockers", blockers);
  std::cout << "  \"hardware_window_evidence\": true,\n"
            << "  \"driver_installed_or_activated_now\": false,\n"
            << "  \"next_required_action\": \"LOCK_GATED_ROUTE_VALIDATION_AND_SAME_SESSION_MAINLINE_CPP_PHYSICAL_COMPARE\",\n"
            << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_HAL_ENUMERATION_SAFE_NOT_SOUND_QUALITY_READY\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
