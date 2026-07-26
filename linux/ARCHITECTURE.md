# Linux Architecture

This document describes the target Linux design for OpenA8DJ. It is not a claim
that the Linux driver exists yet.

```text
diagnostic only, sound quality not validated
```

## Target Stack

```text
ALSA / PipeWire / JACK / DAW / Traktor
        |
        v
ALSA card: OpenA8DJ / Audio 8 DJ
        |
        +-- PCM playback device: 8 channels, A/B/C/D stereo pairs
        +-- PCM capture device: 8 channels, A/B/C/D stereo pairs
        +-- ALSA controls: mode, ground lift, software lock, profiles, diagnostics
        +-- ALSA rawmidi: duplex MIDI bridge
        |
        v
USB CAIAQ transport
        |
        +-- EP1 command/control/MIDI
        +-- isochronous capture endpoint 0x82
        +-- isochronous playback endpoint 0x06
```

The product ID is expected to remain Native Instruments Audio 8 DJ
`17cc:1978`. The Linux work must confirm descriptors on real hardware before
any live driver decision.

## Driver Strategy

The first implementation decision is deliberately still open:

- Extend or specialize upstream `snd-usb-caiaq` if it can meet the routing,
  timing, controls, observability, and quality requirements without creating a
  fragile fork.
- Create a focused `snd-opena8dj` module only if the CAIAQ architecture cannot
  honestly provide full-duplex Audio 8 DJ quality, channel maps, controls, and
  diagnostics.

No decision is final until the `snd-usb-caiaq` audit is complete against the
exact kernel baseline selected for development.

## Cross-Platform Hardware Lessons

Linux now carries a formal bridge document:

```text
linux/MACOS_WINDOWS_HARDWARE_LESSONS.md
```

That document converts the advanced macOS implementation and the Windows
transition contract into Linux requirements. The important imported decisions
are:

- Treat Audio 8 DJ as vendor-specific CAIAQ hardware, not USB Audio Class.
- Preserve endpoint facts: EP1 bulk/control/MIDI traffic, capture isochronous
  endpoint `0x82`, and playback isochronous endpoint `0x06`.
- Keep playback, capture, MIDI, and control paths separate.
- Prefer the CAIAQ capture-paced cadence model first, because macOS and Windows
  analysis both point to packet cadence as a real sound-quality surface.
- Keep the ALSA/client-facing clock stable; USB timing may inform correction
  but must not become a jittery public timeline.
- Keep 44.1 kHz and 48 kHz as first-class validation rates. 88.2 and 96 kHz
  remain planned/diagnostic until physical capture passes.
- Do not claim readiness from install success, clean counters, or enumeration.

## PCM Surface

The target ALSA surface is one 8-channel playback PCM and one 8-channel capture
PCM, with stable channel labels:

| Pair | Channels | Playback name | Capture name |
| --- | ---: | --- | --- |
| A | 1-2 | Output A L/R | Input A L/R |
| B | 3-4 | Output B L/R | Input B L/R |
| C | 5-6 | Output C L/R | Input C L/R |
| D | 7-8 | Output D L/R | Input D L/R |

If ALSA channel-map controls are available for the chosen kernel baseline, the
driver should expose an explicit A/B/C/D map rather than relying only on card
names.

## Audio Formats and Rates

Initial target:

- 24-bit packed CAIAQ transport preserved without lossy conversion.
- 44.1 kHz and 48 kHz first.
- 88.2 kHz and 96 kHz blocked from release claims until physical validation.

The driver must reject unsupported combinations clearly. It must not silently
resample, silently drop channels, or expose rates that have not passed the
validation ladder.

## Full-Duplex Timing Model

The hard problem is not enumeration; it is honest low-latency full-duplex
timing. The implementation must define and test:

- Packet cadence for capture and playback.
- Period accounting per stream.
- Pointer behavior under normal operation and packet anomalies.
- Delay reporting, including queued URBs and pending frames.
- XRUN reporting without suppression.
- Start/stop ordering that does not create stale output or input drift.

`snd-usb-caiaq` currently mirrors playback packet layout from completed capture
URBs. That behavior is important because it may preserve hardware cadence, but
it also means callback duration, output URB availability, panic handling, and
capture starvation are first-order risks.

## Real-Time Rules

The hot path must avoid:

- Heap allocation.
- Logging.
- String formatting.
- Wide locks.
- Variable scans proportional to user-visible state.
- Control/UI work.

Allowed hot-path work is bounded packet parsing, ring copy/pack/unpack,
period-count updates, and URB resubmission using preallocated objects.

The design should use:

- Fixed URB pools.
- Fixed DMA/ring buffers.
- Precomputed stream/channel layout.
- Atomic or small-spinlock state transitions.
- Deferred diagnostics for non-critical reporting.

## Controls

The Audio 8 DJ Linux surface must include ALSA controls for:

- Input mode: timecode vinyl, timecode CD/line, phono.
- Ground lift for vinyl mode.
- Ground lift for CD/line mode.
- Ground lift for phono mode.
- Software lock.
- Profile selection.
- Diagnostics counters and last-error state where appropriate.

Control changes must be serialized away from the isochronous hot path.

Profile semantics must match the macOS/Windows contract:

- DVS vinyl sets timecode-vinyl mode, vinyl ground lift on, other ground lifts
  off, and software lock on.
- DVS CD/line sets timecode-CD-line mode, CD/line ground lift on, other ground
  lifts off, and software lock on.
- Phono/vinyl recording sets phono mode, phono ground lift on, other ground
  lifts off, and software lock on.
- Playback, DAW, MIDI, C/D recording, effects-loop, and microphone workflows do
  not hide A/B control writes.

## MIDI

The target rawmidi surface is duplex and visible through:

- `aconnect -l`
- PipeWire ALSA MIDI compatibility
- JACK MIDI bridge setups
- DAWs that use ALSA rawmidi

MIDI EP1 command traffic must not interfere with PCM timing. Output batching and
input dispatch need bounded behavior.

## Power and Lifecycle

The driver must handle:

- Hotplug and unplug during idle.
- Hot unplug during active playback/capture.
- Suspend/resume.
- Client close while streams are active.
- PipeWire/JACK server restarts.
- Failed control command replies.

Any hardware recovery that resets USB or reloads a live driver requires the
shared hardware lock.

## Observability

The driver should expose enough data to debug quality without polluting the
real-time path:

- Packet status counters.
- URB submit failures.
- Input/output panic counters.
- XRUN counters.
- Last packet length anomalies.
- Rate/depth/bpp configuration.
- Active profile.
- Last control command error.

Counters must be honest. They are diagnostics, not a substitute for physical
capture validation.
