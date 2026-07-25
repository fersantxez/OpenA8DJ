# Reboot Handoff - Audio QA - 2026-06-12

This handoff is for resuming immediately after the laptop reboots.

## Installed Driver State

- Active HAL bundle: `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`.
- Active device name: `Open Audio 8 DJ`.
- Active Core Audio UID: `org.opena8dj.Audio8DJ`.
- Active version before reboot: `0.2.32` build `34`.
- Active hash before reboot:
  `bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434`.
- This is the restored/frozen baseline, not a new candidate.

Do not ask for human listening from this state as a candidate. Continue with
physical iRig QA first.

## Physical QA Route

Confirmed physical route:

```text
Open Audio 8 DJ output -> external mixer -> mixer REC OUT -> iRig Stream input -> macOS capture
```

Core Audio devices before reboot:

```text
iRig Stream      in=2 out=2 rate=48000
Open Audio 8 DJ  in=0 out=8 rate=48000
```

The Audio 8 DJ still exposes no Core Audio inputs in the current driver.

## Known Good Capture Proofs

Strong VLC route proof:

- Run:
  `local-analysis/irig-stream-capture/vlc-long-route-proof-20260612-163849`.
- Captured file:
  `local-analysis/irig-stream-capture/vlc-long-route-proof-20260612-163849/capture.wav`.
- Result:
  - Duration: `49.99 s`.
  - RMS: `-23.16 dBFS` left, `-24.33 dBFS` right.
  - Peak: `-12.00 dBFS` left, `-11.45 dBFS` right.
  - Clipping: `0`.
  - Active seconds detected: `46`.
  - Final health: `OpenA8DJ.driver=0.0%`, `coreaudiod=0.1%`.

Audible-pattern direct route proof:

- Run:
  `local-analysis/irig-stream-capture/audible-route-proof-20260612-162429`.
- Test signal:
  alternating 440 Hz left / 880 Hz right, first through pair A then pair B.
- Result:
  - RMS around `-21.6 dBFS`.
  - Peak around `-7.3 dBFS`.
  - Clipping: `0`.
  - Idle noise floor around `-71 dBFS`.

If the user does not hear the test in headphones, do not conclude the capture
failed. The mixer headphone monitor may not be listening to the same master/REC
path. Trust the iRig capture levels when they show the expected pattern.

## Ambiguous Or Failed Measurements

The music waveform comparator failed through physical iRig capture:

- Pair A:
  `local-analysis/soundcheck/2026-06-12T160644`.
  - Alignment: `0.961347`.
  - SNR: `9.17 dB`.
  - 1-5 kHz residual ratio: `1.667333`.
  - Lag jumps: `64`.
- Pair B:
  `local-analysis/soundcheck/2026-06-12T160737`.
  - Alignment: `0.912414`.
  - SNR: `5.91 dB`.
  - 1-5 kHz residual ratio: `2.639969`.
  - Lag jumps: `66`.

Interpretation: these are real post-card captures, but the current analyzer is
too strict for two independent USB clocks. Add drift/time-warp compensation
before treating waveform SNR as a final pass/fail metric.

Tone spectrum check:

- Run:
  `local-analysis/irig-stream-capture/audio8-tone-thdn-20260612-160916`.
- A 1 kHz tone produced sidebands around the tone, especially near 940 Hz and
  1060 Hz.
- This may match the user-described "old vinyl / badly tuned radio" texture.
- Next analyzer gate should explicitly track sideband modulation around a test
  tone.

## Startup Automation

User LaunchAgents:

- Existing:
  `/Users/fer/Library/LaunchAgents/com.fer.opena8dj.codex-resume.plist`
  opens Codex on `/Users/fer/dev/opena8dj`.
- Added:
  `/Users/fer/Library/LaunchAgents/com.fer.opena8dj.audio-qa-startup.plist`
  runs `scripts/post-reboot-audio-test-startup`.

On login, the audio QA startup script:

- Waits for `Open Audio 8 DJ` and `iRig Stream`.
- Sets default output to `Open Audio 8 DJ`.
- Records an 8 s iRig baseline capture.
- Runs `scripts/audio-stack-health` before any optional media apps.
- Opens Codex on `/Users/fer/dev/opena8dj` and writes `open-codex.log`.
- Writes `RUN_MANIFEST.txt` and `RESUME_FOR_CODEX.md` in the run directory.
- Skips VLC, Spotify, and Traktor by default to avoid surprise playback. Set
  `OPEN_AUDIO_QA_MEDIA_APPS=1` in the LaunchAgent environment to restore the
  older app-opening behavior.
- Runs `scripts/audio-stack-health` again after the quiet startup/app phase.
- Writes logs to:
  `local-analysis/startup/latest`.

The startup script does not auto-play audio.

Startup automation verification before reboot:

- LaunchAgent loaded successfully:
  `com.fer.opena8dj.audio-qa-startup`.
- Manual kickstart run:
  `local-analysis/startup/post-reboot-20260612-165723`.
- Exit code: `0`.
- Devices were ready on attempt `1`.
- iRig startup baseline:
  - Duration: `8.0107 s`.
  - RMS: `-75.52 dBFS` left, `-75.38 dBFS` right.
  - Peak: `-62.01 dBFS` left, `-61.68 dBFS` right.
  - Clipping: `0`.
- Health before apps: `PASS`.
  - `OpenA8DJ.driver=0.0%`.
  - `coreaudiod=0.0%`.
- Health after quiet startup/app phase: `PASS`.
  - `OpenA8DJ.driver=0.0%`.
  - `coreaudiod=0.0%`.
- Codex open step completed:
  `local-analysis/startup/post-reboot-20260612-165723/open-codex.log`.

## First Steps After Reboot

Run:

```text
./build/audio-list
./build/audio-default
./scripts/audio-stack-health --wait 1
```

Then inspect:

```text
local-analysis/startup/latest/audio-stack-health.txt
local-analysis/startup/latest/RUN_MANIFEST.txt
local-analysis/startup/latest/RESUME_FOR_CODEX.md
local-analysis/startup/latest/irig-startup-baseline-summary.json
local-analysis/startup/latest/NEXT_TESTS.txt
```

Next engineering task:

- Update the physical-capture analyzer for independent clocks.
- Add a tone sideband/modulation gate.
- Only after those pass, build or load a new candidate for human listening.
