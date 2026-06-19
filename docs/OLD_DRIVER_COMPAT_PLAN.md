# OpenA8DJ old-driver compatibility plan

This plan captures the safe implementation path for adapting the useful
behavior observed in the legacy OpenA8DJ/caiaq-style driver without copying its
identity, bundle identifiers, signing, or branding.

## Verified diagnosis

- The legacy binary is an x86_64 kernel extension using IOAudioFamily and
  IOUSBFamily. OpenA8DJ is a modern user-space HAL driver, so it must recreate
  the important timing behavior explicitly.
- The legacy driver used a stable USB-frame clock model: sample the USB frame
  around a host timestamp, smooth USB-frame duration with a strong jitter
  filter, periodically refresh the anchor, and publish monotonic audio
  timestamps.
- The legacy macOS kext kept deep isochronous input and output preparation. Its
  `readHandler` selects a free output slot from the completed input transaction,
  copies the successful input transaction byte counts into the output request,
  and then queues output from there. The practical rule is that output must be
  paced by completed USB input, not by an unrelated host-side loop.
- The legacy macOS kext starts streaming with a reset-style `setAudioParams`
  call, clears/pattern-fills the USB byte ring, queues 64 input IRPs, and keeps
  128 output slots available.
- Do not apply the legacy mode 2 input cursor (`0xba08 = 2`) as the playback
  byte start. That was tested in 0.2.11 and produced loud white noise. Playback
  must keep the previous output byte start of 4.
- On this macOS HAL path, explicit future-frame scheduling was less stable than
  capture-paced immediate output. It caused output underproduction and clock
  drift even when Core Audio itself stayed healthy.

## Safety rules

- Keep the OpenA8DJ identity: bundle id, signing name, user-visible name, and
  docs must remain OpenA8DJ/Open Audio 8 DJ.
- Do not retry the previous forced USB-clock GetZeroTimeStamp experiment that
  caused Core Audio CPU instability.
- Make the new path observable before trusting audio listening tests.
- Preserve a fallback path, but count it and keep it visible. A silent fallback
  is not an acceptable success condition.
- Install only after build and smoke tests pass.
- After install, verify the actual loaded bundle, code signature, version, and
  live stream stats before asking for human listening.

## Active research direction

Treat the CAIAQ/Linux USB-audio research as a live, high-value direction, but
use it as a cadence and URB-lifecycle model rather than as a direct port. The
safe next step is instrumentation, not another playback rewrite.

OpenA8DJ remains macOS-first: Core Audio HAL, AudioServerPlugIn, IOUSBHost,
coreaudiod stability, device enumeration, buffer size, `GetZeroTimeStamp`, and
physical output quality outrank Linux parity.

- First prove whether the current driver is matching device cadence on macOS:
  record IOUSBHost completion deltas, IN-completion to OUT-queue delta, OUT
  completion delta, packet lengths/offsets, active output identity, in-flight
  depth, zero-length microframes, queue misses, output read rate, and
  coreaudiod/driver CPU under window stress.
- Compare those traces against physical iRig metrics, especially 1 kHz
  sidebands and music mid-band residuals. Internal stream counters alone have
  already produced false positives.
- If the evidence points at OUT cadence or request lifecycle, then implement
  a macOS capture-paced experiment with preallocated transfer state and output
  packet layout derived from real IOUSBHost observations. Core Audio input
  streams may stay hidden; USB IN is only an internal cadence signal.
- Keep HAL `GetZeroTimeStamp` on a stable Core Audio timeline. Any device-clock
  following belongs inside the USB transport/output-buffer model, not in a
  forced HAL timestamp swap.
- Do not assume ALSA implicit feedback, URBs, `period_elapsed`, or the Linux
  kernel scheduling model have direct AudioServerPlugIn/IOUSBHost equivalents.
- The decisive gate is stable Core Audio, no `audio-list` hang, normal
  coreaudiod idle CPU, clean physical 1 kHz iRig sidebands, acceptable physical
  music SNR/residuals, and only then human listening.

## Implementation phases

1. Add stable USB-frame clock sampling.
   - Sample USB frame and host time using a double-read guard.
   - Maintain a smoothed host-ticks-per-USB-frame value.
   - Expose USB frame, smoothed frame duration, and resync counters in stats.

2. Pace playback from capture completions.
   - Queue capture transfers continuously.
   - For every successful capture transaction, queue a matching output
     transaction with the same payload length.
   - Use the legacy macOS queue sizing as the safe target: 64 capture transfers
     and 128 maximum playback transfers.
   - Keep output immediate on macOS by leaving explicit future-frame scheduling
     disabled in the HAL build.
   - Preserve queue-depth and scheduling counters so a future scheduling
     experiment is visible rather than silently enabled.

3. Add valid playback preroll and elastic output buffering.
   - Feed startup silence separately from active underruns.
   - Keep the Core Audio render buffer and the USB output engine decoupled with
     a bounded elastic ring.
   - Count elastic drops/replays separately from active underruns.

4. Improve underrun accounting.
   - Count active playback underruns separately from controlled startup silence.
   - Keep startup silence valid and deterministic.
   - Avoid treating idle silence as proof of playback failure.

5. Port old macOS stream-start details.
   - Send the reset-style `AUDIO_PARAMS` command before the real stream
     parameters.
   - Keep mode 2 playback packing at byte offset 4; byte offset 2 belongs to
     the legacy input/read cursor, not output payload packing.
   - Keep the mode 2 check-byte pattern generated from the global USB byte
     position.

6. Verify locally.
   - Build all targets and run HAL smoke tests.
   - Install the signed HAL bundle and tools.
   - Restart Core Audio.
   - Confirm the installed binary hash and signature.
   - Confirm live stats show capture/playback transaction and byte counts
     moving together, active underruns at zero, elastic drops at zero, and no
     repeated USB scheduling failures.
   - Run automated playback/capture tests before handing over for listening.

## Success criteria

- The loaded driver is the newly built OpenA8DJ HAL bundle.
- Stream stats show capture-paced playback, with capture/playback bytes closely
  matched.
- Explicit playback scheduling remains off in the verified HAL build.
- Repeated too-old/too-new scheduling errors do not occur.
- Active playback underruns are zero or very close to zero during a short local
  test.
- Elastic drops remain zero during short local playback tests.
- Core Audio remains responsive and does not enter a high-CPU loop.

## Implemented result

The implemented OpenA8DJ HAL keeps the OpenA8DJ identity and uses the old macOS
kext as the compatibility reference. The 0.2.11 attempt misapplied the legacy
mode 2 input cursor as an output byte offset and caused loud white noise. It was
immediately removed from the active HAL folder.

Version 0.2.12 build 14 reverted playback byte start to 4 while keeping the
safer old-kext-derived stream reset and queue sizing.

Version 0.2.13 build 15 changes the investigation path: output is no longer a
plain FIFO. The HAL now passes `mOutputTime.mSampleTime` into the USB engine,
and the USB engine writes output frames into a circular timeline keyed by sample
frame, matching the old kext's `clipOutputSamples`/`queueOutIrp` model more
closely. This should reduce callback-order jitter as a source of crackling.

Installed 0.2.13 build hash:

```text
c0edd2455b3aa47d2abf3633c1f6e0642969650bf88fd14492729005e30cc001
```

After installing 0.2.13, Core Audio enumerated `Open Audio 8 DJ` with 8 inputs
and 8 outputs, and the system default output was explicitly left on MacBook Air
Speakers for a controlled low-volume listening test.

Version 0.2.17 build 19 continues that path. It keeps the timeline-based output
buffer, adds the Core Audio `cfsz` cycle-size selector that coreaudiod probes
internally, and pins the preferred cycle period to the small sub-millisecond
size that Core Audio kept selecting after reload. The user reported that the
timeline build sounded slightly better than the previous FIFO-style build, with
noise spread more homogeneously across frequencies. That is weak positive
evidence to keep improving the timeline/clock-following design.

Separate second-phase symptom: pressing the Codex voice/microphone button made
the Audio 8 DJ output noise increase sharply even though the selected microphone
was the laptop microphone, not the Audio 8 DJ input. Treat that as a separate
Core Audio multi-device scheduling/input-activation problem after the base
playback path is more stable.

Installed 0.2.17 build hash:

```text
80ef71e16177e8ceecde8c358f0725775ab6ba8d4106d6326ca7068c5bb82a0e
```

Version 0.2.18 build 20 tests the first recommendation from the external
review: do not conceal an active output gap by inserting a hard zero. During an
active missing timeline frame, the USB output path now replays the last valid
frame for a very short bounded window with a gentle decay and increments
`outputElasticReplays`. Missing source frames still increment underrun counters.
If listening improves while replay counters rise, the timing/elasticity
hypothesis is strengthened. If replay counters stay at zero while audio is still
bad, the next priority is output framing/check bytes or a real USB clock
follower.

Version 0.2.19 build 21 corrected the public Core Audio buffer model. The legacy
kext is an IOAudioEngine driver and contains a `0x200` frame default, while the
modern HAL had been exposing the hardware-ish 22 frame cadence as the app-facing
cycle size. That made Core Audio run `Open Audio 8 DJ` with sub-millisecond
cycles and matched the overload/client-timeout logs seen when the laptop
microphone was activated. The HAL now advertises 512 frames by default with a
512-4096 frame range, keeping USB micro-packet pacing internal to the transport
instead of forcing apps to run at the USB packet cadence.

Version 0.2.20 build 22 hardens that change by rejecting zero/oversized buffer
requests and normalizing valid requests onto 512, 1024, 2048, or 4096 frames.
This prevents Core Audio or an app from silently moving the device back to a
small public cycle such as 192 frames after reload or when another device, for
example the laptop microphone, joins the graph.

Version 0.2.21 build 23 separates the playback path from accidental input graph
activation. The HAL still describes the Audio 8 input streams, but it no longer
advertises `ReadInput` support by default and it refuses to become the system
default input. This keeps laptop-microphone capture from making Core Audio run
the Audio 8 as a full-duplex device while Spotify or another app is only using
the outputs. Real Audio 8 input/timecode support can be re-enabled behind
`OPENA8DJ_ENABLE_INPUT_IO` once the output path is stable.

Version 0.2.22 build 24 goes one step safer for the playback test: with
`OPENA8DJ_ENABLE_INPUT_IO=0`, input stream objects are not exposed to Core Audio
at all. The device presents as output-only (`in=0 out=8`) so laptop microphone
capture cannot make Core Audio reconcile or start the Audio 8 input side.

Version 0.2.23 build 25 removes the custom `cfsz` property exposure from the
HAL. After the standard buffer-frame-size path was fixed to 512 frames, `cfsz`
was no longer needed and Core Audio was repeatedly probing it during device

## 2026-06-14 binary re-check: old kext output slot model

The old 2.8.0 `NIUSBAudioDriver.kext` in
`old driver/useful-binaries/kext/NIUSBAudioDriver.kext` is a macOS 10.9
x86_64 IOAudioFamily/IOUSBFamily kext, not an AudioServerPlugIn. It matches
Audio 8 DJ via `idVendor=6092`, `idProduct=6520`, class
`de_caiaq_driver_NIUSBAudioDevice`, and user client
`de_caiaq_driver_NIUSBUserClient`.

Useful symbols and strings from the binary:

- `de_caiaq_driver_NIUSBAudioEngine::readHandler(unsigned int, int, IOUSBLowLatencyIsocFrame*)`
- `de_caiaq_driver_NIUSBAudioEngine::writeHandler(unsigned int, int, IOUSBLowLatencyIsocFrame*)`
- `de_caiaq_driver_NIUSBAudioEngine::queueInIrp(isocIrp*, unsigned int)`
- `de_caiaq_driver_NIUSBAudioEngine::queueOutIrp(isocIrp*)`
- `de_caiaq_driver_NIUSBAudioEngine::readFrameList()`
- `de_caiaq_driver_NIUSBAudioEngine::allocIsocIrps(isocIrp*, unsigned int, unsigned int)`
- `de_caiaq_driver_NIUSBAudioEngine::takeSimpleTimeStamp(...)`
- `de_caiaq_driver_NIUSBAudioEngine::jitterFilter(...)`
- string: `Unable to find USB output slot. Resetting.`
- string: `kIOReturnIsoTooNew` / `kIOReturnIsoTooOld`

Reverse-engineered behavioral clues from `otool -tvV`:

- IRP struct stride is `0xf0`.
- Input IRP ring appears to use `0x40` entries.
- Output IRP/slot ring appears to use `0x80` entries.
- Each IRP has 8 low-latency isochronous microframes.
- Each IRP memory descriptor is initialized as `0x200 << 3 = 4096` bytes.
- The base USB microframe size is `0x200` bytes.
- The public/Core Audio buffer base is `0x200 = 512` frames.
- The stream ring math uses `0x20000` bytes per stereo stream/pair.
- Audio stream format exposed by the old IOAudioEngine is 24-bit integer in a
  24-bit container, with stereo streams per pair.
- Sample-rate constants found in `createNewAudioStream`: 44100, 48000, 88200,
  96000, 192000 Hz.
- Output slot search scans up to `0x80` slots from a cursor (`0xba28`) and wraps.
- `readHandler` calls `readFrameList()`, finds a free OUT slot, copies IN
  per-microframe complete-count/status into the OUT slot, then queues IN and OUT
  while holding the slot lock.
- `writeHandler` marks the OUT slot free only after write completion by clearing
  the slot flag at offset pattern `0x4200 + slot * 0xf0`.
- `queueOutIrp` prepares a multi-descriptor for 8 microframes and handles output
  ring wrap by splitting memory descriptors; this is lower-level than the
  current IOUSBHost user-space `enqueueIORequestWithData` path.

Interpretation for the modern HAL:

- The old-driver clue is not simply "make a fixed pool". It is specifically:
  IN completion chooses a still-free OUT slot, derives OUT transaction lengths
  from the actual IN low-latency frame list, and leaves the slot unavailable
  until OUT completion.
- Our transfer pool approximates slot lifetime with `inUse`, but still pays one
  IOUSBHost enqueue/completion transaction per OUT transfer in user space.
- The corrected OUT coalescing experiment lowered CPU only when it reduced OUT
  enqueues, but it either produced no output before the gate hardening or heated
  Core Audio after variable-count correction. Any future slot experiment must
  keep the exact "valid output frames read" gate active and must pass iRig
  sideband/click gates before human listening.
reconciliation. The HAL now relies on standard Core Audio buffer properties
only.

Version 0.2.24 build 26 adds a bounded micro-backoff during the initial HAL
property probe burst. A trace build made Core Audio enumeration reliable because
the file writes slowed the property storm; the release build now reproduces that
stabilizing effect without writing trace logs or touching the realtime audio
path.

Version 0.2.25 build 27 increases that startup property-probe backoff to 3 ms
after the 750 us version still allowed Core Audio to enter the hot
reconciliation loop.

Version 0.2.29 build 31 fixes the public Core Audio timing surface that was
still wrong in the previous listening test. The HAL keeps the app-facing buffer
at 512 frames, but no longer reports 512 as the zero-time-stamp period. That
timestamp period is now a separate 16384-frame value, matching Core Audio's
requirement that this clock period be much larger than a normal I/O buffer.
After reinstall, Core Audio confirmed `buffer=512` and `buffer-bytes=16384`
instead of the earlier `buffer=192`/`buffer-bytes=6144`.

0.2.29 was tested against the microphone symptom without using the Audio 8
input side. A silent output run, an ultra-low-level nonzero output run, and an
ultra-low-level output run while recording from `BuiltInMicrophoneDevice` all
completed with zero active output underruns, zero elastic drops/replays, zero
timeline resets, zero playback failures, and zero mode-2 check/panic errors.

Human listening update: despite the cleaner automated counters, `0.2.29` did
not improve sound quality and may have made glitches worse. Activating/touching
the external microphone caused the music to stop completely. Do not treat
`0.2.29` as a successful audio-quality fix; use it only as evidence that the
automated HAL/USB counters are not sufficient to predict real playback quality.
See `docs/HANDOFF_2026-06-10_AUDIO_CRACKLE.md`.
