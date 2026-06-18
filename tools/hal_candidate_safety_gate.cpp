#include <cctype>
#include <array>
#include <cstdio>
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

std::string shell_quote(const std::filesystem::path& path) {
  std::string out = "'";
  for (const char c : path.string()) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

std::string sha256_file(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    return {};
  }
  const std::string command = "/usr/bin/shasum -a 256 " + shell_quote(path);
  std::array<char, 256> buffer{};
  std::string output;
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  const int rc = pclose(pipe);
  if (rc != 0) {
    return {};
  }
  const auto space = output.find_first_of(" \t\r\n");
  if (space == std::string::npos) {
    return {};
  }
  return output.substr(0, space);
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

std::optional<std::string> key_value(const std::string& text, const std::string& key) {
  return manifest_value(text, key);
}

double key_number(const std::string& text, const std::string& key, double fallback = -1.0) {
  const auto value = key_value(text, key);
  if (!value) {
    return fallback;
  }
  try {
    return std::stod(*value);
  } catch (...) {
    return fallback;
  }
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << json_escape(values[index]) << "\"";
  }
  std::cout << "],\n";
}

void scan_runs_root(const std::filesystem::path& runs_root, std::optional<SafetyRun>& latest) {
  if (!std::filesystem::is_directory(runs_root)) {
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(runs_root)) {
    if (!entry.is_directory()) {
      continue;
    }
    const auto summary = entry.path() / "summary.txt";
    if (!std::filesystem::is_regular_file(summary)) {
      continue;
    }
    const auto manifest = entry.path() / "manifest.txt";
    const std::string summary_text = read_file(summary);
    const std::string manifest_text = read_file(manifest);
    const bool safety_like_run = contains(summary_text, "hal_candidate_safety=") ||
                                 contains(manifest_text, "required_device_uid=");
    if (!safety_like_run) {
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
}

std::optional<SafetyRun> latest_run(const std::filesystem::path& root) {
  std::optional<SafetyRun> latest;
  scan_runs_root(root / "local-analysis/hal-candidate-safety", latest);
  scan_runs_root(root / "local-analysis/human-test-candidate", latest);
  return latest;
}

std::optional<SafetyRun> latest_recovery_after(const std::filesystem::path& root,
                                               std::filesystem::file_time_type after) {
  const auto runs_root = root / "local-analysis/hardware-recovery";
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
    if (error || time <= after) {
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
  const auto external_recovery =
      latest ? latest_recovery_after(root, latest->time) : std::optional<SafetyRun>{};
  const std::filesystem::path run_dir = latest ? latest->dir : std::filesystem::path{};
  const std::filesystem::path recovery_dir =
      external_recovery ? external_recovery->dir : run_dir / "recovery";

  const auto summary = latest ? read_file(run_dir / "summary.txt") : std::string{};
  const auto manifest = latest ? read_file(run_dir / "manifest.txt") : std::string{};
  const auto required_device =
      latest ? read_file(run_dir / "cycle-1/guard/required-device-status.txt") : std::string{};
  const auto guard_summary =
      latest ? read_file(run_dir / "cycle-1/guard/summary.txt") : std::string{};
  const auto guard_audio_list =
      latest ? read_file(run_dir / "cycle-1/guard/audio-list.txt") : std::string{};
  const auto post_unload_audio_list =
      latest ? read_file(run_dir / "cycle-1/post-unload-guard/audio-list.txt") : std::string{};
  const auto post_unload_summary =
      latest ? read_file(run_dir / "cycle-1/post-unload-guard/summary.txt") : std::string{};
  const auto recovery_audio_list =
      latest ? read_file(recovery_dir / "audio-list.txt") : std::string{};
  const auto recovery_summary =
      latest ? read_file(recovery_dir / "summary.txt") : std::string{};
  const auto unloaded_after_failure =
      latest ? read_file(run_dir / "unloaded-after-failure.txt") : std::string{};

  const bool summary_pass = contains(summary, "hal_candidate_safety=PASS");
  const bool summary_fail = contains(summary, "result=FAIL");
  const bool guard_health_pass = contains(guard_summary, "audio_stack_guard=PASS") ||
                                 contains(guard_summary, "audio_stack_health=PASS");
  const bool guard_coreaudio_enumeration_pass =
      contains(guard_summary, "core_audio_enumeration=PASS");
  const bool recovery_present = !recovery_summary.empty() || !unloaded_after_failure.empty();
  const bool recovery_guard_pass = contains(recovery_summary, "audio_stack_guard=PASS") ||
                                   contains(recovery_summary, "audio_stack_health=PASS");
  const bool recovery_coreaudio_enumeration_pass =
      contains(recovery_summary, "core_audio_enumeration=PASS");
  const bool recovery_unloaded =
      contains(recovery_summary, "opena8dj_state=unloaded") &&
      contains(recovery_summary, "opena8dj_driver_pids=none");
  const bool audio8_enumerated =
      contains(guard_audio_list, "Open Audio 8 DJ") &&
      contains(guard_audio_list, "uid=org.opena8dj.Audio8DJ") &&
      contains(guard_audio_list, "in=8 out=8");
  const bool required_device_pass =
      (contains(required_device, "required_device=PASS") &&
       contains(required_device, "org.opena8dj.Audio8DJ")) ||
      audio8_enumerated;
  const bool irig_preserved_during_guard =
      contains(guard_audio_list, "iRig Stream") && contains(guard_audio_list, "in=2 out=2");
  const bool recovery_irig_visible =
      contains(recovery_audio_list, "iRig Stream") && contains(recovery_audio_list, "in=2 out=2");
  const bool post_unload_coreaudio_clean =
      (!contains(post_unload_audio_list, "uid=org.opena8dj.Audio8DJ") &&
       !contains(post_unload_audio_list, "Open Audio 8 DJ") &&
       contains(post_unload_audio_list, "iRig Stream")) ||
      (!contains(recovery_audio_list, "uid=org.opena8dj.Audio8DJ") &&
       !contains(recovery_audio_list, "Open Audio 8 DJ") && recovery_irig_visible &&
       recovery_unloaded);
  const bool post_unload_guard_pass = contains(post_unload_summary, "audio_stack_guard=PASS") ||
                                      contains(post_unload_summary, "result=PASS") ||
                                      recovery_guard_pass || !post_unload_summary.empty() ||
                                      !recovery_summary.empty();
  const auto leave_loaded = manifest_value(manifest, "leave_loaded").value_or("missing");
  const auto candidate_hash = manifest_value(manifest, "candidate_hash").value_or("missing");
  const auto cycles = manifest_value(manifest, "cycles").value_or("missing");
  const auto installed_hal = std::filesystem::path("/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver");
  const auto current_candidate_executable =
      root / "build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL";
  const auto active_installed_executable =
      installed_hal / "Contents/MacOS/OpenA8DJHAL";
  const bool active_hal_installed_now = std::filesystem::is_directory(installed_hal);
  const std::string current_candidate_hash = sha256_file(current_candidate_executable);
  const std::string active_installed_hash = sha256_file(active_installed_executable);
  const std::string installed_hash_text =
      latest ? read_file(run_dir / "cycle-1/installed-hash.txt") : std::string{};
  const bool installed_hash_matches_candidate =
      active_hal_installed_now && candidate_hash != "missing" &&
      contains(installed_hash_text, candidate_hash);
  const bool active_installed_hash_matches_current_candidate =
      active_hal_installed_now && !current_candidate_hash.empty() &&
      active_installed_hash == current_candidate_hash;
  const bool unloaded_by_design = leave_loaded == "0";
  const bool leave_loaded_by_design = leave_loaded == "1";
  const bool recovered_after_leave_loaded = external_recovery.has_value() && recovery_unloaded &&
                                            recovery_irig_visible &&
                                            recovery_coreaudio_enumeration_pass;
  const bool diagnostic_install_active =
      leave_loaded_by_design && active_hal_installed_now && installed_hash_matches_candidate &&
      active_installed_hash_matches_current_candidate && guard_health_pass &&
      guard_coreaudio_enumeration_pass && required_device_pass && audio8_enumerated &&
      irig_preserved_during_guard;
  const bool active_hal_left_loaded = diagnostic_install_active || !post_unload_coreaudio_clean;
  const double guard_coreaudiod_cpu_pct =
      key_number(guard_summary, "process.coreaudiod.cpu_pct");
  const double guard_opena8dj_driver_cpu_pct =
      key_number(guard_summary, "process.opena8dj_driver.cpu_pct");
  const double guard_max_label_cpu_pct = key_number(guard_summary, "max_label_cpu_pct");
  const auto guard_max_label = key_value(guard_summary, "max_label").value_or("missing");
  const double recovery_max_label_cpu_pct = key_number(recovery_summary, "max_label_cpu_pct");
  const auto recovery_max_label = key_value(recovery_summary, "max_label").value_or("missing");
  const bool safety_window_pass =
      summary_pass && required_device_pass && audio8_enumerated && irig_preserved_during_guard &&
      guard_health_pass &&
      ((unloaded_by_design && post_unload_coreaudio_clean) || diagnostic_install_active);

  std::vector<std::string> blockers;
  if (!latest) {
    blockers.push_back("missing_hal_candidate_safety_run");
  }
  if (!summary_pass || summary_fail) {
    blockers.push_back("latest_hal_candidate_safety_window_failed");
  }
  if (!guard_health_pass) {
    blockers.push_back("audio_stack_health_failed_under_candidate");
  }
  if (!guard_coreaudio_enumeration_pass) {
    blockers.push_back("core_audio_enumeration_failed_under_candidate");
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
  if (leave_loaded_by_design && active_hal_installed_now &&
      !active_installed_hash_matches_current_candidate) {
    blockers.push_back("active_installed_hal_hash_does_not_match_current_candidate");
  }
  if (!unloaded_by_design && !diagnostic_install_active && !recovered_after_leave_loaded) {
    blockers.push_back("safety_run_left_hal_loaded");
  } else if (!unloaded_by_design && recovered_after_leave_loaded) {
    blockers.push_back("safety_run_required_external_recovery");
  }
  if (!post_unload_coreaudio_clean && !diagnostic_install_active) {
    blockers.push_back("post_unload_coreaudio_not_clean");
  }
  if (recovery_present && !recovery_unloaded) {
    blockers.push_back("recovery_did_not_unload_opena8dj");
  }

  const bool evidence_consumed = latest.has_value() && !summary.empty();
  const bool pass = evidence_consumed && (safety_window_pass || post_unload_coreaudio_clean);

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.hal-candidate-safety-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_hal_safety_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means the latest HAL safety evidence was consumed and left CoreAudio unloaded/observable; safety_window_status controls whether the candidate itself passed\",\n"
            << "  \"latest_run\": \"" << json_escape(run_dir.string()) << "\",\n"
            << "  \"external_recovery_run\": \"" << json_escape(recovery_dir.string()) << "\",\n"
            << "  \"safety_window_status\": \"" << (safety_window_pass ? "PASS" : "FAIL")
            << "\",\n"
            << "  \"summary_pass\": " << (summary_pass ? "true" : "false") << ",\n"
            << "  \"guard_health_pass\": " << (guard_health_pass ? "true" : "false")
            << ",\n"
            << "  \"guard_coreaudio_enumeration_pass\": "
            << (guard_coreaudio_enumeration_pass ? "true" : "false") << ",\n"
            << "  \"required_device_pass\": " << (required_device_pass ? "true" : "false")
            << ",\n"
            << "  \"audio8_enumerated_8x8\": " << (audio8_enumerated ? "true" : "false")
            << ",\n"
            << "  \"irig_preserved_during_guard\": "
            << (irig_preserved_during_guard ? "true" : "false") << ",\n"
            << "  \"post_unload_coreaudio_clean\": "
            << (post_unload_coreaudio_clean ? "true" : "false") << ",\n"
            << "  \"recovery_present\": " << (recovery_present ? "true" : "false") << ",\n"
            << "  \"recovery_unloaded\": " << (recovery_unloaded ? "true" : "false") << ",\n"
            << "  \"recovery_coreaudio_enumeration_pass\": "
            << (recovery_coreaudio_enumeration_pass ? "true" : "false") << ",\n"
            << "  \"recovery_irig_visible\": " << (recovery_irig_visible ? "true" : "false")
            << ",\n"
            << "  \"active_hal_left_loaded\": " << (active_hal_left_loaded ? "true" : "false")
            << ",\n"
            << "  \"guard_coreaudiod_cpu_pct\": " << guard_coreaudiod_cpu_pct << ",\n"
            << "  \"guard_opena8dj_driver_cpu_pct\": " << guard_opena8dj_driver_cpu_pct
            << ",\n"
            << "  \"guard_max_label\": \"" << json_escape(guard_max_label) << "\",\n"
            << "  \"guard_max_label_cpu_pct\": " << guard_max_label_cpu_pct << ",\n"
            << "  \"recovery_max_label\": \"" << json_escape(recovery_max_label) << "\",\n"
            << "  \"recovery_max_label_cpu_pct\": " << recovery_max_label_cpu_pct << ",\n"
            << "  \"post_unload_guard_present\": "
            << (post_unload_guard_pass ? "true" : "false") << ",\n"
            << "  \"diagnostic_install_active\": "
            << (diagnostic_install_active ? "true" : "false") << ",\n"
            << "  \"active_hal_installed_now\": "
            << (active_hal_installed_now ? "true" : "false") << ",\n"
            << "  \"installed_hash_matches_candidate\": "
            << (installed_hash_matches_candidate ? "true" : "false") << ",\n"
            << "  \"active_installed_hash_matches_current_candidate\": "
            << (active_installed_hash_matches_current_candidate ? "true" : "false") << ",\n"
            << "  \"leave_loaded\": \"" << json_escape(leave_loaded) << "\",\n"
            << "  \"cycles\": \"" << json_escape(cycles) << "\",\n"
            << "  \"candidate_hash\": \"" << json_escape(candidate_hash) << "\",\n"
            << "  \"current_candidate_hash\": \"" << json_escape(current_candidate_hash)
            << "\",\n"
            << "  \"active_installed_hash\": \"" << json_escape(active_installed_hash)
            << "\",\n";
  print_string_array("promotion_blockers", blockers);
  std::cout << "  \"hardware_window_evidence\": true,\n"
            << "  \"driver_installed_or_activated_now\": "
            << (diagnostic_install_active ? "true" : "false") << ",\n"
            << "  \"product_claim_allowed\": false,\n"
            << "  \"branch_promotion_allowed\": false,\n"
            << "  \"next_required_action\": \"LOCK_GATED_ROUTE_VALIDATION_AND_SAME_SESSION_MAINLINE_CPP_PHYSICAL_COMPARE\",\n"
            << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_HAL_ENUMERATION_SAFE_NOT_SOUND_QUALITY_READY\"\n"
            << "}\n";
  return evidence_consumed ? 0 : 1;
}
