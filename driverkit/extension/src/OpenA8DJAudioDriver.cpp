#include "OpenA8DJAudioDriver.iig"

#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

// This file is intentionally excluded from the default CMake build. It is the
// DriverKit SDK binding target shape for a future Xcode/DriverKit SDK build.
// The offline build uses AudioDriverSkeleton to validate the same lifecycle and
// transport contract without installing or activating a dext.

namespace {

opena8djcpp::driverkit::AudioDriverSkeleton g_driver;

}  // namespace

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
  (void)g_driver.stop_driver();
  return IOUserAudioDriver::Stop(provider);
}

kern_return_t OpenA8DJAudioDriver::NewUserClient(uint32_t type, IOUserClient** user_client) {
  return IOUserAudioDriver::NewUserClient(type, user_client, SUPERDISPATCH);
}

kern_return_t OpenA8DJAudioDriver::StartDevice(IOUserAudioObjectID object_id,
                                               IOUserAudioStartStopFlags flags) {
  (void)object_id;
  (void)flags;
  const opena8djcpp::driverkit::AudioStreamConfig config{};
  if (!g_driver.configure_stream(config) && !g_driver.stream_started()) {
    return kIOReturnError;
  }
  return g_driver.start_stream() ? kIOReturnSuccess : kIOReturnError;
}

kern_return_t OpenA8DJAudioDriver::StopDevice(IOUserAudioObjectID object_id,
                                              IOUserAudioStartStopFlags flags) {
  (void)object_id;
  (void)flags;
  return g_driver.stop_stream() ? kIOReturnSuccess : kIOReturnError;
}
