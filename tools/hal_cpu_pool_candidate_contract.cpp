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
  return root.empty() ? std::filesystem::current_path() : root;
}

bool contains(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

void print_bool(const char* name, bool value) {
  std::cout << "  \"" << name << "\": " << (value ? "true" : "false") << ",\n";
}

void print_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << values[index] << "\"";
  }
  std::cout << "]\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto makefile = read_file(root / "Makefile");
  const auto hal_source = read_file(root / "src/hal/OpenA8DJUSB.m");
  const auto script = read_file(root / "scripts/build-hal-cpu-pool-candidate");
  const auto investigation =
      read_file(root / "docs-state/investigations/cpu-optimization-2026-06-19.md");

  const bool sources_present =
      !makefile.empty() && !hal_source.empty() && !script.empty() && !investigation.empty();
  const bool make_target_present =
      contains(makefile, "hal-cpu-pool-candidate:") &&
      contains(makefile, "scripts/build-hal-cpu-pool-candidate") &&
      contains(makefile, "build/OpenA8DJ-cpu-pool.driver") &&
      contains(makefile, "build/hal-candidates/cpu-pool-candidate.json");
  const bool stable_default_flags_promoted =
      contains(makefile, "HAL_TRANSFER_POOL_CURSOR ?= 1") &&
      contains(makefile, "HAL_FAST_ISO_TRANSFER_CONFIG ?= 1") &&
      contains(makefile, "HAL_REUSE_ISOC_COMPLETIONS ?= 0") &&
      contains(makefile, "HAL_RAW_ISOC_COMPLETIONS ?= 0");
  const bool completion_aliases_wired =
      contains(makefile,
               "-DOPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS=$(HAL_REUSE_ISOC_COMPLETIONS)") &&
      contains(makefile,
               "-DOPENA8DJ_RAW_ISOC_COMPLETION_HANDLERS=$(HAL_RAW_ISOC_COMPLETIONS)");
  const bool layout_signature_fast_path =
      contains(hal_source, "@property(nonatomic) uint64_t layoutSignature;") &&
      contains(hal_source, "static uint64_t IsoTransferLayoutSignature") &&
      contains(hal_source, "transfer.layoutSignature == layoutSignature") &&
      contains(hal_source, "transfer.layoutSignature = layoutSignature") &&
      contains(hal_source, "OPENA8DJ_FAST_ISO_TRANSFER_CONFIG");
  const bool script_builds_candidate =
      contains(script, "\"HAL_TRANSFER_POOL_CURSOR=1\"") &&
      contains(script, "\"HAL_FAST_ISO_TRANSFER_CONFIG=1\"") &&
      contains(script, "\"HAL_REUSE_ISOC_COMPLETIONS=0\"") &&
      contains(script, "\"HAL_RAW_ISOC_COMPLETIONS=0\"");
  const bool script_restores_and_tests =
      contains(script, "[\"make\", \"build/hal-smoke\", \"build/hal-parity-smoke\"]") &&
      contains(script, "./build/hal-smoke build/OpenA8DJ.driver") &&
      contains(script, "./build/hal-parity-smoke build/OpenA8DJ.driver") &&
      contains(script, "[\"make\", \"-B\", \"hal\"]") &&
      contains(script, "default_hal_restored") &&
      contains(script, "cpu_pool_hash_matches_stable_default");
  const bool claims_blocked =
      contains(script, "physical_evidence_present") &&
      contains(script, "product_claim_allowed") &&
      contains(script, "CPU_POOL_FREEZE_REQUIRES_EXACT_ARTIFACT_SOUND_VALIDATION") &&
      contains(script, "physical_evidence_present\": False") &&
      contains(script, "product_claim_allowed\": False") &&
      contains(investigation, "CPU pool is now the frozen 0.5.0 stable build profile") ||
      contains(investigation, "0.5.1 responsive output3072 profile");

  std::vector<std::string> blockers;
  if (!sources_present) {
    blockers.push_back("required_sources_missing");
  }
  if (!make_target_present) {
    blockers.push_back("make_target_missing");
  }
  if (!stable_default_flags_promoted) {
    blockers.push_back("stable_default_flags_not_promoted");
  }
  if (!completion_aliases_wired) {
    blockers.push_back("completion_handler_alias_flags_not_wired");
  }
  if (!layout_signature_fast_path) {
    blockers.push_back("layout_signature_fast_path_missing");
  }
  if (!script_builds_candidate) {
    blockers.push_back("cpu_pool_script_flags_missing");
  }
  if (!script_restores_and_tests) {
    blockers.push_back("cpu_pool_script_restore_or_test_missing");
  }
  if (!claims_blocked) {
    blockers.push_back("cpu_pool_claim_block_missing");
  }

  const bool pass = blockers.empty();
  std::cout << "{\n";
  std::cout << "  \"schema\": \"opena8djcpp.hal-cpu-pool-candidate-contract.v1\",\n";
  std::cout << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  std::cout << "  \"safety\": \"source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n";
  print_bool("sources_present", sources_present);
  print_bool("make_target_present", make_target_present);
  print_bool("stable_default_flags_promoted", stable_default_flags_promoted);
  print_bool("completion_aliases_wired", completion_aliases_wired);
  print_bool("layout_signature_fast_path", layout_signature_fast_path);
  print_bool("script_builds_candidate", script_builds_candidate);
  print_bool("script_restores_and_tests", script_restores_and_tests);
  print_bool("claims_blocked", claims_blocked);
  print_array("blockers", blockers);
  std::cout << "}\n";
  return pass ? 0 : 1;
}
