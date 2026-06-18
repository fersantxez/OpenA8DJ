#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool contains_forbidden(const std::filesystem::path& path,
                        const std::vector<std::string>& forbidden,
                        std::vector<std::string>& hits) {
  std::ifstream input(path);
  if (!input) {
    hits.push_back(path.string() + ":unreadable");
    return true;
  }

  std::string line;
  std::uint32_t line_number = 0;
  bool failed = false;
  while (std::getline(input, line)) {
    line_number += 1;
    for (const auto& item : forbidden) {
      if (line.find(item) != std::string::npos) {
        failed = true;
        hits.push_back(path.string() + ":" + std::to_string(line_number) + ":" + item);
      }
    }
  }
  return failed;
}

std::string trim(std::string_view value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(begin, end - begin + 1));
}

std::map<std::string, std::string> parse_make_defaults(const std::filesystem::path& path,
                                                       std::vector<std::string>& failures) {
  std::ifstream input(path);
  std::map<std::string, std::string> defaults;
  if (!input) {
    failures.push_back(path.string() + ":unreadable");
    return defaults;
  }

  std::string line;
  std::uint32_t line_number = 0;
  while (std::getline(input, line)) {
    line_number += 1;
    const auto assign = line.find("?=");
    if (assign == std::string::npos) {
      continue;
    }
    auto name = trim(std::string_view(line).substr(0, assign));
    auto value = trim(std::string_view(line).substr(assign + 2));
    if (name.empty()) {
      failures.push_back(path.string() + ":" + std::to_string(line_number) + ":empty-default-name");
      continue;
    }
    defaults[name] = value;
  }
  return defaults;
}

std::string json_escape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    if (ch == '"' || ch == '\\') {
      escaped.push_back('\\');
    }
    escaped.push_back(ch);
  }
  return escaped;
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

  const std::vector<std::filesystem::path> audited_files = {
      root / "CMakeLists.txt",
      root / "scripts/run-cpp-offline-gates",
      root / "tools/offline_bench.cpp",
      root / "tools/packet_matrix.cpp",
      root / "tools/timecode_matrix.cpp",
      root / "tools/timecode_readiness_gate.cpp",
      root / "tools/dvs_signal_smoke.cpp",
      root / "tools/realtime_audit.cpp",
      root / "tools/transport_budget_model.cpp",
      root / "tools/hot_path_timing_analysis.cpp",
      root / "tools/quality_root_cause_analysis.cpp",
      root / "tools/driverkit_runtime_contract.cpp",
      root / "tools/driverkit_extension_scaffold_contract.cpp",
      root / "tools/driverkit_prepared_transport_contract.cpp",
      root / "tools/driverkit_usb_submit_binding_contract.cpp",
      root / "tools/driverkit_usb_request_lifecycle_contract.cpp",
      root / "tools/driverkit_usb_request_shutdown_contract.cpp",
      root / "tools/prepared_transport_packet_contract.cpp",
      root / "tools/prepared_transport_routing_timecode_contract.cpp",
      root / "tools/prepared_transport_recovery_contract.cpp",
      root / "tools/prepared_slot_scheduler_contract.cpp",
      root / "tools/runtime_adapter_contract.cpp",
      root / "tools/usb_submit_plan_contract.cpp",
      root / "tools/usb_submit_payload_contract.cpp",
      root / "tools/prepared_transport_pressure_gate.cpp",
      root / "tools/prepared_transport_migration_gate.cpp",
      root / "tools/physical_run_compare.cpp",
      root / "tools/direct_usb_path_attribution.cpp",
      root / "tools/irig_idle_capture_gate.cpp",
      root / "tools/physical_window_readiness_gate.cpp",
      root / "tools/soundcheck_wav_quality.cpp",
      root / "tools/hal_logical_capture_batching_contract.cpp",
      root / "tools/driverkit_surface_model.cpp",
      root / "tools/evidence_schema_check.cpp",
      root / "tools/static_policy_check.cpp",
      root / "core/include/opena8djcpp/prepared_transport.hpp",
      root / "core/src/prepared_transport.cpp",
      root / "core/include/opena8djcpp/runtime_adapter.hpp",
      root / "core/src/runtime_adapter.cpp",
      root / "core/include/opena8djcpp/usb_submit_plan.hpp",
      root / "core/src/usb_submit_plan.cpp",
      root / "core/include/opena8djcpp/usb_request_pool.hpp",
      root / "core/src/usb_request_pool.cpp",
  };
  const auto join = [](const char* left, const char* right) {
    return std::string(left) + std::string(right);
  };
  const std::vector<std::string> forbidden = {
      join("su", "do"),
      join("systemextensions", "ctl"),
      join("launch", "ctl"),
      join("killall core", "audiod"),
      join("usb", "audiod"),
      join("/Library/Audio/Plug-Ins/", "HAL"),
      join("/Library/System", "Extensions"),
      join("IOUSB", "Host"),
      join("AudioObjectSet", "PropertyData"),
      join("set-", "default"),
  };

  std::vector<std::string> hits;
  for (const auto& path : audited_files) {
    (void)contains_forbidden(path, forbidden, hits);
  }

  std::vector<std::string> default_failures;
  const auto defaults = parse_make_defaults(root / "Makefile", default_failures);
  const std::map<std::string, std::string> expected_defaults = {
      {"HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC", "0"},
      {"HAL_ISO_FRAMES", "8"},
      {"HAL_PLAYBACK_ISO_FRAMES", "$(HAL_ISO_FRAMES)"},
      {"HAL_CAPTURE_ISO_FRAMES", "$(HAL_ISO_FRAMES)"},
      {"HAL_PLAYBACK_COALESCE_TRANSFERS", "1"},
      {"HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE", "0"},
      {"HAL_EXPLICIT_SCHED", "0"},
      {"HAL_OUTPUT_NATIVE", "0"},
      {"HAL_FAST_OUTPUT_PREFETCH_CLEAR", "0"},
      {"HAL_UNROLLED_OUTPUT_PACK", "0"},
      {"HAL_TRANSFER_POOL_CURSOR", "0"},
      {"HAL_REUSE_ISOC_COMPLETIONS", "0"},
      {"HAL_RAW_ISOC_COMPLETIONS", "0"},
      {"HAL_FAST_ISO_TRANSFER_CONFIG", "0"},
      {"HAL_PLAYBACK_PAYLOAD_GUARD", "0"},
      {"HAL_OUTPUT_SAMPLE_TIME_FOLLOWER", "0"},
      {"HAL_IGNORE_OUTPUT_SAMPLE_TIME", "0"},
      {"HAL_FLUSH_OUTPUT_IN_WRITE_MIX", "0"},
      {"HAL_HOT_PATH_TIMING", "0"},
      {"HAL_STREAM_STATS_ATOMIC_ACCUMULATORS", "0"},
      {"HAL_OUTPUT_START_BYTE", "4"},
      {"HAL_OUTPUT_CHECK_OFFSET", "8"},
      {"HAL_VALID_CAPTURE_OUT_LAYOUT", "0"},
      {"HAL_SELECT_ALT0_BEFORE_ALT1", "0"},
  };
  for (const auto& [name, expected] : expected_defaults) {
    const auto found = defaults.find(name);
    if (found == defaults.end()) {
      default_failures.push_back(name + ":missing");
      continue;
    }
    if (found->second != expected) {
      default_failures.push_back(name + ":expected=" + expected + ":actual=" + found->second);
    }
  }

  const bool path_policy = root == std::filesystem::path("/Users/fer/dev/audio8djcpp");
  const bool pass = hits.empty() && path_policy && default_failures.empty();

  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"audited_files\": " << audited_files.size() << ",\n"
            << "  \"forbidden_hits\": " << hits.size() << ",\n"
            << "  \"path_policy\": " << (path_policy ? "true" : "false") << ",\n"
            << "  \"rejected_default_checks\": " << expected_defaults.size() << ",\n"
            << "  \"default_policy_failures\": " << default_failures.size() << ",\n"
            << "  \"hits\": [";
  for (std::size_t index = 0; index < hits.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << json_escape(hits[index]) << "\"";
  }
  std::cout << "],\n"
            << "  \"default_failures\": [";
  for (std::size_t index = 0; index < default_failures.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\""
              << json_escape(default_failures[index]) << "\"";
  }
  std::cout << "]\n}\n";

  return pass ? 0 : 1;
}
