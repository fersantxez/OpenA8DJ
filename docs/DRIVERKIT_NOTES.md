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

Observed on 2026-06-16:

- `xcrun --sdk macosx --show-sdk-path` works and points to Command Line Tools.
- `xcodebuild -showsdks` fails because the active developer directory is `/Library/Developer/CommandLineTools`, not a full Xcode install.
- `xcrun --sdk driverkit --show-sdk-path` cannot locate a DriverKit SDK.
- No `AudioDriverKit.framework` or `DriverKit.framework` was found under `/Applications`.

Conclusion:

- A real AudioDriverKit dext target cannot be compiled on this machine in the current toolchain state.
- This is an environment/toolchain blocker, not a reason to touch system extensions or install anything automatically.
- The current substitute gate is `opena8djcpp_driverkit_surface_model`, which validates the intended device/stream/sample-rate model offline.

Required before real dext build:

- Full Xcode with DriverKit SDK.
- Appropriate DriverKit and AudioDriverKit entitlements.
- Xcode or CMake/Xcode generator configuration for the dext bundle.
- Explicit user-approved window before any activation, installation, reload, or system-extension command.
