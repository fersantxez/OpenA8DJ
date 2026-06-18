#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

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

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto preflight = read_file(root / "scripts/physical-window-preflight");
  const auto runner = read_file(root / "scripts/run-physical-superiority-window");

  const bool sources_present = !preflight.empty() && !runner.empty();
  const bool preflight_has_prepared_flag =
      contains(preflight, "--prepared-runtime-candidate") &&
      contains(preflight, "prepared_runtime_candidate");
  const bool preflight_hashes_candidate =
      contains(preflight, "executable_sha256") && contains(preflight, "hashlib.sha256()");
  const bool preflight_binds_to_offline_candidate =
      contains(preflight, "prepared_runtime_candidate_gate") &&
      contains(preflight, "hal-prepared-runtime-candidate.json") &&
      contains(preflight, "hal-prepared-runtime-bundle-complete.json") &&
      contains(preflight, "current-offline-gates.json") &&
      contains(preflight, "prepared_runtime_candidate_hash_matches_offline_artifact");
  const bool preflight_requires_dispatch_contract =
      contains(preflight, "prepared_runtime_dispatch_contract_pass") &&
      contains(preflight, "prepared_runtime_dispatch_path_present");
  const bool preflight_preserves_claim_block =
      contains(preflight, "prepared_runtime_transport_claim_blocked_until_physical") &&
      contains(preflight, "runtime_reduction_missing") &&
      contains(preflight, "hal_prepared_runtime_physical_evidence_present") &&
      contains(preflight, "\"physical_evidence_present\": False") &&
      contains(preflight, "\"product_claim_allowed\": False");
  const bool preflight_adds_gate =
      contains(preflight, "prepared_runtime_candidate_evidence_bound") &&
      contains(preflight, "prepared_runtime_candidate_report");
  const bool runner_exposes_prepared_flag =
      contains(runner, "--prepared-runtime-candidate") &&
      contains(runner, "prepared_runtime_candidate=0");
  const bool runner_records_manifest_identity =
      contains(runner, "prepared_runtime_candidate=$prepared_runtime_candidate") &&
      contains(runner, "candidate=${candidate:-}");
  const bool runner_forwards_prepared_flag_to_preflight =
      contains(runner, "preflight_args+=(--prepared-runtime-candidate)");

  const bool pass = sources_present && preflight_has_prepared_flag && preflight_hashes_candidate &&
                    preflight_binds_to_offline_candidate &&
                    preflight_requires_dispatch_contract && preflight_preserves_claim_block &&
                    preflight_adds_gate && runner_exposes_prepared_flag &&
                    runner_records_manifest_identity && runner_forwards_prepared_flag_to_preflight;

  std::cout << "{\n";
  std::cout << "  \"schema\": \"opena8djcpp.prepared-runtime-physical-window-contract.v1\",\n";
  std::cout << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n";
  std::cout << "  \"safety\": \"offline_source_contract_no_audio_coreaudio_usb_driver_or_hardware_touch\",\n";
  print_bool("sources_present", sources_present);
  print_bool("preflight_has_prepared_flag", preflight_has_prepared_flag);
  print_bool("preflight_hashes_candidate", preflight_hashes_candidate);
  print_bool("preflight_binds_to_offline_candidate", preflight_binds_to_offline_candidate);
  print_bool("preflight_requires_dispatch_contract", preflight_requires_dispatch_contract);
  print_bool("preflight_preserves_claim_block", preflight_preserves_claim_block);
  print_bool("preflight_adds_gate", preflight_adds_gate);
  print_bool("runner_exposes_prepared_flag", runner_exposes_prepared_flag);
  print_bool("runner_records_manifest_identity", runner_records_manifest_identity);
  print_bool("runner_forwards_prepared_flag_to_preflight", runner_forwards_prepared_flag_to_preflight);
  std::cout
      << "  \"blocked_claim\": "
      << "\"NO_PREPARED_RUNTIME_PHYSICAL_WINDOW_WITHOUT_OFFLINE_HASH_AND_DISPATCH_EVIDENCE\"\n";
  std::cout << "}\n";

  return pass ? 0 : 1;
}
