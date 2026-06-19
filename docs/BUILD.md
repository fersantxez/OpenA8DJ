# Build

OpenA8DJ `main` builds the modern macOS driver line.

The current public preview uses:

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

## Build The Installer

```sh
make dist
```

Generated release artifacts:

```text
build/OpenA8DJ-<version>.pkg
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>-checksums.txt
```

The preview package may be ad-hoc signed unless signing identities are supplied.
A polished end-user release requires Developer ID signing and Apple
notarization.

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
[`TEST_PLAN.md`](TEST_PLAN.md).
