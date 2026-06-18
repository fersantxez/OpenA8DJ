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

  std::vector<std::string> failures;
  if (hal_source.empty()) failures.push_back("hal_source_missing");
  if (makefile.empty()) failures.push_back("makefile_missing");

  const bool makefile_exposes_flag =
      contains(makefile, "HAL_INPUT_SPSC_RING ?= 0") &&
      contains(makefile, "-DOPENA8DJ_INPUT_SPSC_RING=$(HAL_INPUT_SPSC_RING)");
  if (!makefile_exposes_flag) failures.push_back("makefile_input_spsc_flag_missing");

  const bool makefile_exposes_diagnostic_target =
      contains(makefile, "hal-input-spsc-diagnostic:") &&
      contains(makefile, "HAL_INPUT_SPSC_RING=1") &&
      contains(makefile, "HAL_ISO_FRAMES=8") &&
      contains(makefile, "HAL_CAPTURE_ISO_FRAMES=8") &&
      contains(makefile, "HAL_PLAYBACK_ISO_FRAMES=8") &&
      contains(makefile, "HAL_PLAYBACK_COALESCE_TRANSFERS=1") &&
      contains(makefile, "HAL_OUTPUT_STREAMS=1") &&
      contains(makefile, "HAL_STREAM_USAGE=0");
  if (!makefile_exposes_diagnostic_target) failures.push_back("input_spsc_diagnostic_target_missing");

  const bool source_default_off =
      contains(hal_source, "#ifndef OPENA8DJ_INPUT_SPSC_RING") &&
      contains(hal_source, "#define OPENA8DJ_INPUT_SPSC_RING 0");
  if (!source_default_off) failures.push_back("source_default_off_missing");

  const bool float_ring_has_atomic_cursors =
      contains(hal_source, "atomic_uint_fast64_t readCounter;") &&
      contains(hal_source, "atomic_uint_fast64_t writeCounter;");
  if (!float_ring_has_atomic_cursors) failures.push_back("input_ring_atomic_cursors_missing");

  const bool ring_write_has_spsc_branch =
      contains(hal_source, "#if OPENA8DJ_INPUT_SPSC_RING") &&
      contains(hal_source, "atomic_load_explicit(&ring->readCounter, memory_order_acquire)") &&
      contains(hal_source, "atomic_store_explicit(&ring->writeCounter, write, memory_order_release)");
  if (!ring_write_has_spsc_branch) failures.push_back("ring_write_spsc_branch_missing");

  const bool ring_read_has_spsc_branch =
      contains(hal_source, "atomic_load_explicit(&ring->writeCounter, memory_order_acquire)") &&
      contains(hal_source, "atomic_store_explicit(&ring->readCounter, readCounter, memory_order_release)") &&
      contains(hal_source, "memset(&frames[(size_t)read * ring->channels]");
  if (!ring_read_has_spsc_branch) failures.push_back("ring_read_spsc_branch_missing");

  const bool legacy_mutex_path_preserved =
      contains(hal_source, "pthread_mutex_lock(&ring->mutex);") &&
      contains(hal_source, "pthread_mutex_unlock(&ring->mutex);") &&
      contains(makefile, "HAL_INPUT_SPSC_RING ?= 0");
  if (!legacy_mutex_path_preserved) failures.push_back("legacy_mutex_default_path_not_preserved");

  const bool output_timeline_not_replaced =
      contains(hal_source, "typedef struct OutputTimelineRing") &&
      contains(hal_source, "static uint32_t OutputTimelineWrite") &&
      contains(hal_source, "static uint32_t OutputTimelineReadFrames") &&
      contains(hal_source, "pthread_mutex_lock(&ring->mutex);");
  if (!output_timeline_not_replaced) failures.push_back("output_timeline_changed_or_missing");

  const bool pass = failures.empty();
  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-input-spsc-ring-contract.v1\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means an opt-in input-ring SPSC diagnostic exists without changing default HAL, USB cadence, output timeline, or product readiness claims\",\n"
      << "  \"makefile_exposes_flag\": " << (makefile_exposes_flag ? "true" : "false") << ",\n"
      << "  \"makefile_exposes_diagnostic_target\": "
      << (makefile_exposes_diagnostic_target ? "true" : "false") << ",\n"
      << "  \"source_default_off\": " << (source_default_off ? "true" : "false") << ",\n"
      << "  \"float_ring_has_atomic_cursors\": "
      << (float_ring_has_atomic_cursors ? "true" : "false") << ",\n"
      << "  \"ring_write_has_spsc_branch\": "
      << (ring_write_has_spsc_branch ? "true" : "false") << ",\n"
      << "  \"ring_read_has_spsc_branch\": "
      << (ring_read_has_spsc_branch ? "true" : "false") << ",\n"
      << "  \"legacy_mutex_path_preserved\": "
      << (legacy_mutex_path_preserved ? "true" : "false") << ",\n"
      << "  \"output_timeline_not_replaced\": "
      << (output_timeline_not_replaced ? "true" : "false") << ",\n"
      << "  \"input_spsc_default_active\": false,\n"
      << "  \"input_spsc_overflow_policy\": \"drop_new_when_full_no_unread_overwrite\",\n"
      << "  \"input_spsc_physical_status\": \"UNTESTED\",\n"
      << "  \"input_spsc_product_candidate_allowed\": false,\n";
  print_string_array("failures", failures);
  std::cout
      << ",\n"
      << "  \"blocked_claim\": "
         "\"NO_INPUT_SPSC_CPU_OR_TIMECODE_CLAIM_UNTIL_LOCK_GATED_PHYSICAL_INPUT_AND_DVS_TESTS_PASS\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
