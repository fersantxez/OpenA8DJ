# `snd-usb-caiaq` Audit

This is the first Linux audit pass for deciding whether OpenA8DJ should extend
upstream `snd-usb-caiaq` or create a focused `snd-opena8dj` module.

```text
diagnostic only, sound quality not validated
```

## Sources Reviewed

Primary source reviewed on 2026-06-19:

- Linux upstream `sound/usb/caiaq/audio.c`
- Linux upstream `sound/usb/caiaq/device.c`
- Linux upstream `sound/usb/caiaq/device.h`
- Linux upstream `sound/usb/caiaq/control.c`
- Linux upstream `sound/usb/caiaq/midi.c`

Exact kernel baseline for development is not selected yet. This audit must be
re-run against the chosen kernel tree before code is written.

## What CAIAQ Already Gives Us

Strong signals:

- Audio 8 DJ is explicitly matched as Native Instruments `17cc:1978`.
- CAIAQ device spec includes analog audio in/out, MIDI in/out, and data
  alignment.
- EP1 commands cover device info, audio params, MIDI read/write, read/write IO,
  and auto messages.
- Existing audio path allocates fixed URB pools and uses isochronous capture
  and playback.
- Existing PCM callbacks cover open, close, prepare, trigger, pointer, and
  period elapsed accounting.
- Existing rawmidi support creates duplex ALSA rawmidi devices when firmware
  reports MIDI ports.
- Existing Audio 8 DJ controls include current input mode, ground lift for
  timecode vinyl, ground lift for timecode CD/line, ground lift for phono, and
  software lock.

These are good reasons to treat CAIAQ as the first implementation candidate.

## Risks Found

### PCM Shape

The upstream CAIAQ hardware template is stereo per substream, with multiple
streams represented internally. The OpenA8DJ target wants a clear 8-channel
playback and 8-channel capture surface with A/B/C/D channel maps. The audit must
confirm whether the existing multi-stream representation gives user-space the
right shape for PipeWire, JACK, Traktor, and DAWs.

Risk: user-space sees four stereo PCMs when the product goal requires one
stable multichannel surface, or vice versa.

### Full-Duplex Cadence

The CAIAQ audio callback reads capture packets and creates playback packets with
the same packet layout. This may be the right hardware cadence model, but it
makes capture URB completion central to playback progress.

Risk: capture starvation, input packet anomalies, or output URB exhaustion can
degrade playback timing.

### Pointer and Delay Honesty

The current pointer path uses buffer positions and returns XRUN on panic flags.
The Linux track still needs to prove:

- pointer monotonicity under all supported period sizes.
- delay reporting, if implemented or expected by modern user-space.
- behavior under PipeWire/JACK low-latency scheduling.
- whether panic flags are too coarse for diagnostics.

Risk: clients receive plausible but incomplete timing information.

### Error Semantics

CAIAQ has input and output panic state and warns once. OpenA8DJ must not hide
packet errors, URB failures, XRUNs, or output underruns.

Risk: a one-time warning is not enough for serious diagnostics.

### Hot Path Logging

The source includes warning paths from packet validation and URB exhaustion.
Warnings are useful, but any steady-state packet issue must be rate-limited or
converted to counters outside the hot path.

Risk: repeated error logging worsens an already unstable stream.

### Controls Discoverability

The Audio 8 DJ controls exist, but names and profile behavior are not enough for
the OpenA8DJ user-facing model.

Risk: technically present ALSA controls are hard to discover or script for DVS
and recording workflows.

### MIDI Coupling

MIDI uses EP1 command traffic and rawmidi callbacks. It must not create timing
interference with PCM under Traktor/DVS use.

Risk: MIDI bursts or command traffic increase packet jitter or control latency.

### Suspend, Resume, and Hotplug

The first pass did not complete lifecycle analysis.

Risk: active-stream unplug, suspend/resume, or server restart leaves stale
state, stuck URBs, or misleading ALSA device state.

## Extend vs New Module Criteria

Prefer extending/specializing `snd-usb-caiaq` if all are true:

- Audio 8 DJ can expose the required 8 playback and 8 capture channels with
  stable A/B/C/D naming or channel maps.
- Full-duplex timing remains honest at 44.1 and 48 kHz.
- Existing fixed URB model can meet low CPU and low latency requirements.
- Controls can be presented clearly without breaking other CAIAQ devices.
- Diagnostics can be added without polluting the hot path.
- The change can be scoped to Audio 8 DJ quirks where needed.

Prefer a new `snd-opena8dj` module if any are true:

- CAIAQ's PCM model cannot provide the required user-space surface.
- Timing or delay accounting requires incompatible structural changes.
- Audio 8 DJ-specific controls/profiles would make CAIAQ worse for other
  devices.
- Quality diagnostics require an architecture CAIAQ cannot support cleanly.
- Upstream compatibility constraints block necessary Audio 8 DJ behavior.

Current provisional position: CAIAQ is the first candidate to extend, but no
driver choice is final.

## macOS/Windows Lessons Applied To CAIAQ

The current Linux path is not allowed to treat CAIAQ support as solved merely
because Audio 8 DJ already probes. macOS and Windows work add these
requirements to any CAIAQ extension:

- Keep the capture-paced cadence model as the first hypothesis. CAIAQ already
  mirrors playback packet layout from completed capture URBs, which matches the
  strongest macOS timing clue.
- Add diagnostics around that cadence before changing behavior: capture
  completion deltas, playback completion deltas, capture-to-playback queue
  deltas, short packets, failed transactions, zero-byte/other-size packet
  anomalies, output starvation, and panic causes.
- Preserve ALSA XRUN truth. Diagnostics must explain failures, not suppress
  them.
- Do not add logging, allocation, string formatting, profile handling, or
  user-space policy to isochronous completion paths.
- Keep MIDI/EP1 command traffic and ALSA control writes serialized away from
  playback/capture completion timing.
- Validate repeated start/unpause/client-restart behavior because CAIAQ has a
  historical white-noise start/unpause bug class.
- Validate A/B/C/D pair order before DVS claims because old Audio 8 DJ Linux
  reports included channel-mapping problems that directly affect timecode.

The profile/control layer is now documented in
`linux/MACOS_WINDOWS_HARDWARE_LESSONS.md` and packaged in
`linux/packaging/common/profile-schema.json`.

## Audit Checklist Still Open

- Select exact kernel baseline.
- Inspect ALSA PCM device shape on a Linux host with Audio 8 DJ attached.
- Trace `snd_usb_caiaq_audio_init` and PCM registration details.
- Confirm stream count for Audio 8 DJ from device spec.
- Confirm sample format exposed to user-space.
- Confirm 88.2 kHz handling before exposing it.
- Confirm whether `delay` ops are absent, unnecessary, or required.
- Confirm channel-map feasibility for A/B/C/D.
- Confirm rawmidi port count and names.
- Confirm control element names and indexes through `amixer`.
- Confirm suspend/resume callbacks and disconnect cleanup.
- Confirm no allocation or unbounded work in completion paths.

Hardware-facing checklist items require the shared hardware lock and are not
part of this first package.

## 2026-06-22 Source-Level Findings

Baselines are now tracked in `BASELINES.md`. The first source pass used
kernel.org mainline source and must still be repeated against the exact selected
stable/LTS baseline before kernel code is written.

### Device Match And License

Upstream `device.c` identifies `snd-usb-caiaq` as GPL kernel code and matches
Native Instruments Audio 8 DJ through `USB_PID_AUDIO8DJ`.

Implications:

- Linux kernel-derived work must keep GPL provenance explicit.
- Audio 8 DJ support already exists in the upstream driver; the first serious
  path is extension/audit, not a from-scratch replacement.

### PCM Shape

`audio.c` defines:

- `CHANNELS_PER_STREAM` as `2`.
- `SNDRV_PCM_FMTBIT_S24_3BE` as the PCM format.
- `channels_min` and `channels_max` as `CHANNELS_PER_STREAM`.

`snd_usb_caiaq_audio_init()` computes `n_audio_in` and `n_audio_out` by dividing
the firmware-reported channel counts by `CHANNELS_PER_STREAM`, then passes
those values to `snd_pcm_new()` as the number of playback and capture
substreams.

Conclusion:

- The native CAIAQ model is stereo-pair substreams.
- Audio 8 DJ parity should be stated as "8 physical inputs and 8 physical
  outputs with stable A/B/C/D routing".
- A single ALSA 8-channel presentation is a user-space/profile goal until a
  client requirement and safe implementation path are proven.

### Rates And Format Exposure

The generic CAIAQ hardware template exposes 44.1, 48, and 96 kHz. The Audio 8 DJ
case also adds 88.2 kHz through the device-id switch.

OpenA8DJ Linux policy:

- Treat 44.1 and 48 kHz as the first validation target.
- Do not claim 88.2 or 96 kHz quality until physical capture passes.
- Do not silently resample or hide rate limitations in user-facing docs.

### Isochronous Cadence

The current CAIAQ hot path:

- starts 32 capture URBs;
- uses 8 iso frames per URB;
- mirrors playback packet layout from completed capture URBs;
- fills output and reads input while holding the driver spinlock;
- re-submits inbound URBs from completion context;
- submits outbound URBs from completion context.

This closely matches the important macOS lesson: output cadence and input packet
layout are coupled. It is a useful starting point, but it makes capture
starvation, short packets, output URB starvation, and callback duration
first-order risks.

Required implementation work:

- Add counters around input packet status, short packets, output URB starvation,
  submit failures, panic flags, and period elapsed events.
- Avoid steady-state hot-path logging.
- Avoid heap allocation in completion callbacks.
- Preserve XRUN truth instead of masking it with diagnostics.

### Pointer And XRUN Behavior

The pointer callback returns `SNDRV_PCM_POS_XRUN` when input or output panic is
set. That is a good hard-failure signal, but it is too coarse for OpenA8DJ
diagnostics.

Required implementation work:

- Keep XRUN behavior honest.
- Add separate counters for the causes leading to panic.
- Prove pointer monotonicity under PipeWire/JACK and direct ALSA clients.

### Controls

`control.c` defines Audio 8 DJ controls for:

- current input mode;
- GND lift for TC Vinyl mode;
- GND lift for TC CD/Line mode;
- GND lift for phono mode;
- software lock.

Implementation policy:

- ALSA controls are kernel truth.
- Friendly names, profiles, persistence, and scripted workflows belong in
  `opena8dj-linuxctl`, ALSA UCM, or PipeWire/JACK policy.
- Hardware-affecting control writes require explicit user action and validation
  discipline; package install must never apply profiles.

### MIDI

`midi.c` creates duplex rawmidi devices from firmware-reported MIDI port counts
and sends MIDI through EP1 command traffic.

Risks:

- MIDI shares the command/control path.
- MIDI bursts must not degrade PCM timing.

Required tests:

- `aconnect -l`
- MIDI input/output loopback.
- MIDI burst while PCM full-duplex is active.

### Implementation Decision

The selected first path remains:

```text
upstream-style snd-usb-caiaq extension
```

Do not create `snd-opena8dj` until one of these becomes true:

- CAIAQ cannot expose stable Audio 8 DJ routing.
- CAIAQ timing/pointer/delay behavior cannot be made honest.
- CAIAQ cannot support required diagnostics without harming other devices.
- Upstream review clearly rejects Audio 8 DJ-specific work inside CAIAQ.

## 2026-06-22 Implemented Scaffold

This pass added the first Linux implementation scaffolding outside kernel code:

- `linux/IMPLEMENTATION_PLAN.md`
- `linux/BASELINES.md`
- `linux/LEGAL_AND_PROVENANCE.md`
- `linux/ENUMERATION_PLAN.md`
- `linux/Makefile`
- `linux/tools/opena8dj-linuxctl`
- `linux/tools/README.md`

The tool is read-only. It does not load modules, change controls, reset USB,
play audio, record audio, or restart services.
