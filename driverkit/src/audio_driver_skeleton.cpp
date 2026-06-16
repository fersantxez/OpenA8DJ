#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

namespace opena8djcpp::driverkit {

bool AudioDriverSkeleton::start_driver() {
  if (state_ == AudioDriverState::Started ||
      !validate_driverkit_device_model(device_model_)) {
    return false;
  }
  state_ = AudioDriverState::Started;
  return true;
}

bool AudioDriverSkeleton::stop_driver() {
  if (state_ != AudioDriverState::Started) {
    return false;
  }
  state_ = AudioDriverState::Stopped;
  return true;
}

AudioDriverState AudioDriverSkeleton::state() const {
  return state_;
}

const DriverKitDeviceModel& AudioDriverSkeleton::device_model() const {
  return device_model_;
}

}  // namespace opena8djcpp::driverkit
