# Real-Time Audio Driver Research: Windows and Linux - 2026-06-19

This is the initial research base for a modern, low-latency, high-quality
OpenA8DJ driver architecture outside the current macOS HAL line. It focuses on
Windows as a potential production target and Linux as the strongest public
reference for real-time USB audio behavior, ALSA integration, and CAIAQ timing
clues.

This is not an implementation handoff yet. It is the architectural research
thread that future Windows/Linux work should extend with measurements.

Separated follow-up studies:

- `docs/WINDOWS_IDEAL_REALTIME_AUDIO_DRIVER_STUDY_2026-06-19.md`
- `docs/LINUX_IDEAL_REALTIME_AUDIO_DRIVER_STUDY_2026-06-19.md`

## Executive thesis

For Audio 8 DJ, a Windows release cannot be a repackaged macOS driver and
cannot be just a KMDF control/USB driver. A useful Windows driver must publish
real Windows audio endpoints, expose the full 8-in/8-out topology, support MIDI
and hardware controls, and hit pro-audio latency without letting USB cadence or
DPC jitter turn into audible artifacts.

The best current Windows direction is:

```text
Windows audio clients / DAWs
    |             |
    |             +-- ASIO facade for DAWs that expect ASIO
    |
Windows audio stack
    |
ACX 1.1 / KMDF audio function driver
    |
OpenA8DJ CAIAQ streaming engine
    |
WDF USB transport: EP1 commands, 0x82 capture, 0x06 playback
    |
Native Instruments Audio 8 DJ hardware
```

The second-best direction, if ACX cannot be made to fit this vendor-specific USB
device cleanly, is an AVStream/KS audio driver with the same private USB
streaming engine. PortCls/WaveRT is important conceptually, but Microsoft's
PortCls documentation says PortCls port drivers do not support external buses
such as USB, which makes it a poor direct fit for this hardware.

Linux should be studied as the timing oracle, not as a direct port. The Linux
`snd-usb-caiaq` driver is the strongest public clue because it handles this
hardware family as a capture-paced isochronous device: incoming USB packets are
used to shape outgoing USB packets. That matches the macOS evidence in this repo
where valid sample bytes and clean underrun counters were not enough; physical
audio quality depended on cadence.

## Current repo context

The current Windows workstream already contains an experimental KMDF USB
transport package:

- `windows/driver/OpenA8DJUsb.c`
- `windows/driver/OpenA8DJUsb.inf`
- `windows/include/OpenA8DJShared.h`
- `windows/tools/opena8djctl.c`
- `.github/workflows/windows-driver.yml`

That code maps the known Audio 8 DJ endpoints and exposes capability/control
IOCTLs, but it explicitly reports `WindowsAudioEndpointExposed = FALSE`.
Therefore it is not yet a Windows audio driver in the sense that Spotify,
Traktor, WASAPI, DirectSound, or ASIO hosts can use as an audio interface.

The existing macOS evidence also matters:

- The device uses vendor/product `17cc:1978`.
- Control is on EP1, audio capture on `0x82`, audio playback on `0x06`.
- The hardware is 8 input / 8 output, with A/B/C/D stereo pairs.
- Physical output quality has failed in the past even when internal counters
  looked clean.
- Candidate readiness must be proven with real post-device capture, not just
  code path counters.

## Windows architecture map

### 1. UAC2 in-box driver

Windows ships `usbaudio2.sys` for USB Audio Class 2.0 devices. It is a WaveRT
audio port class miniport and supports class-compliant UAC2 devices. It is not
a solution for Audio 8 DJ unless the hardware/firmware can present a compliant
USB Audio Class 2.0 interface. The current Audio 8 DJ path is vendor-specific
CAIAQ, so OpenA8DJ must provide the missing audio stack itself.

Relevant limit: Microsoft's UAC2 page says asynchronous OUT requires explicit
feedback and that implicit feedback is not supported by the in-box driver. That
matters because Linux CAIAQ-style capture-paced behavior resembles implicit
or private feedback rather than a standard explicit UAC2 feedback endpoint.

### 2. ACX 1.1 / KMDF

ACX is Microsoft's newer audio class extension model, built on KMDF. Microsoft
states ACX 1.1 is recommended for new driver development and is supported from
Windows 10 version 2004 onward. ACX exposes audio concepts as WDF objects and
currently supports WaveRT-based streaming.

This makes ACX the best first investigation target for OpenA8DJ Windows:

- It is the current Microsoft direction for new audio drivers.
- It is KMDF-based, matching the current experimental WDF USB work.
- It supports stream/circuit concepts and RT packet streaming.
- It can coexist with existing Windows audio clients.
- It avoids a user/kernel transition-heavy streaming path.

The key proof-of-fit question is whether an ACX audio driver can own or compose
with the vendor-specific USB transport cleanly enough for 8-in/8-out full-duplex
isochronous streaming. ACX multi-stack support may help if the design separates
the USB transport from the audio circuit, but the first prototype should be
single-stack unless the WDK samples force another shape.

### 3. PortCls / WaveRT

PortCls/WaveRT is still important for understanding Windows audio expectations:
WaveRT wants low hardware latency reporting, FIFO underrun/overrun visibility,
and cyclic/scatter-gather style streaming where possible.

But direct PortCls is probably not the right endpoint for Audio 8 DJ. Microsoft
documents PortCls as typical for PCI/DMA audio devices and explicitly says
PortCls port drivers do not support external buses such as USB. This is why the
current `docs/WINDOWS.md` note about PortCls being risky is correct.

Use WaveRT concepts as requirements, not as a blind implementation target:

- expose honest hardware latency;
- expose glitch counters;
- keep stream buffers cyclic and preallocated;
- provide low minimum period/device-period behavior only after it is stable;
- do not advertise rates/channels that are not proven on hardware.

### 4. AVStream / KS fallback

AVStream is the fallback if ACX cannot model this device. Microsoft documents
AVStream as a multimedia class driver for video and integrated audio/video
streaming, while still saying PortCls is preferred for typical audio adapters.
For a vendor-specific USB interface that PortCls cannot own directly, AVStream
may be more practical than PortCls, but it is a harder, older model.

Decision rule:

- Try ACX first for Windows 10 2004+ and Windows 11.
- Keep AVStream as the fallback prototype.
- Do not deepen a PortCls prototype unless a concrete WDK sample proves a
  vendor-specific external USB audio design that matches this hardware.

### 5. ASIO

For pro audio and Traktor-class workflows on Windows, ASIO still matters. The
Steinberg help center describes ASIO as required by practically all professional
audio programs under Windows, partly because of low latency. Steinberg also
recommends the latest hardware-manufacturer ASIO driver when available.

Microsoft's `low-latency-audio` repository is relevant because it includes a new
UAC2 driver plus an ASIO interface aimed at musician scenarios. As of the page
checked on 2026-06-19, it still says there is no public release and that the
Steinberg ASIO SDK license does not transfer to other builders.

Practical conclusion:

- OpenA8DJ Windows should expose normal Windows endpoints via ACX/AVStream.
- A serious DJ release should also provide an ASIO component or a documented
  ASIO strategy.
- Do not copy ASIO SDK code or assume Microsoft/Yamaha licensing applies.
- The ASIO layer must not become the only working path; Windows system audio and
  WASAPI/exclusive-mode validation are still needed.

## Windows real-time design rules

### Streaming path

The streaming path must be designed as a bounded real-time system:

- Preallocate URBs, packet descriptors, nonpaged buffers, ring buffers, and WDF
  requests before stream start.
- Keep completion handlers short and deterministic.
- No pageable code or pageable data in the hot path.
- No dynamic allocation in USB completion, packet fill, packet consume, or audio
  render/capture callbacks.
- No logging in the hot path except fixed-rate counters in preallocated memory.
- No broad locks shared between render and capture streams.
- Use per-stream locks only when the framework requires them; otherwise prefer
  single-producer/single-consumer rings and atomic counters.
- Process audio in blocks, not per sample.
- Keep format conversion fixed for the active stream format.
- Treat silence as a real buffer state, not something found by scanning large
  buffers.

### USB cadence

The Audio 8 DJ failure mode should be assumed to be cadence-sensitive until
proved otherwise.

The first Windows streaming prototype should not simply queue arbitrary OUT
isochronous URBs at a fixed software interval. It should test the Linux-style
model:

1. Keep a pool of OUT URBs.
2. Keep a pool of IN URBs.
3. Submit IN URBs continuously.
4. On IN completion, validate packet status and packet length.
5. Shape the corresponding OUT URB packet descriptors from the observed IN
   packet layout.
6. Fill OUT packet payload from the render ring or silence.
7. Submit OUT promptly.
8. Requeue IN.

This is not because Linux is automatically correct. It is because both Linux
CAIAQ source and local macOS audio evidence point to this hardware caring about
the exact isochronous packet lifecycle.

### Clocking and format truth

The driver must expose only what it can keep stable:

- First target: 48 kHz, 512 frames, 8-out playback.
- Then 44.1 kHz.
- Then full-duplex 8-in/8-out.
- Only after that: smaller buffers, 88.2/96 kHz, hotplug/sleep/wake stress.

All public endpoint metadata must be backed by actual hardware behavior:

- sample rates;
- channel counts;
- channel labels;
- input mode controls;
- MIDI in/out;
- buffer-size range;
- hardware latency;
- underrun/overrun counters.

### DPC/ISR and ETW validation

Windows validation must include system-level timing, not only driver counters.
Microsoft's media guidance uses WPR/ETW traces and glitch/data-drop analysis to
find bottlenecks in CPU, GPU, disk, network, or the media pipeline. For OpenA8DJ
Windows, every candidate must record:

- ETW trace during deterministic playback;
- DPC/ISR duration by module in Windows Performance Analyzer;
- USB controller activity;
- CPU frequency/power state during stream;
- stream underrun/overrun counters;
- packet status errors;
- measured output capture against a known reference;
- Traktor or DAW buffer setting and reported latency.

LatencyMon-style user reports are useful smoke signals, but WPA/ETW is the
engineering proof.

## Linux architecture map

### ALSA PCM driver shape

ALSA is the kernel audio API to study for Linux. A driver provides a sound card,
PCM devices/substreams, hardware constraints, trigger callbacks, pointer
callbacks, period accounting, controls, and optional MIDI/control interfaces.

For a USB device, ALSA is not just "write samples to USB". It is a contract:

- negotiate supported hardware parameters;
- maintain correct application pointer and hardware pointer accounting;
- wake clients exactly when periods have elapsed;
- report XRUNs truthfully;
- expose channel maps and controls;
- provide timestamps and delay data good enough for JACK/PipeWire/DAWs.

The ALSA API documentation notes that `snd_pcm_period_elapsed()` is typically
called by IRQ handlers when a period of audio frames has been processed. That
shape is directly relevant to a USB completion-driven driver.

### PREEMPT_RT

Modern Linux real-time behavior is shaped by PREEMPT_RT. The kernel docs explain
that PREEMPT_RT turns many formerly non-preemptible paths into preemptible,
scheduler-managed work by using threaded interrupts and sleeping spinlocks with
priority inheritance.

For audio-driver design this means:

- Do not assume hard IRQ context semantics in RT kernels.
- Avoid long raw spinlock sections.
- Keep any primary interrupt handler minimal.
- Use threaded work deliberately and assign priorities carefully.
- Do not create priority inversion between USB completion, ALSA period wakeup,
  and user-space audio threads.
- Test with both normal and PREEMPT_RT kernels if Linux becomes an actual target.

This is not a substitute for good driver design. PREEMPT_RT reduces worst-case
scheduling latency, but a bad USB completion path can still cause glitches.

### Linux CAIAQ as the timing reference

The current Linux `sound/usb/caiaq/audio.c` path is the most important public
reference for Audio 8 DJ family timing. Its `read_completed()` path:

- receives a capture URB;
- finds an unused output URB;
- copies the incoming packet layout into the outgoing URB;
- fills outgoing audio data;
- reads incoming audio data;
- checks elapsed periods;
- submits the output URB;
- requeues capture.

Historical CAIAQ fixes also matter: bugs existed around output URB tracking and
outbound `iso_frame_desc` offsets. Those are exactly the sort of bugs that can
produce audible failures without looking like simple sample-format errors.

The design lesson is precise:

Do not optimize for "more buffering" first. Optimize for correct packet layout,
URB lifecycle, cadence, and truthful period timing.

### Linux observability

A Linux-quality research harness should include:

- ALSA tracepoints for `hwptr`, `applptr`, `xrun`, and `hw_ptr_error`;
- ftrace/perf around USB completion and period wakeups;
- `/proc/asound` state snapshots;
- `aplay`/`arecord` deterministic full-duplex tests;
- JACK/PipeWire low-buffer stress;
- `cyclictest` or equivalent RT latency baseline;
- physical output capture against a known WAV;
- capture of CPU frequency governor and USB autosuspend state.

The ALSA timestamping docs are important because audio apps need meaningful
`avail`, `delay`, trigger timestamps, and audio timestamps. A driver that merely
plays sound but lies about delay will be weak for DVS/timecode and DAWs.

## Cross-platform performance patterns

These are the rules that apply regardless of OS:

- Own the timing model before adding features.
- Prove packet cadence with hardware captures.
- Keep render/capture full-duplex from the start, even if one side initially
  carries silence.
- Treat the device clock as the source of truth where the hardware requires it.
- Avoid arbitrary software timers for isochronous output pacing.
- Preallocate everything used after stream start.
- Use fixed-size rings and monotonic counters.
- Make underrun, overrun, packet error, late completion, and queue-depth metrics
  first-class.
- Keep control UI, profile changes, and logging out of the stream path.
- Hide unproven rates and modes.
- Validate quality with real music and known tones through a separate capture
  interface.

## Windows prototype plan

### Phase W0: source and sample audit

- Clone/read Microsoft Windows driver samples for ACX audio.
- Identify the smallest ACX sample that publishes render/capture endpoints.
- Identify how an ACX RT Packet Stream provides/consumes shared stream buffers.
- Identify whether an ACX driver can own WDF USB targets directly in the same
  stack.
- If not, identify whether ACX multi-stack cross-driver communication is a clean
  way to pair an audio circuit with the existing `OpenA8DJUsb` transport.

Exit criterion: a written "ACX fit/no-fit" memo with the exact sample files and
DDIs used.

### Phase W1: endpoint-only ACX skeleton

- Publish one stereo render endpoint and one stereo capture endpoint.
- Use a deterministic internal ring or tone source.
- No Audio 8 DJ hardware dependency yet.
- Validate WASAPI shared, WASAPI exclusive, and basic DAW enumeration.
- Record ETW traces and confirm no obvious DPC/ISR spikes from the skeleton.

Exit criterion: Windows sees stable audio endpoints and can stream silence/tone
without stutter.

### Phase W2: hardware USB transport under ACX

- Move or reuse the KMDF USB enumeration/pipe mapping.
- Add preallocated isochronous IN/OUT request pools.
- Implement capture-paced OUT shaping.
- Add fixed counters for packet status, queue depth, late completions, XRUNs,
  and render/capture frame totals.
- Start with 48 kHz / 512 frames / Output A/B only.

Exit criterion: deterministic tone reaches the physical output and is captured
externally without speed error, white noise, or packet-correlated artifacts.

### Phase W3: full topology

- Expose all 8 outputs and all 8 inputs.
- Decide whether Windows should see one 8-channel endpoint or four stereo
  endpoint pairs, based on Traktor behavior.
- Add sample-rate switching only after stream stop/start is reliable.
- Add MIDI endpoints and control surface.

Exit criterion: Traktor can address A/B/C/D outputs and inputs correctly.

### Phase W4: ASIO

- Design an ASIO facade over the same kernel streaming engine.
- Keep licensing separate from this repository until the Steinberg SDK terms are
  understood.
- Validate Traktor with the ASIO path and compare latency/CPU/glitch behavior
  against WASAPI exclusive.

Exit criterion: ASIO exists because it improves or unlocks pro workflows, not
because Windows endpoints are incomplete.

## Linux research plan

### Phase L0: source audit

- Read current `snd-usb-caiaq` source around stream start, URB allocation,
  `read_completed()`, `write_completed()`, period accounting, and controls.
- Read generic `snd-usb-audio` endpoint scheduling for implicit/explicit
  feedback behavior.
- Extract the exact packet descriptor rules that differ from the current macOS
  implementation.

Exit criterion: concise annotated notes with functions and hypotheses.

### Phase L1: Linux live behavior

- Boot a modern Linux kernel with the physical Audio 8 DJ.
- Capture descriptors, ALSA card info, supported rates, channel maps, mixer
  controls, and MIDI ports.
- Run playback, capture, and full-duplex tests at 44.1/48 kHz.
- Record ALSA tracepoints and USB completion timing.

Exit criterion: proof of how Linux handles cadence on this exact unit.

### Phase L2: compare against OpenA8DJ

- Compare packet sizes, offsets, output pacing, queue depth, period size, and
  wakeup cadence against the macOS transport.
- Convert differences into hypotheses for Windows and macOS.

Exit criterion: ranked list of timing/quality differences that could explain
audible behavior.

## Open questions

- Can ACX directly own a vendor-specific USB target while publishing proper
  render/capture endpoints, or does OpenA8DJ need a multi-stack ACX design?
- Does Windows require four stereo endpoints for Traktor ergonomics, or is a
  single 8-channel endpoint better with ASIO handling deck routing?
- How should MIDI be exposed on Windows for this vendor-specific device:
  separate WDF/MIDI driver, user-mode service, or integration with the audio
  package?
- Can hardware controls be changed while streams are running without cadence
  disturbance?
- Does Audio 8 DJ require capture-paced output at all rates, or only at certain
  packet layouts?
- Is 88.2 kHz actually stable enough to expose, or should Windows match the
  currently safer 44.1/48 kHz first milestone?

## Required source references

- Microsoft ACX overview:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-audio-class-extensions-overview>
- Microsoft ACX version information:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-version-overview>
- Microsoft ACX streaming:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-streaming>
- Microsoft ACX multi-stack:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-multi-stack>
- Microsoft WDM audio overview:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/getting-started-with-wdm-audio-drivers>
- Microsoft PortCls introduction:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/introduction-to-port-class>
- Microsoft WaveRT miniport development:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/developing-a-wavert-miniport-driver>
- Microsoft low-latency audio:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio>
- Microsoft USB Audio 2.0 driver:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/usb-2-0-audio-drivers>
- Microsoft media glitch analysis:
  <https://learn.microsoft.com/en-us/windows-hardware/test/weg/delivering-a-great-media-experience>
- Microsoft low-latency UAC2/ASIO repository:
  <https://github.com/microsoft/low-latency-audio>
- Steinberg built-in ASIO driver notes:
  <https://helpcenter.steinberg.de/hc/en-us/articles/17863730844946-Steinberg-built-in-ASIO-Driver-information-download>
- USB-IF UAC2 specification page, errata through 2025-04-02:
  <https://www.usb.org/document-library/usb-device-class-definition-audio-devices-release-20-errata-and-ecn-through-april>
- Linux sound subsystem docs:
  <https://docs.kernel.org/sound/index.html>
- Linux ALSA driver API:
  <https://docs.kernel.org/sound/kernel-api/alsa-driver-api.html>
- Linux ALSA driver guide:
  <https://docs.kernel.org/sound/kernel-api/writing-an-alsa-driver.html>
- Linux PREEMPT_RT theory:
  <https://docs.kernel.org/core-api/real-time/theory.html>
- Linux PREEMPT_RT differences:
  <https://docs.kernel.org/core-api/real-time/differences.html>
- Linux ALSA timestamping:
  <https://docs.kernel.org/sound/designs/timestamping.html>
- Linux ALSA tracepoints:
  <https://docs.kernel.org/sound/designs/tracepoints.html>
- Linux USB host-side API:
  <https://docs.kernel.org/driver-api/usb/usb.html>
- Linux URB documentation:
  <https://docs.kernel.org/driver-api/usb/URB.html>
- Current Linux CAIAQ audio source browser:
  <https://codebrowser.dev/linux/linux/sound/usb/caiaq/audio.c.html>
