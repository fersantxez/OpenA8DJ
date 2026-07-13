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

## 2026-07-12 Offline Safety Hardening

The current worktree contains a newer ACX/USB implementation than the older
handoff sections above. This pass made only source and validation changes; no
Audio 8 DJ hardware was connected or touched here.

Implemented:

- Serialized all EP1 bulk command transactions with a device-owned
  `WDFWAITLOCK`, covering control read, control write/readback, and audio
  parameter commands that share the continuous bulk reader response state.
- Set the KMDF device execution level to `WdfExecutionLevelPassive` because
  synchronous USB commands, bounded waits, and the bulk command wait lock are
  not valid work at `DISPATCH_LEVEL`.
- Made the reported Windows endpoint and USB transport capabilities depend on
  `PrepareHardware` and successful ACX circuit registration instead of
  reporting readiness before those gates complete.
- Added explicit `InfVerif` Windows-driver and hardware-signature checks to
  `windows/scripts/build-driver.ps1` and the Windows GitHub Actions preflight.
- Hardened the Windows workflow for fork use with `contents: read` permissions
  and `persist-credentials: false` on checkout; the workflow cannot use its
  checkout token to push changes.
- The local repository still has only the upstream `origin` remote and no fork
  remote or push was configured. The attempted GitHub device authentication
  timed out, so fork creation/push remains an explicit external handoff.
- Extended the offline surface contract to require ACX device initialization,
  render/capture circuit registration, RT stream creation, and RT callback
  wiring, rather than validating endpoint strings alone.
- Made the legacy `START_STREAMING` IOCTL transactional: an unsuccessful
  hardware audio-parameter transaction now rolls back the worker-active state,
  reports the failure, and does not claim that streaming started.
- Made `APPLY_AUDIO_PARAMS` propagate the hardware transaction status instead
  of returning success after a failed write or reply validation.
- Added protocol gates to the audio-parameter sequence: device-info must
  identify as command `0x01`, reset must return `09 00`, and set must return
  `09 01` before the transaction reports success.
- Control-state refresh no longer ignores a failed `AUTO_MSG` bulk write; it
  retries without issuing the dependent read until the setup command succeeds.
- ACX `Run` now propagates `STATUS_DEVICE_NOT_READY` when the device, RT buffer,
  or stream worker is not prepared, instead of reporting a stream that never
  started.
- ACX RT packet allocation now rejects page-rounding arithmetic overflow
  before allocating the MDL-backed buffer.
- Aligned the Windows rate path with the repository's CAIAQ oracle: ACX now
  advertises PCM 44.1/48 kHz, maps rate codes `0`/`1`, calculates 320/352-byte
  packets for the four Audio 8 DJ stereo streams, and derives the continuous
  output transfer length from the selected rate. Live rate changes are
  rejected while the worker is active.
- `STOP_STREAMING` now reports `STATUS_IO_TIMEOUT` if the worker does not stop
  within its bounded wait, instead of claiming a clean stop unconditionally.

Verification in this workspace:

- `python windows/tests/validate_windows_surface_contract.py`: PASS.
- PowerShell parser check for `windows/scripts/build-driver.ps1`: PASS.
- `CC="C:\Program Files\LLVM\bin\clang.exe" python
  windows/tests/run_offline_tests.py`: PASS.
- `windows/scripts/build-driver.ps1 -Configuration Release -Platform x64`:
  PASS. MSBuild reported 0 warnings and 0 errors; ApiValidator passed; InfVerif
  `/w` and `/h` passed; Inf2Cat completed with no errors or warnings.
- `windows/scripts/build-virtual-acx.ps1 -Configuration Release -Platform x64`:
  PASS. The isolated virtual target also reported 0 warnings and 0 errors,
  passed ApiValidator, InfVerif, and Inf2Cat.
- `windows/tests/run-open-a8dj-virtual-endpoint-canary.ps1` in its default
  preflight mode: PASS. It verified the root-only package and made no install,
  PnP mutation, control IOCTL, or hardware access. Installation requires both
  explicit `-AllowVirtualInstall` and `-AllowTestSigned` flags.
- Latest preflight artifact:
  `local-analysis/windows-open-a8dj-virtual-canary-20260712-120427/summary.json`.
- Added `windows/tests/run-open-a8dj-virtual-endpoint-probe.ps1` as the next
  read-only gate. After an authorized virtual install it checks PnP, Media
  presence, surface/capabilities readiness, the USB-stub state, and idle worker
  counters without opening audio streams. The authorized runtime probe now
  passes against the isolated virtual endpoint.
- Hardened `opena8djctl` device selection with the `OPENA8DJ_INSTANCE_ID`
  filter, and made the virtual probe set it to the exact present instance
  (`ROOT\MEDIA\0000`) selected by the `ROOT\OpenA8DJVirtual` hardware ID, so
  read-only IOCTLs cannot silently target a different OpenA8DJ interface when
  multiple devices are present.
- The canary can now run the read-only probe and guaranteed cleanup in one
  authorized invocation by combining `-ProbeCtlPath` with `-RemoveAfter`.
- Canary cleanup now resolves the published `oem*.inf`, records the removal
  exit codes, verifies that no virtual device remains, and fails instead of
  reporting success if cleanup is incomplete.
- Cleanup validation runs before the remaining runtime assertions so an
  anomalous PnP result or bugcheck cannot bypass the cleanup gate.
- Before any authorized install, the canary now records durable JSONL
  checkpoints for preflight, authority verification, bounded `devcon` install,
  PnP state, probe, cleanup, exceptions, and completion. The read-only
  `windows/tests/analyze-open-a8dj-virtual-canary-recovery.ps1` tool correlates
  the last checkpoint with current PnP/service state and System bugcheck or
  Kernel-Power events after a reboot.
- The first authorized `pnputil /add-driver /install` attempts were bounded at
  60 seconds and stopped at `install-start` without creating a virtual device;
  recovery confirmed no bugcheck, no service, and clean rollback. The canary
  now uses bounded WDK `devcon install` to materialize the root device, while
  retaining bounded PnP cleanup.
- Corrected ACX capture streaming-pin directionality: capture now declares
  `AcxPinTypeSource` with `AcxPinCommunicationSource`, while render remains
  `AcxPinTypeSink` with `AcxPinCommunicationSink`; the bridge pin remains
  non-communicating. Both targets rebuild and pass all WDK gates.
- Bumped the physical USB package to `DriverVer=07/12/2026,0.0.135.0` and
  the isolated virtual proof package to `0.0.2.0` so the corrected binaries
  cannot be confused with the earlier `0.0.134.0` artifact.
- ACX teardown now tracks registration per render/capture circuit and preserves
  the incomplete state if any `AcxDeviceRemoveCircuit` call fails; a later
  release can retry the remaining removals instead of falsely reporting a
  clean teardown.
- Latest signing-readiness report
  (`local-analysis/windows/signing-readiness-20260712-131633/`): the unsigned
  submission payload and attestation CAB are structurally ready, but
  `partner_center_ready=false` because the CAB is not EV-signed. The locally
  produced `.sys` is test-signed and its test root is not trusted by the current
  machine; the catalog and CAB remain unsigned.
- The ACX source is WDK-buildable and its circuit/RT callback wiring is present,
  and the isolated virtual deterministic endpoint proof now demonstrates clean
  PnP start, read-only control-surface access, and endpoint enumeration.
- ACX circuit registration is now transactional: partial `PrepareHardware`
  registration rolls back, and `ReleaseHardware` removes registered circuits
  before the transport state is reset.
- Added an isolated `OpenA8DJVirtual` ACX proof target. Its WDK build,
  ApiValidator, InfVerif `/w` and `/h`, and Inf2Cat gates pass; its INF targets
  only `ROOT\OpenA8DJVirtual` and is not part of the USB package. Runtime proof
  completed through `ROOT\MEDIA\0000`, with the exact hardware ID preserved.
- Virtual driver loading and read-only endpoint enumeration are proven. Audio
  streaming, physical USB load, and physical audio validation remain
  intentionally unverified; the connected Audio 8 DJ was recorded but never
  targeted.

Completion audit (2026-07-12):

| Requirement | Evidence | Status |
| --- | --- | --- |
| ACX circuit, format, RT callback, and USB worker implementation | Current source plus successful WDK builds and isolated runtime proof | PASS for the isolated virtual target; physical USB path remains unproven |
| USB and isolated virtual package buildability | MSBuild, ApiValidator, InfVerif `/w` and `/h`, Inf2Cat | PASS |
| Virtual package cannot target Audio 8 DJ USB hardware | Root-only `ROOT\\OpenA8DJVirtual` INF and preflight artifact | PASS |
| Safe temporary install/probe/rollback path | `local-analysis/windows-open-a8dj-virtual-canary-20260712-122817/summary.json`, checkpoints, and recovery analysis | PASS; install/probe/cleanup completed with zero residual devices |
| Virtual PnP and WASAPI endpoint enumeration | `local-analysis/windows-open-a8dj-virtual-probe-20260712-122912/summary.json` | PASS; API 27, audio endpoints ready, USB transport stub, streaming not started |
| Physical USB load, streaming, and audio validation | Hardware canary and physical capture evidence | Intentionally not run |
| Production signing and Partner Center submission | Signing-readiness report | Pending EV signature |
| Secure GitHub fork/push handoff | Read-only workflow and local remote state | External authentication/fork handoff pending |

## 2026-07-12 Physical Canary Output Reproduction and Prompt Automation

- The unsigned-driver prompt is now handled inside the bounded physical canary.
  While `pnputil` is running, the canary accepts only a `Windows Security`
  window whose exact option is `Install this driver software anyway`, and only
  after `DrvInst.exe` points to `OpenA8DJUsb.inf` whose SHA-256 matches the
  validated package. The package hash is persisted in the checkpoint artifact.
- This automation successfully installed and rebound `0.0.135.0` as
  `oem20.inf` in `local-analysis/windows-a8dj-load-canary-20260712-133809/`.
  The read-only control probe passed, and the device was restored to
  `oem169.inf` / `0.0.131.0`; `oem20.inf` was deleted and the final PnP
  snapshot was `OK`/`CM_PROB_NONE`.
- The output probe log reached `diagnostics-before-returned` and then stopped
  at `target-open-start` while opening the 8-channel MME `OutputStream`. No
  `target-open-returned` record was produced. The bounded probe was terminated
  and the canary rollback completed; recovery analysis recorded zero bugchecks:
  `local-analysis/windows-a8dj-load-canary-20260712-133809/recovery-analysis.json`.
- A clean read-only canary with the package absent exercised the prompt path
  from start to finish: the log recorded the SHA-256-gated automatic approval,
  the candidate loaded and passed the read-only control probe, and the canary
  reached its durable `complete` checkpoint with zero bugchecks before
  restoring `oem169.inf` and deleting `oem20.inf`:
  `local-analysis/windows-a8dj-load-canary-20260712-134212/summary.json` and
  `local-analysis/windows-a8dj-load-canary-20260712-134212/recovery-analysis.json`.
- While adding prompt polling, the external-process wrapper briefly lost its
  in-loop deadline. That was found during the canary, the child probe was
  terminated without killing the rollback coordinator, and the wrapper now
  enforces the deadline on every polling iteration. Parser, surface-contract,
  and offline tests pass after the fix.

Current physical state remains safe: Secure Boot enabled, exact USB device
`OK`/`CM_PROB_NONE`, `oem169.inf` / `0.0.131.0`, and no output probe running.

## 2026-07-12 Physical Canary Crash and Recovery

The first bounded physical canaries established a useful safety baseline:

- `0.0.135.0` loaded and rebound to the exact device
  `USB\\VID_17CC&PID_1978\\SN-HKM6Q6EDKP___` as `oem20.inf`, with PnP
  `OK`/`CM_PROB_NONE`.
- The read-only surface/capabilities/stream/diagnostics probe passed.
- The bounded input endpoint probe opened four MME capture endpoints with no
  host status events. It proved endpoint opening only; no physical input signal
  was present.
- The canary restored the previous `oem169.inf` / `0.0.131.0`, removed the
  candidate package, and recorded zero bugchecks for that run.
- After the URB-lifetime fix, a second corrected physical canary loaded
  `0.0.135.0`, passed the read-only probe and a one-second MME input probe,
  restored `oem169.inf`, removed `oem20.inf`, and reached its durable complete
  checkpoint with zero bugchecks:
  `local-analysis/windows-a8dj-load-canary-20260712-132016/summary.json`.
  Recovery analysis also returned cleanly from
  `local-analysis/windows-a8dj-load-canary-20260712-132016/recovery-analysis.json`.
- A corrected physical output canary then loaded 0.0.135.0 and reached the
  output checkpoint with no new bugcheck, but the MME 8-channel output probe
  still timed out after 32 seconds. It rolled back cleanly to `oem169.inf` and
  deleted `oem20.inf`; artifact:
  `local-analysis/windows-a8dj-load-canary-20260712-132242/summary.json`.
- The earlier direct probe against `ROOT\\MEDIA\\0000` was not valid evidence
  of an isolated virtual driver: the new dump shows that endpoint was served
  by the installed physical `OpenA8DJUsb.sys` image. The supposed virtual
  reproduction therefore remains unproven and must not be used to separate
  the ACX render path from the USB URB crash.

The subsequent output probe exposed a real kernel failure in the previously
installed driver (`oem169.inf`, `0.0.131.0`) while opening the output endpoint:

- Windows recorded bugcheck `0x0000007E` with access violation
  `0xC0000005` at 12:56:48, and automatically rebooted without necessarily
  showing a blue screen.
- Dump: `C:\Windows\Minidump\071226-24968-01.dmp`.
- The dump stack ends in `OpenA8DJUsb+0x7542` during
  `WdfObjectDelete` -> `USBD_UrbFree`, identifying unsafe isochronous URB
  lifetime cleanup. Debugger output is preserved in
  `local-analysis/windows-bugcheck-071226-24968-01-cdb.txt`.
- The new recovery analyzer
  (`windows/tests/analyze-a8dj-driver-load-canary-recovery.ps1`) reproduced the
  diagnosis from checkpoints plus the post-reboot WER/Kernel-Power events in
  `local-analysis/windows-a8dj-load-canary-20260712-125035/recovery-analysis.json`.
- The current source now aborts the isochronous output pipe after a failed
  transfer, stops the worker, and refuses to delete the URB memory if the
  abort itself fails. The new source builds with 0 warnings/errors and passes
  the offline surface and audio-engine contracts.

The machine is currently safe and restored: Secure Boot remains enabled, the
physical device is `OK`/`CM_PROB_NONE` on `oem169.inf` / `0.0.131.0`, and any
future physical load or output probe remains restricted to the bounded canary
with explicit crash-recovery checkpoints.

Updated completion status:

| Requirement | Evidence | Status |
| --- | --- | --- |
| Physical load/rebind and rollback | `local-analysis/windows-a8dj-load-canary-20260712-132016/summary.json` | PASS for corrected bounded load, read-only probe, and input endpoint opening |
| Physical output safety | `local-analysis/windows-bugcheck-071226-24968-01-cdb.txt`, corrected canary artifact, and WER events | Previous binary crashed during URB cleanup; corrected binary did not crash but output still timed out |
| Driver recovery after unexpected reboot | Current PnP state and preserved minidump/checkpoints | PASS for recovery evidence; no claim of runtime safety yet |
| Production signing and Partner Center submission | Signing-readiness report | Pending EV signature |
| Secure GitHub fork/push handoff | Read-only workflow and local remote state | External authentication/fork handoff pending |

## 2026-07-12 Second Bugcheck During Misclassified Output Probe

- After reboot, WER recorded another `0x0000007E` / `0xC0000005`. The dump
  timestamp is 14:41:46 local and the dump is
  `C:\\Windows\\Minidump\\071226-26140-01.dmp`.
- CDB again shows `OpenA8DJUsb+0x7542` followed by
  `WdfObjectDelete` -> `USBD_UrbFree` -> `ucx01000!Xrb_Free`. The loaded image
  is the old `OpenA8DJUsb.sys` from the `0.0.131.0` DriverStore package, not
  the corrected `0.0.135.0` candidate. Evidence is preserved in
  `local-analysis/windows-bugcheck-071226-26140-01-cdb.txt` and
  `local-analysis/windows-bugcheck-071226-26140-01-detail.txt`.
- The endpoint probe was stopped, `ROOT\\MEDIA\\0000` was removed, and the
  physical device was verified `OK`/`CM_PROB_NONE` on `oem169.inf` / `0.0.131.0`.
  No driver or endpoint process remains. The corrected binary still has not
  passed a successful output-open test; the goal remains incomplete.

## 2026-07-12 Third Bugcheck: New Binary, Worker Teardown Ownership

- The next bounded physical canary was started at approximately 15:30:32
  local with the rebuilt `0.0.135.0` package. Windows created
  `C:\\Windows\\Minidump\\071226-21609-01.dmp` at 15:31:56; the reboot was
  therefore caused during this canary, not by the diagnostic commands run
  after recovery. The wrapper output files are
  `local-analysis/physical-canary-20260712-153032.stdout.txt` and
  `local-analysis/physical-canary-20260712-153032.stderr.txt`.
- This dump is again bugcheck `0x0000007E` / access violation
  `0xC0000005`, but it is materially different from the two previous
  crashes: CDB loaded the newly built image from
  `opena8djusb.inf_amd64_84c92b8f64573678`, timestamp 15:28:18, and the
  failing return address is `OpenA8DJUsb+0x5d50`, resolved with the matching
  PDB as `OpenA8DJ_EvtStreamWorkItem+0x720`.
- The exact stack is
  `OpenA8DJ_EvtStreamWorkItem+0x720` -> `WdfObjectDelete` ->
  `Wdf01000!FxUsbUrb::Dispose` -> `USBD_UrbFree` ->
  `ucx01000!Xrb_Free` -> `Wdf01000!FxObject::ProcessDestroy`.
  The debugger's locals show that the object being deleted is
  `captureUrbMemory` and that both old `*UrbSafeToDelete` flags were still
  true. Evidence is preserved in
  `local-analysis/windows-bugcheck-071226-21609-01-cdb.txt`.
- The relevant lifetime rule is now explicit in the source. The two worker
  URB memory objects are parented to `WDFUSBDEVICE`, cached in the device
  context, reused by the worker, and never deleted by the worker or by the
  temporary control helpers. `EvtDeviceReleaseHardware` first stops the USB
  targets and flushes the work item; parent cleanup owns the final delete.
  This avoids relying on `WdfObjectDelete` while a synchronous USB helper may
  have used an internal request after a timeout or cancellation attempt.
- The review also found a separate protocol defect: the four isochronous URB
  construction paths had set `Hdr.Function` but omitted
  `Hdr.Length = GET_ISO_URB_SIZE(...)`. All four paths now initialize the
  required header length. The worker's capture and playback buffers are also
  parent-owned `WDFMEMORY` children rather than pool allocations freed while
  an uncertain USB request might still reference them.
- The worker now checks `DeviceStopping` and `StreamStopRequested` after
  capture and before submitting the paired output URB. Any failed synchronous
  USB transfer retires the transport by setting `DeviceStopping`, preventing
  a second stream from reusing a URB whose request completion is not yet
  independently proven.
- The isochronous paths now use an explicit `WDFREQUEST` plus completion
  routine. A timeout requests cancellation with `WdfRequestCancelSentRequest`
  and waits for the completion event before deleting a temporary request or
  returning ownership of a temporary buffer. The worker's cached objects
  remain parent-owned as a second lifetime barrier. The surface validator now
  rejects any reintroduction of the internal `Request=NULL` isochronous path.

## 2026-07-12 Virtual Output Gate After Completion-Lifetime Repair

- The rebuilt virtual package passed the isolated output canary at
  `local-analysis/windows-open-a8dj-virtual-output-canary-20260712-161210`.
- It installed as `oem21.inf`, selected exactly one endpoint named
  `Speakers (OpenA8DJ Virtual ACX`, opened it at 44.1 kHz/8 channels, wrote
  44,100 silence frames, and reported zero host status events, underruns,
  overruns, packet errors, or late completions.
- The canary recorded `output_stream_probe_exit=0`, `delete_driver_exit=0`,
  `bugcheck_events_since_start=0`, and no remaining virtual devices. Secure
  Boot was enabled throughout. The physical USB device was not rebound or
  loaded during this gate.
- This is the first runtime evidence after replacing the isochronous internal
  request path with explicit completion-tracked requests. It validates the
  ACX/render isolation and cleanup harness, but it does not yet prove the
  physical USB transport; that remains a separate bounded canary.
- This interpretation is grounded in the Microsoft KMDF contracts:
  `WdfUsbTargetPipeSendUrbSynchronously` uses an internal request when passed
  `NULL` and the driver cannot cancel that request; its timeout return is not
  a proof that the URB can immediately be destroyed. `WdfWorkItemFlush` waits
  for the callback to return, and `WdfIoTargetCancelSentIo` waits for queued
  requests to complete before stopping the target. See the official
  references for
  [`WdfUsbTargetPipeSendUrbSynchronously`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfusb/nf-wdfusb-wdfusbtargetpipesendurbsynchronously),
  [`WdfUsbTargetDeviceCreateIsochUrb`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfusb/nf-wdfusb-wdfusbtargetdevicecreateisochurb),
  [`WdfWorkItemFlush`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfworkitem/nf-wdfworkitemflush), and
  [`WDF_IO_TARGET_SENT_IO_ACTION`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfiotarget/ne-wdfiotarget-_wdf_io_target_sent_io_action).
- No further physical load is authorized from this state until the new
  parent-owned lifetime implementation is rebuilt and independently reviewed.
  The rebuilt driver and virtual proof target compile with zero warnings or
  errors; the surface contract and offline audio tests pass. The physical
  device is restored to `oem169.inf` / `0.0.131.0`, `OK`/`CM_PROB_NONE`, with
  Secure Boot still enabled.

## 2026-07-12 Emergency Rollback After Repeated Physical Reset

- Two later physical attempts produced new unexpected resets and minidumps:
  `C:\\Windows\\Minidump\\071226-22906-01.dmp` at 16:21:16 local and
  `C:\\Windows\\Minidump\\071226-22359-01.dmp` at 16:28:03 local. Both CDB
  captures show the same failing stack:
  `OpenA8DJUsb!OpenA8DJ_EvtStreamWorkItem+0x720` ->
  `WdfObjectDelete` -> `Wdf01000!FxUsbUrb::Dispose` -> `USBD_UrbFree` ->
  `ucx01000!Xrb_Free`. Evidence is preserved in
  `local-analysis/windows-bugcheck-071226-22906-01-cdb.txt` and
  `local-analysis/windows-bugcheck-071226-22359-01-cdb.txt`.
- The loaded image for both dumps was still the DriverStore package
  `opena8djusb.inf_amd64_84c92b8f64573678`, timestamp 15:28:18. This means
  the crash signature did not move from the old worker `WdfObjectDelete`
  site; the next investigation must first prove that Windows is loading the
  rebuilt binary expected by the source tree before any physical test is
  allowed.
- Emergency rollback was applied at 16:33 local. The exact physical device
  `USB\\VID_17CC&PID_1978\\SN-HKM6Q6EDKP___` was disabled, the active
  `0.0.135.0` package `oem20.inf` was uninstalled and deleted, and the driver
  service was set to `Start=4` before an intentional reboot:
  `OpenA8DJ emergency rollback: disable USB driver service and physical device
  before reboot`. The reboot event is recorded as Event ID 1074 at 16:33:13.
- After the user reported another reset, post-boot checks showed no new
  minidump after 16:28:03. The current boot time is 16:33:32 local, so that
  reset was the planned rollback reboot, not a new bugcheck. Secure Boot is
  enabled (`Confirm-SecureBootUEFI=True`).
- The remaining stale packages `oem168.inf`, `oem169.inf`, and `oem170.inf`
  were then uninstalled and deleted. A final `pnputil /enum-drivers` scan
  found no remaining `OpenA8DJ` / `opena8djusb.inf` package, and the
  `C:\\Windows\\System32\\DriverStore\\FileRepository\\opena8djusb.inf_*`
  directories are gone.
- The orphaned kernel service `OpenA8DJUsbAcx` was stopped/not loaded, then
  deleted with `sc.exe delete OpenA8DJUsbAcx`. A final `sc.exe qc
  OpenA8DJUsbAcx` returns `1060`, confirming that the service no longer exists.
- Final safe state at 16:40 local: the physical Audio 8 DJ device is present
  but disabled (`CM_PROB_DISABLED`), no OpenA8DJ USB transport package remains
  in DriverStore, no OpenA8DJ kernel service remains, and no physical or
  virtual canary is authorized until the binary/package identity problem and
  worker URB lifetime bug are resolved offline.

## 2026-07-12 Physical Candidate 0.0.136 Safety Architecture Ready

- The replacement candidate builds and packages cleanly as version
  `0.0.136.0`, build fingerprint
  `9f92c397aa1ae35ae06a0b7c0d1b74f711bde06c9877b2d602cd65751f718b57`.
  Its `OpenA8DJUsb.sys` SHA-256 is
  `7f57c6c6716fdbaa94b9e5275323dd458a8c3c430fc925986abecbeda3de2ba3`.
  The generated package manifest is the source of truth for every file hash;
  installation and canary scripts reject mismatches and ambiguous DriverStore
  selections.
- The stale-binary and URB-lifetime failure modes are now guarded separately.
  Isochronous requests have explicit completion ownership, timeout cancellation,
  and a synchronous target purge/drain barrier before object teardown. Capture
  and output URBs and buffers are device-parented and cannot be deleted by the
  worker while UCX may still own an XRB.
- A physical load starts inert. It does not start the interrupt reader, issue a
  control transfer, submit an isochronous URB, or start ACX streaming. Each later
  phase (`ControlRead`, `IsoCapture`, `IsoOutput`, `Streaming`) needs an exact
  one-operation authorization and records durable checkpoints plus rolling KMDF
  IFR diagnostics.
- The earliest physical `DriverEntry` action sets the service to `Start=4` and
  flushes that registry change before WPP, WDF, ACX, or USB initialization. If
  the machine bugchecks, the candidate therefore cannot load again on the next
  boot. A SYSTEM boot-recovery task independently preserves the last checkpoint,
  disables the exact device and service, and removes the package.
- Build validation passed with zero warnings or errors, including ApiValidator,
  InfVerif and Inf2Cat. The Windows surface contract and complete offline test
  suite pass. The final stage-only physical gate passed without loading the
  driver; its evidence is in
  `local-analysis/windows/physical-one-shot-20260712-171853-stage`.
- A virtual installation under Secure Boot reached Windows Code 52 despite the
  exact test certificate being trusted. The first physical runtime gate therefore
  requires a one-boot Windows startup with driver-signature enforcement disabled.
  Secure Boot does not need to be disabled permanently for the first attempt.
- Current handoff state: the exact Audio 8 DJ device remains disabled
  (`CM_PROB_DISABLED`), the OpenA8DJ service does not exist, and there are zero
  `opena8djusb.inf_*` physical package directories in DriverStore. No unexpected
  reset occurred while building or staging this candidate.
- The only authorized next command is the bounded `LoadInert` phase:
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File windows\\tests\\run-a8dj-driver-load-canary.ps1 -PackageDir windows\\dist\\Release\\x64 -Phase LoadInert -AllowPhysicalLoad -AcknowledgeCrashRisk`.
  It must not be run until the user confirms the one-boot signature-enforcement
  exception. No control, isochronous, or streaming phase is implied by that
  confirmation.

## 2026-07-12 First Bounded Physical Gates and EP1 Diagnosis

- Candidate `0.0.136.0` passed the first physical `LoadInert` gate. The loaded
  driver reported API 28 and the exact expected fingerprint while disarmed,
  with `prepared=1`, `worker=0`, no USB operation authorization, and no crash or
  reset event. Evidence is in
  `local-analysis/windows/physical-one-shot-20260712-173301-loadinert`.
- The first `ControlRead` gate stopped on Win32 error 121 without a crash. A
  comparison with the upstream Linux CAIAQ implementation showed that EP1 uses
  a 64-byte command/reply URB, not the original 512-byte Windows reader buffer.
- Candidate `0.0.138.0` added the required KMDF `stop -> configure -> start`
  continuous-reader transition. Its diagnostic capture then localized the
  failure to `0xC0000206` (`STATUS_INVALID_BUFFER_SIZE`) before any EP1 command
  completed; this was not a device timeout or an isochronous failure.
- Microsoft documents that KMDF rejects a read buffer that is not a multiple of
  the high-speed pipe maximum packet size unless
  `WdfUsbTargetPipeSetNoMaximumPacketSizeCheck` is used. Candidate `0.0.139.0`
  applies that exception only to the 64-byte CAIAQ EP1 bulk-in reader. It builds,
  signs, packages, and passes all offline gates with fingerprint
  `e4db518fefef24a11aa33f1ac926acc8768587a0aabffc8b39014d01565fd84d`
  and SYS SHA-256
  `43b0aafc6a03f47f8921698636b36f728fade4ff4789f92aa1b71c05235d7c95`.
- Stage and `LoadInert` for `0.0.139.0` passed. Before its `ControlRead` could
  run, Windows reported that the exact device was pending a system reboot from
  the preceding PnP operations. The preflight rejected the enabled device and
  did not stage or load the driver.
- The harness now treats `pnputil` text as well as final PnP state as security
  relevant: package removal may asynchronously rebind the vendor driver, so
  normal cleanup and boot recovery poll and reassert `CM_PROB_DISABLED` after
  settling. The next attempt requires another one-boot driver-signature
  enforcement exception to clear Windows' pending PnP operation.
- No BSOD, unexpected reset, Event 41, Event 6008, or bugcheck event occurred in
  any of these bounded gates. No isochronous or streaming phase was attempted.

## 2026-07-12 Physical EP1 and One-Shot Isochronous Gates Passed

- After clearing the pending PnP operation with a one-boot signature-enforcement
  exception, candidate `0.0.139.0` passed `ControlRead`. The 64-byte EP1 reader
  returned coherent Audio 8 DJ hardware state (`timecode-vinyl`, ground-lift
  flags) in the bounded operation. Evidence:
  `local-analysis/windows/physical-one-shot-20260712-175826-controlread`.
- `IsoCapture` passed with five scheduled packets, four 320-byte completions,
  zero USBD errors, and nonzero physical capture bytes. Evidence:
  `local-analysis/windows/physical-one-shot-20260712-175930-isocapture`.
- `IsoOutput` passed with 1,280 bytes of silence, zero playback errors, and a
  paired capture snapshot with zero USBD errors. Evidence:
  `local-analysis/windows/physical-one-shot-20260712-180033-isooutput`.
- The first `Streaming` gate returned Win32 170 / `STATUS_DEVICE_BUSY` before
  starting the worker. Diagnostics proved `worker-iterations=0`, zero USB input
  and output packets, and no crash/reset event. The cause was a state-machine
  defect: the physical context is deliberately initialized with
  `StreamStopRequested=1`, but the legacy START IOCTL required it to already be
  zero before atomically claiming the inactive worker.
- Candidate `0.0.140.0` fixes START so it first claims an inactive worker, then
  performs the stopped-to-running transition; configuration failure restores
  the stopped state and releases the worker. It builds, signs, packages, and
  passes offline tests with fingerprint
  `a032333453fb77dd3b7a1af8306fa7ed827321eb2af1564421751ebe4f2d954d`
  and SYS SHA-256
  `51c33bbd553b159c3ef7cfbd86d6508cd5407c875ea5d5a85bd62097a399db22`.
- Repeated package removal/rebind cycles again left Windows with a pending PnP
  reboot before `0.0.140.0` could be physically staged. OpenA8DJ is absent from
  DriverStore and its service is absent. The next physical action is a fresh
  Stage/LoadInert gate followed by one second of bounded Streaming; no longer
  duration is authorized until that gate passes.

## 2026-07-12 Streaming BSOD Root Cause and 0.0.141 Containment

- Candidate `0.0.140.0` loaded inert and verified the exact build fingerprint,
  then crashed during the bounded `Streaming` canary cleanup. Evidence is in
  `local-analysis/windows/physical-one-shot-20260712-181655-streaming`.
- The crash was a real bugcheck, not a normal reboot: Event 1001 reported
  bugcheck `0x0000007e` with exception `0xffffffffc0000005`, dump
  `C:\Windows\MEMORY.DMP`, report id `3a911b73-c2c0-4cec-bf9b-791cf756d065`.
  The last durable checkpoint was `loaded-binary-verified-inert`; the later
  operation output files were zero-filled by the crash.
- Minidump stack analysis localized the fault to PnP removal teardown in
  `Wdf01000!USBD_UrbFree -> Wdf01000!FxUsbUrb::Dispose ->
  ucx01000!Xrb_Free`, not to a direct OpenA8DJ frame. This implicated the
  previous lifetime strategy for isochronous URB/XRB memory objects parented to
  the long-lived `UsbDevice`.
- Candidate `0.0.141.0` removes persistent streaming URB fields from the device
  context. Streaming still reuses plain WDF transfer buffers, but capture and
  output now allocate an isochronous URB per transfer, wait for completion or
  `WdfIoTargetPurgeIoAndWait`, copy telemetry, and delete the URB before
  returning to the worker.
- `0.0.141.0` builds with zero warnings/errors, passes ApiValidator, InfVerif
  `/w` and `/h`, Inf2Cat, catalog signing/verification, the Windows surface
  contract, and the offline audio engine contract. Current package fingerprint:
  `c5e197181a96a3fff162466c54318d2e92da7c4d18f40ce76961ddd46df90ba4`.
  Current SYS SHA-256:
  `ad4b21176c824d919613119c6ada66b850778f0561b177a97b4d90a575ce0f83`.
- Post-crash cleanup removed the old `oem20.inf` OpenA8DJ package, deleted the
  `OpenA8DJCanaryRecovery` scheduled task, confirmed `OpenA8DJUsbAcx` service
  absence, and left the exact hardware disabled under the vendor service. The
  current boot is normal (`bcdedit` has no one-boot signature-enforcement
  exception), so the next physical load requires a fresh F7/driver-signature
  enforcement disabled reboot.

## 2026-07-12 0.0.141 Signature Gate and Canary Repairs

- A physical `Streaming` attempt reached only package staging and binding in
  `local-analysis/windows/physical-one-shot-20260712-183730-streaming`. Windows
  bound the exact `0.0.141.0` package as `oem20.inf` but reported
  `CM_PROB_UNSIGNED_DRIVER`; therefore `DriverEntry` and USB streaming did not
  execute. There was no new bugcheck, Event 41, or reset.
- Rollback succeeded: `oem20.inf`, `OpenA8DJUsbAcx`, and the boot recovery task
  are absent, and the exact device is disabled under the Native Instruments
  service. The next physical attempt still requires Startup Settings option 7.
- The BSOD had left the shared hardware-lock owner file filled with NUL bytes.
  Lock recovery now treats a malformed owner as stale only after a five-second
  creation grace period, preserving the mkdir/owner-write race guard.
- The canary no longer calls the nonexistent `reg flush` command. It persists
  the one-shot `Start` value with the native `RegFlushKey` API and now stops
  immediately with a specific diagnostic when binding reports
  `CM_PROB_UNSIGNED_DRIVER`.
- Earlier prompt automation remains historical evidence in the section above.
  In the current execution environment, Windows Security is a user-mediated
  boundary: the canary validates the exact INF hash and installs its test
  certificate, but a remaining `Install this driver software anyway` prompt
  must be accepted by the user.

## 2026-07-12 0.0.141 Immediate Streaming BSOD and 0.0.142 Ownership Fix

- After a confirmed Startup Settings option-7 boot at 18:41:54, the exact
  `0.0.141.0` package bound successfully with `CM_PROB_NONE`. No Windows
  Security prompt appeared because the package certificate was already trusted.
- The one-second `Streaming` canary then bugchecked immediately. Event 1001
  recorded `0x0000007e (0xffffffffc0000005, 0xfffff803857b5120,
  0xfffff581b07d2378, 0xfffff581b07d1bb0)`, dump
  `C:\Windows\MEMORY.DMP`, report id
  `94390782-1821-4d66-bf84-b31f4ff4a379`. Evidence is in
  `local-analysis/windows/physical-one-shot-20260712-184929-streaming`.
- The last durable canary checkpoint was `physical-bind-returned`; the dump
  stack independently proves that the streaming worker reached its first
  isochronous capture transfer. Private symbols resolve the fault exactly to
  `OpenA8DJ_CaptureIsoSnapshotWithPayload+0x3e8` at its
  `WdfObjectDelete(urbMemory)` call. The stack is
  `WdfObjectDelete -> FxUsbUrb::Dispose -> USBD_UrbFree -> Xrb_Free ->
  WdfObjectDereferenceActual -> FxMemoryObject::Release`, where UCX attempts a
  final release of the same zero-refcount memory object during its destruction.
- Microsoft documents that an isochronous URB memory object can be parented to
  either `WDFUSBDEVICE` or `WDFREQUEST`. The matching OSR/NTDEV lifetime case
  recommends creating the request first, parenting the URB memory to that
  request, and reclaiming only the request after completion so KMDF performs
  child teardown in the correct order.
- Candidate `0.0.142.0` implements that ownership model for capture and output:
  each transfer creates one request; its URB memory (and the capture transfer
  buffer) are request children; the completion/timeout barrier retires all I/O;
  and only the request is explicitly deleted. Direct
  `WdfObjectDelete(urbMemory)` calls and redundant full-URB zeroing are gone.
- Post-crash rollback deleted `oem20.inf`, the recovery task, and the stopped
  `OpenA8DJUsbAcx` service, then left the exact device disabled under the Native
  Instruments driver (`CM_PROB_DISABLED`). No candidate remains loadable at
  boot.
- `0.0.142.0` passes a clean Release x64 build, ApiValidator, InfVerif `/w` and
  `/h`, Inf2Cat, signed-catalog verification, all offline contracts, and Visual
  Studio driver code analysis with zero remaining warnings. SDV is unavailable
  in the installed WDK because Microsoft removed it from VS2022-era kits.
  Fingerprint:
  `e3b0c1eef9228de2530779743d71abc5add458c0f1156ffd4fe7a5aca2ffecd4`;
  SYS SHA-256:
  `8bac5f946018f60b14080019edeb9358fd362f381a4b46e23cea8051f49081fa`.
- `windows/tests/test-windows-hardware-lock.ps1` now reproduces the crash-left
  lock states without hardware: free, fresh ownerless race, old malformed owner,
  live owner rejection, dead owner recovery, stale evidence, and release. It is
  part of `run-offline-tests.ps1` and passes with the full offline suite.
- The unused legacy queued bulk-read helper, which independently deleted a
  device-parented WDF memory object, has been removed; EP1 control traffic uses
  the already proven continuous reader. The compile-gated async output slots
  now also parent their URBs to their requests and never directly delete URB
  memory. An explicit `OPENA8DJ_ENABLE_ASYNC_OUTPUT=1` Release/PREfast build
  passes, in addition to the normal disabled-path build.

## 2026-07-12 0.0.143 Ownership Diagnostic and 0.0.144 Streaming BSOD

- Candidate `0.0.143.0` instrumented request, buffer, and URB creation. Its
  bounded `IsoCapture` gate failed safely at checkpoint `292` with
  `STATUS_INVALID_PARAMETER`: a request created by `WdfRequestCreate` had its
  default `WDFDRIVER` parent and was therefore not a valid parent chain for
  `WdfUsbTargetDeviceCreateIsochUrb`.
- Candidate `0.0.144.0` rooted each internal request at `WDFUSBDEVICE`, parented
  URB memory to that request, and called `WdfRequestReuse` after completion.
  Two isolated captures and one isolated silence output passed, including
  terminal checkpoints `304` and `314` with nonzero physical data.
- The one-second Streaming gate then bugchecked with `0x7e/c0000005`. Private
  symbols resolve the fault to
  `OpenA8DJ_CaptureIsoSnapshotWithPayload+0x750`, inside
  `WdfObjectDelete(request)`, after checkpoint `303` confirmed that
  `WdfRequestReuse` succeeded and before checkpoint `304` could run. The stack
  again is `FxUsbUrb::Dispose -> USBD_UrbFree -> ucx01000!Xrb_Free ->
  FxMemoryObject::Release -> FxObject::ProcessDestroy` with a null object.
  Evidence:
  `local-analysis/windows/physical-one-shot-20260712-194852-streaming`.
- Post-crash rollback removed `oem20.inf` and `OpenA8DJUsbAcx`, restored the
  Native Instruments `oem44.inf/a8djusb_svc` binding, and disabled the exact
  device. Candidate `0.0.144.0` is prohibited from any further load.

## 2026-07-12 0.0.145 Legacy KMDF Plain-URB Backend

- Candidate `0.0.145.0` removes the failing XRB lifecycle by replacing
  `WdfUsbTargetDeviceCreateWithParameters(USBD_CLIENT_CONTRACT_VERSION_602)`
  with the supported compatibility API `WdfUsbTargetDeviceCreate`.
- Isochronous URBs are now ordinary nonpaged `WDFMEMORY` allocations sized by
  `GET_ISO_URB_SIZE`; the source and surface contract prohibit
  `WdfUsbTargetDeviceCreateIsochUrb`, `USBD_UrbAllocate`, and `USBD_UrbFree`.
- Capture and output each have a persistent request and plain URB allocated
  outside the transfer hot path. Transfers serialize through
  `IsoTransportLock`, reformat/send/wait/reuse the persistent request, and end
  at slot-idle checkpoints `304`/`314`; they do not create or delete framework
  objects per transfer.
- `EvtDeviceReleaseHardware` stops/purges USB targets, flushes the stream work
  item, then destroys the idle plain-URB transport resources before resetting
  the pipe map.
- Recovery now registers an explicit delayed SYSTEM task command with a frozen
  state path, verifies package/service absence plus `CM_PROB_DISABLED`, records
  `recovery_succeeded`, and removes itself only after all rollback invariants
  hold. An idempotent recovery simulation passes.
- A new `IsoStress` canary performs 25 capture/output cycles in one authorized
  IOCTL, requiring all 25 completions, zero capture/playback errors, nonzero
  output bytes, and terminal checkpoint `314` before Streaming is permitted.
- Release x64 build, ApiValidator, InfVerif `/w` and `/h`, Inf2Cat, signed
  catalog verification, offline contracts, hardware-lock tests, normal
  PREfast, and async-output PREfast all pass. Frozen package identity:
  fingerprint
  `f9d534473a44f08f95034bb50b63715afd64590247dfb51a103bd094bb00ff99`;
  SYS SHA-256
  `4a20b637ec6baa2e968d10e077936b1614c101ed6abf6d1960cfabadf23e6538`.
- No physical load of `0.0.145.0` has occurred yet. The current boot followed a
  bugcheck and therefore no longer has the one-boot signature-enforcement
  exception. The next authorized sequence is Stage, then a fresh option-7 boot,
  followed by LoadInert, ControlRead, IsoCapture, IsoOutput, IsoStress, and only
  then one-second Streaming.

## 2026-07-12 0.0.146-0.0.158 Physical ACX/WASAPI Bring-Up

- `0.0.146.0` changed the high-speed isochronous packet group from five to the
  required multiple of eight. Capture, output, 25-transfer `IsoStress`, and
  bounded streaming then completed through checkpoints `304`, `314`, and
  `409/509` without a bugcheck.
- `0.0.147-0.0.150` removed obsolete canary gates from bounded diagnostics and
  ACX `Run`, and stopped recoverable stream errors from poisoning the PnP
  `DeviceStopping` state. Independent WASAPI render and capture opened at
  44.1 kHz with zero packet errors.
- `0.0.151-0.0.153` explicitly assigns the ACX 48 kHz default, publishes a
  serialized `PKEY_AudioEngine_OEMFormat`, uses the 7.1 surround channel mask
  for the aggregate 8-channel endpoint, and rotates the experimental KS
  reference strings so Windows cannot migrate the invalid old 44.1 kHz/mask-0
  endpoint state. WASAPI now enumerates 8-in/8-out plus three stereo pairs at
  48 kHz; aggregate exclusive mode accepts both 44.1 and 48 kHz.
- `0.0.154` tolerates up to three consecutive transient capture/output USB
  failures without terminating the ACX worker. A prior five-case matrix hang
  caused by one transient capture failure no longer reproduces. The full
  8x8/48 kHz WASAPI matrix (pairs A-D and all pairs) completed with balanced
  `Prepare/Run/Pause/Release`, zero status events, zero clicks, and zero USB
  errors. A later 20-second run completed despite one recovered transient.
- `0.0.155` makes proven control reads/profile writes available without manual
  canary arming and rejects hardware writes while the audio worker is active.
  `timecode-vinyl` write/readback matched exactly with no protocol mismatch.
- `0.0.156-0.0.157` experimentally grouped 32 packets per URB to reduce CPU.
  Capture completed, but output exceeded the practical transfer limit and
  produced thousands of packet errors. The experiment was stopped without a
  crash and is prohibited from further use.
- `0.0.158.0` is the current safe candidate. It restores the proven eight-packet
  transport while retaining format, retry, and hardware-control fixes. Release
  build, ApiValidator, InfVerif, Inf2Cat, catalog verification, inert load,
  `iso-silence`, and 8-channel WASAPI 48 kHz output all pass with zero errors.
  Package fingerprint:
  `106854e0907af7d16adea656ea1719fee7038498c56d0a2c05f864ece7f78621`;
  SYS SHA-256:
  `72b35d0f975a1c0d907e84d04ca2d3ccc3c9734a86f8a2d3e293a6e58dc611de`.
- No BSOD or unexpected reset occurred anywhere in the `0.0.145-0.0.158`
  physical sequence. Historical crash events remain limited to 19:50 or
  earlier. `OpenA8DJUsbAcx` remains `Start=4`, and
  `OpenA8DJCanaryRecovery` remains armed while the temporary candidate is
  loaded.

Remaining gates:

- CPU attribution shows the synchronous isochronous worker consumes about one
  logical core in `System` during full duplex. Production work must replace the
  per-URB synchronous wait/reformat loop with a bounded preallocated async
  capture/output pipeline; larger URBs are not a valid workaround.
- Windows 10 MIDI requires a KS/PortCls MIDI 1.0 child driver over the shared
  EP1 transport. End-to-end proof also requires a physical DIN OUT-to-IN cable.
- Traktor Pro 3 is installed and launches, but its canvas cannot be captured by
  the current Windows automation helper (`SetIsBorderRequired`, `0x80004002`),
  so no blind UI changes were made. ASIO is optional for the WASAPI-capable base
  driver and requires an explicit GPLv3-versus-proprietary Steinberg license
  decision if implemented.

## 2026-07-12 0.0.159 Hot-Path Trace Test and Async-Engine Decision

- `0.0.159.0` moves the successful isochronous checkpoint budget ahead of WPP
  emission. It keeps every error, the first 64 transfer checkpoints, one IFR
  sample per 1024 later checkpoints, and the current checkpoint/status in
  crash-dump memory. Sampled events never execute `KdPrint` or registry I/O.
- Release, PREfast/DriverMinimumRules, ApiValidator, InfVerif, Inf2Cat, offline
  contracts, catalog signing, and signature verification pass. Package identity:
  fingerprint `b2747206413c08071e8972a549a4b0d1efedb2938f2296f8fb7b9eee5bc02776`;
  SYS SHA-256 `5795486ee1c48cabd2835715e667b77f47836427c4fe156897963c136e4b5665`.
- Escalated physical gates all completed without reset or bugcheck: inert load,
  control read, one capture, one silence output, 25 IN/OUT iterations, and a
  one-second continuous stream. Evidence is under the corresponding
  `physical-one-shot-20260712-221607` through `-222801` directories.
- The ControlRead canary expected obsolete checkpoint 210 even though normal
  control reads intentionally no longer consume destructive authorization.
  Its expected checkpoint is now 200; operation output plus the finally-block
  disarm prove the safe path.
- Functional 8x8 WASAPI remained clean for 20 seconds and 12 seconds at 48 kHz:
  no status events, clicks, clipping, or per-pair render failures. Trace sampling
  did not fix CPU. Independent counters attribute about 84 percent of one
  logical processor to `System`, versus about 1 percent to `audiodg`; therefore
  the synchronous capture/wait/process/output/wait transport remains the cause.
- A direct activation of the old compile-gated async-output experiment was
  rejected before physical load: its slots are worker-stack-owned, capture
  remains synchronous, and its lifetime model is insufficient for PnP. The
  next candidate must use device-context-owned capture and output slots,
  preallocated request/URB/buffer ownership, generation checks, and a single
  purge/drain barrier.

## 2026-07-13 0.0.164 Async Audio and Truthful Controls Candidate

- `0.0.160` replaced the synchronous USB loop with four persistent,
  device-owned isochronous slots per direction. Completions only publish state;
  passive work items perform PCM conversion, request reuse, and resubmission.
  Generation checks, serialized purge, explicit engine states, and a complete
  drain barrier protect stop, PnP release, and resource destruction.
- Physical validation reduced attributed `System` CPU from roughly 84 percent
  of one logical processor to about 18 percent during the 20-second attribution
  run. Full-duplex 8x8 WASAPI remained clean at 48 kHz shared and 44.1 kHz
  exclusive, with zero packet errors, late completions, underruns, or overruns.
- `0.0.161` added bounded EP1 retry/backoff and closed a pre-existing teardown
  race by serializing `SendBulkCommand` with `ReleaseHardware`, capturing pipe
  handles under the lock, and aborting retries when PnP teardown begins.
- Direct ground-lift experiments proved that this physical unit always reads
  byte 3 back as `03`. Both the established six-byte write and the Linux-style
  63-byte EP1 payload completed at USB level but did not change that readback.
  `0.0.163` therefore made `gnd-*` truthful readback-only controls: the CLI
  returns error 50 before SET, and the kernel rejects external requests that
  alter those bits before sending USB traffic.
- `0.0.164` refreshes physical control state before applying a profile, fixing
  stale ground bytes after ACX/PnP cycles. The API 34 control contract passes
  12/12 cases: nine supported profile/mode/lock writes and three expected
  readback-only ground commands, with clean restoration.
- Final package identity: version `0.0.164.0`, fingerprint
  `b56cfb2c1c69951d5e8f5f7d24c37c198528316554d69693f7624ce5a3ac47c3`,
  SYS SHA-256 `4ae0ff79af94ab6be1342d4bfcca8bc22e8a6f968484d0cc703bb4015891fae8`.
  Release `/W4 /WX`, PREfast/DriverMinimumRules, ApiValidator, InfVerif,
  Inf2Cat, catalog verification, and all offline contracts pass.
- Final physical gates pass without reset or bugcheck: inert load, control read,
  control contract, capture, output, one-second streaming/drain, real 8x8
  WASAPI, 44.1 kHz exclusive, and repeated 48 kHz start/stop. The installed
  service remains `Start=4`; SYSTEM boot recovery remains armed and Ready.
- Remaining non-audio completion gates are external: MIDI endpoint validation
  needs a DIN OUT-to-IN loopback cable, and Traktor UI validation needs either
  human interaction or a working capture path. Native ASIO remains optional
  because the base driver is functional through WASAPI and requires an explicit
  licensing decision.
