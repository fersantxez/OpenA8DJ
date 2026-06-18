# DriverKit Notes

Date: 2026-06-16

## Official Apple References Used

- DriverKit: https://developer.apple.com/documentation/driverkit
- AudioDriverKit: https://developer.apple.com/documentation/audiodriverkit
- WWDC21 Create audio drivers with DriverKit: https://developer.apple.com/videos/play/wwdc2021/10190/
- System Extensions and DriverKit: https://developer.apple.com/system-extensions/
- Deprecated Kernel Extensions and System Extension Alternatives: https://developer.apple.com/support/kernel-extensions/
- Installing System Extensions and Drivers: https://developer.apple.com/documentation/SystemExtensions/installing-system-extensions-and-drivers
- USBDriverKit IOUSBHostPipe: https://developer.apple.com/documentation/usbdriverkit/iousbhostpipe

## Architecture Reading

- AudioDriverKit is the target because Apple describes it as the modern framework for audio driver extensions that communicate with CoreAudio HAL from DriverKit.
- WWDC21 states that from macOS Monterey the AudioDriverKit model can use a single dext instead of a separate audio server plug-in plus driver extension.
- The HAL communicates with the audio dext through AudioDriverKit-managed user client plumbing.
- The key object mapping is:
  - `IOUserAudioDriver`: dext entry point.
  - `IOUserAudioDevice`: device state, IO start/stop, timestamps, and configuration changes.
  - `IOUserAudioStream`: input/output stream memory exposed to HAL.
- Stream memory should be represented with `IOMemoryDescriptor`/`IOBufferMemoryDescriptor` and should ideally correspond to the hardware/DMA-facing memory model.
- Hardware timestamp handling must use zero timestamp updates as close as possible to the device clock.
- Configuration changes such as sample-rate changes must go through the HAL configuration-change path; the driver should not mutate running IO state ad hoc.

## Entitlements And Deployment Constraints

- DriverKit requires Apple entitlements for distribution.
- AudioDriverKit dexts require DriverKit/audio access; hardware transport may also require USBDriverKit entitlement.
- System Extensions run in user space and require activation through the SystemExtensions flow.
- No dext activation, deactivation, install, reload, signing, or system service mutation is part of the offline phase.

## Audio 8 DJ Implication

- The core should remain transport-agnostic until USB packet behavior is fully captured in tests.
- DriverKit shell should map the Audio 8 DJ as one device with 8 input channels and 8 output channels, arranged as A/B/C/D stereo pairs.
- USB transport likely needs a separate abstraction that can later be backed by USBDriverKit/IOUSBHostPipe if entitlement and endpoint control requirements permit.

## Local Toolchain Blocker

Observed on 2026-06-16 and refreshed on 2026-06-18 by
`opena8djcpp_driverkit_sdk_preflight_gate`:

- `xcrun --show-sdk-path` works and points to
  `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`.
- `xcodebuild -showsdks` fails because the active developer directory is
  `/Library/Developer/CommandLineTools`, not a full Xcode install.
- `xcrun --sdk driverkit --show-sdk-path` cannot locate a DriverKit SDK.
- No `DriverKit*.sdk` or `AudioDriverKit` path was found under
  `/Applications/Xcode.app` or `/Library/Developer/CommandLineTools`.
- No `/Applications/Xcode*.app` is currently installed.
- `xcodes` CLI is installed and usable (`2.0.2`), and the available list shows
  `26.5 (17F42) [Apple Silicon]` for this host, but `xcodes installed` reports
  zero installed Xcodes.
- `aria2c` is installed, so `xcodes` can use the faster download path.
- `/Applications` currently has about `12.641 GiB` free. The preflight requires
  `80 GiB` free before attempting full Xcode installation.

Conclusion:

- A real AudioDriverKit dext target cannot be compiled on this machine in the current toolchain state.
- This is an environment/toolchain blocker, not a reason to touch system
  extensions or install/activate any driver.
- The current substitute gates are `opena8djcpp_driverkit_surface_model` and
  `opena8djcpp_driverkit_shell_contract`, which validate the intended
  device/stream/sample-rate model and bounded lifecycle offline.
- `opena8djcpp_driverkit_sdk_preflight_gate` must continue to block real dext
  readiness while `product_driverkit_build_allowed=false`, and must keep
  `noninteractive_xcode_install_prerequisites_met=false` while disk space is
  below the Xcode installation threshold.

Required before real dext build:

- Full Xcode with DriverKit SDK.
- At least `80 GiB` free on the `/Applications` volume before attempting a
  full Xcode install with `xcodes`.
- Appropriate DriverKit and AudioDriverKit entitlements.
- Xcode or CMake/Xcode generator configuration for the dext bundle.
- Explicit user-approved window before any activation, installation, reload, or system-extension command.

## Prepared Transport Contract

The current CPU evidence points at USB transfer enqueue/requeue work as the
hotspot. Until the real DriverKit SDK is available, the project models the
required backend contract offline in
`tools/driverkit_prepared_transport_contract.cpp`.

Required architecture:

- HAL/CoreAudio-facing code reads and writes bounded audio rings only.
- A prepared transport backend owns USB-facing slots and steady-state requeue.
- HAL steady-state direct USB requeue count must be `0`.
- The backend may enqueue prepared slots during start/prepare, outside the
  audio hot path.
- No fallback allocations are allowed after streaming starts.
- Completion cadence must remain `1x`; gaps above `1.25x` are rejected.
- Capture/playback timestamps must stay monotonic.
- 8 inputs, 8 outputs, A/B/C/D stereo routing, and timecode profile semantics
  must remain represented.

This gate does not compile a real dext and does not activate a system
extension. It exists to prevent the future DriverKit implementation from
smuggling the current HAL enqueue bottleneck back into the hot path.

## Extension Scaffold

Added on 2026-06-17:

- `driverkit/extension/Info.plist.template`
- `driverkit/extension/OpenA8DJAudioDriver.entitlements.template`
- `driverkit/extension/OpenA8DJAudioDriver.iig`
- `driverkit/extension/OpenA8DJAudioDevice.iig`
- `driverkit/extension/src/OpenA8DJAudioDriver.cpp`
- `driverkit/extension/src/OpenA8DJAudioDevice.cpp`

The scaffold follows the Apple AudioDriverKit shape:

- `IOKitPersonalities` identify the DriverKit service class.
- `IOUserAudioDriver` is the dext entry point.
- `IOUserAudioDevice` owns IO start/stop and configuration-change sequencing.
- Stream buffers must later be represented by `IOMemoryDescriptor`/
  `IOBufferMemoryDescriptor` and exposed to HAL through `IOUserAudioStream`.
- DriverKit audio and USB transport entitlements are required before a real
  build or activation.

Local contract:

- `opena8djcpp_driverkit_extension_scaffold_contract`.
- The scaffold must remain excluded from the default CMake build until the
  DriverKit SDK is present.
- The contract must report `system_extension_activated=false` and
  `driver_installed_or_activated=false`.

This is an architectural scaffold only. It does not remove the need for full
Xcode/DriverKit SDK, provisioning, signing, user-approved system-extension
activation, USBDriverKit endpoint implementation, or locked physical evidence.

## Runtime Contract Reinforcement

Added on 2026-06-17 after the capture-paced playback refill rejection:

- `AudioDriverSkeleton` now models the DriverKit runtime obligations that must
  survive the real dext port:
  - five IO memory descriptors: one 8-channel input stream plus four
    2-channel output streams;
  - 32-bit float stream-memory accounting for the HAL/AudioDriverKit-facing
    side;
  - monotonic zero timestamp updates while IO is running;
  - explicit rejection of sample-rate/buffer configuration changes while IO is
    running;
  - counters for memory-layout builds/failures, timestamp updates/regressions,
    accepted configuration changes, and rejected configuration changes.
- `opena8djcpp_driverkit_runtime_contract` verifies:
  - `io_memory_descriptors=5`;
  - `io_memory_total_bytes=4096` for 64-frame buffers;
  - valid zero timestamp sequence accepted;
  - duplicate/regressing zero timestamp sequence rejected in a separate negative
    test;
  - running configuration change rejected;
  - stopped configuration change accepted;
  - 44.1 kHz and 48 kHz pressure rows still pass without fallback allocations
    or HAL steady requeues.
- `opena8djcpp_driverkit_extension_scaffold_contract` now requires the scaffold
  to name `IOMemoryDescriptor`, `UpdateCurrentZeroTimestamp`,
  `GetCurrentZeroTimestamp`, `RequestDeviceConfigurationChange`, and
  `PerformDeviceConfigurationChange`.

Interpretation:

- This is better DriverKit discipline, not product readiness.
- It prevents a future dext implementation from skipping timing and
  configuration contracts, but it does not solve the current physical
  quality/USB enqueue bottleneck by itself.

## Runtime Binding Gap Gate

Added on 2026-06-18:

- `opena8djcpp_driverkit_runtime_binding_gap_gate` reads the DriverKit extension
  source and the prepared C++ backend source.
- It requires the prepared backend to be present while explicitly detecting that
  the extension-facing runtime hooks remain stubs:
  - `StartIO` passes through to `IOUserAudioDevice::StartIO`;
  - `StopIO` passes through to `IOUserAudioDevice::StopIO`;
  - `PerformDeviceConfigurationChange` returns `kIOReturnUnsupported`;
  - `AbortDeviceConfigurationChange` returns success without real recovery;
  - stream memory and zero timestamp bindings are absent;
  - `StartDevice` still uses default `AudioStreamConfig{}`.

Interpretation:

- PASS means the blocker is correctly exposed in offline evidence.
- PASS does not mean the DriverKit extension is runnable, signed, installed, or
  ready for hardware.
- The next implementation step is to bind `IOUserAudioDevice` to the skeleton's
  stream memory, timestamps, configuration policy, and USB request adapter.

## Audio Device Runtime Binding Model

Added on 2026-06-18:

- `AudioDeviceRuntimeBinding` is a pure C++ model of the future
  `IOUserAudioDevice` binding.
- It owns the device-facing sequence around `AudioDriverSkeleton`:
  - configure stream;
  - publish the five stream-memory descriptors;
  - start IO only after stream memory exists;
  - publish a monotonic zero timestamp at IO start;
  - reject configuration changes while IO is running;
  - accept configuration changes while stopped and republish stream memory;
  - make `StopIO` idempotent;
  - stop IO before driver shutdown.
- `opena8djcpp_driverkit_device_binding_contract` verifies the sequence
  without DriverKit SDK, dext install, CoreAudio, USB, or hardware.

Interpretation:

- PASS means the next extension binding target is represented by executable C++
  evidence.
- PASS does not replace the runtime binding gap gate. The extension source still
  needs to call this binding and then be built with a real DriverKit SDK before
  any runnable dext claim.
