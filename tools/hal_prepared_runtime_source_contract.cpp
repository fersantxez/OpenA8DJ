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

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
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
  const auto makefile = read_file(root / "Makefile");

  const bool source_present = !hal_source.empty();
  const bool makefile_present = !makefile.empty();
  const bool default_off =
      contains(makefile, "HAL_PREPARED_USB_SUBMIT_RUNTIME ?= 0") &&
      contains(makefile, "HAL_PREPARED_USB_PLAYBACK_ONLY_RUNTIME ?= 0") &&
      contains(makefile, "HAL_PREPARED_USB_SLOTS_PER_SUBMIT ?= 8");
  const bool cflags_expose_runtime =
      contains(makefile,
               "-DOPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME=$(HAL_PREPARED_USB_SUBMIT_RUNTIME)") &&
      contains(makefile,
               "-DOPENA8DJ_HAL_PREPARED_USB_PLAYBACK_ONLY_RUNTIME=$(HAL_PREPARED_USB_PLAYBACK_ONLY_RUNTIME)") &&
      contains(makefile,
               "-DOPENA8DJ_HAL_PREPARED_USB_SLOTS_PER_SUBMIT=$(HAL_PREPARED_USB_SLOTS_PER_SUBMIT)");
  const bool opt_in_target_present =
      contains(makefile, "hal-prepared-runtime:") &&
      contains(makefile, "HAL_PREPARED_USB_SUBMIT_RUNTIME=1") &&
      contains(makefile, "HAL_PREPARED_USB_SLOTS_PER_SUBMIT=8") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES=64") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS=8");
  const bool playback_scheduler_candidate_present =
      contains(makefile, "hal-playback-scheduler-candidate:") &&
      contains(makefile, "--candidate build/OpenA8DJ-playback-scheduler.driver") &&
      contains(makefile, "--json-out build/hal-candidates/playback-scheduler-candidate.json") &&
      contains(makefile, "--playback-only");
  const bool opt_in_target_build_only =
      contains(makefile, "hal-prepared-runtime:\n\t$(MAKE) -B hal") &&
      !contains(makefile, "hal-prepared-runtime:\n\t$(MAKE) -B install-hal");
  const bool source_declares_runtime_macros =
      contains(hal_source, "#ifndef OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME") &&
      contains(hal_source, "#ifndef OPENA8DJ_HAL_PREPARED_USB_PLAYBACK_ONLY_RUNTIME") &&
      contains(hal_source, "#ifndef OPENA8DJ_HAL_PREPARED_USB_SLOTS_PER_SUBMIT");
  const bool source_has_compile_time_geometry_guards =
      contains(hal_source, "#if OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME") &&
      contains(hal_source,
               "OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER != \\\n"
               "    (OPENA8DJ_ISO_FRAMES_PER_TRANSFER * "
               "OPENA8DJ_HAL_PREPARED_USB_SLOTS_PER_SUBMIT)") &&
      contains(hal_source,
               "OPENA8DJ_HAL_PREPARED_USB_PLAYBACK_ONLY_RUNTIME && \\\n"
               "    OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER != "
               "OPENA8DJ_ISO_FRAMES_PER_TRANSFER") &&
      contains(hal_source,
               "(OPENA8DJ_PLAYBACK_ISO_FRAMES_PER_TRANSFER * "
               "OPENA8DJ_PLAYBACK_COALESCE_TRANSFERS) != \\\n"
               "    (OPENA8DJ_ISO_FRAMES_PER_TRANSFER * "
               "OPENA8DJ_HAL_PREPARED_USB_SLOTS_PER_SUBMIT)");
  const bool source_exposes_runtime_geometry_constants =
      contains(hal_source, "kPreparedUsbSlotsPerSubmit") &&
      contains(hal_source, "kPreparedRuntimeEnabled") &&
      contains(hal_source, "kPreparedRuntimeCaptureGeometry") &&
      contains(hal_source, "kPreparedRuntimePlaybackGeometry") &&
      contains(hal_source, "kPreparedRuntimeGeometrySupported") &&
      contains(hal_source, "kPreparedRuntimeCaptureEnabled") &&
      contains(hal_source, "kPreparedRuntimePlaybackEnabled");
  const bool default_geometry_preserved =
      appears_before(makefile, "HAL_ISO_FRAMES ?= 8", "HAL_CAPTURE_ISO_FRAMES ?= $(HAL_ISO_FRAMES)") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS ?= 1");
  const bool runtime_claim_still_blocked = true;

  std::vector<std::string> blockers;
  if (!source_present || !makefile_present) blockers.push_back("required_sources_missing");
  if (!default_off) blockers.push_back("prepared_runtime_not_default_off");
  if (!cflags_expose_runtime) blockers.push_back("prepared_runtime_cflags_missing");
  if (!opt_in_target_present) blockers.push_back("prepared_runtime_target_missing");
  if (!playback_scheduler_candidate_present) {
    blockers.push_back("playback_scheduler_candidate_target_missing");
  }
  if (!opt_in_target_build_only) blockers.push_back("prepared_runtime_target_not_build_only");
  if (!source_declares_runtime_macros) blockers.push_back("prepared_runtime_macros_missing");
  if (!source_has_compile_time_geometry_guards) {
    blockers.push_back("prepared_runtime_compile_time_geometry_guards_missing");
  }
  if (!source_exposes_runtime_geometry_constants) {
    blockers.push_back("prepared_runtime_geometry_constants_missing");
  }
  if (!default_geometry_preserved) blockers.push_back("default_hal_geometry_drifted");

  const bool pass = blockers.empty();

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-prepared-runtime-source-contract.v1\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"meaning\": \"PASS means a default-off prepared HAL runtime build profile is source-guarded; it is not runtime, hardware, CPU, or product readiness\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  print_bool("source_present", source_present);
  print_bool("makefile_present", makefile_present);
  print_bool("prepared_runtime_default_off", default_off);
  print_bool("prepared_runtime_cflags_exposed", cflags_expose_runtime);
  print_bool("prepared_runtime_opt_in_target_present", opt_in_target_present);
  print_bool("playback_scheduler_candidate_target_present", playback_scheduler_candidate_present);
  print_bool("prepared_runtime_opt_in_target_build_only", opt_in_target_build_only);
  print_bool("source_declares_runtime_macros", source_declares_runtime_macros);
  print_bool("source_has_compile_time_geometry_guards", source_has_compile_time_geometry_guards);
  print_bool("source_exposes_runtime_geometry_constants", source_exposes_runtime_geometry_constants);
  print_bool("default_geometry_preserved", default_geometry_preserved);
  print_bool("runtime_claim_still_blocked", runtime_claim_still_blocked);
  print_string_array("blockers", blockers);
  std::cout
      << "  \"opt_in_build_command\": \"make -B hal-prepared-runtime\",\n"
      << "  \"next_required_action\": \"COMPILE_OPT_IN_HAL_PREPARED_RUNTIME_THEN_LOCK_GATED_ROUTE_REVALIDATION_BEFORE_ANY_PHYSICAL_AB\",\n"
      << "  \"blocked_claim\": \"NO_CPU_OR_PRODUCT_CLAIM_FROM_SOURCE_GUARD_OR_LOCAL_BUILD_WITHOUT_LOCK_GATED_SAME_SESSION_PHYSICAL_EVIDENCE\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
