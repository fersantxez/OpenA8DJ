# USB Audio Cadence Research - 2026-06-12

This note captures the external research thread behind the current OpenA8DJ
audio-quality direction. It is meant for agents testing alternatives, not for
end-user release notes.

## macOS-first boundary

OpenA8DJ is a macOS/Core Audio driver first. Linux `snd-usb-caiaq` is only a
comparative reference for hardware sensitivity to USB cadence; it is not an
implementation target.

Any idea borrowed from Linux must be translated into a macOS hypothesis and
proved through macOS contracts and physical output:

- Keep `GetZeroTimeStamp` as a stable Core Audio timeline. Do not force it to a
  USB clock or unstable re-anchor model.
- Put any device-cadence following inside the USB transport/output-buffer path,
  not in the public Core Audio API surface.
- Instrument macOS first: IOUSBHost completion deltas, IN-completion to
  OUT-queue delta, OUT completion delta, packet lengths and offsets, output
  in-flight depth, queue misses, zero-length microframes, output read rate,
  coreaudiod/driver CPU, and physical iRig sidebands.
- Do not assume ALSA implicit feedback, URBs, `period_elapsed`, or Linux kernel
  scheduling have direct macOS equivalents.
- If a capture-paced experiment is tried, Core Audio input streams may remain
  hidden, USB IN may be used internally only as a cadence signal, OUT layout and
  cadence must come from real observations, and rollback must be immediate.
- The decisive gate is not Linux parity. It is stable Core Audio, no
  `audio-list` hang, normal idle coreaudiod CPU, clean physical 1 kHz iRig
  sidebands, acceptable physical music SNR/residuals, and only then human
  listening.

Central rule: Linux gives suspicions; macOS plus iRig decides.

## Local symptom pattern

The current OpenA8DJ failures should be treated as timing/cadence failures until
proved otherwise:

- Internal gates have produced false positives. Builds have shown clean sample
  bytes, clean packing checks, and zero traditional underrun counters while the
  physical output still sounded like radio/noise-filter distortion.
- Physical iRig captures showed low analog SNR, high mid/high-band residuals,
  click outliers, and lag jumps after the Audio 8 DJ DAC path. Silence through
  the same capture route was clean, so the capture chain itself is not the main
  noise source.
- Repeated 1 kHz tone captures showed sidebands around the tone, especially near
  940 Hz and 1060 Hz. The relative sideband level stayed bad across a level
  sweep, so this is not just hard clipping or output headroom.
- CPU-correlated noise remained visible even when `active-underruns`,
  `elastic-replays`, and playback failures were zero.
- Activating a separate Core Audio input client, such as the Codex/laptop
  microphone path, has made Audio 8 DJ output worse even when the Audio 8 DJ was
  not selected as the input device. Keep this as a separate input/output
  coupling or scheduling symptom.

Practical consequence: do not qualify another listening candidate from memory
captures or stream counters alone. A candidate needs a post-device physical
capture or an equally decisive cadence metric.

## External evidence

### Linux CAIAQ driver pattern

The Linux `snd-usb-caiaq` driver is the strongest public reference for this
hardware family. Its current audio path does not treat playback as an
independent stream with arbitrary packet timing. In `read_completed()`, the
driver:

- receives an isochronous input URB,
- finds an unused output URB by tracking an active-output bit mask,
- copies the received packet layout into the output URB, including packet
  lengths and offsets,
- fills the output packet data,
- updates playback/capture period accounting,
- submits the output URB,
- then requeues the inbound URB.

Reference:
<https://codebrowser.dev/linux/linux/sound/usb/caiaq/audio.c.html>

This matches the leading OpenA8DJ hypothesis: the content of the samples may be
valid while the device still receives an output cadence that its DAC cannot turn
into clean audio.

### CAIAQ bug history

Public ALSA history around `snd-usb-caiaq` includes fixes for:

- tracking submitted output URBs,
- correcting outbound `iso_frame_desc` offsets,
- dropping bogus isochronous packets,
- fixing elapsed-period reporting,
- limiting repeated streaming warnings because logging in that path is
  expensive.

Reference:
<https://www.alsa-project.org/wiki/Changes_v1.0.19_v1.0.20>

A Linux 2.6.32 stable update also backported CAIAQ changes for output URB
tracking and outbound ISO descriptor offsets.

Reference:
<https://lists.ubuntu.com/archives/kernel-team/2011-October/016549.html>

Takeaway: similar drivers have already had bugs where the failure was not "more
buffer", but wrong output URB lifecycle, wrong ISO offsets, bad packet filtering,
or expensive work in the streaming path.

### Generic Linux USB-audio clocking techniques

The generic Linux USB-audio driver gives useful design clues even when the Audio
8 DJ protocol is vendor-specific:

- Capture endpoints use small URBs because the period boundary is only known
  after packets return.
- Playback endpoints with implicit sync use the same packet/URB parameters as
  their corresponding capture endpoint.
- Playback endpoints without implicit sync distribute packets so an ALSA period
  fits regularly into a small number of URBs, then limit queue size to avoid
  unnecessary latency.
- For implicit-feedback playback, Linux can delay playback URB submission and
  let capture completions drive the playback sink.

Reference:
<https://codebrowser.dev/linux/linux/sound/usb/endpoint.c.html>

This reinforces the next OpenA8DJ direction: build a measured, deterministic
output cadence model before experimenting with larger queues or explicit USB
frame scheduling again.

### Core Audio timing constraints

Apple's `GetZeroTimeStamp` contract returns the device sample time, host time,
and clock seed. If the seed changes, the host assumes a new timeline and
resynchronizes.

Reference:
<https://developer.apple.com/documentation/coreaudio/audioserverplugindriverinterface/getzerotimestamp>

Apple's AudioDriverKit driver guidance states that HAL uses the sample-time /
host-time pair to run and synchronize I/O.

Reference:
<https://developer.apple.com/documentation/AudioDriverKit/creating-an-audio-device-driver>

This explains why the previous forced USB-clock `GetZeroTimeStamp` experiment
was dangerous: the HAL timeline must remain stable and host-consistent while the
USB output side separately follows the device cadence.

### Real-time audio path discipline

Realtime audio guidance is consistent across PortAudio/JACK-style ecosystems:
avoid allocations, file or console I/O, OS calls, mutexes, priority inversion,
and variable-time work in the audio callback or equivalent streaming path.

References:

- <https://portaudio.com/docs/v19-doxydocs/writing_a_callback.html>
- <https://github.com/PortAudio/portaudio/wiki/Tips_Callbacks>
- <https://jackaudio.org/faq/linux_rt_config.html>

For OpenA8DJ this means that transfer allocation, Objective-C object churn,
logging, USB frame polling, broad locks, and per-frame work in the streaming path
are all suspect until proven cheap under stress.

## Ranked hypotheses

1. Bad USB OUT cadence is producing analog modulation/noise even when sample
   bytes and counters look correct.
2. Driver CPU and realtime-path jitter are creating periodic sidebands and
   load-correlated distortion.
3. Output queue depth alone is not the answer. Prequeue/saturation experiments
   have already created no-audio or radio/noise-filter failures.
4. The input/capture side may still be the right timing source, even while Core
   Audio input streams stay hidden from clients.
5. HAL `GetZeroTimeStamp` should remain a stable Core Audio timeline. Device
   cadence following belongs in the USB transport model, not in a forced HAL
   timestamp re-anchor loop.

## Guardrails for alternative tests

Do:

- Start from a known-good source baseline or exact restored binary baseline.
- Land instrumentation separately from playback-behavior changes.
- Use the physical iRig route for release gates:
  `Open Audio 8 DJ output -> mixer -> mixer REC OUT -> iRig Stream input`.
- Require repeated 1 kHz tone captures before music or human listening.
- Track sideband ratios around 1 kHz, lag jumps, analog SNR, click outliers,
  active underruns, elastic replays, playback queue depth, output read rate,
  CPU, and Core Audio stability.
- Keep input/microphone stress as a second-phase test after output-only playback
  is stable.

Do not:

- Ask for human listening if physical gates fail.
- Treat zero `active-underruns` or zero playback failures as a pass by itself.
- Reuse the rejected 0.2.60 prequeue direction without a new cadence proof.
- Force HAL `GetZeroTimeStamp` directly to USB frame timing.
- Reload source-built candidates from the unsafe current tree before isolating a
  known-good source baseline.
- Hide failures by reducing gain unless sidebands/residuals improve, not just
  absolute level.

## Alternative-testing agent prompt

Use this prompt for an implementation agent that will test alternatives:

```text
You are working in /Users/fer/dev/opena8dj on the OpenA8DJ macOS user-space
driver for Native Instruments Audio 8 DJ. Your goal is to test alternative
technical approaches for fixing the audible crackle/radio/noise-filter
distortion without leaving the user's machine in a bad Core Audio state.

Context you must preserve:

- The current failure is probably not just sample packing. Internal sample
  captures and stream counters have passed while physical output remained bad.
- The strongest hypothesis is bad USB isochronous OUT cadence and realtime-path
  jitter. The Linux snd-usb-caiaq driver is the best public clue: it drives OUT
  from IN completion/layout, tracks active output URBs, copies real packet
  lengths/offsets, and avoids treating playback as an arbitrary independent
  queue.
- A 1 kHz physical iRig capture has shown sidebands near the tone, especially
  around 940 Hz and 1060 Hz. Sidebands stayed bad across level sweeps, so do not
  call this clipping until proven.
- The Codex/laptop microphone symptom is separate: activating a Core Audio input
  client can worsen Audio 8 output even when Audio 8 is not selected as input.
  Test output-only first, then input/mic stress later.
- Human listening is decisive, but do not ask for it until automated physical
  gates pass.

Hard safety rules:

1. Before changing playback behavior, identify a known-good source or binary
   baseline. Do not keep installing candidates from the current unsafe source
   tree if Core Audio enumeration hangs or coreaudiod goes hot.
2. Add instrumentation separately from behavior changes.
3. Do not force HAL GetZeroTimeStamp to the USB clock. Keep HAL's Core Audio
   sample-time/host-time timeline stable.
4. Do not treat zero active-underruns, zero elastic-replays, or zero playback
   failures as sufficient. They are necessary but not a release gate.
5. Do not ask the user to listen to any candidate that fails physical iRig tone
   or music gates.
6. If a candidate makes audio-list hang, coreaudiod exceed 100%, or output peak
   stay at zero during playback, reject it, unload OpenA8DJ from HAL, recover
   Core Audio, and document the rollback.

Suggested alternatives to test:

A. CAIAQ-style capture-paced output:
   - Keep Core Audio input streams hidden if needed, but use USB IN completions
     internally as the cadence source.
   - For each completed IN microframe/URB, derive the OUT packet lengths and
     offsets from the actual inbound layout.
   - Track active OUT requests with a bounded bitmap or equivalent.
   - Requeue IN promptly and submit OUT only when a valid packet layout exists.

B. Realtime-path cleanup without behavior change:
   - Preallocate transfer descriptors, transaction arrays, buffers, and event
     records at stream start.
   - Remove Objective-C allocation, logging, file I/O, USB frame polling loops,
     broad locks, and non-O(1) scans from the streaming path.
   - Prove byte-identical output behavior before testing sound quality.

C. Output cadence instrumentation:
   - Record capture completion deltas, OUT queue deltas, OUT completion deltas,
     OUT packet lengths, output in-flight depth, queue misses, zero-length
     microframes, and output read-rate.
   - Compare those metrics against sideband ratio and CPU profile.

D. Conservative elastic follower:
   - If Core Audio and USB cadence drift, adjust the output read pointer
     gradually with bounded single-frame corrections.
   - Avoid large drops/replays and avoid hard-zero insertion during active audio.

Required gate order for each candidate:

1. Build identity: version/build/hash, git diff summary, compile flags, sample
   rate, buffer size, output topology, and output gain.
2. Smoke/Core Audio enumeration: Open Audio 8 DJ appears, expected in/out shape,
   coreaudiod idle is normal.
3. Internal deterministic WAV gate: Core Audio written vs driver consumed vs USB
   packed bytes, but treat this only as a diagnostic.
4. Physical 1 kHz tone gate through iRig. Run at least three captures. Report
   sideband ratio, strongest sideband frequencies and dB relative to 1 kHz,
   peak/RMS, clipping, lag jumps, CPU, and stream counters.
5. Physical music gate through iRig only if tone passes.
6. Human listening only if physical gates pass and the candidate is currently
   installed, loaded, and stable.
7. Second-phase microphone/input stress only after output-only playback passes.

Report format:

- Executive verdict: pass/fail/rejected and why.
- Candidate identity: version/build/hash and exact change.
- What changed and what did not change.
- Metrics table: stream counters, cadence counters, CPU, tone sidebands, music
  SNR/residuals, lag jumps, click outliers.
- Physical route used.
- Comparison with previous best baseline.
- Rollback/recovery actions taken.
- Next recommendation.

Primary source references:

- Linux CAIAQ audio path:
  https://codebrowser.dev/linux/linux/sound/usb/caiaq/audio.c.html
- Linux USB-audio endpoint/cadence model:
  https://codebrowser.dev/linux/linux/sound/usb/endpoint.c.html
- ALSA CAIAQ change history:
  https://www.alsa-project.org/wiki/Changes_v1.0.19_v1.0.20
- Linux stable CAIAQ output URB tracking / ISO offset fixes:
  https://lists.ubuntu.com/archives/kernel-team/2011-October/016549.html
- Apple GetZeroTimeStamp:
  https://developer.apple.com/documentation/coreaudio/audioserverplugindriverinterface/getzerotimestamp
- Apple AudioDriverKit timing guidance:
  https://developer.apple.com/documentation/AudioDriverKit/creating-an-audio-device-driver
- PortAudio realtime callback guidance:
  https://portaudio.com/docs/v19-doxydocs/writing_a_callback.html
  https://github.com/PortAudio/portaudio/wiki/Tips_Callbacks
```
