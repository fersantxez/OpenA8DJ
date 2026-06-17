# DriverKit Extension Scaffold

This directory is a non-installing scaffold for the future AudioDriverKit dext.
It is intentionally excluded from the default CMake build because this machine
currently lacks the DriverKit SDK.

Offline contracts already prove the product-facing behavior that this scaffold
must preserve:

- `opena8djcpp_driverkit_surface_model`
- `opena8djcpp_driverkit_shell_contract`
- `opena8djcpp_driverkit_runtime_contract`
- `opena8djcpp_driverkit_prepared_transport_contract`
- `opena8djcpp_driverkit_usb_submit_binding_contract`
- `opena8djcpp_driverkit_usb_request_lifecycle_contract`
- `opena8djcpp_driverkit_usb_request_shutdown_contract`
- `opena8djcpp_prepared_transport_packet_contract`
- `opena8djcpp_prepared_transport_routing_timecode_contract`
- `opena8djcpp_prepared_transport_recovery_contract`

Before this scaffold can become a runnable dext:

- install/select full Xcode with DriverKit SDK;
- obtain DriverKit, DriverKit Audio Family, and USB transport entitlements;
- bind `OpenA8DJAudioDriver` to real `IOUserAudioDriver`;
- bind `OpenA8DJAudioDevice` and `IOUserAudioStream` buffers to the prepared
  transport backend;
- allocate/map `IOMemoryDescriptor` stream buffers for the 8 input channels and
  four 2-channel output streams;
- publish monotonic zero timestamps through `UpdateCurrentZeroTimestamp` and
  answer HAL reads through `GetCurrentZeroTimestamp`;
- handle sample-rate and buffer changes through
  `RequestDeviceConfigurationChange` / `PerformDeviceConfigurationChange` only
  after IO has stopped;
- implement USBDriverKit transport for Audio 8 DJ endpoints;
- map USBDriverKit async requests to the preallocated request pool without
  fallback allocation;
- preserve explicit stop cancellation accounting and reject late completions
  after cancel;
- run the full offline gate suite;
- request a locked physical test window before any install or activation.

Do not install, activate, unload, reload, or sign this scaffold from default
developer commands.
