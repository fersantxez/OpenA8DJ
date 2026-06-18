# Mainline Physical Route Notes

Source scope: read-only archaeology of `/Users/fer/dev/opena8dj` on 2026-06-18.
No audio tests, hardware probes, CoreAudio changes, USB resets, installs, builds,
formatting, or mutations were run in mainline or Rust. This note is the only
written output from the pass.

## Hard boundary

- `/Users/fer/dev/opena8dj` and `/Users/fer/dev/audio8djrust` are read-only for
  this C++ redesign evidence pass.
- Hardware/audio/CoreAudio/USB actions require the global lock and an approved
  test window. This pass did not acquire the lock because it did not touch the
  hardware path.
- The shared lock contract is documented in
  `/Users/fer/dev/opena8dj/docs/SHARED_HARDWARE_COORDINATION.md:1-47` and
  `/Users/fer/dev/opena8dj/docs/SHARED_HARDWARE_COORDINATION.md:82-105`.

## Confirmed physical route

Mainline's decisive physical QA route is:

```text
Open Audio 8 DJ output -> external mixer -> mixer REC OUT -> iRig Stream input -> macOS capture
```

Evidence:

- `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:18-33`
  records the confirmed route, device names, and Core Audio shape:
  `iRig Stream in=2 out=2 rate=48000` and `Open Audio 8 DJ in=0 out=8
  rate=48000`.
- `/Users/fer/dev/opena8dj/docs/AUDIO8DJ_CONNECTORS_AND_CAPTURE.md:42-53`
  explains why the safe capture path is post-DAC through a mixer output into a
  separate USB recorder, without disturbing turntable/multicore wiring.
- `/Users/fer/dev/opena8dj/docs/AUDIO8DJ_CONNECTORS_AND_CAPTURE.md:55-72`
  identifies `iRig Stream` as the confirmed external capture device and the
  soundcheck capture arguments: `--capture-device "iRig Stream"
  --capture-channels 1,2`.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:3-8` records the
  calibrated physical route as `Open Audio 8 DJ -> external mixer -> mixer REC
  OUT`, capture `iRig Stream`, channels `1,2`, `48 kHz`, fixture
  `nueva-mexico-baseline-268s-60s-48k-s16.wav`, and calibrated test level
  `--target-peak-db -16`.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-13.md:727-734` says the
  decisive route remains `Open Audio 8 DJ -> mixer REC OUT -> iRig Stream` and
  warns that clipped iRig runs are not accepted as fidelity proof.

## Known route proof artifacts

- Strong VLC route proof:
  `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:35-50`
  points to
  `local-analysis/irig-stream-capture/vlc-long-route-proof-20260612-163849`.
  The saved capture summary at
  `/Users/fer/dev/opena8dj/local-analysis/irig-stream-capture/vlc-long-route-proof-20260612-163849/capture-summary.json:1-11`
  shows 49.99 s, 48 kHz, RMS about -23/-24 dBFS, peaks about -12/-11 dBFS,
  zero clipping, and 46 active seconds.
- Audible-pattern route proof:
  `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:51-65`
  points to
  `local-analysis/irig-stream-capture/audible-route-proof-20260612-162429` and
  records alternating left/right tones, no clipping, and idle noise around
  -71 dBFS.
- The reboot handoff warns not to treat lack of headphone monitor audio as a
  capture failure if the mixer headphone monitor is not listening to the same
  master/REC path:
  `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:63-65`.

## Mainline soundcheck workflow

The mainline validation ladder is documented in
`/Users/fer/dev/opena8dj/docs/AUTOMATED_SOUNDCHECK.md:36-70`:

1. `make soundcheck-preflight`
2. `make soundcheck SOUNDCHECK_CAPTURE="..."`
3. normal compile/smoke checks
4. `make simulated-output-soundcheck` when no physical path exists
5. full `make soundcheck` only when a real loopback/capture path exists

Makefile defaults and targets:

- Version anchor in this checkout: `/Users/fer/dev/opena8dj/Makefile:1-2`
  records `VERSION := 0.3.135`.
- Soundcheck defaults: `/Users/fer/dev/opena8dj/Makefile:110-126` sets
  capture channels `1,2`, pair `A`, rate `48000`, buffer `512`, full seconds
  `20`, preflight seconds `5`, full mode `dense`, and preflight mode `start`.
- Physical/candidate defaults:
  `/Users/fer/dev/opena8dj/Makefile:148-186` uses `iRig Stream` for capture
  diagnosis and physical bench, and points physical comparisons at
  `local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615/physical-music-gate.json`.
- `soundcheck-preflight` and `soundcheck` command expansion is in
  `/Users/fer/dev/opena8dj/Makefile:343-356`.
- Candidate, iRig recovery, quality-window, and shared-lock targets are in
  `/Users/fer/dev/opena8dj/Makefile:421-523`.

`scripts/run-soundcheck` behavior C++ should mirror or remain compatible with:

- It blocks full analog runs without `--capture-device`:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:761-767`.
- It blocks virtual/pre-device capture routes unless explicitly overridden for
  diagnostics:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:769-781`.
- `--prepare-only` creates the real-music fixture and exits with a
  `SOUNDCHECK PREFLIGHT: READY` summary:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:791-803`.
- It configures `org.opena8dj.Audio8DJ` to requested rate/buffer before full
  runs:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:805-812`.
- It records `captured.wav` from the selected device/channels while playing the
  reference through the requested output pair:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:817-838`.
- If the capture device name looks like iRig, it runs
  `scripts/physical-music-quality-gate` automatically and writes
  `physical-music-gate.txt`, `physical-music-gate.json`, and
  `physical-coupling-profile.json`:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:859-919`.
- The summary fields include alignment, time warp/drift, SNR, 1-5 kHz residual,
  high-band residual, quiet mid-band noise, CPU/noise correlation, click
  outliers, lag jumps, clipping, driver CPU, and coreaudiod CPU:
  `/Users/fer/dev/opena8dj/scripts/run-soundcheck:643-684`.

## Physical quality gate

Mainline treats physical iRig evidence as mandatory for release or human
listening:

- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-13.md:1-6` says internal
  counters, CPU, and underrun checks are necessary but not sufficient; a
  candidate needs physical iRig tone/music gates and must beat the listening
  baseline.
- `/Users/fer/dev/opena8dj/docs/QUALITY_PASS_PROTOCOL.md:37-45` says software
  loopback, virtual capture, aggregate routes, and system/player stream capture
  do not prove what the Audio 8 DJ DAC/USB cadence produced.
- `/Users/fer/dev/opena8dj/docs/QUALITY_PASS_PROTOCOL.md:46-56` requires keeping
  CPU/coupling artifacts when analog capture is available.
- `/Users/fer/dev/opena8dj/docs/PHYSICAL_MUSIC_QUALITY_GATE.md:1-10` defines the
  gate as analysis of existing reference/capture files from the Audio 8 DJ ->
  mixer -> iRig route.
- `/Users/fer/dev/opena8dj/docs/PHYSICAL_MUSIC_QUALITY_GATE.md:48-87` lists the
  default absolute and baseline-relative PASS/FAIL checks.
- `/Users/fer/dev/opena8dj/docs/PHYSICAL_MUSIC_QUALITY_GATE.md:83-93` explains
  why p95/median spread and spectral-coloration checks matter for intermittent
  crackle, CPU-coupled noise, and metallic/high-pass failures.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2277-2310` adds the
  coloration metrics and validates them against archived physical baseline
  `0.3.111`.

Important mainline physical baseline artifact:

- `/Users/fer/dev/opena8dj/local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615/summary.txt:1-37`
  is a 20 s physical iRig run on pair A at 48 kHz / 512 buffer. It failed strict
  gates but is the current comparator JSON used by Make defaults.
- `/Users/fer/dev/opena8dj/local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615/physical-music-gate.txt:1-49`
  records representative baseline numbers: alignment `0.97035576`, mid-band
  residual `1.43190631`, high-band residual `1.34802799`, quiet mid noise
  `-36.58 dBFS`, 2 clicks, 49 lag jumps, driver avg/p95 `19.97/25.57%`, and
  coreaudiod p95 `16.28%`. It fails strict release thresholds, so C++ should
  treat it as a practical comparator/floor, not as a release-quality target.

## iRig loss and recovery evidence

Known recovery facts:

- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-13.md:646-662` records a
  case where iRig was physically plugged in, USB still showed it, but Core Audio
  did not publish it until USB-audio/Core Audio daemons were restarted. After
  recovery, iRig was 2 in / 2 out at 48 kHz and a capture-open proof succeeded
  on channels `1,2`.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2020-2024` records the
  harder case: if iRig is missing from the USB tree, software cannot reset it by
  id because no iRig USB object exists.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2122-2188` records
  richer readiness output: USB enumeration failure fields, failed hub port
  detail, and `next_recovery_action`.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2222-2275` adds stable
  iRig readiness before long physical gates; default stable poll threshold is 3.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2423-2508` documents
  the autonomous recovery supervisor: it refreshes Core Audio/USB audio services,
  attempts only an exact iRig reset (`0x1963:0x0059`), never resets Audio 8 DJ,
  and runs full gates only after Audio 8 DJ and iRig are stable.

`scripts/capture-device-diagnose` contract:

- Usage says it is read-only and does not start audio, reset USB, or restart
  Core Audio:
  `/Users/fer/dev/opena8dj/scripts/capture-device-diagnose:12-25`.
- It keys the mainline Obj-C driver by exact UID `org.opena8dj.Audio8DJ` and
  distinguishes Rust UID `org.opena8dj.Audio8DJ-rust`:
  `/Users/fer/dev/opena8dj/scripts/capture-device-diagnose:103-107` and
  `/Users/fer/dev/opena8dj/scripts/capture-device-diagnose:279-283`.
- It matches iRig by IK Multimedia / `iRig Stream` name and by VID/PID
  `0x1963:0x0059`:
  `/Users/fer/dev/opena8dj/scripts/capture-device-diagnose:257-288`.
- It emits `physical_capture_status`, reason, Audio 8 DJ visibility, iRig USB
  and Core Audio visibility, external input count, failed USB ports, and
  `next_recovery_action`:
  `/Users/fer/dev/opena8dj/scripts/capture-device-diagnose:342-418`.

Saved 0.3.135 missing-iRig status:

- `/Users/fer/dev/opena8dj/local-analysis/quality-window/0.3.135-atomic-written-candidate-20260614-204913/result.txt:1-5`
  is blocked as `BLOCKED_PHYSICAL_CAPTURE`, reason `irig_missing_from_usb_tree`.
- `/Users/fer/dev/opena8dj/local-analysis/quality-window/0.3.135-atomic-written-candidate-20260614-204913/capture-device-diagnose-before/result.txt:1-23`
  shows Audio 8 DJ present, iRig absent from USB/CoreAudio, external Core Audio
  inputs 0, USB enumeration failures on `AppleUSB20HubPort`.
- `/Users/fer/dev/opena8dj/local-analysis/candidate-status/0.3.135-final-status-fresh-irig-recovery-20260614-205640/capture-device-diagnose/result.txt:1-23`
  records the same missing-iRig state and next action
  `run_autonomous_audio_qa_software_recovery`.
- `/Users/fer/dev/opena8dj/local-analysis/candidate-status/0.3.135-final-status-fresh-irig-recovery-20260614-205640/audio-stack-health.txt:1-12`
  shows the audio stack itself was PASS/idle at that point, so the blocker was
  physical capture readiness, not hot Core Audio.

## Post-reboot/startup evidence

- `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:98-124`
  describes the startup automation: wait for `Open Audio 8 DJ` and `iRig
  Stream`, set default output, record an 8 s iRig baseline if ready, run stack
  health, write `RUN_MANIFEST.txt` and `RESUME_FOR_CODEX.md`, skip VLC/Spotify/
  Traktor by default, and write to `local-analysis/startup/latest`.
- `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:148-172`
  lists first post-reboot checks and says the next engineering task was
  independent-clock physical analyzer work plus tone sideband/modulation gating
  before human listening.
- `/Users/fer/dev/opena8dj/scripts/post-reboot-audio-test-startup:44-77`
  writes the manifest/resume handoff and states no human listening until physical
  gates pass.
- `/Users/fer/dev/opena8dj/scripts/post-reboot-audio-test-startup:90-147`
  polls Audio 8 DJ and iRig readiness through `capture-device-diagnose`.
- `/Users/fer/dev/opena8dj/scripts/post-reboot-audio-test-startup:168-179`
  records the 8 s iRig startup baseline only if both devices are ready.
- `/Users/fer/dev/opena8dj/scripts/post-reboot-audio-test-startup:268-294`
  records the recommended physical soundcheck command:

```text
./scripts/run-soundcheck --skip-build --music-file "local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav" --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 20 --mode dense --target-peak-db -18 --drift-profile --cpu-profile
```

Saved post-reboot missing-iRig artifact:

- `/Users/fer/dev/opena8dj/local-analysis/startup/post-reboot-20260614-143609/device-readiness.txt:1-6`
  shows Audio 8 DJ present but iRig missing from USB/Core Audio.
- `/Users/fer/dev/opena8dj/local-analysis/startup/post-reboot-20260614-143609/irig-startup-baseline-summary.json:1-7`
  shows startup iRig baseline was skipped because `irig_missing_from_usb_tree`.

## Mainline baseline and current candidate context

- Historical frozen reboot baseline:
  `/Users/fer/dev/opena8dj/docs/REBOOT_HANDOFF_2026-06-12_AUDIO_QA.md:5-16`
  records active HAL `OpenA8DJ.driver`, device UID `org.opena8dj.Audio8DJ`,
  version `0.2.32` build `34`, SHA256
  `bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434`.
  It is a restored/frozen baseline, not a new candidate.
- Later physical route conclusions:
  `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-13.md:782-810` says `0.3.64`
  was the best loaded physical-sound recovery, but not release quality because
  CPU and pair-B click/noise remained too high; ISO5 normal OUT cadence seemed
  required for acceptable physical output.
- 0.3.135 loaded/no-iRig context:
  `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2601-2678` records
  0.3.135 as loaded and internally stronger, with output pair/timecode/playback
  CPU PASS and no-iRig click-risk PASS, but blocked from human listening because
  the physical iRig gate could not run.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md:3-33` records the
  no-iRig baseline pass for 0.3.135 and says physical iRig capture remains
  mandatory before release or human listening.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md:96-116` records
  restored 0.3.135 verification and final no-iRig state as the best measured
  software-only candidate from that pass.
- `/Users/fer/dev/opena8dj/local-analysis/digital-audio-quality-gate/0.3.135-no-irig-baseline-20260615-001316-20260615-001316/result.txt:1-74`
  is the saved digital/no-iRig proof: simulated A/B/C/D PASS, alignment 1.0,
  SNR about 75.22 dB, residual ratio 0.000669, repeated click-risk PASS, and
  explicitly `physical_gate_required_after_this=1`,
  `physical_gate_replaced_by_this=0`.

## What C++ must replicate for fair comparison

Minimum comparison contract:

1. Use the same physical chain for post-device evidence:
   `Open Audio 8 DJ output -> external mixer -> mixer REC OUT -> iRig Stream
   input -> macOS capture`.
2. Use `iRig Stream` as the external capture device, channels `1,2`, 48 kHz.
3. Treat Audio 8 DJ's own Core Audio inputs as unavailable unless C++ implements
   and validates input capture separately. Mainline route assumes the current
   OpenA8DJ Core Audio shape can be output-only for physical measurement.
4. Keep software/digital gates as preflight only. A digital PASS never replaces
   physical iRig evidence.
5. Generate or preserve equivalent artifacts: reference WAV, captured WAV,
   CPU profile, coupling profile, physical gate text/JSON, audio stack health,
   capture-device diagnosis, and run manifest.
6. Match the baseline fixture and defaults when comparing directly:
   `local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav`,
   pair `A` first, rate `48000`, buffer `512`, 20 s dense run, capture channels
   `1,2`, and a safe target peak such as the documented `-16` or `-18 dB`
   physical command. Do not compare clipped runs.
7. Report the same headline metrics: alignment/quality alignment, time-warp
   drift, SNR, 1-5 kHz residual ratio/dBFS, high-band residual ratio/dBFS, quiet
   mid-band noise, mid-band CPU correlation and source, click outliers/rate,
   lag jumps, clipped frames, driver avg/p95 CPU, and coreaudiod p95.
8. Preserve iRig readiness state before physical gates. If iRig is missing from
   USB or Core Audio, the candidate is `NOT_READY` / `BLOCKED_PHYSICAL_CAPTURE`,
   not ready for human listening.
9. Require stable iRig readiness across multiple polls before long physical
   gates; mainline default is 3 stable polls.
10. Respect the shared hardware lock for any future C++ action that plays audio,
    records iRig, changes default devices/rate/buffer, restarts services,
    installs/reloads HAL, touches USB, launches media apps, or runs CPU/UI stress.
11. Never use virtual loopback, aggregate/multi-output, VLC/system-stream
    capture, or MacBook microphone capture as release-quality proof. Those can
    only be diagnostics.
12. Compare against mainline 0.3.135 as a software-only/no-iRig anchor and
    against the 0.3.111/0.3.64 physical artifacts as practical physical floors,
    while remembering those physical artifacts are not release-quality passes.

## Immediate C++ follow-up checklist

- Implement a C++ documentation/test contract that emits the same readiness
  fields as `capture-device-diagnose`: `physical_capture_status`, `reason`,
  exact C++ driver UID, found iRig USB by name/id, found iRig Core Audio, external
  input count, USB enumeration failures, failed USB ports, and next action.
- Before any physical comparison run, verify the C++ driver is the intended
  active device by exact UID so Rust/mainline/C++ drivers cannot satisfy each
  other's gates by display name.
- Port or wrap the physical analyzer behavior rather than inventing a looser
  criterion: time-warp alignment, spectral coloration, window p95/median spread,
  click outliers, CPU coupling, and clipped-frame rejection are all part of the
  current evidence model.
- Use mainline's saved artifacts for offline calibration first. Do not spend a
  hardware window until C++ can parse and summarize those artifacts consistently.
