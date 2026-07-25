# Audio Stack CPU Incident - 2026-06-12

## Summary

At about 15:14-15:24 EDT the machine entered a Core Audio routing storm.
`coreaudiod` first reached about 140% CPU, then multiple audio/media clients
and system services climbed together. The installed OpenA8DJ HAL was still the
known baseline, version 0.2.32 build 34, hash
`bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434`.

This did not look like a leftover test worker. No `run-soundcheck`,
`audio-wav-play`, `audio-record`, `opena8dj-control`, `ffmpeg`, `afconvert`, or
CPU stress worker remained alive.

## Evidence

- `coreaudiod` PID 86898 reached about 140% CPU before it was restarted.
- `Core Audio Driver (OpenA8DJ.driver)` itself stayed low after the restart.
- macOS generated `cpu_resource` diagnostics at 15:22 for `audioaccessoryd`,
  `mediaremoted`, Spotify, QuickLookUIService, WebKit GPU, and
  `universalaccessd`.
- The diagnostic stacks for `audioaccessoryd`, `mediaremoted`, Spotify, and
  NIHardwareAgent all converged on CoreAudio property notification handling:
  `HALC_ProxyNotifications`, `ProxyObject_PropertiesChanged`, and
  `HALC_ShellSimpleProxyList::Reconcile`.
- Between 15:10 and 15:16, `coreaudiod` logged about 1,617 repeated
  registrations of `com.apple.AirPlayXPCHelper`.
- In the same interval, `ContinuityCaptureAgent` logged about 946 device
  discovery/status lines for the nearby iPhone continuity-capture path,
  repeatedly reporting no usable continuity capture device.

## Interpretation

The most likely failure mode was a storm of Core Audio route/property-change
notifications. AirPlay, Continuity Capture, call routing, Bluetooth smart
routing, Spotify, Native Instruments' translated `NIHardwareAgent`, and the
OpenA8DJ HAL were all in the same Core Audio graph. Once the notification loop
started, every interested client tried to reconcile the device list repeatedly.

This also explains why the Codex record button can take a couple of seconds to
activate: pressing it wakes macOS microphone/capture routing, not only the
local MacBook microphone. On this machine that includes Continuity Capture
iPhone discovery, call route updates, Control Center privacy UI, CoreSpeech,
and audio accessory routing.

## Recovery That Worked

1. Restart `coreaudiod`.
2. If the cascade remains hot, restart the audio/media/capture services:
   AirPlayXPCHelper, mediaremoted, audioaccessoryd, audiomxd,
   systemsoundserverd, ControlCenter, ContinuityCaptureAgent, callservicesd,
   PerfPowerServices, corespeechd, heard, avconferenced, Spotify, and
   NIHardwareAgent.
3. Recheck that `coreaudiod` and `Core Audio Driver (OpenA8DJ.driver)` are both
   near 0% CPU before any further test.

The repository now has `scripts/audio-stack-health` and `make
audio-stack-health` / `make audio-stack-reset` for this.

## Follow-up Incident at 17:34-17:44 EDT

During source-build testing, `0.2.66` build 68 reproduced a more dangerous
failure mode:

- The build passed `hal-smoke` before install.
- After install, Core Audio public enumeration hung in `build/audio-list`.
- `coreaudiod` went over 100% CPU.
- Rolling back the OpenA8DJ bundle to P2 was not sufficient on its own; the
  Core Audio stack stayed hot because it had already entered a bad route/device
  reconciliation state.
- The decisive recovery was to move `OpenA8DJ.driver` out of
  `/Library/Audio/Plug-Ins/HAL`, restart `coreaudiod`, and then force-restart
  the stuck media/audio services.
- With OpenA8DJ unloaded, `build/audio-list` returned immediately and listed
  only `iRig Stream`, `MacBook Air Microphone`, and `MacBook Air Speakers`.
- Final safe sample after recovery showed about `85%` idle CPU.

This means any future source-built candidate must pass a Core Audio enumeration
timeout gate after install/reload, not only `hal-smoke`. A build that passes
smoke but hangs `audio-list` is rejected before playback.

The repository now also has `scripts/audio-stack-guard`:

```text
scripts/audio-stack-guard --wait 1 --enumeration-timeout 5 --min-idle-pct 20
scripts/audio-stack-guard --recover --unload-opena8dj
make audio-stack-guard
make audio-stack-recover
```

The guard writes a run directory under `local-analysis/audio-stack-guard/`.
It records watched-process CPU, global idle CPU, Core Audio enumeration
pass/fail/hung, device enumeration output, an instant CPU snapshot, and every
process/HAL move done during recovery.

Use `--recover --unload-opena8dj` for emergencies. Do not use recovery as a
normal part of successful testing; a candidate that requires recovery after
install is rejected.

## Prevention

- Do not start a soundcheck while the audio stack is hot.
- `scripts/run-soundcheck` now blocks full playback/capture runs unless
  `scripts/audio-stack-guard` reports both an idle-enough audio stack and a
  non-hung Core Audio device enumeration.
- Treat any delay in microphone activation, repeated Continuity Capture logs,
  or AirPlayXPCHelper registration bursts as an environment instability signal
  before judging a driver candidate.
- Keep Codex microphone/capture testing separate from Audio 8 DJ playback
  quality testing; activating the microphone changes the macOS capture graph.
- Do not install source-built HAL candidates from the current tree directly
  into a listening test. First isolate a source baseline that can be installed,
  reloaded, enumerated, and unloaded repeatedly without `coreaudiod` CPU spikes
  or `audio-list` hangs.
