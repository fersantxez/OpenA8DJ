# Windows Implementation Progress

## 2026-06-25 Stop-State Incident

Current label:

```text
windows-driver: work in progress, not available for use, unstable, can hang/reboot host
```

The latest Windows tablet session ended with the user reporting another
hang/reboot around the attempted short-stream canary sequence. All earlier
positive hardware notes in this file are now historical debugging evidence
only. They must not be read as a tester-ready or usable Windows driver claim.

The last observed device state before the stop was:

```text
Manufacturer Name: OpenA8DJ
Status: Problem
Problem Code: 52 (0x34) [CM_PROB_UNSIGNED_DRIVER]
Problem Status: 0xC0000428
Driver Name: oem169.inf
```

Stop rule: do not run OpenA8DJ Windows hardware tests, Traktor tests, iRig
probes, ISO diagnostics, stream start/stop, driver install/rebind, or reboot
automation until crash dumps and event logs have been reviewed offline.

See `docs/WINDOWS_DRIVER_INCIDENT_2026-06-25.md`.

Date: 2026-06-22
Branch: `windows/rebuild-surface`
Host used for this pass: macOS
Hardware use: none

## Status

This pass adds an offline Windows audio-engine prototype and repeatable macOS
contract tests. It does not produce a Windows driver build, a usable Windows
audio endpoint, an ASIO driver, MIDI endpoints, or an installable release.

Current label remains:

```text
offline/prototype only
diagnostic only, sound quality not validated
not a Windows release candidate
```

## Implemented In This Pass

- `windows/audio/OpenA8DJAudioEngine.h`
- `windows/audio/OpenA8DJAudioEngine.c`
- `windows/tests/audio_engine_contract_test.c`
- `windows/tests/validate_windows_surface_contract.py`
- `windows/tests/run_offline_tests.py`
- `windows/tests/run-offline-tests.ps1`

The audio engine is intentionally user-mode/offline prototype code. It models
the shared engine contract that ACX/WASAPI and ASIO should later use, but it is
not compiled into the current KMDF driver.

Implemented engine behavior:

- 8 input channels and 8 output channels.
- Stable rates limited to 44.1 kHz and 48 kHz.
- Buffer frame validation from 15 to 4096 frames.
- Render ring writes and reads.
- Deterministic render underrun behavior: zero-fill and count.
- Capture ring writes and reads.
- Deterministic capture overrun behavior: overwrite oldest frame and count.
- 24-bit big-endian sample pack/unpack helpers for CAIAQ packet work.

Implemented contract checks:

- The driver surface exposes only 44.1/48 kHz as stable rates.
- The driver rejects normal start-streaming requests until a real engine exists.
- Audio endpoint, MIDI, ASIO, and hardware controls remain non-ready/planned in
  the reported surface.
- The CLI usage string no longer advertises 88.2/96 kHz as normally selectable.
- The offline engine compiles with Clang on macOS and passes deterministic ring
  and sample-conversion tests.
- The Windows PowerShell runner can compile and execute the same offline C
  contract on a Windows host with Python and either clang or MSVC available.

## Verified Here

Executed on macOS:

```sh
windows/tests/run_offline_tests.py
```

Observed result:

```text
PASS: Windows surface contract is truthful for offline/macOS validation
PASS: OpenA8DJ offline audio engine contract
PASS: OpenA8DJ Windows offline tests
```

This verifies only source-level contracts and portable C behavior. It does not
verify WDK compilation, driver loading, Windows endpoint enumeration, USB
streaming, MIDI, ASIO, latency, CPU, DPC, Traktor, or sound quality.

## Next Implementation Steps

1. Add a Windows-host CI/manual gate that runs the WDK build for
   `OpenA8DJWindows.sln`.
2. Add `InfVerif` and `Inf2Cat` verification to the Windows build script.
3. Add an ACX proof-of-fit skeleton that can enumerate a deterministic virtual
   endpoint without Audio 8 DJ attached.
4. Replace the current start rejection only after the transport and engine can
   submit preallocated isochronous request pools safely.
5. Add Windows hardware-use logs and acquire the shared hardware lock before
   any physical Audio 8 DJ test.
6. Validate clean install/uninstall, Device Manager state, endpoint visibility,
   Traktor ASIO/WASAPI behavior, MIDI, CPU/DPC, external capture, and human
   listening before any tester candidate.

## Blockers To Full Windows Completion

- No Windows VM or WDK toolchain is available in this macOS session.
- No Microsoft driver signing or attestation can be performed here.
- No physical Audio 8 DJ hardware validation was started in this pass.
- No Windows audio endpoint can be claimed functional until ACX/KS code
  enumerates and streams on real Windows.
- No build can be handed to a tester as a normal candidate without exact
  artifact sound-quality validation.

## 2026-06-23 Windows Hardware Session Notes

Host: Windows tablet with Audio 8 DJ and iRig Stream attached.
Branch: `windows/rebuild-surface`.

Observed hardware progress:

- OpenA8DJ test packages through `0.0.11.0` could bind to the physical Audio 8
  DJ when Windows was booted in a driver-signature-disabled state.
- The USB transport reached a diagnostic state with four pipes mapped:
  bulk out, bulk in, isochronous in, and isochronous out.
- A diagnostic isochronous capture snapshot completed and returned packet
  lengths/statuses from the physical device.

Critical safety finding:

- `0.0.11.0` crashed the host with bugcheck `0x00000144`.
- `C:\Windows\MEMORY.DMP` showed the stack entering
  `OpenA8DJ_SendBulkUrbSynchronously` from `OpenA8DJ_ApplyAudioParams`, then
  `WdfUsbTargetPipeSendUrbSynchronously`, before the USB stack bugchecked.
- Treat the explicit bulk URB command path and any output-writing diagnostics as
  unsafe until redesigned.

Immediate mitigation:

- `0.0.12.0` disables the two diagnostic routes that write to the hardware:
  `IOCTL_OPENA8DJ_ISO_SILENCE_PULSE` and
  `IOCTL_OPENA8DJ_APPLY_AUDIO_PARAMS` now return `STATUS_NOT_SUPPORTED`.
- Audio 8 DJ was left disabled in Device Manager after install attempts to avoid
  mixer noise or another USB-stack crash.
- iRig Stream USB device remained present; its Line In endpoint was disabled
  locally to force dictation back to the Realtek microphone.

Signing/install state:

- `0.0.12.0` builds, signs with the local test certificate, packages, and passes
  offline tests on the Windows host.
- It installs into the Driver Store as `oem72.inf`, but Secure Boot is enabled,
  so Windows reports `CM_PROB_UNSIGNED_DRIVER` and will not load the test-signed
  kernel driver.
- `windows/scripts/test-signing-readiness.ps1` generated an attestation CAB:
  `windows/dist/attestation/OpenA8DJUsb-Release-x64-attestation.cab`.
- The remaining signing blocker is an organization EV/code-signing certificate
  and Microsoft Partner Center Hardware Dashboard submission, or an equivalent
  Microsoft-signed driver package.

## 2026-06-23 Late Windows Hardware Progress

Host: Windows tablet with Audio 8 DJ and iRig Stream attached.
Branch: `windows/rebuild-surface`.

Installed package:

- Current local test package: `OpenA8DJUsb` `06/23/2026,0.0.23.0`.
- Current installed package observed on the host: `oem83.inf`.
- Device Manager/PnP state after install and tests:
  - Friendly name: `Audio 8 DJ`
  - Manufacturer: `OpenA8DJ`
  - Service: `OpenA8DJUsb`
  - Status: `OK`
  - Problem: `CM_PROB_NONE`

USB/control progress:

- The EP1 continuous bulk reader design replaced the unsafe explicit bulk URB
  read path that had crashed in `0.0.11.0`.
- `opena8djctl audio-params` now succeeds without crashing the host:
  - device-info reply starts `01 18 00 02 ...`
  - reset reply is `09 00`
  - set reply is `09 01`
- `opena8djctl iso-capture` continues to complete with `nt-status=0` and no
  packet errors.

Physical output progress:

- `0.0.20.0` re-enabled a single diagnostic isochronous silence pulse.
- `0.0.21.0` added a bounded diagnostic `opena8djctl iso-tone` command.
- `0.0.23.0` calibrates that tone generator with a 64-sample table for the
  current packet cadence.
- `opena8djctl iso-tone 250 0 4096` completed:
  - requested/completed transfers: `250/250`
  - first/last capture status: `0x00000000`
  - first/last playback status: `0x00000000`
  - capture/playback errors: `0/0`
  - playback bytes: `1021664`
- A physical iRig capture of Audio 8 DJ output A after the calibrated burst was
  recorded in:
  `local-analysis/windows-opena8dj-tone-20260623-220230`
- Captured metrics:
  - dominant frequency: `1000.32 Hz`
  - peak: `0.4629`
  - clipping count above 0.98: `0`
  - active duration: `0.2509 s`
  - tail peak after 100 ms: `0.0167`
  - PortAudio/sounddevice status events: none

Verification:

- WDK Release x64 build passed with 0 warnings and 0 errors.
- Catalog generation and signature verification passed.
- Offline Windows contract tests passed:
  - Windows surface contract
  - offline audio engine contract
  - aggregate Windows offline tests

Remaining gap:

- This is still a KMDF USB diagnostic driver, not a normal Windows audio
  endpoint yet.
- Windows applications still cannot select OpenA8DJ through WASAPI/DirectSound
  until an ACX or KS endpoint layer is implemented.
- The new physical output path is useful for bounded hardware QA and packet
  cadence work, but it is not a production streaming engine.

## 2026-06-24 Diagnostic Output Quality/Performance Pass

Host: Windows tablet with Audio 8 DJ and iRig Stream attached.

Installed package:

- Current installed kernel package: `OpenA8DJUsb` `06/24/2026,0.0.26.0`.
- Current observed Driver Store package: `oem86.inf`.
- Device state after the pass: `Status=OK`, `Problem=CM_PROB_NONE`,
  `Service=OpenA8DJUsb`.

Implemented changes:

- `OPENA8DJ_DRIVER_API_VERSION` advanced to `15`.
- `opena8djctl iso-tone` gained diagnostic parameters for:
  - fixed isochronous output packet bytes;
  - tone period table selection.
- Supported tone periods are now `40`, `48`, `56`, and `64` samples.
- The CLI default changed to the best measured diagnostic profile:
  - transfers: `50`
  - output pair: `0`
  - amplitude: `4096`
  - packet bytes: `352`
  - period samples: `40`
- The old calibrated 1 kHz diagnostic remains available with:
  `opena8djctl iso-tone 250 0 2048 0 64`.

Measured physical results:

- Previous best 1 kHz-ish profile:
  - command: `opena8djctl iso-tone 250 0 2048 0 64`
  - frequency: about `1000 Hz`
  - left THD: about `-17.7 dB`
  - elapsed command time: about `335 ms`
- Best measured diagnostic-quality/performance profile:
  - command: `opena8djctl iso-tone 250 0 2048 352 40`
  - frequency: about `1201 Hz`
  - left THD: about `-27.4 dB`
  - SINAD: about `-1.0 dB`
  - elapsed command time for the 352-byte profile family: about `203 ms`
  - playback bytes per 250-transfer run: `704000`
- Compared with the old variable-length profile, the best measured profile:
  - improves measured THD by roughly `9.7 dB`;
  - reduces diagnostic command elapsed time by roughly `39%`;
  - sends fewer playback bytes for the same transfer count;
  - kept capture/playback USB errors at zero in the measured runs.

Run directories:

- Amplitude sweep for old variable profile:
  `local-analysis/windows-opena8dj-sweep-20260624-082002`
- Packet-size sweep:
  `local-analysis/windows-opena8dj-packet-sweep-20260624-082227`
- Period sweep:
  `local-analysis/windows-opena8dj-period-sweep-20260624-082443`
- Best-profile amplitude sweep:
  `local-analysis/windows-opena8dj-best-sweep-20260624-082540`
- Period-40 comparison:
  `local-analysis/windows-opena8dj-period40-test-20260624-082728`

Verification:

- WDK Release x64 build passed with 0 warnings and 0 errors.
- Catalog generation and signature verification passed.
- Offline Windows contract tests passed after the changes.
- Final `opena8djctl iso-tone` default run completed with:
  - requested/completed transfers: `50/50`
  - `packet-bytes=352`
  - `period-samples=40`
  - capture/playback status: `0x00000000`
  - capture/playback errors: `0`
- Final `opena8djctl iso-silence` completed after the tone and the device
  remained `OK`.

Interpretation:

- The best measured diagnostic profile is not a 1 kHz laboratory tone; it is
  the cleanest bounded physical output profile found in this pass.
- The quality bottleneck is now likely the lack of a real continuous
  preallocated streaming engine rather than one more static tone-table tweak.
- Next meaningful improvement should move from synchronous diagnostic bursts to
  the real capture-paced isochronous render/capture engine and then expose it
  through ACX/KS.

## 2026-06-24 Continuous Isochronous Silence Stream

Host: Windows tablet with Audio 8 DJ and iRig Stream attached.

Installed package:

- Current installed kernel package: `OpenA8DJUsb` `06/24/2026,0.0.27.0`.
- Current observed Driver Store package: `oem87.inf`.
- Device state after install and tests: `Status=OK`, `Problem=CM_PROB_NONE`,
  `Service=OpenA8DJUsb`.

Implemented changes:

- `OPENA8DJ_DRIVER_API_VERSION` advanced to `16`.
- Added a passive WDF work item as the first continuous local streaming worker.
- `IOCTL_OPENA8DJ_START_STREAMING` now starts a capture-paced isochronous
  silence engine instead of rejecting all start requests.
- `IOCTL_OPENA8DJ_STOP_STREAMING` requests worker shutdown, waits for the worker
  to drain, and reports the final stream counters.
- The Windows surface now reports:
  - USB transport: ready;
  - isochronous engine: ready when both pipes and the worker are present;
  - audio endpoint: still planned;
  - safety policy: `start runs local silence stream; no Windows endpoint yet`.

Measured local stream stability:

- Three-second start/stop run:
  - render frames: `288464`;
  - capture frames: `419639`;
  - USB input packets: `26229`;
  - USB output packets: `26224`;
  - packet errors, underruns, overruns: `0`.
- Fifteen-second start/stop run:
  - render frames: `1734920`;
  - capture frames: `2523447`;
  - USB input packets: `157717`;
  - USB output packets: `157720`;
  - packet errors, underruns, overruns: `0`.

Verification:

- WDK Release x64 build passed with 0 warnings and 0 errors.
- Package install succeeded locally under unsigned-driver load posture.
- Offline Windows tests passed after the 0.0.27 contract update:
  - Windows surface contract;
  - offline audio engine contract;
  - aggregate Windows offline tests.

Remaining gap:

- This is now a real continuous USB isochronous worker, but it still feeds
  locally generated silence rather than WASAPI application audio.
- Windows applications still cannot select OpenA8DJ as a playback/capture
  endpoint until the ACX or KS endpoint layer is implemented.
- The next implementation step is to initialize ACX safely on this KMDF USB
  driver and add the smallest render/capture circuit skeleton that can
  enumerate without disturbing the stable USB transport.
