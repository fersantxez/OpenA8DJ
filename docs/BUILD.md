# Reproducible Build Plan

This document is the build/release engineering contract for the C++ redesign.
It describes the desired build shape before mutating the build system. It does
not require hardware, Core Audio device access, USB access, install steps, or
system extension loading.

## Decision

Use CMake as the canonical build system.

Xcode should be supported only as a generated developer view:

```sh
cmake -S . -B build/xcode -G Xcode
```

Do not maintain a hand-edited `.xcodeproj` as the source of truth. A checked-in
Xcode project will drift from CI and is too easy to turn into a second release
pipeline. CMake gives one graph for local builds, CI, offline tests, packaging,
and optional generated Xcode projects.

The immediate priority is not package generation. The first milestone is a
pure offline C++ core with executable tests.

## Non-Mutating Default

The default build must be safe on a developer machine:

- no `sudo`;
- no writes outside the build directory;
- no `/Library/Audio/Plug-Ins/HAL` writes;
- no `/Library/LaunchAgents` writes;
- no `/usr/local/bin` writes;
- no `launchctl`;
- no `killall coreaudiod`;
- no `systemextensionsctl`;
- no USB device claiming;
- no Core Audio default-device changes;
- no sound playback or recording.

Anything that can touch the system, Core Audio runtime, USB hardware, or audio
hardware must be opt-in and excluded from the default `all` and `test` targets.

## Toolchain Contract

Recommended minimum local toolchain:

```text
CMake >= 3.28
Ninja >= 1.11
Xcode command line tools from the active Xcode selected by xcode-select
AppleClang from xcrun
```

Recommended configure command for the safe path:

```sh
cmake -S . -B build/cmake/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DOPENA8DJ_BUILD_HAL=OFF \
  -DOPENA8DJ_BUILD_DRIVERKIT=OFF \
  -DOPENA8DJ_BUILD_INSTALLERS=OFF \
  -DOPENA8DJ_BUILD_HARDWARE_TOOLS=OFF \
  -DOPENA8DJ_ENABLE_INSTALL_TARGETS=OFF
```

Recommended release configure command:

```sh
cmake -S . -B build/cmake/release -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DOPENA8DJ_BUILD_HAL=OFF \
  -DOPENA8DJ_BUILD_DRIVERKIT=OFF \
  -DOPENA8DJ_BUILD_INSTALLERS=OFF \
  -DOPENA8DJ_BUILD_HARDWARE_TOOLS=OFF \
  -DOPENA8DJ_ENABLE_INSTALL_TARGETS=OFF
```

Build and test:

```sh
cmake --build build/cmake/debug
ctest --test-dir build/cmake/debug --output-on-failure
```

These commands are the target behavior for the future CMake implementation.

## Build Options

The CMake options should make the safe path explicit:

```text
OPENA8DJ_BUILD_CORE=ON
OPENA8DJ_BUILD_TESTS=ON
OPENA8DJ_BUILD_HAL=OFF
OPENA8DJ_BUILD_DRIVERKIT=OFF
OPENA8DJ_BUILD_TOOLS=ON
OPENA8DJ_BUILD_HARDWARE_TOOLS=OFF
OPENA8DJ_BUILD_INSTALLERS=OFF
OPENA8DJ_ENABLE_INSTALL_TARGETS=OFF
OPENA8DJ_ENABLE_CODESIGN=OFF
OPENA8DJ_WARNINGS_AS_ERRORS=ON
```

`OPENA8DJ_BUILD_TOOLS=ON` may build offline tools only. Tools that enumerate
Core Audio devices, open an Audio 8 DJ USB interface, play audio, record audio,
change device defaults, or change hardware state must be classified as hardware
tools and stay behind `OPENA8DJ_BUILD_HARDWARE_TOOLS=ON`.

## Target Graph

The first CMake graph should be small and deterministic:

```text
opena8dj_core
  Pure C++ static library. No Apple frameworks. No Objective-C. No USB. No
  Core Audio. No filesystem writes.

opena8dj_core_tests
  Executable test binary for deterministic unit tests.

opena8dj_packet_tests
  Executable test binary for Audio 8 DJ packet packing/decoding parity.

opena8dj_topology_tests
  Executable test binary for 8-in/8-out stream and channel layout rules.

opena8dj_dvs_tests
  Executable test binary for DVS/timecode profile policy and channel routing
  invariants, using synthetic data only.

opena8djcpp_dvs_packet_input_decode
  Offline gate executable that packs synthetic DVS carriers into Mode 2 bytes,
  decodes them through input profiles with caller-owned buffers, and verifies
  deck isolation without hardware.
```

Only after these pass should the build add adapter targets:

```text
opena8dj_hal_adapter
  Objective-C/C shim that adapts the pure core to the current HAL bundle.

opena8dj_hal_bundle
  AudioServerPlugIn bundle built into the build directory only. No install.

opena8dj_driverkit_dext
  Future DriverKit/system-extension bundle built into the build directory only.
  Disabled by default.
```

Packaging targets come last and must depend on a release preset, not on the
developer default.

## Core C++ Extraction Order

Extract offline logic before touching HAL, USB, DriverKit, or packaging.

1. Channel topology:
   - 8 input channels;
   - 8 output channels;
   - input pairs A/B/C/D;
   - output pairs A/B/C/D;
   - one 8-channel input stream and four stereo output streams for the current
     macOS-facing contract.

2. Sample conversion:
   - `float` to signed 24-bit sample conversion;
   - gain application;
   - clipping/saturation behavior;
   - endian encoding helpers.

3. Mode-2 output packing:
   - start byte;
   - transfer size;
   - four stereo stream lanes;
   - check byte generation;
   - panic/check bit validation;
   - decode path for tests.

4. DVS/timecode policy:
   - vinyl profile;
   - CD/line profile;
   - input pair mapping;
   - leakage and channel-balance invariants using synthetic signals.

5. Runtime metrics schema:
   - counters and status vocabulary only;
   - no physical capture or listening claims from offline tests.

The existing Python mode-2 validation and simulated output soundcheck are good
behavioral references, but the new C++ core tests should own the deterministic
packet and topology contract directly.

## Test Policy

Every test executable must be runnable by CTest and must not require hardware.

Required initial tests:

```text
topology exposes exactly 8 input and 8 output channels
topology maps A/B/C/D to the expected stereo pairs
float-to-s24 clamps positive and negative full-scale samples
mode-2 packer emits deterministic bytes for fixed synthetic frames
mode-2 decoder round-trips all four output pairs
check-byte validation catches corrupt lane markers
start-byte matrix covers at least 0..5
transfer-size matrix covers at least 48, 80, and 352 bytes
DVS routing keeps Deck A/B input mapping isolated from C/D
```

Recommended CI command:

```sh
cmake --build build/cmake/release
ctest --test-dir build/cmake/release --output-on-failure
```

No CI job should install the HAL, restart Core Audio, claim USB hardware, or
load a DriverKit extension.

## Presets

When CMake is implemented, add `CMakePresets.json` with these presets:

```text
dev-safe
  Ninja, Debug, offline core/tests only.

ci-safe
  Ninja, RelWithDebInfo, offline core/tests only, warnings as errors.

mac-hal-build
  Builds HAL bundle into build dir. No install target enabled.

driverkit-skeleton
  Builds DriverKit bundle into build dir. Disabled by default. No load/install.

release-package
  Builds package artifacts only after explicit selection.
```

The default preset should be `dev-safe`.

## DriverKit Preparation

DriverKit should be prepared structurally, not activated operationally.

Current local blocker:

- The active developer directory is Command Line Tools, not full Xcode.
- `xcrun --sdk driverkit --show-sdk-path` cannot locate the DriverKit SDK.
- No local `DriverKit*.sdk` or `AudioDriverKit` path is present under the
  checked developer locations.

Until that changes, the DriverKit target remains an offline C++ shell contract
and must not be described as a real dext build.

Acceptable early structure:

```text
src/driverkit/
  Audio8DJDriver.cpp
  Audio8DJDriver.h
  Info.plist.in
  entitlements.plist.in
```

Rules:

- `OPENA8DJ_BUILD_DRIVERKIT=OFF` by default;
- build output stays under `build/`;
- no install target in the default graph;
- no `systemextensionsctl`;
- no launch, activation, deactivation, or unload commands;
- no entitlement/signing assumptions in offline CI;
- DriverKit code depends on `opena8dj_core` for packet/topology logic instead
  of duplicating it.

DriverKit readiness is a compile/link milestone only until an authorized
hardware and system-extension validation window exists.

## Packaging And Signing

Packaging is not part of the first milestone.

When restored under CMake, packaging should be explicit:

```text
package-pkg
package-dmg
checksums
```

Signing inputs must be passed as cache variables or environment variables and
must never be inferred from the developer machine:

```text
OPENA8DJ_SIGN_IDENTITY
OPENA8DJ_PKG_SIGN_IDENTITY
OPENA8DJ_DMG_SIGN_IDENTITY
OPENA8DJ_NOTARIZE_PROFILE
```

Unsigned local artifacts are allowed only when the command explicitly requests
them. Public release artifacts still require the legal/provenance, signing, and
notarization gates documented elsewhere.

## CI Shape

Initial CI should have one safe macOS job:

```text
configure ci-safe
build
ctest
upload test logs on failure
```

Later CI can add:

```text
mac-hal-build
driverkit-skeleton
release-package
windows existing WDK workflow
```

The HAL, DriverKit, and package jobs should remain build-only unless a separate
authorized validation environment is defined.

## Release Gates

A build can be called reproducible when:

- the source revision is recorded;
- the CMake preset is recorded;
- the active Xcode/AppleClang version is recorded;
- build options are recorded in `CMakeCache.txt`;
- CTest produces a complete pass/fail log;
- artifacts are written only under `build/`;
- checksums are generated for package outputs;
- no install, load, Core Audio restart, USB claim, playback, or recording step
  occurred during the build/test phase.

Offline tests can prove packet, topology, routing, and deterministic conversion
behavior. They cannot prove analog audio quality, Traktor behavior, physical
capture quality, or human listening quality. Those remain separate authorized
validation gates.

The decorrelated channel-matrix fixture can be prepared offline without audio
hardware:

```sh
make channel-matrix-prepare CHANNEL_MATRIX_PAIR=A CHANNEL_MATRIX_RATE=48000 CHANNEL_MATRIX_SECONDS=8 CHANNEL_MATRIX_PEAK=0.30
```

This command only writes evidence under `local-analysis/channel-matrix`. The
matching `scripts/run-channel-matrix-gate --run-physical` path is a hardware
gate, not a build step, and requires the global audio lock plus an authorized
physical window.

## HAL CPU Experiment Flags

The HAL build currently samples hot stream stats every 16 capture/playback
completion callbacks by default:

```sh
make hal
```

This is equivalent to:

```sh
make HAL_HOT_STREAM_STATS_INTERVAL=16 hal
```

Use interval `1` only when dense per-transfer stats are needed for a specific
diagnostic:

```sh
make HAL_HOT_STREAM_STATS_INTERVAL=1 hal
```

Atomic sampled stream-stat accumulators remain disabled by default:

```sh
make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 hal
```

The opt-in experimental path remains available for A/B validation:

```sh
make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=1 hal
```

The flag only changes sampled stream-stat accounting in the USB completion hot
path. It does not change audio packet bytes, routing, sample rates, advertised
channels, USB scheduling policy, install behavior, or DriverKit artifacts. Any
CPU or quality claim for this flag requires a locked physical soundcheck and
comparison against the recorded mainline/C++ baselines. The first locked
physical run with the flag enabled did not clear those gates, so the default
must remain disabled until better evidence exists.

Do not use stats-off as a default CPU profile:

```sh
make HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0 hal
```

That variant passed offline and install safety, but the locked physical run
still failed quality and measured driver p95 at `36.8%`, worse than the
interval-16 default. It is only useful as a diagnostic A/B if a future question
specifically needs telemetry removed.

Do not reintroduce sparse output-cycle clear as a CPU profile. That experiment
passed HAL safety but failed physical music quality and worsened driver CPU p95
to `38.3%`. It was removed instead of being kept as a disabled build flag.

## Analysis Environment

Precise offline audio analysis can use the local Python environment under the
worktree:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-analysis.txt
```

This environment is local-only and ignored by git. It is used for FFT/CSD/Welch
analysis scripts such as:

```sh
.venv/bin/python scripts/analyze-lti-transfer-quality.py --help
.venv/bin/python scripts/analyze-soundcheck-failure-modes.py --help
```

Do not install these packages globally as part of the build. They are analysis
dependencies, not runtime driver dependencies.

## Migration Order

1. Add CMake core/test skeleton.
2. Extract pure C++ topology and packet code.
3. Port deterministic packet tests from the Python reference behavior.
4. Add CTest and `dev-safe`/`ci-safe` presets.
5. Move offline CI to CMake.
6. Add HAL bundle build as a build-dir-only target.
7. Add generated Xcode support through CMake.
8. Add DriverKit skeleton as disabled build-dir-only target.
9. Reintroduce package/checksum targets behind explicit release presets.

Do not start with installers, HAL installation, DriverKit loading, or physical
audio validation. The reproducible foundation is the offline core first.
