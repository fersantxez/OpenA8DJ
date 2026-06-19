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

std::string between(std::string_view text, std::string_view start, std::string_view end) {
  const auto start_pos = text.find(start);
  if (start_pos == std::string_view::npos) {
    return {};
  }
  const auto body_start = start_pos + start.size();
  const auto end_pos = text.find(end, body_start);
  if (end_pos == std::string_view::npos) {
    return {};
  }
  return std::string(text.substr(body_start, end_pos - body_start));
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

  const auto append_body = between(hal_source,
                                   "- (BOOL)appendInputByte:(uint8_t)byte",
                                   "- (uint32_t)decodeCaptureBytes:");
  const auto decode_body = between(hal_source,
                                   "- (uint32_t)decodeCaptureBytes:",
                                   "- (void)refillOutputPrefetch");

  std::vector<std::string> failures;
  if (hal_source.empty()) failures.push_back("hal_source_missing");
  if (append_body.empty()) failures.push_back("append_input_byte_body_missing");
  if (decode_body.empty()) failures.push_back("decode_capture_bytes_body_missing");

  const bool config_type_present =
      contains(hal_source, "typedef struct OpenA8DJInputTransformConfig") &&
      contains(hal_source, "uint8_t swapMask;") &&
      contains(hal_source, "uint8_t invertLeftMask;") &&
      contains(hal_source, "uint8_t invertRightMask;") &&
      contains(hal_source, "uint32_t sourceMap;");
  if (!config_type_present) failures.push_back("input_transform_config_missing");

  const bool append_accepts_config =
      contains(append_body, "config:(const OpenA8DJInputTransformConfig *)config");
  if (!append_accepts_config) failures.push_back("append_input_byte_config_arg_missing");

  const bool append_uses_config =
      contains(append_body, "config->swapMask") &&
      contains(append_body, "config->invertLeftMask") &&
      contains(append_body, "config->invertRightMask") &&
      contains(append_body, "config->sourceMap");
  if (!append_uses_config) failures.push_back("append_input_byte_not_using_config");

  const bool append_has_no_atomic_loads = !contains(append_body, "atomic_load(") &&
                                          !contains(append_body, "atomic_load_explicit(");
  if (!append_has_no_atomic_loads) failures.push_back("append_input_byte_still_loads_atomics");

  const bool decode_snapshots_once =
      contains(decode_body, "const OpenA8DJInputTransformConfig inputConfig =") &&
      contains(decode_body, "atomic_load(&_inputSwapMask)") &&
      contains(decode_body, "atomic_load(&_inputInvertLeftMask)") &&
      contains(decode_body, "atomic_load(&_inputInvertRightMask)") &&
      contains(decode_body, "atomic_load(&_inputSourceMap)");
  if (!decode_snapshots_once) failures.push_back("decode_capture_snapshot_missing");

  const auto inactive_fast_path_pos = decode_body.find("if (!atomic_load(&_inputDecodeActive))");
  const auto snapshot_pos = decode_body.find("const OpenA8DJInputTransformConfig inputConfig =");
  const auto stats_memset_pos = decode_body.find("memset(&inputStatsDelta, 0, sizeof(inputStatsDelta))");
  const bool snapshot_after_inactive_fast_path =
      inactive_fast_path_pos != std::string::npos &&
      snapshot_pos != std::string::npos &&
      inactive_fast_path_pos < snapshot_pos;
  if (!snapshot_after_inactive_fast_path) {
    failures.push_back("decode_capture_snapshot_before_inactive_fast_path");
  }

  const bool stats_clear_after_inactive_fast_path =
      inactive_fast_path_pos != std::string::npos &&
      stats_memset_pos != std::string::npos &&
      inactive_fast_path_pos < stats_memset_pos;
  if (!stats_clear_after_inactive_fast_path) {
    failures.push_back("decode_capture_stats_clear_before_inactive_fast_path");
  }

  const bool decode_passes_config =
      contains(decode_body, "config:&inputConfig") &&
      contains(decode_body, "inputStats:&inputStatsDelta");
  if (!decode_passes_config) failures.push_back("decode_capture_does_not_pass_config");

  const bool pass = failures.empty();
  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.hal-input-decode-snapshot-contract.v1\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means input transform atomics are snapshotted once per capture chunk instead of per decoded sample\",\n"
      << "  \"config_type_present\": " << (config_type_present ? "true" : "false") << ",\n"
      << "  \"append_accepts_config\": " << (append_accepts_config ? "true" : "false") << ",\n"
      << "  \"append_uses_config\": " << (append_uses_config ? "true" : "false") << ",\n"
      << "  \"append_has_no_atomic_loads\": " << (append_has_no_atomic_loads ? "true" : "false") << ",\n"
      << "  \"decode_snapshots_once\": " << (decode_snapshots_once ? "true" : "false") << ",\n"
      << "  \"snapshot_after_inactive_fast_path\": " << (snapshot_after_inactive_fast_path ? "true" : "false") << ",\n"
      << "  \"stats_clear_after_inactive_fast_path\": " << (stats_clear_after_inactive_fast_path ? "true" : "false") << ",\n"
      << "  \"decode_passes_config\": " << (decode_passes_config ? "true" : "false") << ",\n"
      << "  \"audio_bytes_changed\": false,\n"
      << "  \"physical_quality_claim_allowed\": false,\n";
  print_string_array("failures", failures);
  std::cout << "\n}\n";

  return pass ? 0 : 1;
}
