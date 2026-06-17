#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

bool contains_all(const std::string& data, const std::vector<std::string>& needles) {
  for (const auto& needle : needles) {
    if (data.find(needle) == std::string::npos) {
      return false;
    }
  }
  return true;
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

  const auto extension = root / "driverkit/extension";
  const auto info = read_file(extension / "Info.plist.template");
  const auto entitlements = read_file(extension / "OpenA8DJAudioDriver.entitlements.template");
  const auto driver_iig = read_file(extension / "OpenA8DJAudioDriver.iig");
  const auto device_iig = read_file(extension / "OpenA8DJAudioDevice.iig");
  const auto driver_cpp = read_file(extension / "src/OpenA8DJAudioDriver.cpp");
  const auto device_cpp = read_file(extension / "src/OpenA8DJAudioDevice.cpp");
  const auto readme = read_file(extension / "README.md");

  const bool files_present = !info.empty() && !entitlements.empty() && !driver_iig.empty() &&
                             !device_iig.empty() && !driver_cpp.empty() &&
                             !device_cpp.empty() && !readme.empty();
  const bool info_pass = contains_all(info, {
      "CFBundlePackageType",
      "DEXT",
      "IOKitPersonalities",
      "OpenA8DJAudioDriver",
      "IOUserAudioDriverUserClientProperties",
      "IOUserAudioDriverUserClient",
      std::string("IOUSB") + "HostDevice",
      "idVendor",
      "<integer>6092</integer>",
      "idProduct",
      "<integer>6520</integer>",
      "OpenA8DJExpectedInterface",
      "<integer>0</integer>",
  });
  const bool entitlement_pass = contains_all(entitlements, {
      "com.apple.developer.driverkit",
      "com.apple.developer.driverkit.family.audio",
      "com.apple.developer.driverkit.transport.usb",
      "<integer>6092</integer>",
      "<integer>6520</integer>",
  });
  const bool iig_pass = contains_all(driver_iig, {
      "IOUserAudioDriver",
      "Start(IOService* provider)",
      "Stop(IOService* provider)",
      "NewUserClient",
      "StartDevice",
      "StopDevice",
  }) && contains_all(device_iig, {
      "IOUserAudioDevice",
      "StartIO",
      "StopIO",
      "PerformDeviceConfigurationChange",
      "AbortDeviceConfigurationChange",
  });
  const bool runtime_binding_pass = contains_all(driver_cpp, {
      "AudioDriverSkeleton",
      "start_driver",
      "configure_stream",
      "start_stream",
      "stop_stream",
      "IOUserAudioDriver::NewUserClient",
  }) && contains_all(device_cpp, {
      "IOUserAudioDevice::StartIO",
      "IOUserAudioDevice::StopIO",
      "kIOReturnUnsupported",
  });
  const bool safety_pass = contains_all(readme, {
      "non-installing scaffold",
      "lacks the DriverKit SDK",
      "Do not install",
      "locked physical test window",
  });
  const bool default_build_excludes_extension =
      read_file(root / "CMakeLists.txt").find("driverkit/extension/src/OpenA8DJAudioDriver.cpp") ==
      std::string::npos;

  const bool pass = files_present && info_pass && entitlement_pass && iig_pass &&
                    runtime_binding_pass && safety_pass && default_build_excludes_extension;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.driverkit-extension-scaffold-contract.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"offline non-installing DriverKit dext scaffold contract; PASS is not a signed or runnable driver\",\n"
            << "  \"system_extension_activated\": false,\n"
            << "  \"driver_installed_or_activated\": false,\n"
            << "  \"driverkit_sdk_required_for_default_build\": false,\n"
            << "  \"files_present\": " << (files_present ? "true" : "false") << ",\n"
            << "  \"info_plist_pass\": " << (info_pass ? "true" : "false") << ",\n"
            << "  \"entitlements_pass\": " << (entitlement_pass ? "true" : "false") << ",\n"
            << "  \"iig_pass\": " << (iig_pass ? "true" : "false") << ",\n"
            << "  \"runtime_binding_pass\": " << (runtime_binding_pass ? "true" : "false") << ",\n"
            << "  \"safety_pass\": " << (safety_pass ? "true" : "false") << ",\n"
            << "  \"default_build_excludes_extension\": "
            << (default_build_excludes_extension ? "true" : "false") << ",\n"
            << "  \"vendor_id\": \"0x17cc\",\n"
            << "  \"product_id\": \"0x1978\"\n"
            << "}\n";

  return pass ? 0 : 1;
}
