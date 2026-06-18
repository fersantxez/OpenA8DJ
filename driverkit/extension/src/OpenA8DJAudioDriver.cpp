#include "OpenA8DJAudioDriver.iig"

#include "opena8djcpp/driverkit/audio_device_runtime_binding.hpp"
#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <cstdint>

// This file is intentionally excluded from the default CMake build. It is the
// DriverKit SDK binding target shape for a future Xcode/DriverKit SDK build.
// The offline build uses AudioDriverSkeleton to validate the same lifecycle and
// transport contract without installing or activating a dext.

namespace {

opena8djcpp::driverkit::AudioDriverSkeleton g_driver;
opena8djcpp::driverkit::AudioDeviceRuntimeBinding g_device_binding(g_driver);
std::uint64_t g_next_placeholder_sample_time = 0;
std::uint64_t g_next_placeholder_host_time = 1;

opena8djcpp::driverkit::AudioStreamConfig default_stream_config() {
  return opena8djcpp::driverkit::AudioStreamConfig{
      .sample_rate = 48000,
      .buffer_frames = 64,
      .transport =
          opena8djcpp::PreparedTransportConfig{
              .iso_frames = 8,
              .capture_slots = 16,
              .playback_slots = 16,
          },
      .usb_slots_per_submit = 8,
      .usb_bytes_per_slot = opena8djcpp::kMode2DefaultTransferBytes,
      .usb_initial_capture_slots = 8,
      .usb_initial_playback_slots = 8,
      .usb_request_slots = 8,
      .usb_request_completion_depth = 4,
      .usb_retain_submit_descriptors = false,
  };
}

kern_return_t to_kern_return(bool ok) {
  return ok ? kIOReturnSuccess : kIOReturnError;
}

}  // namespace

namespace opena8djcpp::driverkit::extension_bridge {

kern_return_t ConfigureDevice() {
  return to_kern_return(g_device_binding.configure_device(default_stream_config()));
}

kern_return_t StartIO(IOUserAudioStartStopFlags flags) {
  (void)flags;
  if (g_driver.stream_started()) {
    return kIOReturnSuccess;
  }
  if (!g_device_binding.stream_memory_bound()) {
    const auto configure_result = ConfigureDevice();
    if (configure_result != kIOReturnSuccess) {
      return configure_result;
    }
  }
  const auto sample_time = g_next_placeholder_sample_time;
  const auto host_time = g_next_placeholder_host_time;
  if (!g_device_binding.start_io(sample_time, host_time)) {
    return kIOReturnError;
  }
  g_next_placeholder_sample_time += g_driver.stream_config().buffer_frames;
  g_next_placeholder_host_time += 1;
  return kIOReturnSuccess;
}

kern_return_t StopIO(IOUserAudioStartStopFlags flags) {
  (void)flags;
  return to_kern_return(g_device_binding.stop_io());
}

kern_return_t PerformDeviceConfigurationChange(uint64_t action, OSObject* change_info) {
  (void)action;
  (void)change_info;
  if (g_driver.stream_started()) {
    return kIOReturnBusy;
  }
  return to_kern_return(
      g_device_binding.request_configuration_change(default_stream_config()));
}

kern_return_t AbortDeviceConfigurationChange(uint64_t action, OSObject* change_info) {
  (void)action;
  (void)change_info;
  return to_kern_return(g_device_binding.abort_configuration_change());
}

kern_return_t ShutdownDriver() {
  return to_kern_return(g_device_binding.shutdown_driver());
}

}  // namespace opena8djcpp::driverkit::extension_bridge

bool OpenA8DJAudioDriver::init() {
  return IOUserAudioDriver::init();
}

void OpenA8DJAudioDriver::free() {
  IOUserAudioDriver::free();
}

kern_return_t OpenA8DJAudioDriver::Start(IOService* provider) {
  const auto result = IOUserAudioDriver::Start(provider);
  if (result != kIOReturnSuccess) {
    return result;
  }
  return g_driver.start_driver() ? kIOReturnSuccess : kIOReturnError;
}

kern_return_t OpenA8DJAudioDriver::Stop(IOService* provider) {
  (void)opena8djcpp::driverkit::extension_bridge::ShutdownDriver();
  return IOUserAudioDriver::Stop(provider);
}

kern_return_t OpenA8DJAudioDriver::NewUserClient(uint32_t type, IOUserClient** user_client) {
  return IOUserAudioDriver::NewUserClient(type, user_client, SUPERDISPATCH);
}

kern_return_t OpenA8DJAudioDriver::StartDevice(IOUserAudioObjectID object_id,
                                               IOUserAudioStartStopFlags flags) {
  (void)object_id;
  (void)flags;
  return opena8djcpp::driverkit::extension_bridge::ConfigureDevice();
}

kern_return_t OpenA8DJAudioDriver::StopDevice(IOUserAudioObjectID object_id,
                                              IOUserAudioStartStopFlags flags) {
  (void)object_id;
  return opena8djcpp::driverkit::extension_bridge::StopIO(flags);
}
