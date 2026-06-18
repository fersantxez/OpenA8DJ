#include "OpenA8DJAudioDevice.iig"

// Future binding point for IOUserAudioDevice, IOUserAudioStream objects,
// IOMemoryDescriptor stream buffers, zero timestamps, and configuration-change
// sequencing. The real dext must publish monotonic zero timestamps with
// UpdateCurrentZeroTimestamp, answer HAL timing through GetCurrentZeroTimestamp,
// and accept RequestDeviceConfigurationChange only when IO is stopped. The
// default offline build validates these policies with pure C++ contracts before
// this file is compiled with the DriverKit SDK.
namespace opena8djcpp::driverkit::extension_bridge {
kern_return_t StartIO(IOUserAudioStartStopFlags flags);
kern_return_t StopIO(IOUserAudioStartStopFlags flags);
kern_return_t PerformDeviceConfigurationChange(uint64_t action, OSObject* change_info);
kern_return_t AbortDeviceConfigurationChange(uint64_t action, OSObject* change_info);
}  // namespace opena8djcpp::driverkit::extension_bridge

bool OpenA8DJAudioDevice::init() {
  return IOUserAudioDevice::init();
}

void OpenA8DJAudioDevice::free() {
  IOUserAudioDevice::free();
}

kern_return_t OpenA8DJAudioDevice::StartIO(IOUserAudioStartStopFlags flags) {
  return opena8djcpp::driverkit::extension_bridge::StartIO(flags);
}

kern_return_t OpenA8DJAudioDevice::StopIO(IOUserAudioStartStopFlags flags) {
  return opena8djcpp::driverkit::extension_bridge::StopIO(flags);
}

kern_return_t OpenA8DJAudioDevice::PerformDeviceConfigurationChange(uint64_t action,
                                                                    OSObject* change_info) {
  return opena8djcpp::driverkit::extension_bridge::PerformDeviceConfigurationChange(action,
                                                                                    change_info);
}

kern_return_t OpenA8DJAudioDevice::AbortDeviceConfigurationChange(uint64_t action,
                                                                  OSObject* change_info) {
  return opena8djcpp::driverkit::extension_bridge::AbortDeviceConfigurationChange(action,
                                                                                  change_info);
}
