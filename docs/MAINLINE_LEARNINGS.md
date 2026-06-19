# Mainline Learnings From `/Users/fer/dev/opena8dj`

Extraction date: 2026-06-16.

Scope: read-only archaeology of the C/Objective-C mainline in `/Users/fer/dev/opena8dj`. No hardware tests, no CoreAudio/USB actions, no recovery scripts, no builds, and no writes were performed in `/Users/fer/dev/opena8dj` or `/Users/fer/dev/audio8djrust`.

Reference paths below are relative to `/Users/fer/dev/opena8dj`.

## High-Level Carryaways

- Treat the current source tree as newer than the checked-out commit. `git log` shows `08745b7 Release OpenA8DJ 0.3.25 Traktor preview` as the branch head, while the working tree and docs carry later uncommitted 0.3.135-era findings. Use file contents and quality reports, not commit history alone, as the source of truth for the current C mainline.
- The safe current build anchor is `0.3.135`, but only as a no-iRig/software-only baseline. It restored the best measured software-only state after 0.3.136-0.3.138 experiments, with iRig absent and physical listening still blocked (`docs/QUALITY_RUNS_2026-06-15.md:7-33`, `docs/QUALITY_RUNS_2026-06-15.md:75-116`).
- Internal USB/HAL counters are necessary but not sufficient. Several candidates had clean counters and failed analog listening or iRig captures (`docs/USB_AUDIO_CADENCE_RESEARCH_2026-06-12.md:42-60`, `docs/OLD_DRIVER_COMPAT_PLAN.md:302-312`).
- Keep HAL time stable. Device cadence following belongs inside the USB transport/output-buffer path, not in `GetZeroTimeStamp` or the public Core Audio clock surface (`docs/OLD_DRIVER_COMPAT_PLAN.md:66-73`, `docs/MACOS_USB_CADENCE_IMPLEMENTATION_PLAN_2026-06-12.md:64-80`).
- The unresolved quality problem is cadence/cost coupling around IOUSBHost isochronous enqueue/completion timing, not just sample packing, underrun counters, or queue depth (`docs/QUALITY_RUNS_2026-06-14.md:718-734`, `docs/USB_AUDIO_CADENCE_RESEARCH_2026-06-12.md:168-180`).

## Current Build Defaults

The Makefile currently labels the source as `VERSION := 0.3.135` (`Makefile:1-2`). Important defaults:

- Output gain `0.50f`, output prefetch 64 frames, one 8-channel input stream, four stereo output streams (`Makefile:63-73`).
- USB transfer policy: `HAL_ISO_FRAMES=64`, capture queue 8, playback queue 8, capture-paced playback enabled, capture-paced output lead 1, no playback coalescing (`Makefile:75-83`).
- Clock/scheduling policy: USB clock anchor disabled, USB zero timestamp disabled, explicit scheduling disabled (`Makefile:87-92`).
- Output format policy: non-native 24-bit output path by default, fast prefetch clear enabled, output start byte 4 (`Makefile:93-105`).
- Lifecycle policy: background preopen enabled, 10 second stop grace, stop isochronous on stop, reset audio params before stream (`Makefile:103-109`).

Implication for the C++ port: start with these defaults as the current no-iRig anchor, but do not treat them as release-qualified physical-audio defaults.

## USB Protocol

Device and endpoint constants are centralized in `src/hal/OpenA8DJUSB.m:180-195`:

- Vendor/product: `0x17cc:0x1978`.
- Control bulk OUT/IN endpoints: `0x01` and `0x81`.
- Isochronous capture/playback endpoints: `0x82` and `0x06`.
- Interface/config/alternate setting: interface 0, configuration 1, alternate setting 1.
- Commands: `GET_DEVICE_INFO=0x01`, `READ_IO=0x04`, `WRITE_IO=0x05`, `AUDIO_PARAMS=0x09`, `MIDI_READ=0x06`, `MIDI_WRITE=0x07`, `AUTO_MSG=0x0b`.

Command transport is bulk OUT plus expected EP1 replies. `sendCommand` tracks pending replies, retries up to three times, and waits up to two seconds (`src/hal/OpenA8DJUSB.m:2060-2142`).

Startup/open flow:

- Device/interface matching checks vendor/product/interface/config (`src/hal/OpenA8DJUSB.m:1129-1153`).
- Open seizes the device, configures it, opens the interface, selects alternate setting 1, discovers the four pipes, reads device info/controls, and starts IPC (`src/hal/OpenA8DJUSB.m:2357-2440`).
- Streaming start optionally resets `AUDIO_PARAMS`, sends the selected audio params, resets stats/rings/state, sets output start byte, starts capture/playback, and enters the worker loop (`src/hal/OpenA8DJUSB.m:3273-3375`).

## Packet Format And Mode 2 Layout

The transport is eight channels arranged as four stereo streams (`kStreams=4`, `kChannelsPerStream=2`) with 24-bit audio samples carried in 4 USB bytes per sample when `dataAlignment >= 2` (`src/hal/OpenA8DJUSB.m:196-225`, `src/hal/OpenA8DJUSB.m:1256-1278`).

Sample conversion:

- Input 24-bit samples are decoded as signed big-endian into 32-bit integers (`src/hal/OpenA8DJUSB.m:1280-1290`).
- Output Float32 is clipped, scaled by `OPENA8DJ_OUTPUT_GAIN`, converted to signed 24-bit, and emitted big-endian by default unless `OPENA8DJ_OUTPUT_NATIVE_I24` is enabled (`src/hal/OpenA8DJUSB.m:1292-1323`).

Mode 2 framing:

- The check byte is `(stream << 1) | ((~group) & 1)` (`src/hal/OpenA8DJUSB.m:1378-1382`).
- Capture decode treats each 32-byte group as four check bytes plus interleaved stereo stream payload. It validates stream check bytes, tracks the high-bit output panic flag, reconstructs four stereo streams, maps them through the input source map, and writes an 8-channel input frame when all streams are present (`src/hal/OpenA8DJUSB.m:3632-3755`).
- Playback fill emits the same 32-byte Mode 2 cadence: stream bytes, check bytes at the expected slot, then more stream bytes. Tail handling preserves the check-byte position when a partial packet ends mid-group (`src/hal/OpenA8DJUSB.m:3877-3951`).
- The current safe playback start byte is 4. The code comments record that legacy cursor byte 2 was not used for playback because it caused check bytes to be sent as audio payload in prior experiments (`src/hal/OpenA8DJUSB.m:3315-3320`, `docs/OLD_DRIVER_COMPAT_PLAN.md:24-29`).

## Channel Map And HAL Surface

The HAL exposes:

- One 8-channel input stream when `HAL_INPUT_STREAMS=1`.
- Four stereo output streams, A/B/C/D.
- Eight total input and output channels.
- Supported nominal sample rates: 44100, 48000, 88200, 96000 Hz.

Key references: `src/hal/OpenA8DJHAL.c:83-107`, `src/hal/OpenA8DJHAL.c:400-414`, `src/hal/OpenA8DJHAL.c:471-493`, `src/hal/OpenA8DJHAL.c:978-984`.

Channel names are A/B/C/D left/right for input and output (`src/hal/OpenA8DJHAL.c:343-370`). Stream configuration returns one 8-channel input buffer and four 2-channel output buffers (`src/hal/OpenA8DJHAL.c:702-739`). Channel layouts are discrete 0-7 (`src/hal/OpenA8DJHAL.c:741-787`).

For output copy, each stereo output stream is copied into its matching pair inside one internal 8-channel buffer. If configured as one output stream, all eight channels can be copied at once (`src/hal/OpenA8DJHAL.c:1078-1097`).

## Sample Rate Handling

HAL sample-rate changes go through Core Audio configuration change requests. `ApplySampleRate` validates the rate, resets sample-time state, and bumps the configuration seed (`src/hal/OpenA8DJHAL.c:1340-1356`, `src/hal/OpenA8DJHAL.c:1902-1995`).

USB rate code mapping:

- 44100 -> 0
- 48000 -> 1
- 96000 -> 2
- 88200 -> 4

Reference: `src/hal/OpenA8DJUSB.m:1202-1209`.

`AUDIO_PARAMS` payload is `[rateCode, depth, bytesPerPacket little-endian, 1]`, with depth currently 2. A reset-style params call may be sent before stream start (`src/hal/OpenA8DJUSB.m:2195-2247`).

Bytes per USB packet are computed from sample rate, drift tolerance, bytes/sample, channels per stream, stream count, and data alignment, then capped at 512 bytes (`src/hal/OpenA8DJUSB.m:1256-1278`).

## Buffer Policy

HAL buffer sizes are normalized to 512, 1024, 2048, or 4096 frames, with 512 preferred/minimum and 4096 maximum advertised (`src/hal/OpenA8DJHAL.c:83-107`, `src/hal/OpenA8DJHAL.c:1012-1018`, `src/hal/OpenA8DJHAL.c:1371-1384`).

The USB output engine uses a timeline ring rather than pushing Core Audio buffers straight to USB:

- Ring size: 32768 frames.
- Startup latency: 8192 frames.
- Restart latency: 4096 frames.
- Target latency: 8192 frames.
- Elastic high water: 24576 frames.

Reference: `src/hal/OpenA8DJUSB.m:196-225`.

`OutputTimelineWrite` writes frames at absolute sample times and handles stale/future gaps, restarts, and target-latency drops (`src/hal/OpenA8DJUSB.m:988-1051`). `OutputTimelineRead` feeds startup silence, drops excess queued frames, and zero-fills missing frames while counting active underruns separately from startup silence (`src/hal/OpenA8DJUSB.m:1053-1128`).

Legacy/old-driver guidance says the old kext queued deep input/output rings, paced output from input completions, and kept HAL app timing separate from USB cadence (`docs/OLD_DRIVER_COMPAT_PLAN.md:12-23`, `docs/OLD_DRIVER_COMPAT_PLAN.md:82-97`, `docs/OLD_DRIVER_COMPAT_PLAN.md:222-280`).

## HAL Callbacks

`gDriverInterface` wires the AudioServerPlugIn callbacks for initialization, device creation, property get/set, start/stop, zero timestamp, and IO operations (`src/hal/OpenA8DJHAL.c:237-283`).

Important callback behavior:

- `StartIO` starts the Core Audio clock and calls `OpenA8DJUSBStart(gSampleRate)` on the first running client (`src/hal/OpenA8DJHAL.c:1997-2023`).
- `StopIO` flushes pending output, disables input decode, stops isochronous streaming when configured, stops the clock, and schedules delayed close/preopen behavior (`src/hal/OpenA8DJHAL.c:2025-2056`, `src/hal/OpenA8DJHAL.c:1238-1277`).
- `GetZeroTimeStamp` uses the stable HAL/Core Audio timeline. USB-anchor timestamping exists behind `OPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP`, but the current Makefile disables it (`src/hal/OpenA8DJHAL.c:2058-2114`, `Makefile:87-92`).
- `BeginIOOperation` prepares input or output cycle state; `DoIOOperation` copies input/output stream data; `EndIOOperation` flushes output only after the expected stream set is complete (`src/hal/OpenA8DJHAL.c:2116-2210`).

A prior failure mode was flushing when any output stream arrived. Core Audio may call `EndIOOperation` per stream; flushing the first touched stream corrupts the cycle. The safe rule is to flush only when all four expected output streams have been seen (`docs/HANDOFF_2026-06-10_AUDIO_CRACKLE.md:248-256`, `src/hal/OpenA8DJHAL.c:1164-1175`).

## Output Path

Core Audio writes Float32 interleaved stream buffers. The HAL collects them into one internal 8-channel output cycle buffer, preserving stream pair placement (`src/hal/OpenA8DJHAL.c:1036-1076`, `src/hal/OpenA8DJHAL.c:1078-1097`).

When the cycle is complete, `FlushOutputCycle` sends the internal 8-channel buffer to `OpenA8DJUSBWriteOutputAtSampleTime` with the Core Audio sample time if available (`src/hal/OpenA8DJHAL.c:1146-1162`).

USB output write behavior:

- Requires 8 channels.
- If a valid sample time is supplied, it writes to that absolute timeline position.
- Otherwise it appends at timeline max+1 or startup latency.
- It records restarts, drops, late writes, and active underruns separately.

Reference: `src/hal/OpenA8DJUSB.m:4748-4853`.

Playback is capture-paced in the current Makefile. Capture completions supply the valid packet layout/counts used to queue playback. Stale capture frames whose byte count does not match the expected layout are filtered before decode, because early stale IOUSBHost completions can contain old layout sizes (`src/hal/OpenA8DJUSB.m:4030-4307`).

## Input Decode

Current HAL input shape is visible as one 8-channel stream (`Makefile:67-69`, `src/hal/OpenA8DJHAL.c:471-493`). `PrepareInputCycle` enables input decode, reads USB input into an 8-channel Float32 buffer, and then client input callbacks copy the requested stream slice (`src/hal/OpenA8DJHAL.c:1100-1143`).

USB input decode can be disabled. If disabled, HAL reads return zeros and avoid filling the input ring (`src/hal/OpenA8DJUSB.m:4717-4746`). Playback/output-only profiles deliberately disable input decode (`src/tools/opena8dj-control.c:381-420`).

Input controls and routing:

- Raw control state stores input mode, ground lift bits, software lock, input transforms, input source map, and decode enabled state (`src/hal/OpenA8DJUSB.m:2249-2294`).
- `readControls` sends `AUTO_MSG`, then `READ_IO`, then applies a compatibility fixup if needed (`src/hal/OpenA8DJUSB.m:2296-2334`).
- Input transform/source commands support pair swap, invert, mono, mute, and pair source routing through IPC (`src/tools/opena8dj-control.c:274-379`).

## Timecode And Vinyl Findings

The 0.3.25 Traktor preview restored a usable Traktor-facing surface: four stereo outputs, one 8-channel input stream, CAIAQ control state, CoreMIDI endpoint scaffolding, and 44.1/48 kHz local validation. It still required physical matrix testing for vinyl/CD-line, pair order, latency, and Traktor scope before release (`docs/TRAKTOR_TIMECODE.md:7-23`).

Control profile expectations:

- `timecode-vinyl`: input mode 0, vinyl ground-lift bit, software lock on, input decode on.
- `timecode-cd-line`: input mode 1, line/CD intent, software lock on, input decode on.
- `phono`: input mode 2, software lock on, input decode on.

References: `docs/TRAKTOR_TIMECODE.md:39-50`, `docs/TRAKTOR_TIMECODE.md:72-88`, `src/tools/opena8dj-control.c:381-420`.

Deck mapping starts with Deck A on Input A and Deck B on Input B (`docs/TRAKTOR_TIMECODE.md:63-68`). Timecode depends on real input quality, so 88.2/96 kHz exposure is lower priority than proving 44.1/48 kHz timecode behavior (`docs/TRAKTOR_TIMECODE.md:94-110`).

The timecode smoke gate is non-destructive only in the sense that it does a short controlled duplex software/HAL smoke under the audio gate lock and restores playback mode/input-decode off afterward. It is not a substitute for Traktor/vinyl/iRig physical validation (`scripts/timecode-smoke-gate:16-24`, `scripts/timecode-smoke-gate:76-154`).

## Recovery, Locks, And Quality Scripts

Do not run recovery scripts casually. Many are state-changing by design.

- `scripts/audio-gate-lock.sh` is the global lock for hardware/audio gates. It writes lock owner metadata under `${AUDIO_GATE_LOCK_ROOT:-$HOME/.opena8dj/hardware-gate.lock}` and supports inherited nested locks (`scripts/audio-gate-lock.sh:16-47`, `scripts/audio-gate-lock.sh:102-143`).
- `scripts/audio-stack-health` can perform read-only health checks, but `--reset` can kill/restart audio processes and therefore is state-changing (`scripts/audio-stack-health:95-127`, `scripts/audio-stack-health:139-196`).
- `scripts/audio-stack-guard` includes recovery paths that can unload OpenA8DJ and kill services; recovery mode acquires the global lock (`scripts/audio-stack-guard:19-31`, `scripts/audio-stack-guard:238-295`).
- `scripts/capture-device-diagnose` is intended as read-only capture-state diagnosis. It verifies iRig USB vendor/product `0x1963:0x0059` and Core Audio UID, and reports `READY` or blocker reasons (`scripts/capture-device-diagnose:16-17`, `scripts/capture-device-diagnose:263-329`).
- `scripts/irig-usb-recovery-diagnose` diagnoses and can run iRig recovery, but explicitly restarts audio/USB services and may trigger exact-device reset tooling. It never resets Audio 8 DJ, but it is still state-changing (`scripts/irig-usb-recovery-diagnose:16-17`, `scripts/irig-usb-recovery-diagnose:222-234`).
- `scripts/candidate-preflight` is the intended ladder: output-pair smoke, timecode smoke, playback CPU/digital gate, physical bench, iRig/full physical gate, and only then candidate-listen (`scripts/candidate-preflight:20-32`, `scripts/candidate-preflight:148-257`).
- `scripts/candidate-listen-gate` requires physical iRig tone, physical music, stress, baseline comparison, no clicks, and valid measurement status before human listening (`scripts/candidate-listen-gate:30-39`, `scripts/candidate-listen-gate:271-376`).

## Quality Baselines And Metrics

Quality protocol:

- Physical analog loopback must be after the Audio 8 DJ DAC path, not a software loopback (`docs/QUALITY_PASS_PROTOCOL.md:37-44`).
- Required run artifacts include build variant, summary, report path, sample rate/buffer/output gain, CPU, underruns, elastic events, sideband/noise metrics, and verdict (`docs/QUALITY_PASS_PROTOCOL.md:3-31`).
- Human/email handoff is allowed only after automated gates pass on the exact installed and loaded build (`docs/QUALITY_PASS_PROTOCOL.md:58-69`, `docs/MACOS_USB_CADENCE_IMPLEMENTATION_PLAN_2026-06-12.md:424-436`).

Physical music gate thresholds include alignment, peak level, clipping, mid/high residuals, p95/max/spread, quiet-band noise, click outliers, lag jumps, CPU correlation, and driver/coreaudiod CPU (`docs/PHYSICAL_MUSIC_QUALITY_GATE.md:48-82`).

Known physical route:

- `Open Audio 8 DJ output -> external mixer -> mixer REC OUT -> iRig Stream input -> macOS capture`.
- Do not disturb turntable wiring.
- Do not use software loopback as release proof.

References: `docs/AUDIO8DJ_CONNECTORS_AND_CAPTURE.md:42-85`, `docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:18-24`.

Baseline findings:

- 2026-06-13 physical baselines showed final-0324/ISO5 metrics and established that clean counters alone cannot qualify a candidate (`docs/QUALITY_RUNS_2026-06-13.md:3-30`).
- 0.2.55/0.2.63/0.2.64-era work showed the core tradeoff: lower ISO cadence could sound cleaner but raise CPU; higher ISO reduced CPU but often produced physical noise/clicks/no-output failures. The likely issue is IOUSBHost transport cadence/cost, not just packet bytes (`docs/QUALITY_RUNS_2026-06-13.md:724-810`).
- 2026-06-14 profiling showed the hot path dominated by IOUSBHost capture/playback enqueue, with packing/gain/stat loops smaller. That means simple sample/gain/stat tweaks are unlikely to solve the physical issue alone (`docs/QUALITY_RUNS_2026-06-14.md:718-734`).
- 0.3.133/0.3.135-era internal gates passed repeatedly with ISO64/q8 and stable CPU/counters, but iRig absence blocked physical qualification (`docs/QUALITY_RUNS_2026-06-14.md:950-1006`, `docs/QUALITY_RUNS_2026-06-15.md:15-33`).
- 0.3.136 and 0.3.137 were safe but did not materially improve the baseline; 0.3.138 was rejected due CPU safety regression; 0.3.135 was restored (`docs/QUALITY_RUNS_2026-06-15.md:35-116`).

## Recent Mainline Findings To Preserve

- `08745b7` is the last checked-out branch commit and represents the 0.3.25 Traktor preview. Later 0.3.135 facts are present in the dirty working tree and docs, so source archaeology must preserve both the committed preview surface and the newer uncommitted quality state.
- Core Audio surface in the current tree is richer than older capture-route docs that said the driver exposed no Core Audio inputs. Treat `docs/AUDIO8DJ_CONNECTORS_AND_CAPTURE.md` as still valid for cabling/release-proof route, but stale for HAL input topology.
- Old-driver reverse engineering points to input-completion-paced output slot lifecycle: input completion chooses a free output slot, derives output transaction lengths from the actual input low-latency frame list, and keeps the slot busy until output completion (`docs/OLD_DRIVER_COMPAT_PLAN.md:222-280`).
- Avoid forced USB `GetZeroTimeStamp`. Prior USB-zero-timestamp experiments produced instability/silent failures, and the current Makefile keeps it disabled (`docs/MACOS_USB_CADENCE_IMPLEMENTATION_PLAN_2026-06-12.md:30-36`, `Makefile:87-92`).
- Do not trust "zero underruns" as a listening gate. Physical iRig sidebands, residuals, clicks, lag jumps, CPU coupling, and human listening after gates are the real quality surface (`docs/USB_AUDIO_CADENCE_RESEARCH_2026-06-12.md:239-292`).

## Porting Guidance For `audio8djcpp`

1. Preserve HAL/Core Audio timing semantics first: stable sample/host timeline, normalized buffer sizes, supported rates, and no USB-clock public timestamp experiment.
2. Model USB as the real cadence domain: capture-paced playback, valid packet layout filtering, bounded output timeline, separate startup silence from active underruns, and explicit counters for drops/replays/late writes.
3. Preserve Mode 2 packet shape exactly before changing performance: 32-byte group structure, check-byte cadence, big-endian 24-bit default output, output start byte 4, and eight-channel internal frame order.
4. Keep profile-driven input decode: playback/output-only decode off; timecode/vinyl/CD/phono decode on with control-state compatibility.
5. Build metrics before behavior changes. Cadence diagnostics should be preallocated, aggregate, and free of per-transfer logging, allocation, broad locks, or file I/O in streaming paths (`docs/MACOS_USB_CADENCE_IMPLEMENTATION_PLAN_2026-06-12.md:117-127`).
6. Never label a C++ candidate as better than 0.3.135 unless it beats the baseline through the same ladder, including physical iRig gates when hardware is available.

## 2026-06-18 Metrics Archaeology Refresh

Read-only subagent `019eda9d-29ff-7902-88da-741f3b9a4c30` confirmed that the
mainline script implementation is stricter than some older documentation.
For C++ promotion, use the executable gate defaults from
`/Users/fer/dev/opena8dj/scripts/physical-music-quality-gate:428-459` when
they are stricter:

- alignment `>=0.970`;
- capture peak `0.020..0.920`;
- capture RMS `-28..-10 dBFS`;
- clipped frames `0`;
- mid residual `<=1.38`, p95 `<=1.40`, max `<=1.46`;
- mid spread p95/median `<=1.03`, max/median `<=1.06`;
- high residual `<=1.32`;
- metallic coloration `<=6 dB`;
- quiet mid residual `<=-32.5 dBFS`;
- click outliers/window clicks/clicks per second `0`;
- lag jumps over 2 frames `<=3`;
- time-warp lag drift `<=8.0` and score `>=0.85`;
- CPU/noise correlation `<=0.08`;
- driver CPU avg/p95 `<=8/12%`, coreaudiod p95 `<=8%`.

The older prose in `docs/PHYSICAL_MUSIC_QUALITY_GATE.md` uses looser values
such as alignment `0.925`, lag jumps `45`, and CPU correlation `0.16`. Those
values are useful history, not promotion thresholds for the C++ line.

The same read-only pass reaffirmed the baseline split:

- `0.3.135` for digital/no-iRig quality and CPU/resource comparison;
- `0.3.25` for 8-in/8-out and Traktor/timecode topology;
- `0.3.24` for historical physical tone floor;
- physical route proof still requires the external route
  `Open Audio 8 DJ -> external mixer -> mixer REC OUT -> iRig Stream -> macOS
  capture`.

## 2026-06-18 No-Repeat Findings From Mainline C

This pass was triggered after the C++ candidate reproduced a split already seen
in the C line: good-looking counters and useful Timecode behavior can coexist
with unusable white-noise or metallic playback. The following mainline findings
are treated as constraints for C++ work, not optional background.

### Do Not Repeat

- Do not retry `HAL_OUTPUT_START_BYTE=2` as a playback fix. Mainline tested the
  legacy mode-2 input cursor as an output byte start in 0.2.11 and documented it
  as loud white noise; playback byte start must stay at 4 unless a future
  packet-level proof explicitly disproves the old finding
  (`docs/OLD_DRIVER_COMPAT_PLAN.md:24-29`, `docs/OLD_DRIVER_COMPAT_PLAN.md:136-142`).
- Do not promote any build from underrun/panic counters alone. Mainline 0.2.29
  had clean automated counters but did not improve listening quality, and the
  current C++ output1/start variants repeated the same false-positive pattern
  (`docs/OLD_DRIVER_COMPAT_PLAN.md:302-312`).
- Do not use explicit future-frame USB scheduling as a quick path. The mainline
  records it as less stable than capture-paced immediate output and associated
  with output underproduction or drift (`docs/OLD_DRIVER_COMPAT_PLAN.md:27-29`).
- Do not expose USB micro-packet cadence as the public CoreAudio app buffer.
  Mainline corrected the public model back to 512-frame default and 512-4096
  frame range after small public cycle sizes interacted badly with input-client
  activation (`docs/OLD_DRIVER_COMPAT_PLAN.md:190-203`).
- Do not treat ISO grouping, OUT coalescing, QoS, queue-depth changes, reusable
  completions, or fixed-slot flags as untried simple wins. Mainline already ran
  a physical matrix across those families; lower ISO tended to sound cleaner
  with high CPU, while larger ISO/coalescing reduced CPU at the cost of clicks,
  noise, or no useful output (`docs/QUALITY_RUNS_2026-06-13.md:724-810`,
  `docs/QUALITY_RUNS_2026-06-14.md:42-130`,
  `docs/QUALITY_RUNS_2026-06-14.md:160-220`,
  `docs/QUALITY_RUNS_2026-06-14.md:736-748`).
- Do not poll hot stream stats during physical playback acceptance unless the
  test is explicitly about observability overhead. Mainline found playback-time
  stream-stats polling perturbed CPU/jitter and hardened the harness to avoid it
  by default (`docs/QUALITY_RUNS_2026-06-14.md:132-158`).
- Do not rebuild, reload, or ask for human listening while the iRig physical
  route is absent or idle capture is unhealthy. Mainline added early capture
  diagnostics and physical-bench sanity checks precisely to avoid wasting
  candidate cycles when the measurement route is invalid
  (`docs/TESTING.md:188-258`).

### Preserve As C++ Architecture

- Preserve the old-driver-derived transport hypothesis: continuous capture
  transfers, output queued from successful capture completions, 64 capture
  transfers, up to 128 output slots, output transaction lengths derived from the
  actual successful input frame list, and output slots freed only by output
  completion (`docs/OLD_DRIVER_COMPAT_PLAN.md:16-23`,
  `docs/OLD_DRIVER_COMPAT_PLAN.md:82-97`,
  `docs/OLD_DRIVER_COMPAT_PLAN.md:222-280`).
- Keep stable CoreAudio timing public and put device-clock following inside the
  USB transport/output-buffer model rather than in `GetZeroTimeStamp`
  (`docs/OLD_DRIVER_COMPAT_PLAN.md:54-73`).
- Keep playback-quality isolation separate from Timecode/input enablement.
  Mainline deliberately hid or disabled Audio 8 input I/O during output-only
  debugging after laptop-mic/input-client activation perturbed playback. The C++
  line still needs full input for Traktor, but output-quality candidates must be
  measured in a controlled playback profile before being combined with duplex
  Timecode (`docs/OLD_DRIVER_COMPAT_PLAN.md:205-216`,
  `docs/TESTING.md:561-583`).
- Preserve the Traktor-facing surface from 0.3.25: four stereo outputs A/B/C/D,
  one 8-channel input stream with A/B/C/D stereo pairs, timecode-vinyl mode 0,
  software lock on, and first validation at 44.1/48 kHz
  (`docs/TRAKTOR_TIMECODE.md:7-23`, `docs/TRAKTOR_TIMECODE.md:39-109`).
- Use the automated real-music source-reference ladder before human listening:
  prepare deterministic music fixture, run simulated output when no physical
  route exists, run analog iRig capture when the route exists, then compare
  alignment/SNR/residual/clicks/lag/clipping/CPU coupling
  (`docs/AUTOMATED_SOUNDCHECK.md:1-70`,
  `docs/PHYSICAL_MUSIC_QUALITY_GATE.md:48-82`).

### Current Baseline Meaning

- `0.3.135` is the current no-iRig/software baseline, not a physical-quality
  winner. It passed digital/no-iRig gates with stable resource numbers after
  0.3.136 and 0.3.137 failed to improve it, and 0.3.138 overheated the stack
  (`docs/QUALITY_RUNS_2026-06-15.md:7-116`).
- `0.3.64` and `0.3.111` are useful physical/cadence evidence points, not
  release targets. They show the physical-quality/CPU tradeoff and identify
  IOUSBHost isochronous enqueue pressure as the dominant cost center
  (`docs/QUALITY_RUNS_2026-06-13.md:782-810`,
  `docs/QUALITY_RUNS_2026-06-14.md:718-734`,
  `docs/QUALITY_RUNS_2026-06-14.md:750-759`).
- The C++ line must beat mainline by measured evidence across source-reference
  playback quality, CPU/resource use, routing A/B/C/D, input/timecode behavior,
  and physical route stability. Compilation, enumeration, clean counters, or
  Timecode success alone are not enough.

## 2026-06-18 Reusable Mainline Flags And Parameters

This inventory separates reusable behavior from rejected compile-time toggles.
The C++ line should convert stable choices into typed policy/configuration, not
carry over a large Makefile-style flag surface.

### Adopt As Product Policy

- Channel topology:
  - 8 hardware inputs and 8 hardware outputs;
  - four stereo output pairs A/B/C/D;
  - one 8-channel input stream with A/B/C/D stereo pairs for Traktor/timecode;
  - sample rates 44.1 and 48 kHz as first product gates, with 88.2/96 kHz kept
    as protocol-supported but lower-priority validation targets.
  References: `Makefile:66-70`, `docs/TRAKTOR_TIMECODE.md:7-23`,
  `docs/TESTING.md:473-486`.
- Public CoreAudio buffer policy: 512-frame default/preferred buffer, exposed
  range 512/1024/2048/4096. Do not expose USB micro-packet cadence to clients
  (`docs/OLD_DRIVER_COMPAT_PLAN.md:190-203`).
- Output sample format and packing:
  - Float32 input from CoreAudio;
  - signed 24-bit output payload;
  - big-endian I24 bytes by default;
  - mode-2 check-byte cadence preserved;
  - output start byte 4;
  - product output headroom/gain starts at 0.50.
  References: `Makefile:64`, `Makefile:82`, `Makefile:100`,
  `docs/HANDOFF_2026-06-10_AUDIO_CRACKLE.md:125-139`,
  `docs/HANDOFF_2026-06-10_AUDIO_CRACKLE.md:519-525`,
  `scripts/validate-mode2-output-packing.py:11-24`.
- Input decode policy:
  - playback/output-only/VLC/Spotify profile keeps Audio 8 input decode off;
  - timecode-vinyl/timecode-cd-line/phono profile enables input decode;
  - `timecode-vinyl` uses input mode 0 with software lock on.
  References: `docs/QUALITY_RUNS_2026-06-13.md:561-615`,
  `docs/TRAKTOR_TIMECODE.md:39-88`.
- Timing/scheduling policy:
  - keep HAL/CoreAudio public timing stable;
  - keep USB zero timestamp and forced public USB clock anchoring off;
  - keep explicit isochronous future scheduling off;
  - use capture-paced output with lead 1 as the safe behavioral baseline.
  References: `Makefile:73-81`, `docs/QUALITY_RUNS_2026-06-12.md:1275-1350`,
  `docs/TESTING.md:488-492`.
- Stream startup policy:
  - send reset-style `AUDIO_PARAMS` before real stream parameters;
  - do not skip it for start-latency reasons because the mainline skip test did
    not improve client start time.
  References: `Makefile:108`, `docs/QUALITY_RUNS_2026-06-14.md:847-853`.
- Real-time build policy:
  - default optimization equivalent to `-O2`, not `-O3`;
  - diagnostic capture/log-heavy paths disabled in product builds;
  - no per-buffer or per-transfer disk I/O.
  References: `Makefile:63`, `Makefile:105`,
  `docs/QUALITY_RUNS_2026-06-14.md:256-263`,
  `docs/HANDOFF_2026-06-10_AUDIO_CRACKLE.md:533-545`.

### Reusable As Diagnostics Or Gates

- Keep a no-wake control read equivalent to `OPENA8DJ_CONTROL_NO_WAKE=1`, so
  reading stats cannot itself wake or perturb CoreAudio. Mainline added this
  after monitor reads caused false activity (`docs/QUALITY_RUNS_2026-06-13.md:500-513`).
- Keep player timing equivalent to `OPENA8DJ_PLAYER_TIMING=1` only in tools, not
  in the real-time driver path (`scripts/playback-cpu-gate:564`,
  `src/tools/audio-wav-play.c:37`).
- Keep offline mode-2 packing validation with:
  - all A/B/C/D streams;
  - default start byte 4;
  - transfer byte target 352 at 48 kHz;
  - check-byte/panic-flag validation.
  References: `scripts/validate-mode2-output-packing.py:11-24`,
  `scripts/validate-mode2-output-packing.py:334-410`,
  `docs/TESTING.md:483-510`.
- Keep simulated-output and real-music gates as first-class C++ QA tools:
  preflight fixture, simulated output when no hardware route exists, physical
  iRig source-reference capture when it does (`docs/AUTOMATED_SOUNDCHECK.md:1-70`).
- Keep physical route readiness checks equivalent to:
  - exact iRig USB/CoreAudio identity;
  - short healthy capture before full gate;
  - route-dirty classification for sane-level catastrophic mismatches.
  References: `docs/QUALITY_RUNS_2026-06-14.md:649-667`,
  `docs/QUALITY_RUNS_2026-06-14.md:1010-1080`.
- Keep cadence/stream counters, but sample them outside the hot path and never
  let them replace the physical capture gate. `HAL_HOT_STREAM_STATS_INTERVAL`
  showed that sampling telemetry can reduce overhead slightly, but it was not a
  standalone product win (`docs/QUALITY_RUNS_2026-06-14.md:864-873`,
  `docs/QUALITY_RUNS_2026-06-15.md:35-73`).

### Useful Hypotheses, Not Reusable Product Flags

- `HAL_ISO_FRAMES=64`, capture/playback queues `8/8`, output prefetch 64,
  preopen-on-init, and stop-ISO-on-stop are the best no-iRig/software internal
  baseline family. They produced excellent CPU and start latency internally, but
  are not physically qualified because earlier high-ISO families often sounded
  worse. Treat this as a C++ transport hypothesis requiring physical proof, not
  as a product default (`docs/QUALITY_RUNS_2026-06-14.md:883-941`,
  `docs/QUALITY_RUNS_2026-06-14.md:950-1006`,
  `docs/QUALITY_RUNS_2026-06-15.md:15-33`).
- ISO5 with deeper capture queue remains the safer physical-cadence clue, but
  CPU is too high. Reuse the cadence/lifecycle model, not the exact Objective-C
  implementation or its CPU profile (`docs/QUALITY_RUNS_2026-06-12.md:1312-1349`,
  `docs/QUALITY_RUNS_2026-06-13.md:782-810`,
  `docs/QUALITY_RUNS_2026-06-14.md:574-588`).
- Background preopen/open-while-idle is a useful latency idea only if the C++
  implementation proves idle CPU stays low and playback stats stay reliable.
  Mainline warm-open variants were mixed until the later keep-open design
  (`docs/QUALITY_RUNS_2026-06-14.md:855-881`,
  `docs/QUALITY_RUNS_2026-06-14.md:918-941`,
  `docs/QUALITY_RUNS_2026-06-14.md:950-978`).

### Do Not Reuse As Product Flags

- `HAL_OUTPUT_START_BYTE=2`: known white-noise playback failure.
- `HAL_OUTPUT_NATIVE=1`: catastrophic native-I24 physical failure in mainline.
- `HAL_EXPLICIT_SCHED=1` / USB zero timestamp / public USB-clock anchoring:
  unstable or wrong layer for this HAL path.
- `HAL_OPTFLAGS=-O3`: safety failure with hot `coreaudiod`.
- `HAL_FLUSH_TOUCHED_OUTPUT=1`: worsened tone/click behavior in C and is not a
  safe general fix for single-pair clients.
- `HAL_STREAM_USAGE=1`: intended to reduce unnecessary input work, but worsened
  physical tone in mainline and caused a C++ safety spike in the current run.
- `HAL_CAPTURE_QUEUE=128`: worsened tone/click behavior.
- `HAL_PLAYBACK_COALESCE_TRANSFERS>1`: lowered CPU in some runs but produced no
  useful output or physical degradation.
- `HAL_REUSE_ISOC_COMPLETIONS=1`, `HAL_FAST_ISO_TRANSFER_CONFIG=1`,
  `HAL_LEGACY_OUT_SLOTS=1`, `HAL_TRANSFER_POOL_CURSOR=1`, and
  `HAL_STRICT_TRANSFER_POOL=1`: useful for understanding lifecycle problems, but
  the flag implementations caused hot CoreAudio, no output, or no measured
  product win. Reimplement the lifecycle cleanly in C++ only with new gates.
- `HAL_USB_QUEUE_QOS=2`: no CPU win and worse physical residual/click behavior.
- `HAL_STOP_ISOC_ON_STOP=0`: created active underruns/resets after StopIO.
- `HAL_BACKGROUND_WARM_OPEN=1` as originally implemented: can make gates blind to
  active playback; only the later open-but-not-streaming idea is worth redesign.
- `HAL_HOT_STREAM_STATS=0`, `HAL_OUTPUT_AMPLITUDE_STATS=0`,
  `HAL_OUTPUT_WRITE_STATS=0`, and atomic hot-count rewrites: not product wins;
  some destabilized the stack. Replace with C++ observability snapshots instead
  of compile-time stat-removal toggles.
