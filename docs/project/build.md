# Build

OpenA8DJ `main` builds the modern macOS driver line.

The current 0.5.1 responsive line uses:

- a Core Audio HAL bundle for the installable macOS driver;
- IOUSBHost for Audio 8 DJ USB transport;
- CoreMIDI for MIDI endpoints;
- a pure C++ core for packet, routing, timecode, and real-time contracts;
- DriverKit/AudioDriverKit scaffolding for the future System Extension path.

## Build The macOS Driver

```sh
make clean
make all
```

Main local outputs:

```text
build/OpenA8DJ.driver
build/opena8dj-control
build/opena8dj-midid
build/audio-inspect
build/audio-io-test
build/audio-pair-tone
build/midi-list
```

The default HAL build is the frozen 0.5.1 `output3072` profile:

```text
HAL_TRANSFER_POOL_CURSOR=1
HAL_FAST_ISO_TRANSFER_CONFIG=1
HAL_REUSE_ISOC_COMPLETIONS=0
HAL_RAW_ISOC_COMPLETIONS=0
HAL_OUTPUT_START_LATENCY_FRAMES=3072
HAL_OUTPUT_RESTART_LATENCY_FRAMES=1536
HAL_OUTPUT_TARGET_LATENCY_FRAMES=3072
HAL_OUTPUT_ELASTIC_HIGH_WATER_FRAMES=9216
```

## Build The Installer

```sh
make driver-dist
```

Generated public driver artifacts:

```text
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>-checksums.txt
```

The PKG is built inside the DMG but is not uploaded as a separate GitHub asset.
Artifacts produced without release signing identities must be labeled as
previews. Use `make release-signed`, `make notarize`, and
`make verify-signed-release` for a polished signed replacement.

## Build The Official Signed Release

The official release path is Developer ID signing and Apple notarization. This
requires an active Apple Developer Program team, a
`Developer ID Application` certificate, a `Developer ID Installer` certificate,
and a local `notarytool` keychain profile.

```sh
make clean
make release-signed \
  SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)" \
  PKG_SIGN_IDENTITY="Developer ID Installer: Example Team (TEAMID)" \
  DMG_SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)"
make notarize NOTARY_PROFILE=OpenA8DJNotary
make verify-signed-release
```

Do not describe DMG/PKG assets as Developer ID signed or Apple-notarized unless
`make verify-signed-release` passes. A build that merely compiles or packages is
not enough.

## C++ Offline Build

Use CMake for pure C++ contracts and the DriverKit shell:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline
ctest --test-dir build/cpp-offline --output-on-failure
```

This path must remain safe: no install, no Core Audio restart, no default-device
change, no USB reset, no playback, no recording, and no DriverKit activation.

## DriverKit Probe

The DriverKit/AudioDriverKit path is build-only until a real SDK, signing, and
activation plan are available:

```sh
cmake -S . -B build/driverkit-sdk-probe \
  -DOPENA8DJCPP_ENABLE_DRIVERKIT_SDK_BUILD=ON
cmake --build build/driverkit-sdk-probe \
  --target opena8djcpp_driverkit_extension_build_probe
```

That target must never install or activate a System Extension.

## Install For Local Validation

Only install during an authorized hardware/audio window:

```sh
sudo installer -pkg build/OpenA8DJ-<version>.pkg -target /
```

Uninstall:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

Hardware/audio validation requires the global lock described in
[the test plan](validation/test-plan.md).
