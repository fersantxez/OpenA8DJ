#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/statvfs.h>

namespace {

struct CommandResult {
  int status = -1;
  std::string output;
};

CommandResult run_command(const char* command) {
  CommandResult result;
  std::array<char, 256> buffer{};
  FILE* pipe = popen(command, "r");
  if (pipe == nullptr) {
    return result;
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output += buffer.data();
  }
  result.status = pclose(pipe);
  while (!result.output.empty() &&
         (result.output.back() == '\n' || result.output.back() == '\r')) {
    result.output.pop_back();
  }
  return result;
}

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
  return out.str();
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

bool path_exists(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::path(path), ec);
}

std::uint32_t non_empty_line_count(const std::string& text) {
  std::uint32_t lines = 0;
  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty()) {
      ++lines;
    }
  }
  return lines;
}

std::uint64_t free_bytes_at_path(const char* path) {
  struct statvfs stats {};
  if (statvfs(path, &stats) != 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(stats.f_bavail) *
         static_cast<std::uint64_t>(stats.f_frsize);
}

}  // namespace

int main() {
  constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;
  constexpr double kMinimumXcodeInstallFreeGiB = 80.0;

  const auto xcode_select = run_command("xcode-select -p 2>&1");
  const auto driverkit_sdk = run_command("xcrun --sdk driverkit --show-sdk-path 2>&1");
  const auto xcodebuild_showsdks = run_command("xcodebuild -showsdks 2>&1");
  const auto driverkit_sdk_path =
      run_command("xcodebuild -version -sdk driverkit Path 2>&1");
  const auto driverkit_sdk_version =
      run_command("xcodebuild -version -sdk driverkit SDKVersion 2>&1");
  const auto driverkit_clang = run_command("xcrun --sdk driverkit --find clang 2>&1");
  const auto driverkit_iig = run_command("xcrun --sdk driverkit --find iig 2>&1");
  const auto codesign_path = run_command("xcrun --find codesign 2>&1");
  const auto xcode_apps = run_command("ls -1d /Applications/Xcode*.app 2>/dev/null");
  const auto xcodes_path = run_command("command -v xcodes 2>/dev/null");
  const auto xcodes_version = run_command("xcodes version 2>&1");
  const auto xcodes_installed = run_command("xcodes installed 2>&1");
  const auto aria2_path = run_command("command -v aria2c 2>/dev/null");
  const auto applications_free_bytes = free_bytes_at_path("/Applications");
  const double applications_free_gib =
      static_cast<double>(applications_free_bytes) / kBytesPerGiB;

  const bool sdk_available =
      driverkit_sdk.status == 0 && contains(driverkit_sdk.output, "DriverKit");
  const bool xcodebuild_driverkit_sdk_visible =
      xcodebuild_showsdks.status == 0 && contains(xcodebuild_showsdks.output, "DriverKit");
  const bool driverkit_sdk_path_available =
      driverkit_sdk_path.status == 0 && contains(driverkit_sdk_path.output, "DriverKit");
  const bool driverkit_sdk_path_exists = path_exists(driverkit_sdk_path.output);
  const bool driverkit_sdk_version_available = driverkit_sdk_version.status == 0 &&
                                               !driverkit_sdk_version.output.empty() &&
                                               !contains(driverkit_sdk_version.output, "error:");
  const bool clang_available = driverkit_clang.status == 0 && path_exists(driverkit_clang.output);
  const bool iig_available = driverkit_iig.status == 0 && path_exists(driverkit_iig.output);
  const bool codesign_available = codesign_path.status == 0 && path_exists(codesign_path.output);
  const bool selected_full_xcode = contains(xcode_select.output, "Xcode") &&
                                   contains(xcode_select.output, ".app/Contents/Developer");
  const bool xcode_app_present = !xcode_apps.output.empty();
  const bool xcodes_cli_present = !xcodes_path.output.empty();
  const bool xcodes_cli_usable = xcodes_version.status == 0 && contains(xcodes_version.output, "2.");
  const bool aria2_present = !aria2_path.output.empty();
  const bool xcode_install_disk_space_ok =
      applications_free_gib >= kMinimumXcodeInstallFreeGiB;
  const auto installed_count = non_empty_line_count(xcodes_installed.output);
  const bool noninteractive_xcode_install_prerequisites_met =
      xcodes_cli_present && xcodes_cli_usable && aria2_present && xcode_install_disk_space_ok;
  const bool build_only_probe_allowed =
      sdk_available && selected_full_xcode && xcodebuild_driverkit_sdk_visible &&
      driverkit_sdk_path_available && driverkit_sdk_path_exists &&
      driverkit_sdk_version_available && clang_available && iig_available &&
      codesign_available;
  const bool product_driverkit_build_allowed = build_only_probe_allowed;
  const bool real_driverkit_claim_blocked = !product_driverkit_build_allowed;

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-sdk-preflight-gate.v2\",\n"
            << "  \"result\": \"PASS\",\n"
            << "  \"meaning\": \"offline developer-tool preflight; PASS means the environment state is measured, not that DriverKit can build\",\n"
            << "  \"safety\": \"no_hardware_no_audio_no_coreaudio_no_usb_no_driver_install_no_system_extension_activation\",\n"
            << "  \"hardware_touched\": false,\n"
            << "  \"coreaudio_touched\": false,\n"
            << "  \"usb_touched\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"sdk_required_for_real_driverkit\": true,\n"
            << "  \"xcrun_driverkit_sdk_available\": " << (sdk_available ? "true" : "false") << ",\n"
            << "  \"xcrun_driverkit_sdk_output\": \"" << json_escape(driverkit_sdk.output) << "\",\n"
            << "  \"driverkit_sdk_path\": \"" << json_escape(driverkit_sdk_path.output) << "\",\n"
            << "  \"driverkit_sdk_path_available\": "
            << (driverkit_sdk_path_available ? "true" : "false") << ",\n"
            << "  \"driverkit_sdk_path_exists\": "
            << (driverkit_sdk_path_exists ? "true" : "false") << ",\n"
            << "  \"driverkit_sdk_version\": \""
            << json_escape(driverkit_sdk_version.output) << "\",\n"
            << "  \"driverkit_sdk_version_available\": "
            << (driverkit_sdk_version_available ? "true" : "false") << ",\n"
            << "  \"xcode_select_path\": \"" << json_escape(xcode_select.output) << "\",\n"
            << "  \"developer_dir_effective\": \"" << json_escape(xcode_select.output) << "\",\n"
            << "  \"selected_full_xcode\": " << (selected_full_xcode ? "true" : "false") << ",\n"
            << "  \"xcodebuild_driverkit_sdk_visible\": "
            << (xcodebuild_driverkit_sdk_visible ? "true" : "false") << ",\n"
            << "  \"xcodebuild_showsdks_output\": \""
            << json_escape(xcodebuild_showsdks.output) << "\",\n"
            << "  \"xcode_app_present\": " << (xcode_app_present ? "true" : "false") << ",\n"
            << "  \"xcode_apps\": \"" << json_escape(xcode_apps.output) << "\",\n"
            << "  \"driverkit_clang_path\": \"" << json_escape(driverkit_clang.output) << "\",\n"
            << "  \"clang_available\": " << (clang_available ? "true" : "false") << ",\n"
            << "  \"driverkit_iig_path\": \"" << json_escape(driverkit_iig.output) << "\",\n"
            << "  \"iig_available\": " << (iig_available ? "true" : "false") << ",\n"
            << "  \"codesign_path\": \"" << json_escape(codesign_path.output) << "\",\n"
            << "  \"codesign_available\": " << (codesign_available ? "true" : "false") << ",\n"
            << "  \"xcodes_cli_present\": " << (xcodes_cli_present ? "true" : "false") << ",\n"
            << "  \"xcodes_cli_usable\": " << (xcodes_cli_usable ? "true" : "false") << ",\n"
            << "  \"xcodes_version\": \"" << json_escape(xcodes_version.output) << "\",\n"
            << "  \"xcodes_installed_count\": " << installed_count << ",\n"
            << "  \"aria2_present\": " << (aria2_present ? "true" : "false") << ",\n"
            << "  \"applications_free_bytes\": " << applications_free_bytes << ",\n"
            << "  \"applications_free_gib\": " << applications_free_gib << ",\n"
            << "  \"xcode_install_minimum_free_gib\": " << kMinimumXcodeInstallFreeGiB << ",\n"
            << "  \"xcode_install_disk_space_ok\": "
            << (xcode_install_disk_space_ok ? "true" : "false") << ",\n"
            << "  \"recommended_xcode_version_for_current_host\": \"26.5 (17F42) [Apple Silicon]\",\n"
            << "  \"noninteractive_install_tooling_present\": "
            << (xcodes_cli_present ? "true" : "false") << ",\n"
            << "  \"fast_download_helper_present\": " << (aria2_present ? "true" : "false") << ",\n"
            << "  \"noninteractive_xcode_install_prerequisites_met\": "
            << (noninteractive_xcode_install_prerequisites_met ? "true" : "false") << ",\n"
            << "  \"build_only_probe_allowed\": "
            << (build_only_probe_allowed ? "true" : "false") << ",\n"
            << "  \"product_driverkit_build_allowed\": "
            << (product_driverkit_build_allowed ? "true" : "false") << ",\n"
            << "  \"real_driverkit_claim_blocked\": "
            << (real_driverkit_claim_blocked ? "true" : "false") << ",\n"
            << "  \"blocked_claim\": \""
            << (real_driverkit_claim_blocked
                    ? "NO_REAL_DRIVERKIT_DEXT_BUILD_OR_READINESS_CLAIM_WITHOUT_DRIVERKIT_SDK_AND_SELECTED_FULL_XCODE"
                    : "")
            << "\",\n"
            << "  \"next_required_action\": \""
            << (product_driverkit_build_allowed
                    ? "Build DriverKit target into build directory only; do not install or activate without lock-gated window"
                    : (noninteractive_xcode_install_prerequisites_met
                           ? "Install and select full Xcode with DriverKit SDK, then rerun this gate before any real dext build claim"
                           : "Free enough disk for full Xcode installation, then install/select Xcode with DriverKit SDK and rerun this gate"))
            << "\"\n"
            << "}\n";

  return 0;
}
