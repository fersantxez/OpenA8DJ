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
  const auto xcode_apps = run_command("ls -1d /Applications/Xcode*.app 2>/dev/null");
  const auto xcodes_path = run_command("command -v xcodes 2>/dev/null");
  const auto xcodes_version = run_command("xcodes version 2>&1");
  const auto xcodes_installed = run_command("xcodes installed 2>&1");
  const auto aria2_path = run_command("command -v aria2c 2>/dev/null");
  const auto applications_free_bytes = free_bytes_at_path("/Applications");
  const double applications_free_gib =
      static_cast<double>(applications_free_bytes) / kBytesPerGiB;

  const bool sdk_available = driverkit_sdk.status == 0 && contains(driverkit_sdk.output, "DriverKit");
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
  const bool product_driverkit_build_allowed = sdk_available && selected_full_xcode;
  const bool real_driverkit_claim_blocked = !product_driverkit_build_allowed;

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-sdk-preflight-gate.v1\",\n"
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
            << "  \"xcode_select_path\": \"" << json_escape(xcode_select.output) << "\",\n"
            << "  \"selected_full_xcode\": " << (selected_full_xcode ? "true" : "false") << ",\n"
            << "  \"xcode_app_present\": " << (xcode_app_present ? "true" : "false") << ",\n"
            << "  \"xcode_apps\": \"" << json_escape(xcode_apps.output) << "\",\n"
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
