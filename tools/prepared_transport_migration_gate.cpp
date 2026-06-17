#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Gate {
  std::string name;
  bool pass = false;
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

std::string json_key(const std::string& key) {
  return "\"" + key + "\"";
}

std::optional<std::size_t> json_key_position(const std::string& json,
                                             const std::string& key,
                                             std::size_t search_from = 0) {
  const auto pos = json.find(json_key(key), search_from);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  return pos;
}

std::optional<std::string> json_string(const std::string& json,
                                       const std::string& key,
                                       std::size_t search_from = 0) {
  const auto key_pos = json_key_position(json, key, search_from);
  if (!key_pos) {
    return std::nullopt;
  }
  const auto colon = json.find(':', *key_pos);
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (start >= json.size() || json[start] != '"') {
    return std::nullopt;
  }
  ++start;
  std::string value;
  bool escaped = false;
  for (std::size_t index = start; index < json.size(); ++index) {
    const char c = json[index];
    if (escaped) {
      value.push_back(c);
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      return value;
    }
    value.push_back(c);
  }
  return std::nullopt;
}

std::optional<double> json_number(const std::string& json,
                                  const std::string& key,
                                  std::size_t search_from = 0) {
  const auto key_pos = json_key_position(json, key, search_from);
  if (!key_pos) {
    return std::nullopt;
  }
  const auto colon = json.find(':', *key_pos);
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (json.compare(start, 4U, "null") == 0) {
    return std::nullopt;
  }
  std::size_t end = start;
  while (end < json.size()) {
    const char c = json[end];
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' ||
          c == 'e' || c == 'E')) {
      break;
    }
    ++end;
  }
  if (end == start) {
    return std::nullopt;
  }
  try {
    const double value = std::stod(json.substr(start, end - start));
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<bool> json_bool(const std::string& json,
                              const std::string& key,
                              std::size_t search_from = 0) {
  const auto key_pos = json_key_position(json, key, search_from);
  if (!key_pos) {
    return std::nullopt;
  }
  const auto colon = json.find(':', *key_pos);
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (json.compare(start, 4U, "true") == 0) {
    return true;
  }
  if (json.compare(start, 5U, "false") == 0) {
    return false;
  }
  return std::nullopt;
}

bool result_pass(const std::string& json) {
  return json_string(json, "result").value_or("") == "PASS";
}

bool result_fail(const std::string& json) {
  return json_string(json, "result").value_or("") == "FAIL";
}

bool finite(double value) {
  return std::isfinite(value);
}

double number_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

bool number_is_zero(const std::string& json, const std::string& key) {
  const double value = number_or_nan(json_number(json, key));
  return finite(value) && value == 0.0;
}

bool number_at_most(const std::string& json, const std::string& key, double maximum) {
  const double value = number_or_nan(json_number(json, key));
  return finite(value) && value <= maximum;
}

std::string json_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const char c : value) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

void print_bool(const char* name, bool value) {
  std::cout << "  \"" << name << "\": " << (value ? "true" : "false") << ",\n";
}

void print_number(const char* name, double value) {
  std::cout << "  \"" << name << "\": ";
  if (finite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
  std::cout << ",\n";
}

void print_gate_rows(const std::vector<Gate>& gates) {
  std::cout << "  \"gates\": [\n";
  for (std::size_t index = 0; index < gates.size(); ++index) {
    std::cout << "    {\"name\": \"" << json_escape(gates[index].name)
              << "\", \"result\": \"" << (gates[index].pass ? "PASS" : "FAIL") << "\"}";
    if (index + 1U < gates.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }
  std::cout << "  ],\n";
}

bool all_pass(const std::vector<Gate>& gates) {
  for (const auto& gate : gates) {
    if (!gate.pass) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto base = root / "local-analysis/cpp-offline";
  const auto driverkit_prepared = read_file(base / "driverkit-prepared-transport-contract.json");
  const auto driverkit_hotpath = read_file(base / "driverkit-prepared-hotpath-contract.json");
  const auto packet = read_file(base / "prepared-transport-packet-contract.json");
  const auto routing_timecode =
      read_file(base / "prepared-transport-routing-timecode-contract.json");
  const auto recovery = read_file(base / "prepared-transport-recovery-contract.json");
  const auto scheduler = read_file(base / "prepared-slot-scheduler-contract.json");
  const auto pressure = read_file(base / "prepared-transport-pressure-gate.json");
  const auto runtime = read_file(base / "driverkit-runtime-contract.json");
  const auto hot_path = read_file(base / "hot-path-timing-analysis.json");
  const auto quality_root = read_file(base / "quality-root-cause-analysis.json");
  const auto timecode = read_file(base / "timecode-readiness-gate.json");
  const auto product_compare = read_file(base / "physical-run-product-superiority.json");
  const auto promotion = read_file(base / "promotion-readiness-offline-check.json");

  const double scheduler_gap = number_or_nan(json_number(scheduler, "max_completion_gap_ratio"));
  const double scheduler_logical_gap =
      number_or_nan(json_number(scheduler, "max_safe_logical_audio_gap_ratio"));
  const double scheduler_usb_submit_reduction =
      number_or_nan(json_number(scheduler, "max_safe_usb_submit_reduction_ratio"));
  const double transport_gap =
      number_or_nan(json_number(driverkit_prepared, "max_completion_gap_ratio"));
  const double fixed_to_fill =
      number_or_nan(json_number(hot_path, "fixed_queue_to_playback_fill_ratio"));
  const double pressure_total_frames = number_or_nan(json_number(pressure, "total_frames"));
  const double pressure_rows = number_or_nan(json_number(pressure, "row_count"));
  const double runtime_pressure_total_frames =
      number_or_nan(json_number(runtime, "pressure_total_frames"));
  const double hotpath_total_frames = number_or_nan(json_number(driverkit_hotpath, "total_frames"));
  const double hotpath_max_publications =
      number_or_nan(json_number(driverkit_hotpath, "max_ring_publications_per_period"));
  const double hotpath_min_reduction =
      number_or_nan(json_number(driverkit_hotpath, "min_publication_reduction_ratio"));

  const bool prepared_contracts_pass =
      result_pass(driverkit_prepared) && result_pass(driverkit_hotpath) && result_pass(packet) &&
      result_pass(routing_timecode) && result_pass(recovery) && result_pass(scheduler) &&
      result_pass(pressure) &&
      finite(pressure_rows) && pressure_rows >= 8.0 && finite(pressure_total_frames) &&
      pressure_total_frames >= 3684000.0;
  const bool zero_hal_requeue_in_safe_contracts =
      number_is_zero(driverkit_prepared, "minimum_hal_steady_requeues_for_safe") &&
      number_is_zero(scheduler, "minimum_hal_steady_requeues_for_safe") &&
      number_is_zero(packet, "hal_steady_requeues") &&
      number_is_zero(routing_timecode, "hal_steady_requeues") &&
      number_is_zero(recovery, "hal_steady_requeues") &&
      number_is_zero(pressure, "total_hal_steady_requeues") &&
      number_is_zero(driverkit_hotpath, "hal_steady_requeues");
  const bool no_fallback_or_ring_faults =
      number_is_zero(packet, "fallback_allocations") &&
      number_is_zero(routing_timecode, "fallback_allocations") &&
      number_is_zero(recovery, "fallback_allocations") &&
      number_is_zero(pressure, "total_fallback_allocations") &&
      number_is_zero(driverkit_hotpath, "fallback_allocations") &&
      number_is_zero(driverkit_hotpath, "hal_hot_path_allocations") &&
      number_is_zero(recovery, "capture_ring_overruns") &&
      number_is_zero(recovery, "capture_ring_underruns") &&
      number_is_zero(recovery, "playback_ring_overruns") &&
      number_is_zero(recovery, "playback_ring_underruns");
  const bool timestamp_and_cadence_safe =
      number_is_zero(packet, "timestamp_regressions") &&
      number_is_zero(recovery, "timestamp_regressions") &&
      number_at_most(driverkit_prepared, "max_completion_gap_ratio", 1.25) &&
      finite(scheduler_gap) && scheduler_gap <= 1.25 &&
      finite(scheduler_logical_gap) && scheduler_logical_gap <= 1.0 &&
      number_is_zero(scheduler, "safe_logical_audio_gap_violations") &&
      number_is_zero(scheduler, "safe_slot_order_errors") &&
      number_is_zero(pressure, "failures");
  const bool usb_submit_batching_supported =
      result_pass(scheduler) && finite(scheduler_usb_submit_reduction) &&
      scheduler_usb_submit_reduction >= 8.0;
  const bool routing_and_timecode_safe =
      number_is_zero(packet, "channel_identity_failures") &&
      json_bool(packet, "product_safe").value_or(false) &&
      result_pass(routing_timecode) && number_is_zero(routing_timecode, "failures") &&
      result_pass(timecode) && json_bool(timecode, "offline_timecode_pass").value_or(false) &&
      !json_bool(timecode, "product_timecode_ready").value_or(true);
  const bool runtime_bridge_safe =
      result_pass(runtime) && !json_bool(runtime, "system_extension_activated").value_or(true) &&
      !json_bool(runtime, "driver_installed_or_activated").value_or(true) &&
      number_is_zero(runtime, "hal_steady_requeues") &&
      number_is_zero(runtime, "fallback_allocations") &&
      number_is_zero(runtime, "capture_ring_overruns") &&
      number_is_zero(runtime, "capture_ring_underruns") &&
      number_is_zero(runtime, "playback_ring_overruns") &&
      number_is_zero(runtime, "playback_ring_underruns") &&
      number_is_zero(runtime, "timestamp_regressions") &&
      number_is_zero(runtime, "channel_identity_failures") &&
      json_bool(runtime, "running_product_safe").value_or(false) &&
      number_is_zero(runtime, "pressure_failures") &&
      number_is_zero(runtime, "pressure_total_hal_steady_requeues") &&
      number_is_zero(runtime, "pressure_total_fallback_allocations") &&
      finite(runtime_pressure_total_frames) && runtime_pressure_total_frames >= 184200.0;
  const bool driverkit_prepared_hotpath_safe =
      result_pass(driverkit_hotpath) && finite(hotpath_total_frames) &&
      hotpath_total_frames >= 921000.0 && finite(hotpath_max_publications) &&
      hotpath_max_publications <= 4.0 && finite(hotpath_min_reduction) &&
      hotpath_min_reduction >= 8.0 && number_is_zero(driverkit_hotpath, "failures") &&
      number_is_zero(driverkit_hotpath, "hal_steady_requeues") &&
      number_is_zero(driverkit_hotpath, "fallback_allocations");
  const bool performance_hypothesis_supported =
      result_pass(hot_path) &&
      json_string(hot_path, "attribution").value_or("") ==
          "fixed_queue_requeue_enqueue_dominant" &&
      finite(fixed_to_fill) && fixed_to_fill >= 6.0 &&
      result_pass(quality_root) &&
      json_bool(quality_root, "fixed_transport_cpu_suspect").value_or(false);
  const bool product_promotion_blocked =
      result_fail(product_compare) &&
      !json_bool(product_compare, "branch_promotion_supported").value_or(true) &&
      !json_bool(promotion, "branch_promotion_allowed").value_or(true);
  const bool quality_claim_blocked =
      result_pass(quality_root) &&
      !json_bool(quality_root, "product_readiness_allowed").value_or(true) &&
      json_bool(quality_root, "candidate_physical_quality_fails").value_or(false);

  const std::vector<Gate> gates = {
      {"prepared_transport_contracts_pass", prepared_contracts_pass},
      {"zero_hal_requeue_in_safe_contracts", zero_hal_requeue_in_safe_contracts},
      {"no_fallback_or_ring_faults", no_fallback_or_ring_faults},
      {"timestamp_and_cadence_safe", timestamp_and_cadence_safe},
      {"logical_iso8_usb_submit_batching_supported", usb_submit_batching_supported},
      {"routing_and_timecode_safe_offline_only", routing_and_timecode_safe},
      {"driverkit_runtime_bridge_offline_safe", runtime_bridge_safe},
      {"driverkit_prepared_hotpath_batch_publication_safe", driverkit_prepared_hotpath_safe},
      {"performance_hypothesis_supported_by_hot_path_timing", performance_hypothesis_supported},
      {"product_promotion_still_blocked", product_promotion_blocked},
      {"quality_claim_still_blocked", quality_claim_blocked},
  };

  const bool migration_candidate_supported = all_pass(gates);

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.prepared-transport-migration-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (migration_candidate_supported ? "PASS" : "FAIL")
            << "\",\n"
            << "  \"meaning\": \"PASS means offline migration evidence supports preparing a controlled hardware candidate, not product readiness or superiority\",\n";
  print_bool("migration_candidate_supported", migration_candidate_supported);
  print_bool("product_ready", false);
  print_bool("branch_promotion_supported", false);
  print_bool("physical_ab_required_before_claim", true);
  print_number("driverkit_prepared_max_completion_gap_ratio", transport_gap);
  print_number("prepared_slot_scheduler_max_completion_gap_ratio", scheduler_gap);
  print_number("prepared_slot_scheduler_max_safe_logical_audio_gap_ratio",
               scheduler_logical_gap);
  print_number("prepared_slot_scheduler_max_safe_usb_submit_reduction_ratio",
               scheduler_usb_submit_reduction);
  print_number("fixed_queue_to_playback_fill_ratio", fixed_to_fill);
  print_number("prepared_transport_pressure_rows", pressure_rows);
  print_number("prepared_transport_pressure_total_frames", pressure_total_frames);
  print_number("driverkit_runtime_pressure_total_frames", runtime_pressure_total_frames);
  print_number("driverkit_prepared_hotpath_total_frames", hotpath_total_frames);
  print_number("driverkit_prepared_hotpath_max_ring_publications_per_period",
               hotpath_max_publications);
  print_number("driverkit_prepared_hotpath_min_publication_reduction_ratio",
               hotpath_min_reduction);
  print_gate_rows(gates);
  std::cout << "  \"mode\": \"offline_migration_only\",\n"
            << "  \"next_allowed_action\": \"LOCK_GATED_PREPARED_TRANSPORT_A_B_HARDWARE_WINDOW_ONLY_AFTER_RUNTIME_BINDING\",\n"
            << "  \"blocked_claim\": \"NO_CLAIM_OF_BETTER_SOUND_QUALITY_FUNCTIONALITY_TIMECODE_OR_CPU_UNTIL_SAME_SESSION_PHYSICAL_A_B_PASSES\"\n"
            << "}\n";

  return migration_candidate_supported ? 0 : 1;
}
