#include "OpenA8DJAudioDevice.iig"

// Future binding point for IOUserAudioDevice, IOUserAudioStream objects,
// IOMemoryDescriptor stream buffers, zero timestamps, and configuration-change
// sequencing. The default offline build validates these policies with pure C++
// contracts before this file is compiled with the DriverKit SDK.

bool OpenA8DJAudioDevice::init() {
  return IOUserAudioDevice::init();
}

void OpenA8DJAudioDevice::free() {
  IOUserAudioDevice::free();
}

kern_return_t OpenA8DJAudioDevice::StartIO(IOUserAudioStartStopFlags flags) {
  return IOUserAudioDevice::StartIO(flags);
}

kern_return_t OpenA8DJAudioDevice::StopIO(IOUserAudioStartStopFlags flags) {
  return IOUserAudioDevice::StopIO(flags);
}

kern_return_t OpenA8DJAudioDevice::PerformDeviceConfigurationChange(uint64_t action,
                                                                    OSObject* change_info) {
  (void)action;
  (void)change_info;
  return kIOReturnUnsupported;
}

kern_return_t OpenA8DJAudioDevice::AbortDeviceConfigurationChange(uint64_t action,
                                                                  OSObject* change_info) {
  (void)action;
  (void)change_info;
  return kIOReturnSuccess;
}
