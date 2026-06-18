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

## HAL/Control Tool Coupling

The local HAL Makefile path is used for locked physical HAL tests. `make hal`
must build both:

- `build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL`
- `build/opena8dj-control`

The control tool reads the HAL stream-stats IPC payload during soundchecks, so
stale control binaries can corrupt the evidence even when the driver itself is
fresh. `build/opena8dj-control` therefore depends on `src/hal/OpenA8DJUSB.m`,
and `scripts/run-cpp-offline-gates` runs
`scripts/check-stream-stats-contract.py` to compare the duplicated
`OpenA8DJStreamStatsPayload` field sequence in HAL and control source.
The stream stats payload also exposes active HAL runtime geometry so physical
evidence can be attributed to the exact candidate configuration:
`logicalIsoFramesPerTransfer`, `captureIsoFramesPerTransfer`,
`playbackBaseIsoFramesPerTransfer`, `playbackIsoFramesPerTransfer`,
`playbackCoalesceTransfers`, `captureQueueDepth`, and `playbackQueueTarget`.
`opena8djcpp_hal_runtime_geometry_observability_contract` guards that HAL
populates those fields and `opena8dj-control` prints them in both human and
machine-readable output.
Runtime submit counters are also part of the physical evidence path:
`captureTransfersSubmitted` and `playbackTransfersSubmitted` must flow from the
HAL payload through `opena8dj-control`, `run-soundcheck` TSV snapshots, and
`scripts/analyze-stream-stats.py`.

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
- `xcodes` CLI is installed and usable (`2.0.2`), and it lists
  `26.5 (17F42) [Apple Silicon]` as the matching full Xcode for this host, but
  `xcodes installed` currently returns zero installed Xcodes.
- `aria2c` is installed, so future `xcodes` downloads can use the faster
  multi-connection helper.
- `/Applications` currently has about `12.641 GiB` free. The preflight uses a
  conservative `80 GiB` minimum before attempting a full Xcode install, because
  the machine must have room for the download, expansion, and rollback margin.
- `opena8djcpp_driverkit_sdk_preflight_gate` records this state in
  `local-analysis/cpp-offline/driverkit-sdk-preflight-gate.json` and blocks any
  real DriverKit/dext build claim while the SDK and selected full Xcode are
  missing.

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

Do not use playback coalescing as a default CPU profile:

```sh
make HAL_PLAYBACK_COALESCE_TRANSFERS=2 hal
```

The isolated coalesce2 run passed HAL safety and reduced driver CPU p95 to
`28.5%`, but it damaged physical music quality (`quality_alignment_score`
`0.898854`, SNR `5.85 dB`). It remains an explicit experiment only.

Do not use output-only no-capture ISO as a default CPU profile:

```sh
make HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1 hal
```

This variant is buildable and useful as a diagnostic, but physical evidence
rejects it as a product optimization. The first locked run submitted no
playback transfers until `writeOutput` was allowed to trigger playback fill in
that mode. After that fix, playback resumed and driver p95 fell to `8.0%`, but
the run still failed badly: `quality_alignment_score=0.183990`, SNR floor
`-21.45 dB`, mid/high residual `17.171794/11.452494`, and coreaudiod p95
`28.3%`.

Do not use ignored HAL output sample time as a default timing profile:

```sh
make HAL_IGNORE_OUTPUT_SAMPLE_TIME=1 hal
```

This variant is buildable and useful only as a diagnostic. It forces HAL output
cycles to write contiguous timeline frames instead of using CoreAudio
`mOutputTime.mSampleTime`, matching the direct USB tool's write model more
closely. The locked physical run still failed quality and CPU:
`quality_alignment_score=0.963508`, SNR floor `10.20 dB`, mid/high residual
`1.440572/1.369361`, `32` lag jumps, driver p95 `22.6%`, and coreaudiod p95
`44.7%`. The default must remain `HAL_IGNORE_OUTPUT_SAMPLE_TIME=0`.

Direct-like queue/prefetch margin can be built for diagnostics:

```sh
make HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64 \
  HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256 hal
```

This variant passed Pair A channel matrix, but failed the locked physical music
and CPU gates: `quality_alignment_score=0.966043`, SNR floor `10.15 dB`,
mid/high residual `1.442529/1.373910`, `25` lag jumps, driver p95 `23.7%`,
and coreaudiod p95 `86.6%`. Do not make q64/prefetch256 the default without
new evidence that improves both real-music quality and total CPU.

Explicit isochronous scheduling is diagnostic-only:

```sh
make HAL_EXPLICIT_SCHED=1 build/opena8dj-usb-play
make HAL_EXPLICIT_SCHED=1 HAL_EXPLICIT_SCHED_FAIL_FALLBACK=1 build/opena8dj-usb-play
```

The fallback flag is intentionally default-off. In locked physical tests it
reduced queue-failure storming (`2805` failures down to `135`) but still failed
physical quality and latency gates. Do not ship or promote explicit scheduling
without new physical evidence that beats the default path.

HAL bundle packaging must install the plist with explicit readable
permissions. A stale build artifact with `Contents/Info.plist` mode `0600`
caused CoreAudio enumeration to miss `org.opena8dj.Audio8DJ` even though USB
saw the Audio 8 DJ. The HAL build rule uses `install -m 644` for the plist to
prevent that regression.

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
.venv/bin/python scripts/analyze-runtime-discontinuities.py --help
.venv/bin/python scripts/analyze-physical-latency.py --help
.venv/bin/python scripts/prepare-latency-marker.py --help
.venv/bin/python scripts/analyze-latency-marker-peaks.py --help
.venv/bin/python scripts/analyze-driver-capture.py --help
.venv/bin/python scripts/analyze-packed-usb-capture.py --help
```

Do not install these packages globally as part of the build. They are analysis
dependencies, not runtime driver dependencies.

## Physical CPU Measurement Notes

`scripts/run-soundcheck` normally polls `opena8dj-control stream-stats` during
playback. That is useful for glitch forensics, but it adds observability load
to CPU profiles. For a lower-perturbation CPU A/B, keep the physical lock and
capture path but disable monitor polling:

```sh
scripts/run-soundcheck --no-monitor-stream-stats --cpu-profile ...
```

Use full `--stream-stats-snapshots` runs when validating glitch counters or
readiness. Monitor-free CPU runs are diagnostic evidence, not sufficient
product readiness by themselves.

## Direct USB Diagnostic Soundcheck

The direct USB playback path can be tested against the same external iRig
capture route without installing a HAL driver:

```sh
make direct-usb-soundcheck SOUNDCHECK_CAPTURE="iRig Stream" SOUNDCHECK_SECONDS=12
```

This target uses `scripts/run-direct-usb-soundcheck`, acquires the global
hardware lock, and does not change default devices, install drivers, reset USB,
or restart CoreAudio. It is diagnostic-only. By default it uses
`build/opena8dj-usb-play`, the direct USB tool built with the current HAL
transport flags. A first Pair A control run with the older
`opena8dj-usb-play-plain-gain05` variant and lead `8192` failed real-music
quality hard (`quality_alignment_score=0.103211`, worst-channel SNR
`-24.31 dB`, mid/high residual `17.114359/16.212469`); that run is useful as a
negative control, not as the final direct-vs-HAL music comparison.

For long-form USB diagnostic evidence, compare the packed USB stream across
the same duration as the music fixture. The default USB scan is intentionally
short for layout discovery; use `--usb-compare-seconds` for product evidence:

```sh
.venv/bin/python scripts/analyze-driver-capture.py reference.wav \
  --usb-raw opena8dj-output-packed-usb.raw \
  --pair A --usb-compare-seconds 12
```

The direct USB tool can force the playback control profile and collect raw USB
diagnostics without installing a HAL:

```sh
AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" \
  scripts/run-direct-usb-soundcheck --capture-device "iRig Stream" \
  --pair A --playback-profile --collect-usb-diagnostics
```

Reset timing experiments are build flags, not validated defaults:

```sh
make HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY=0 \
     HAL_AUDIO_PARAMS_RESET_SETTLE_USEC=250000 \
     build/opena8dj-usb-play
```

As of 2026-06-17, keep `HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY=1` for candidate
builds. No-wait/no-settle failed startup, `100ms` produced no captured energy,
and `250ms`/`500ms` have not shown stable startup plus quality improvement.

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

## DriverKit Extension Scaffold

`driverkit/extension/` contains a non-installing DriverKit dext scaffold:

- `Info.plist.template`
- `OpenA8DJAudioDriver.entitlements.template`
- `OpenA8DJAudioDriver.iig`
- `OpenA8DJAudioDevice.iig`
- `src/OpenA8DJAudioDriver.cpp`
- `src/OpenA8DJAudioDevice.cpp`

The scaffold is intentionally excluded from the default CMake build. The
offline contract is:

```sh
cmake --build build/cpp-offline --target opena8djcpp_driverkit_extension_scaffold_contract
./build/cpp-offline/opena8djcpp_driverkit_extension_scaffold_contract
```

The full offline gate runs it automatically. A future real dext target must be
opt-in, build-dir-only, and unavailable unless the DriverKit SDK and required
entitlements are present. No install, load, unload, reload, signing, or
SystemExtensions command may be part of the default build.
## C++ Loopback Analyzer

The offline build includes `opena8djcpp_loopback_quality_analysis`.

Selftest:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_loopback_quality_analysis
./build/cpp-offline/opena8djcpp_loopback_quality_analysis
```

The full offline gate runs the analyzer automatically and writes:

- `local-analysis/cpp-offline/loopback-quality-analysis.json`
- the `loopback_quality_analysis` summary in
  `local-analysis/cpp-offline/current-offline-gates.json`

The analyzer is offline and dependency-free. It does not open audio devices,
touch USB, install drivers, or activate system extensions.

## C++ Channel Leakage Contract

The offline build includes `opena8djcpp_channel_leakage_tone_contract`.

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_channel_leakage_tone_contract
./build/cpp-offline/opena8djcpp_channel_leakage_tone_contract
```

The full offline gate runs it automatically and writes:

- `local-analysis/cpp-offline/channel-leakage-tone-contract.json`
- the `channel_leakage_tone_contract` summary in
  `local-analysis/cpp-offline/current-offline-gates.json`

This is a digital Mode 2 pack/decode and tone-analysis contract. It does not
replace physical A/B/C/D external-loopback evidence.

## C++ Capture Matrix Quality Analyzer

The offline build includes `opena8djcpp_capture_matrix_quality_analysis`.

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_capture_matrix_quality_analysis
./build/cpp-offline/opena8djcpp_capture_matrix_quality_analysis
```

The full offline gate runs it automatically and writes:

- `local-analysis/cpp-offline/capture-matrix-quality-analysis.json`
- the `capture_matrix_quality_analysis` summary in
  `local-analysis/cpp-offline/current-offline-gates.json`

Capture mode reads stored run directories only:

```sh
./build/cpp-offline/opena8djcpp_capture_matrix_quality_analysis /path/to/run-dir
```

It expects `fixture/reference.wav` and `captured.wav` in each run directory.
It does not open audio devices, touch USB, install drivers, or activate system
extensions.

## HAL Hot-Path Timing Diagnostic Build

Use only to prepare a locked physical CPU-attribution window:

```sh
make hal-hotpath-diagnostic
```

This builds `build/OpenA8DJ.driver` with:

- `HAL_HOT_PATH_TIMING=1`
- `HAL_HOT_STREAM_STATS_INTERVAL=1`
- `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=1`
- `HAL_TRANSFER_LEDGER=0`
- `HAL_PLAYBACK_PAYLOAD_GUARD=0`
- `HAL_CADENCE_DIAGNOSTIC=0`

It does not install, load, unload, activate, or touch hardware by itself.

After building a diagnostic HAL, restore the default local bundle before any
non-diagnostic packaging or install step:

```sh
make -B hal
```

## C++ Hot-Path Timing Analyzer

The offline build includes `opena8djcpp_hot_path_timing_analysis`.

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_hot_path_timing_analysis
./build/cpp-offline/opena8djcpp_hot_path_timing_analysis
```

The tool reads stored `stream-stats-summary.json` files under
`local-analysis/hot-path-timing/`. It does not open audio devices, touch USB,
install drivers, or activate system extensions.

## C++ Quality Root-Cause Analyzer

The offline build includes `opena8djcpp_quality_root_cause_analysis`.

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_quality_root_cause_analysis
./build/cpp-offline/opena8djcpp_quality_root_cause_analysis
```

The tool reads existing evidence only and reports whether promotion is blocked
by route health, analog quality, lag/timebase, or fixed transport CPU evidence.
It does not open audio devices, touch USB, install drivers, or activate system
extensions.

## C++ Timecode Readiness Gate

The offline build includes `opena8djcpp_timecode_readiness_gate`.

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_timecode_readiness_gate
./build/cpp-offline/opena8djcpp_timecode_readiness_gate
```

The tool aggregates stored offline timecode/DVS evidence and reports whether
physical Traktor/timecode-vinyl validation is still blocked. It does not open
audio devices, touch USB, launch Traktor, install drivers, or activate system
extensions.

The JSON includes explicit `offline_coverage` and
`physical_requirements_remaining` fields so a passing offline DVS gate cannot
be mistaken for physical Traktor/timecode readiness.

## Known-Good Route Soundcheck Harness

The physical route isolation wrapper is:

```sh
scripts/run-known-good-route-soundcheck --help
```

It is intentionally hardware-lock gated and requires an explicit non-Audio8
output device plus an explicit capture device:

```sh
scripts/run-known-good-route-soundcheck \
  --output-device "<known-good output device name>" \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --reference-wav /absolute/path/to/reference.wav \
  --seconds 12 \
  --run-dir local-analysis/known-good-route/<timestamp>-known-good-irig
```

The wrapper builds `build/audio-wav-play`, `build/audio-record`, and the native
C++ soundcheck analyzer, starts the capture, plays the reference WAV through
the selected output, and then writes both analyzer outputs:

- `metrics.json` from `scripts/analyze-soundcheck-capture.py`;
- `native-quality.json` from `opena8djcpp_soundcheck_wav_quality`.

`audio-wav-play` now supports `--device` and `--device-uid` for diagnostics.
When one of those options is used, it does not set device sample rate by
default and it requires the WAV sample rate to match the selected device. This
prevents a route check from silently changing system audio state.

Do not use this wrapper as a product claim. It is a route-health prerequisite
for later Audio 8 DJ and mainline/C++ same-session comparisons.

The selected known-good output must be a wired source into the same capture
chain. Built-in speakers / acoustic paths such as `MacBook Air Speakers` are
rejected by default. `--allow-built-in-output-acoustic-diagnostic` exists only
for diagnostics and cannot create promotion evidence.

## Physical Superiority Window Runner

The full physical evidence window is scripted but dry-run by default:

```sh
scripts/run-physical-superiority-window \
  --run-dir local-analysis/physical-superiority-window/<timestamp> \
  --capture-device "iRig Stream" \
  --known-good-output-device "<non-Audio8 output>" \
  --reference-wav /absolute/path/to/reference.wav
```

That command writes a plan only. It does not play, record, install, unload,
restart CoreAudio, or touch USB.

Actual execution requires `--execute`, an explicit read-only mainline HAL
bundle, and an explicit C++ HAL candidate:

```sh
scripts/run-physical-superiority-window \
  --execute \
  --mainline-candidate /absolute/path/to/mainline/OpenA8DJ.driver \
  --candidate build/OpenA8DJ.driver \
  --known-good-output-device "<non-Audio8 output>" \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --reference-wav /absolute/path/to/reference.wav \
  --music-file /absolute/path/to/music.wav \
  --seconds 12 \
  --run-dir local-analysis/physical-superiority-window/<timestamp>
```

This is HAL-candidate physical validation only. It is not DriverKit/dext
activation. Before acquiring the hardware lock, the runner executes
`scripts/physical-window-preflight`, a read-only check for iRig/CoreAudio
capture visibility, Audio 8 DJ USB visibility, candidate bundles, fixture
files, explicit non-Audio8 output visibility, and lock availability. The
runner then acquires the global hardware lock, runs the known-good route check
first, then installs/reloads the explicit mainline HAL candidate,
runs a mainline Audio 8 soundcheck, unloads it, installs/reloads the explicit
C++ HAL candidate, runs the C++ Audio 8 soundcheck, analyzes both WAV captures
with the native C++ analyzer, compares same-session mainline vs C++ physical
metrics, runs promotion-readiness evaluation against the C++ evidence, and
cleans up by unloading the active candidate unless `--leave-loaded` is supplied.

`--candidate-only` exists only for diagnostics. It writes a blocked
same-session comparison result and cannot support any branch-promotion or
readiness claim.

`--skip-known-good` also exists only for diagnostics. It skips route
revalidation, writes `known_good_rc=1`, and prevents a successful runner exit.
The promotion evaluator must be called with the same window's
`same-session-physical-compare.json`; evidence from separate windows is
rejected for branch promotion.

Preflight PASS means the requested devices and files are visible. It does not
prove the physical route; the known-good route capture must still pass.

## Privileged Driver Sampling During Soundcheck

For CPU attribution diagnostics, `scripts/run-soundcheck` can sample the active
Core Audio driver helper while playback is running:

```sh
scripts/run-soundcheck \
  --skip-build \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --pair A \
  --seconds 12 \
  --sample-driver-process \
  --sample-driver-delay 1 \
  --sample-driver-seconds 7 \
  --run-dir local-analysis/soundcheck/<timestamp>
```

This requires `sudo -n sample` to be available. Output is written to
`driver-sample/status.json`, `driver-sample/opena8dj-driver.sample.txt`, and
`driver-sample/analysis.json`. This is diagnostic evidence only; it does not
replace physical quality gates or same-session mainline comparison.

## DriverKit SDK Availability

Current local status:

```sh
xcrun --sdk driverkit --show-sdk-path
```

Result on this machine:

- DriverKit SDK is not available.
- `xcode-select -p` points at `/Library/Developer/CommandLineTools`.
- No `/Applications/Xcode*.app` is currently visible.

Implication:

- The C++ core, HAL candidate, offline gates, and DriverKit model contracts can
  build here.
- A real DriverKit/System Extension/dext target cannot be compiled here until
  full Xcode with DriverKit SDK, or an equivalent configured build host, is
  available.
- Do not substitute a macOS full installer for this dependency; the missing
  component is the DriverKit SDK/toolchain.

## Local Audiophile Analysis Environment

High-precision offline WAV analysis uses a local virtualenv inside this
worktree. It is ignored by git and does not modify the system Python:

```sh
scripts/setup-analysis-env
```

Pinned dependencies live in `requirements-analysis.txt`:

- `numpy`
- `scipy`
- `soundfile`

The gate runner uses `.venv-analysis/bin/python` by default. To use another
interpreter, set:

```sh
ANALYSIS_PY=/path/to/python scripts/run-cpp-offline-gates
```

Self-test:

```sh
. .venv-analysis/bin/activate
scripts/analyze-audiophile-wav.py \
  --self-test \
  --json-out local-analysis/cpp-offline/audiophile-wav-analysis-self-test.json
```

Compiled C++ cross-check:

```sh
cmake -S . -B build/cpp-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpp-release --target opena8djcpp_audiophile_wav_analysis
./build/cpp-release/opena8djcpp_audiophile_wav_analysis \
  --self-test \
  --json-out local-analysis/cpp-offline/audiophile-wav-analysis-cpp-self-test.json
```

Analyze an existing physical run without touching hardware:

```sh
. .venv-analysis/bin/activate
scripts/analyze-audiophile-wav.py \
  --reference local-analysis/physical-superiority-window/<run>/<leg>/fixture/reference.wav \
  --capture local-analysis/physical-superiority-window/<run>/<leg>/captured.wav \
  --seconds 12 \
  --json-out local-analysis/physical-superiority-window/<run>/<leg>/audiophile-wav-analysis.json
```

The C++ analyzer accepts the same `--reference`, `--capture`,
`--reference-channels`, `--capture-channels`, `--seconds`, and threshold
arguments. Both tools are read-only with respect to audio hardware. They only
read WAV files and write JSON evidence.

The physical superiority window script runs both analyzers automatically for
each Audio 8 soundcheck leg when `fixture/reference.wav` and `captured.wav`
exist:

- `audiophile-wav-analysis-cpp.json`
- `audiophile-wav-analysis.json`

The offline gate runner also executes a compiled analysis-stack contract:

```sh
./build/cpp-release/opena8djcpp_audiophile_analysis_stack_contract
```

This is a static fail-closed check. It verifies that the C++ analyzer, the
Python/SciPy oracle, physical-window execution, same-session comparison, and
product-claim gate all require dual analyzer evidence. PASS here is not a
quality claim; it only proves the measurement stack is wired so missing analyzer
evidence cannot be ignored.

The first C++ migration of the LTI transfer-quality analysis is:

```sh
cmake --build build/cpp-release --target opena8djcpp_lti_transfer_quality
./build/cpp-release/opena8djcpp_lti_transfer_quality --self-test
```

It emits `opena8djcpp.lti-transfer-quality-cpp.v1` JSON with the same core
fields used by the Python/SciPy LTI evidence: scalar and LTI residual ratios,
coherence by band, transfer magnitude by band, and LTI SNR delta. It is not yet
a replacement for Python in product claims; the next required step is a parity
gate against saved physical evidence.

The parity guard is:

```sh
./build/cpp-release/opena8djcpp_lti_transfer_quality_parity_gate
```

It reruns the C++ LTI analyzer on saved same-session physical evidence and
compares it against the existing Python/SciPy JSON. The current intended result
is `PASS` with `lti_parity_pass=true`; that means the native C++ LTI analyzer is
numerically close enough to the Python/SciPy oracle for the saved evidence used
by this parity gate. Product quality and branch-promotion claims remain blocked
by the physical route, same-session A/B, runtime CPU, and timecode vinyl gates.

## Opt-In Capture USB Batching Candidate

Default HAL build:

```sh
make -B hal
```

Opt-in capture-batched diagnostic build:

```sh
make -B hal \
  HAL_ISO_FRAMES=8 \
  HAL_CAPTURE_ISO_FRAMES=64 \
  HAL_CAPTURE_QUEUE=8 \
  HAL_PLAYBACK_ISO_FRAMES=8
```

This build keeps the logical playback/audio cadence at ISO8 while requesting
larger physical capture transfers. It is intended to test lower capture enqueue
cadence without making global ISO64 the default.

Guard:

```sh
./build/cpp-release/opena8djcpp_hal_logical_capture_batching_contract
```

Passing this guard only proves the source/build contract. It does not prove
sound quality, CPU superiority, Traktor/timecode behavior, routing safety, or
branch-promotion readiness. Any physical run still requires the global hardware
lock and same-window route revalidation plus mainline/C++ comparison.
