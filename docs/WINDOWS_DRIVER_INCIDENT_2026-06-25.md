# Windows Driver Incident - 2026-06-25

Status: **stop all Windows hardware testing**.

The OpenA8DJ Windows driver is **not available for use**. It is work in
progress, unstable, and has caused local Windows tablet hangs/reboots during
Audio 8 DJ driver testing. Do not install it for normal use, DJ playback,
Traktor, iRig capture, unattended validation, or production work.

## What Happened

During the v134 Windows tablet session, the driver had passed several narrow
gates:

- A load canary observed `OpenA8DJUsb` version `0.0.134.0` with PnP status
  `OK`, `CM_PROB_NONE`, and no bugcheck event during that canary.
- A read-only control canary saw API version `27`, `streaming: no`,
  `start_requests: 0`, `worker_iterations: 0`, and no bugcheck event during
  that canary.
- A passive local hardware smoke saw the Audio 8 DJ associated with OpenA8DJ,
  USB pipes ready, iRig Stream present/OK, and the Realtek microphone array as
  the dictation default.

Those gates were too narrow to prove safety, quality, or stability.

The next intended step was a very short stream canary: query stream state,
query diagnostics, start streaming, wait 500 ms, stop streaming in `finally`,
then verify `streaming: no` and no bugcheck event.

The first wrapper for that canary had a PowerShell bug: it used a parameter
named `$Args`, which conflicted with PowerShell's automatic `$Args` variable.
That attempt called `opena8djctl` without the intended subcommands and did not
start streaming.

After fixing the wrapper, the corrected attempt failed before streaming because
`opena8djctl` could not find the OpenA8DJ driver interface:

```text
OpenA8DJ Windows driver interface not found. Install OpenA8DJUsb first.
```

A passive `pnputil` check then showed the Audio 8 DJ still associated with
OpenA8DJ but blocked by Windows signature policy:

```text
Manufacturer Name: OpenA8DJ
Status: Problem
Problem Code: 52 (0x34) [CM_PROB_UNSIGNED_DRIVER]
Problem Status: 0xC0000428
Driver Name: oem169.inf
```

The user then observed another machine hang/reboot. Treat that as an
unresolved system-stability incident even though the last observed command was
not intended to start streaming.

## Current Interpretation

The Windows driver must be treated as unsafe until proven otherwise. In
particular:

- Prior successful canaries do not prove stability.
- Prior iRig/Traktor notes are historical evidence only and are superseded by
  this incident.
- A `CM_PROB_UNSIGNED_DRIVER` state means the driver package may be associated
  while the kernel driver interface is unavailable.
- Any operation that loads, starts, stops, streams, sends ISO diagnostics, or
  rebinds the device can affect system stability and must be avoided until the
  incident is reviewed offline.

## Do Not Run

Do not run these commands or equivalent hardware paths until the incident is
explicitly reviewed:

- `opena8djctl start`
- `opena8djctl stop`
- `opena8djctl iso-silence`
- `opena8djctl iso-tone`
- `opena8djctl audio-params`
- Traktor smoke or active tests
- WASAPI, PortAudio, or iRig probes
- driver install, uninstall, restart, rebind, disable, or enable commands
- reboot or boot-option automation

## Required Next Work

If Windows development resumes, start offline only:

1. Inspect Windows Event Log and crash dumps from the reboot incidents before
   touching the device again.
2. Determine why the host returned to `CM_PROB_UNSIGNED_DRIVER` after a boot
   that was expected to allow unsigned drivers.
3. Quarantine or remove the short-stream wrapper until it has been reviewed.
4. Rebuild the safety plan so every hardware step has a checkpoint, a stop
   condition, and a recovery path.
5. Do not claim sound quality, Traktor stability, or CPU performance until
   physical capture tests pass after the stability issue is fixed.

## Repository Label

Use this label for the Windows driver until the incident is resolved:

```text
windows-driver: work in progress, not available for use, unstable, can hang/reboot host
```

## 2026-07-12 Offline Hardening Follow-up

The source was hardened offline in local commit `0602672` before any new
hardware load:

- removed persistent registry writes from ACX real-time callbacks;
- added a `PASSIVE_LEVEL` guard to the remaining stage recorder;
- added active-stream locking and `EX_RUNDOWN_REF` protection so the worker
  cannot use an ACX stream context while its RT packet MDL is being freed.

Verification completed without touching the device:

- WDK Release x64 compile/link: 0 warnings, 0 errors;
- `ApiValidator`: passed;
- Windows surface and offline audio-engine contracts: passed;
- `Inf2Cat`: passed with 0 errors and 0 warnings;
- test certificate signing and catalog membership verification: passed;
- attestation CAB export: passed.

The patched package remains unvalidated at kernel runtime. The elevated
attempt to enable test signing was rejected by Secure Boot with:
`The value is protected by Secure Boot policy and cannot be modified or
deleted.` A normal-boot installation therefore requires Microsoft attestation
or HLK/WHQL signing; disabling Secure Boot is not an acceptable normal-user
installation path. Do not install, bind, stream, or run Traktor/iRig tests from
this package until that signing gate and a supervised runtime canary are
complete.
