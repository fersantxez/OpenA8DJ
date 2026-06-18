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
  const auto makefile = read_file(root / "Makefile");
  const auto migration = read_file(evidence / "prepared-transport-migration-gate.json");
  const auto product_quality = read_file(evidence / "product-quality-claim-gate.json");
  const auto physical_window = read_file(evidence / "physical-window-readiness-gate.json");
  const auto hal_safety = read_file(evidence / "hal-candidate-safety-gate.json");

  const bool evidence_present = !hal_source.empty() && !makefile.empty() && !migration.empty() &&
                                !product_quality.empty() && !physical_window.empty() &&
                                !hal_safety.empty();

  const bool hal_has_direct_usb_enqueue =
      contains(hal_source, "enqueueIORequestWithData:transfer.data") &&
      contains(hal_source, "- (BOOL)queuePlaybackWithRequests:") &&
      contains(hal_source, "[self queueCaptureTransfer]");
  const bool hal_default_capture_paced =
      contains(makefile, "HAL_PLAYBACK_CAPTURE_PACED ?= 1") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1");
  const bool hal_has_no_runtime_prepared_submit =
      !contains(hal_source, "PreparedUsbSubmitPlanner") &&
      !contains(hal_source, "OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME");
  const bool hal_has_logical_physical_capture_split =
      contains(hal_source, "OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER") &&
      contains(hal_source,
               "kCaptureIsoFramesPerTransfer = OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(hal_source, "ExpectedIsoTransferTicksForFrames(kCaptureIsoFramesPerTransfer)");

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
      hal_has_direct_usb_enqueue && hal_default_capture_paced && hal_has_no_runtime_prepared_submit;
  const bool product_claim_blocked =
      runtime_reduction_missing && offline_prepared_model_supported && current_quality_blocked &&
      physical_ab_blocked && hal_load_is_only_safety;

  std::vector<std::string> blockers;
  if (!evidence_present) {
    blockers.push_back("required_runtime_evidence_missing");
  }
  if (runtime_reduction_missing) {
    blockers.push_back("hal_runtime_still_direct_usb_enqueue");
    blockers.push_back("hal_runtime_prepared_submit_not_integrated");
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

  const bool pass = evidence_present && product_claim_blocked;

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
      << "  \"hal_has_no_runtime_prepared_submit\": "
      << (hal_has_no_runtime_prepared_submit ? "true" : "false") << ",\n"
      << "  \"hal_has_logical_physical_capture_split\": "
      << (hal_has_logical_physical_capture_split ? "true" : "false") << ",\n"
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
         "\"IMPLEMENT_REAL_PREPARED_USB_SUBMIT_RUNTIME_OR_FULL_DRIVERKIT_USB_TRANSPORT\",\n"
      << "  \"blocked_claim\": "
         "\"NO_CPU_OR_AUDIOPHILE_SUPERIORITY_CLAIM_WHILE_HAL_RUNTIME_STILL_REQUEUES_USB_DIRECTLY\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
