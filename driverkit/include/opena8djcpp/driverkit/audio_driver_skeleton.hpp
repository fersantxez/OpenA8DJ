#pragma once

#include "opena8djcpp/driverkit_model.hpp"

namespace opena8djcpp::driverkit {

enum class AudioDriverState {
  Created,
  Started,
  Stopped,
};

// Compile this target only inside a DriverKit-capable Xcode project. The pure
// C++ core stays independent so offline gates can run without system mutation.
class AudioDriverSkeleton {
 public:
  [[nodiscard]] bool start_driver();
  [[nodiscard]] bool stop_driver();
  [[nodiscard]] AudioDriverState state() const;
  [[nodiscard]] const DriverKitDeviceModel& device_model() const;

 private:
  DriverKitDeviceModel device_model_ = make_driverkit_device_model();
  AudioDriverState state_ = AudioDriverState::Created;
};

}  // namespace opena8djcpp::driverkit
