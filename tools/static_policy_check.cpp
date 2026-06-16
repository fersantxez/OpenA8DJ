#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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
      root / "tools/dvs_signal_smoke.cpp",
      root / "tools/realtime_audit.cpp",
      root / "tools/driverkit_surface_model.cpp",
      root / "tools/evidence_schema_check.cpp",
      root / "tools/static_policy_check.cpp",
  };
  const auto join = [](const char* left, const char* right) {
    return std::string(left) + std::string(right);
  };
  const std::vector<std::string> forbidden = {
      join("su", "do"),
      join("systemextensions", "ctl"),
      join("launch", "ctl"),
      join("killall core", "audiod"),
      join("core", "audiod"),
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

  const bool path_policy = root == std::filesystem::path("/Users/fer/dev/audio8djcpp");
  const bool pass = hits.empty() && path_policy;

  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"audited_files\": " << audited_files.size() << ",\n"
            << "  \"forbidden_hits\": " << hits.size() << ",\n"
            << "  \"path_policy\": " << (path_policy ? "true" : "false") << ",\n"
            << "  \"hits\": [";
  for (std::size_t index = 0; index < hits.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << hits[index] << "\"";
  }
  std::cout << "]\n}\n";

  return pass ? 0 : 1;
}
