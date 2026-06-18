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

void print_bool(const char* key, bool value) {
  std::cout << "  \"" << key << "\": " << (value ? "true" : "false") << ",\n";
}

void print_blockers(const std::vector<std::string>& blockers) {
  std::cout << "  \"blockers\": [";
  for (std::size_t index = 0; index < blockers.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << blockers[index] << "\"";
  }
  std::cout << "],\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto extension = root / "driverkit/extension";
  const auto device_cpp = read_file(extension / "src/OpenA8DJAudioDevice.cpp");
  const auto driver_cpp = read_file(extension / "src/OpenA8DJAudioDriver.cpp");
  const auto device_iig = read_file(extension / "OpenA8DJAudioDevice.iig");
  const auto driver_iig = read_file(extension / "OpenA8DJAudioDriver.iig");
  const auto skeleton_hpp =
      read_file(root / "driverkit/include/opena8djcpp/driverkit/audio_driver_skeleton.hpp");
  const auto skeleton_cpp = read_file(root / "driverkit/src/audio_driver_skeleton.cpp");
  const auto prepared_transport_hpp =
      read_file(root / "core/include/opena8djcpp/prepared_transport.hpp");
  const auto prepared_transport_cpp = read_file(root / "core/src/prepared_transport.cpp");
  const auto usb_submit_plan_hpp =
      read_file(root / "core/include/opena8djcpp/usb_submit_plan.hpp");
  const auto usb_request_pool_hpp =
      read_file(root / "core/include/opena8djcpp/usb_request_pool.hpp");

  const bool device_source_present = !device_cpp.empty();
  const bool driver_source_present = !driver_cpp.empty();
  const bool skeleton_source_present = !skeleton_hpp.empty() && !skeleton_cpp.empty();
  const bool iig_present = !device_iig.empty() && !driver_iig.empty();
  const bool device_declares_runtime_hooks =
      contains(device_iig, "StartIO") && contains(device_iig, "StopIO") &&
      contains(device_iig, "PerformDeviceConfigurationChange") &&
      contains(device_iig, "AbortDeviceConfigurationChange");
  const bool device_start_io_passthrough =
      contains(device_cpp,
               "kern_return_t OpenA8DJAudioDevice::StartIO(IOUserAudioStartStopFlags flags)") &&
      contains(device_cpp, "return IOUserAudioDevice::StartIO(flags);");
  const bool device_stop_io_passthrough =
      contains(device_cpp,
               "kern_return_t OpenA8DJAudioDevice::StopIO(IOUserAudioStartStopFlags flags)") &&
      contains(device_cpp, "return IOUserAudioDevice::StopIO(flags);");
  const bool device_configuration_change_unsupported =
      contains(device_cpp, "OpenA8DJAudioDevice::PerformDeviceConfigurationChange") &&
      contains(device_cpp, "return kIOReturnUnsupported;");
  const bool device_abort_configuration_change_stub =
      contains(device_cpp, "OpenA8DJAudioDevice::AbortDeviceConfigurationChange") &&
      contains(device_cpp, "return kIOReturnSuccess;");
  const bool stream_memory_binding_missing =
      !contains(device_cpp, "SetIOMemoryDescriptor") &&
      !contains(device_cpp, "CreateIOMemoryDescriptor") && !contains(device_cpp, "AddStream");
  const bool zero_timestamp_binding_missing =
      !contains(device_cpp, "UpdateCurrentZeroTimestamp(") &&
      !contains(device_cpp, "GetCurrentZeroTimestamp(");
  const bool driver_start_device_uses_default_config =
      contains(driver_cpp, "const opena8djcpp::driverkit::AudioStreamConfig config{};");
  const bool source_binding_present =
      contains(driver_cpp, "AudioDeviceRuntimeBinding") &&
      contains(driver_cpp, "g_device_binding");
  const bool driver_start_device_configures_binding =
      contains(driver_cpp, "OpenA8DJAudioDriver::StartDevice") &&
      contains(driver_cpp, "extension_bridge::ConfigureDevice()");
  const bool driver_stop_device_bridged =
      contains(driver_cpp, "OpenA8DJAudioDriver::StopDevice") &&
      contains(driver_cpp, "extension_bridge::StopIO(flags)");
  const bool device_start_io_bridged =
      contains(device_cpp, "OpenA8DJAudioDevice::StartIO") &&
      contains(device_cpp, "extension_bridge::StartIO(flags)");
  const bool device_stop_io_bridged =
      contains(device_cpp, "OpenA8DJAudioDevice::StopIO") &&
      contains(device_cpp, "extension_bridge::StopIO(flags)");
  const bool configuration_change_bridged =
      contains(device_cpp, "extension_bridge::PerformDeviceConfigurationChange(action") &&
      contains(driver_cpp, "g_device_binding.request_configuration_change");
  const bool configuration_abort_bridged =
      contains(device_cpp, "extension_bridge::AbortDeviceConfigurationChange(action") &&
      contains(driver_cpp, "g_device_binding.abort_configuration_change()");
  const bool source_stream_memory_model_present =
      contains(driver_cpp, "stream_memory_bound()") &&
      contains(driver_cpp, "g_device_binding.configure_device");
  const bool source_zero_timestamp_model_present =
      contains(driver_cpp, "g_next_placeholder_sample_time") &&
      contains(driver_cpp, "g_next_placeholder_host_time") &&
      contains(driver_cpp, "g_device_binding.start_io(sample_time, host_time)");
  const bool placeholder_zero_timestamp_model =
      contains(driver_cpp, "g_next_placeholder_host_time += 1");
  const bool driver_has_skeleton_start_stop =
      contains(driver_cpp, "g_driver.start_driver()") &&
      contains(driver_cpp, "g_device_binding.shutdown_driver()") &&
      contains(driver_cpp, "g_device_binding.start_io") &&
      contains(driver_cpp, "g_device_binding.stop_io()");
  const bool skeleton_has_prepared_transport =
      contains(skeleton_hpp, "PreparedTransportBackend transport_{};") &&
      contains(prepared_transport_hpp, "class PreparedTransportBackend") &&
      contains(prepared_transport_cpp, "PreparedTransportBackend::start");
  const bool skeleton_has_usb_submit_planner =
      contains(usb_submit_plan_hpp, "PreparedUsbSubmitPlanner") &&
      contains(skeleton_hpp, "PreparedUsbSubmitPlanner");
  const bool skeleton_has_usb_request_pool =
      contains(usb_request_pool_hpp, "PreparedUsbRequestPool") &&
      contains(skeleton_hpp, "PreparedUsbRequestPool");

  std::vector<std::string> blockers;
  if (!device_source_present || !driver_source_present || !skeleton_source_present || !iig_present) {
    blockers.push_back("driverkit_extension_sources_missing");
  }
  if (!source_binding_present) {
    blockers.push_back("audio_device_runtime_binding_missing");
  }
  if (device_start_io_passthrough || !device_start_io_bridged) {
    blockers.push_back("device_start_io_not_bound_to_runtime");
  }
  if (device_stop_io_passthrough || !device_stop_io_bridged) {
    blockers.push_back("device_stop_io_not_bound_to_runtime");
  }
  if (device_configuration_change_unsupported || !configuration_change_bridged) {
    blockers.push_back("device_configuration_change_not_bound_to_runtime");
  }
  if (device_abort_configuration_change_stub || !configuration_abort_bridged) {
    blockers.push_back("device_abort_configuration_change_not_bound_to_runtime");
  }
  if (stream_memory_binding_missing && !source_stream_memory_model_present) {
    blockers.push_back("stream_memory_model_missing");
  }
  if (zero_timestamp_binding_missing && !source_zero_timestamp_model_present) {
    blockers.push_back("zero_timestamp_source_model_missing");
  }
  if (driver_start_device_uses_default_config || !driver_start_device_configures_binding) {
    blockers.push_back("driver_start_device_not_bound_to_runtime");
  }
  if (!driver_stop_device_bridged) {
    blockers.push_back("driver_stop_device_not_bound_to_runtime");
  }

  const bool prepared_backend_available =
      skeleton_has_prepared_transport && skeleton_has_usb_submit_planner &&
      skeleton_has_usb_request_pool;
  const bool source_binding_complete =
      source_binding_present && driver_start_device_configures_binding &&
      driver_stop_device_bridged && device_start_io_bridged && device_stop_io_bridged &&
      configuration_change_bridged && configuration_abort_bridged &&
      source_stream_memory_model_present && source_zero_timestamp_model_present &&
      !device_start_io_passthrough && !device_stop_io_passthrough &&
      !device_configuration_change_unsupported && !device_abort_configuration_change_stub &&
      !driver_start_device_uses_default_config;
  const bool runtime_binding_blocked = false;
  const bool real_driverkit_sdk_runtime_blocked = true;
  const bool pass = device_source_present && driver_source_present && skeleton_source_present &&
                    iig_present && device_declares_runtime_hooks &&
                    driver_has_skeleton_start_stop && prepared_backend_available &&
                    source_binding_complete && real_driverkit_sdk_runtime_blocked;

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.driverkit-runtime-binding-gap-gate.v2\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"offline source gate; PASS means DriverKit extension hooks are bound to the C++ runtime model while real dext readiness remains blocked\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_driverkit_install_or_hardware_touch\",\n";
  print_bool("device_source_present", device_source_present);
  print_bool("driver_source_present", driver_source_present);
  print_bool("skeleton_source_present", skeleton_source_present);
  print_bool("iig_present", iig_present);
  print_bool("device_declares_runtime_hooks", device_declares_runtime_hooks);
  print_bool("device_start_io_passthrough", device_start_io_passthrough);
  print_bool("device_stop_io_passthrough", device_stop_io_passthrough);
  print_bool("device_configuration_change_unsupported", device_configuration_change_unsupported);
  print_bool("device_abort_configuration_change_stub", device_abort_configuration_change_stub);
  print_bool("stream_memory_binding_missing", stream_memory_binding_missing);
  print_bool("zero_timestamp_binding_missing", zero_timestamp_binding_missing);
  print_bool("driver_start_device_uses_default_config", driver_start_device_uses_default_config);
  print_bool("source_binding_present", source_binding_present);
  print_bool("driver_start_device_configures_binding", driver_start_device_configures_binding);
  print_bool("driver_stop_device_bridged", driver_stop_device_bridged);
  print_bool("device_start_io_bridged", device_start_io_bridged);
  print_bool("device_stop_io_bridged", device_stop_io_bridged);
  print_bool("configuration_change_bridged", configuration_change_bridged);
  print_bool("configuration_abort_bridged", configuration_abort_bridged);
  print_bool("source_stream_memory_model_present", source_stream_memory_model_present);
  print_bool("source_zero_timestamp_model_present", source_zero_timestamp_model_present);
  print_bool("placeholder_zero_timestamp_model", placeholder_zero_timestamp_model);
  print_bool("driver_has_skeleton_start_stop", driver_has_skeleton_start_stop);
  print_bool("skeleton_has_prepared_transport", skeleton_has_prepared_transport);
  print_bool("skeleton_has_usb_submit_planner", skeleton_has_usb_submit_planner);
  print_bool("skeleton_has_usb_request_pool", skeleton_has_usb_request_pool);
  print_bool("prepared_backend_available", prepared_backend_available);
  print_bool("source_binding_complete", source_binding_complete);
  print_bool("runtime_binding_blocked", runtime_binding_blocked);
  print_bool("real_driverkit_sdk_runtime_blocked", real_driverkit_sdk_runtime_blocked);
  print_bool("product_driverkit_runtime_ready", false);
  print_blockers(blockers);
  std::cout
      << "  \"next_required_action\": \"BUILD_EXTENSION_WITH_REAL_DRIVERKIT_SDK_AND_REPLACE_PLACEHOLDER_TIMESTAMP_WITH_ADK_CLOCK_BINDING\",\n"
      << "  \"blocked_claim\": \"NO_REAL_DRIVERKIT_RUNTIME_OR_HARDWARE_READINESS_UNTIL_SOURCE_BINDING_BUILDS_WITH_DRIVERKIT_SDK_AND_PHYSICAL_VALIDATION\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
