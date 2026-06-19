# Platform Support

OpenA8DJ is currently centered on the macOS C++ driver line.

## macOS

Status: current canonical platform.

The macOS package contains:

- Core Audio HAL driver for `Open Audio 8 DJ`
- IOUSBHost transport for the Audio 8 DJ
- CoreMIDI bridge
- `opena8dj-control`
- optional OpenA8DJ Control Center/tools package
- packaging for DMG/PKG distribution

Primary validated rates:

```text
44.1 kHz
48 kHz
```

Primary workflows:

- 8 output channels / A-B-C-D deck routing
- 8 input channels / A-B-C-D input routing
- Traktor Timecode Vinyl
- MIDI I/O
- profile control and diagnostics

## Windows

Status: experimental and unvalidated.

Windows work should live in Windows-specific branches and paths. It must not
change the macOS HAL, packaging, DriverKit scaffolding, or release process
unless a change is explicitly platform-neutral and tested on macOS.

Community feedback is welcome, especially:

- whether the Audio 8 DJ enumerates correctly;
- which Windows driver model is practical;
- routing and latency observations;
- logs from failed installs or failed enumeration.

## Linux

Status: experimental and unvalidated.

Linux work should live in Linux-specific branches and paths. The historical
Linux CAIAQ / `snd-usb-caiaq` work remains valuable background knowledge, but
the current `main` branch is not a Linux driver.

Community feedback is welcome, especially:

- ALSA/JACK/PipeWire routing observations;
- whether current kernels already expose useful Audio 8 DJ behavior;
- gaps between Linux behavior and the macOS C++ implementation;
- reproducible logs and hardware details.

## Rust Lab

Status: experimental lab/oracle.

Rust work is kept separate from the macOS runtime. It can contribute tests,
analyzers, benchmarks, or design ideas, but code should not enter the macOS
driver path unless it wins on measurable quality, stability, CPU/resource use,
latency, build reliability, and debugging cost.

## Legacy C

Status: historical reference.

The older C/Objective-C implementation is preserved on the `legacy` branch for
comparison and recovery. It should not be merged into `main` wholesale.
