# Linux Quality and Performance Gates

The Linux track must not claim audiophile quality until physical evidence
supports it.

```text
diagnostic only, sound quality not validated
```

## Rule

Passing build checks, enumeration, and PCM smoke tests is not enough. A Linux
candidate becomes a listening candidate only after the exact built and loaded
artifact has passed physical audio validation.

## Shared Hardware Lock

Hardware-affecting gates require:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

The lock is mandatory for:

- Audio 8 DJ playback or capture.
- iRig or other external capture.
- USB reset, driver bind/unbind, module load/unload against live hardware.
- Traktor tests.
- CPU/audio latency measurements.
- Any test that changes sample rate, buffer size, default device, or live
  routing.

Documentation, source audit, static analysis, and non-installing builds do not
require the lock.

## Validation Ladder

### 1. Build Hygiene

Goal: prove the tree builds cleanly without touching live hardware.

Evidence:

- Kernel version and headers used.
- Exact branch and commit.
- Compiler version.
- Warnings and static analysis results.
- Confirmation that no macOS/Windows install scripts were touched.

Required before moving on:

- No accidental live driver install.
- No claimed readiness from placeholder targets.

### 2. Enumeration

Goal: prove the driver enumerates Audio 8 DJ as intended.

Evidence:

- `lsusb -v` for `17cc:1978`.
- `dmesg` probe logs.
- ALSA card listing.
- PCM playback/capture devices.
- rawmidi devices.
- ALSA controls.
- Channel-map visibility where supported.

Failure policy:

- Do not continue to audio smoke tests if controls, rawmidi, or channel counts
  are missing.

### 3. PCM Smoke

Goal: prove basic non-destructive PCM behavior.

Evidence:

- 44.1 kHz playback open/prepare/trigger/stop.
- 48 kHz playback open/prepare/trigger/stop.
- 44.1 kHz capture open/prepare/trigger/stop.
- 48 kHz capture open/prepare/trigger/stop.
- Full-duplex open/start/stop at both rates.
- `pointer` monotonicity.
- No hidden XRUNs.

This gate does not prove sound quality.

### 4. Routing Matrix

Goal: prove A/B/C/D isolation and channel order.

Evidence:

- Pair A L/R maps only to channels 1/2.
- Pair B L/R maps only to channels 3/4.
- Pair C L/R maps only to channels 5/6.
- Pair D L/R maps only to channels 7/8.
- Capture A/B/C/D maps back in the same order.
- No channel swap after sample-rate changes or client restart.

### 5. CPU and Latency

Goal: prove the implementation is PREEMPT_RT-friendly and DJ-safe.

Evidence:

- CPU usage under idle, playback, capture, and full-duplex.
- Period wakeup stability.
- XRUN counts under realistic PipeWire/JACK settings.
- Callback duration or trace evidence for bounded completion handlers.
- No logging in the hot path during steady state.

Hardware lock required.

### 6. Physical Capture

Goal: prove real audio quality through hardware.

Evidence:

- Real music playback through Audio 8 DJ.
- External capture through a known-good capture path, such as iRig Stream when
  available.
- WAV-vs-reference comparison.
- Residual/noise/clipping/click metrics.
- Human listening only after measurements are clean.
- Exact loaded module hash and kernel version.

This is the first gate that can support an audiophile-quality claim.

### 7. DVS and MIDI

Goal: prove Traktor/DVS and MIDI use cases.

Evidence:

- Timecode vinyl input stability.
- Timecode CD/line input stability.
- Ground lift and input mode behavior.
- rawmidi in/out through `aconnect`.
- PipeWire/JACK MIDI visibility.
- No PCM timing degradation during MIDI traffic.

### 8. Resilience

Goal: prove the driver behaves safely under real lifecycle stress.

Evidence:

- Hot unplug while idle.
- Hot unplug while streaming.
- Suspend/resume.
- PipeWire restart.
- JACK restart.
- Client crash while stream is active.
- Driver unbind/rebind with lock held.
- Error counters retained and visible.

## Hot-Path Performance Contract

The isochronous completion path must not perform:

- Heap allocation.
- Unbounded logging.
- String formatting.
- Wide locks.
- Control command round trips.
- User-space policy work.
- Variable scans unrelated to the fixed stream layout.

Allowed work:

- Bounded packet status checks.
- Bounded sample copy/pack/unpack.
- Fixed-ring pointer updates.
- Period elapsed accounting.
- URB resubmission.

## Readiness Labels

Use these labels exactly:

- `design only`: documents or plans with no code path.
- `diagnostic only, sound quality not validated`: code or scaffold that may help
  inspection but lacks physical quality validation.
- `enumerates only`: hardware appears but PCM quality is unproven.
- `pcm smoke passed`: basic PCM operation passed but sound quality unproven.
- `candidate for physical capture`: ready to run the physical capture gate.
- `candidate for human listening`: exact artifact passed physical capture.
- `release candidate`: human listening and regression gates passed.

The current Linux package is:

```text
diagnostic only, sound quality not validated
```
