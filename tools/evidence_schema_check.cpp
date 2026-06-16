#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool file_contains(const std::filesystem::path& path, const std::string& needle) {
  std::ifstream input(path);
  if (!input) {
    return false;
  }
  const std::string data((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  return data.find(needle) != std::string::npos;
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

  const std::vector<std::filesystem::path> required = {
      root / "docs/CANDIDATE_MANIFEST.json",
      root / "local-analysis/cpp-offline/current-offline-gates.json",
      root / "local-analysis/cpp-offline/ctest-default.txt",
      root / "local-analysis/cpp-offline/ctest-release.txt",
      root / "local-analysis/cpp-offline/packet-matrix.json",
      root / "local-analysis/cpp-offline/protocol-contract.json",
      root / "local-analysis/cpp-offline/simulated-output-matrix.json",
      root / "local-analysis/cpp-offline/mode2-python-oracle.txt",
      root / "local-analysis/cpp-offline/mode2-cross-oracle-parity.json",
      root / "local-analysis/cpp-offline/timecode-matrix.json",
      root / "local-analysis/cpp-offline/timecode-signal-analysis.json",
      root / "local-analysis/cpp-offline/dvs-signal-smoke.json",
      root / "local-analysis/cpp-offline/dvs-packet-input-decode.json",
      root / "local-analysis/cpp-offline/realtime-audit.json",
      root / "local-analysis/cpp-offline/driverkit-surface-model.json",
      root / "local-analysis/cpp-offline/driverkit-shell-contract.json",
      root / "local-analysis/cpp-offline/jitter-model.json",
      root / "local-analysis/cpp-offline/static-policy.json",
      root / "local-analysis/cpp-offline/offline-bench-release.json",
      root / "docs/PHYSICAL_TEST_WINDOW_PLAN.md",
      root / "docs/OFFLINE_READINESS_REPORT.md",
  };

  std::uint32_t missing = 0;
  for (const auto& path : required) {
    if (!std::filesystem::is_regular_file(path)) {
      missing += 1;
    }
  }

  const bool summary_pass = file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"status\": \"PASS\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"hardware_touched\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"coreaudio_touched\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_touched\": false");

  const bool manifest_pass =
      file_contains(root / "docs/CANDIDATE_MANIFEST.json", "\"worktree\": \"/Users/fer/dev/audio8djcpp\"") &&
      file_contains(root / "docs/CANDIDATE_MANIFEST.json", "\"scope\": \"offline_only\"");

  const bool pass = missing == 0 && summary_pass && manifest_pass;
  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"required_files\": " << required.size() << ",\n"
            << "  \"missing_files\": " << missing << ",\n"
            << "  \"summary_pass\": " << (summary_pass ? "true" : "false") << ",\n"
            << "  \"manifest_pass\": " << (manifest_pass ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
