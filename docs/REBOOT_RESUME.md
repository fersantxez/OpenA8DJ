# Reboot Resume Plan

Date: 2026-06-16

Purpose: allow a reboot and automatic login while returning Codex to the isolated C++/DriverKit worktree without touching audio hardware or system audio state.

## Current Safe Resume Contract

- Automatic login user: `fer`.
- FileVault status before reboot: off.
- Resume LaunchAgent: `com.fer.audio8djcpp.codex-resume`.
- LaunchAgent program: `/Users/fer/dev/audio8djcpp/scripts/post-login-codex-resume`.
- Resume evidence path: `/Users/fer/dev/audio8djcpp/local-analysis/reboot-resume/latest`.

The resume script only opens Codex on `/Users/fer/dev/audio8djcpp` and writes a local marker. It must not touch CoreAudio, USB, Traktor, VLC, Spotify, Audio 8 DJ, iRig, driver installation, default devices, sample rate, or buffer size.

## Disabled For This Reboot

The older `com.fer.opena8dj.audio-qa-startup` LaunchAgent is unsafe for the current C++ architecture reboot because it can change the default audio device and attempt iRig recording from the mainline tree. It should remain disabled until an explicit physical-test window is approved with the global audio lock.

The older `com.fer.opena8dj.codex-resume` LaunchAgent opens the mainline path. For this reboot, the C++ LaunchAgent is the intended resume entrypoint.

## After Login

First checks:

```bash
cd /Users/fer/dev/audio8djcpp
git branch --show-current
git status --short
ls -la local-analysis/reboot-resume/latest
sed -n '1,120p' local-analysis/reboot-resume/latest/RESUME_FOR_CODEX.md
```

Optional offline sanity gate:

```bash
scripts/run-cpp-offline-gates
```

No physical audio/hardware test is authorized by this reboot plan.

## 2026-06-16 Post-Reboot Result

Result: FAIL.

The system rebooted, but Codex did not automatically recover into the expected working session. This must be fixed later. Do not treat the current LaunchAgent setup as reliable until a reboot test proves:

- automatic login completes,
- Codex opens without manual intervention,
- Codex opens in `/Users/fer/dev/audio8djcpp`,
- `local-analysis/reboot-resume/latest` is created after the actual reboot,
- the assistant can continue from the C++ handoff without manual recovery.

Current priority has moved to hardware quality testing under the global hardware lock.
