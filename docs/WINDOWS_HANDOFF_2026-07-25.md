# OpenA8DJ Windows handoff — 2026-07-25

## Delivered

The experimental Windows workstream is documented and published on `dev` and
then fast-forwarded into `main`:

- `OpenA8DJUsb` x64, version `0.0.183.0`, API 44.
- Self-extracting EXE and matching ZIP under `windows/releases/`.
- INF, SYS, CAT, test certificate, diagnostics tool, install/verify/uninstall
  scripts, manifests, and SHA-256 checksums.
- Simple installation guide:
  `docs/WINDOWS_TESTER_INSTALL_GUIDE_UNSIGNED_2026-06-19.md`.
- Installer design and user-facing Windows status:
  `windows/installer/README.md`, `windows/README.md`, and `docs/WINDOWS.md`.

## Installation contract

This is an experimental test-signed kernel driver, not a Microsoft-signed,
WHQL, or attestation-certified product. A user must close audio applications,
unplug the Audio 8 DJ, run `shutdown /r /o /t 0` from an Administrator Terminal,
choose Startup Settings option 7, boot Windows, double-click the EXE, approve
UAC, accept **Install this driver software anyway** if Windows Security asks,
and reconnect the Audio 8 DJ.

Do not use `-ForceInstallDespiteSecureBoot`, do not modify BCD settings, and do
not combine Startup Settings option 7 with `-EnableTestSigning`. A kernel driver
can still crash or reboot Windows. Use a recovery-capable test machine.

## Validation evidence

- EXE `--verify-only`: passed.
- ZIP contents: INF/SYS/CAT, `opena8djctl.exe`, scripts, manifests, and
  `README-FIRST.txt`: present.
- Physical Audio 8 DJ + iRig Stream route: endpoints present and healthy.
- Traktor playback: 8 channels active, all four render pairs advanced, and
  driver underrun/overrun/packet/late counters stayed at zero.
- Tone loopback after removing the turntable: 64.5 dB SNR, −73.1 dB THD,
  0.03 Hz frequency error, no clipping or status events.
- Real music loopback: playback completed at 44.1 and 48 kHz with no PortAudio
  status events or clipping; the analyzer measured about 0.936 correlation and
  8.5 dB SNR on the connected pair.
- No BSOD or unexpected reboot occurred during the final validation session.

The offline runner passed the Windows surface and hardware-lock contracts. Its
C-based tests were not executed on the publishing host because no C compiler
was installed. MIDI, ASIO, complete DVS/timecode input validation, sleep/wake,
and Microsoft signing/certification remain open limitations.

## macOS protection

macOS remains the primary supported product. The Windows merge is additive:
there are no changes to `macos/`, `driverkit/`, `src/`, `CMakeLists.txt`, or
`Makefile`. The macOS `main` baseline remains the existing release commit before
the Windows merge, and the Windows installer contains no macOS payloads.

## Reproducibility and recovery

- EXE SHA-256:
  `ccdc2f2782f080067b7b5944abd977ffcc1727b171e3aeaedb49216fcc80b9cc`
- ZIP SHA-256:
  `ea02c534b76ff3d669b52bc5af0c8356dcb42b3a6f22787d9f2f4bf3f2885a4c`
- Read `docs/WINDOWS_DRIVER_INCIDENT_2026-06-25.md` before changing driver
  loading or recovery behavior.
- If installation fails, save `verify.cmd` output and Windows Event Viewer
  evidence; do not repeatedly reinstall or force the driver through Secure Boot.
