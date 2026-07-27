# Vintage Compatible driver mode design

Status: implementation contract for an experimental, unverified MVP.

## Purpose and claim boundary

`vintage-compatible` is a session-scoped driver mode that selects the safest
observable policies in OpenA8DJ for applications which depended on behavior
documented for the original Audio 8 DJ macOS driver. It is an independent
compatibility mode, not the original driver and not an emulation of its private
implementation.

The catalog and every user-facing surface must call it **Vintage Compatible
(Experimental — Unverified)** until a declared conformance matrix has passed.
The bare word `compatible` in the canonical ID is a goal and policy name. It is
not a certification or a statement of exact parity.

“Reproduce exactly” means only this falsifiable objective:

> For a named OpenA8DJ build, macOS version, Audio 8 DJ hardware/firmware
> identity, sample rate, buffer, channel surface, application/version and test
> case, every required observable in the declared matrix matches its
> independently obtained expectation within the declared tolerance, with no
> hidden fallback.

The result is scoped to that matrix. It must never be generalized to all
applications, macOS versions, hardware revisions, sample rates or behavior
which was not observed. The MVP cannot reach full old-driver parity because
known surface gaps remain; it can only make those gaps visible and provide a
safe subset for later comparison.

OpenA8DJ retains its own name, bundle ID, device UID, signing identity and
artwork. This work must not copy, redistribute, load, disassemble or
automatically operate proprietary driver binaries, installers, firmware,
panels, logos or screenshots. No feature gate may depend on proprietary
software being installed.

## Evidence and provenance rules

Every expectation has one provenance class:

| Class | Permitted source | Strength |
| --- | --- | --- |
| `repo-implementation` | Current MIT source, build configuration and tests | Proves current behavior only |
| `repo-observation` | A fact already recorded in repository documentation and obtained under `docs/LEGAL.md` | Candidate legacy expectation; provenance path must be named |
| `public-documentation` | Public hardware/manual/API fact with title, URL/version and retrieval date recorded in a future report | Candidate legacy expectation |
| `owned-hardware-observation` | New behavior observed from lawfully owned hardware under the shared lock, with raw artifact manifest | Strongest comparison evidence |
| `unknown` | Memory, inference, hearsay, unattributed value or unverifiable material | Never a required expectation and never evidence of compatibility |

The implementation may encode facts from the first two classes below. A future
campaign may add the next two in a report, not by silently changing constants.
`docs/OLD_DRIVER_COMPAT_PLAN.md` is an allowed repository observation, not a
license to repeat or inspect the proprietary material mentioned by that file.
`docs/LEGAL.md`, `BRAND_POLICY.md` and `NOTICE.md` remain authoritative.

No listening impression, clean counter set or single application launch proves
compatibility. A report records source, observer, date, hardware/firmware,
software versions, exact commands, expected/actual values, tolerance and raw
artifact hashes.

## Findings in the current tree

The implementation contract starts from these observable facts:

- The HAL publishes the OpenA8DJ identity and an 8-input/8-output channel
  surface: one 8-channel input stream and four stereo output streams in the
  shipping build.
- The HAL advertises 44.1, 48, 88.2 and 96 kHz. The USB transport has explicit
  rate codes for all four.
- The public buffer defaults to 512 frames, advertises 512–4096 frames and
  normally normalizes positive requests to 512, 1024, 2048 or 4096.
- The HAL zero-timestamp period is 16384 frames. The shipping build keeps the
  experimental USB-derived HAL timestamp path disabled and uses a monotonic
  host-time projection.
- The shipping build sends a reset-style `AUDIO_PARAMS` request before the
  real stream parameters, uses capture-completion-paced playback and starts
  mode-2 output packing at byte 4.
- Driver modes are transactional, process-session-only state with
  requested/effective/pending reporting and immutable per-stream policy
  snapshots.
- Hardware input profiles are a separate axis. `timecode-optimized` adds an
  explicit arm/evidence lifecycle and stores only `balanced` or `performance`
  as its fallback today.

Repository observations in `docs/OLD_DRIVER_COMPAT_PLAN.md` additionally record
a 512-frame legacy public buffer base, 24-bit stereo streams per pair, rates
44.1/48/88.2/96/192 kHz, deep 64-IN/128-OUT preparation, reset-style stream
start, capture-paced output and output byte start 4. They also record that
misapplying input cursor 2 to playback produced loud noise.

These facts show why the MVP cannot claim complete parity: the modern HAL uses
`Float32` client buffers, one 8-channel input stream, does not expose 192 kHz,
and the shipping transfer geometry is not the documented 64/128 legacy slot
model. Those differences are reported capabilities/gaps, not concealed.

## Rate decision, including 88.2 kHz

The Vintage Compatible MVP keeps exactly the current four-rate allowlist:

```text
44100, 48000, 88200, 96000
```

88.2 kHz is retained. It is both present in the current HAL/USB rate-code path
(`repo-implementation`) and recorded among the old driver's sample-rate
constants in `docs/OLD_DRIVER_COMPAT_PLAN.md` (`repo-observation`). There is no
evidentiary basis for treating it differently from 44.1, 48 or 96 kHz.

192 kHz is not added by this runtime mode. Although the repository observation
records it on the old Core Audio surface, the current USB transport has no
192-kHz rate mapping and the HAL does not advertise it. Adding it would be a
transport feature requiring separate implementation and physical validation.
The capability `rate192000` is therefore false and reason
`rate_192000_not_implemented` remains a full-parity gap.

Selection does not change the current sample rate. While Vintage Compatible is
effective, an idle configuration request may select any of the four supported
rates through the existing Core Audio configuration-change protocol. Unknown,
non-finite and 192-kHz requests fail closed. Rate changes while any I/O client
is active are rejected for this mode; they are never applied mid-stream.

## Observable MVP policy

This mode is not an alias for `balanced`. Its descriptor contains an explicit
Vintage policy and changes buffer normalization while it is effective:

| Policy field | Vintage Compatible MVP |
| --- | --- |
| Public channel/stream surface | Existing OpenA8DJ 8-in/8-out surface, unchanged and reported as a gap from the documented legacy stream partition |
| Supported rates | 44.1/48/88.2/96 kHz; no rate is selected implicitly |
| Preferred/current buffer on entry | Exactly 512 frames required by preflight |
| Buffer request normalization | Every valid request in 1–4096 normalizes to 512 while effective |
| Buffer/rate mutation boundary | Idle Core Audio boundary only |
| Output start/restart/target | 8192/4096/8192 frames, snapshotted like `balanced` |
| Worker QoS | Default, not performance QoS |
| HAL timestamp policy | Host-monotonic, 16384-frame period; no forced USB zero timestamp |
| USB playback cadence | Capture-completion paced, compile-time invariant |
| Stream start | Reset-style `AUDIO_PARAMS` then actual parameters, compile-time invariant |
| Mode-2 playback start | Byte 4, compile-time invariant |
| Hardware input profile | Untouched |

The fixed-512 normalization is deliberately narrower than normal OpenA8DJ. It
is a real, reversible runtime policy for the only public buffer value supported
by both current implementation evidence and the documented legacy default. It
does **not** assert that the old driver rejected or normalized every other
request the same way.

The advertised 512–4096 range remains unchanged in this MVP for existing HAL
surface compatibility. As with the shipping normalizer, read-back is
authoritative: a request for 1024 while Vintage Compatible is effective reads
back as 512. A future exact property-range change requires its own
`RequestDeviceConfigurationChange` action, corresponding perform/abort paths,
idle-only commit and property notifications. It must not be smuggled through a
mode setter.

Entering Vintage Compatible when the current public buffer is not 512 fails
preflight without mutation. The caller must first request 512 through Core
Audio and retry. The mode must not directly mutate `gSampleRate`,
`gBufferFrames`, stream lists or formats from the private IPC worker.

## Build-invariant preflight and capability snapshot

The implementation exposes a pure, offline-testable preflight descriptor. At
minimum it records the actual compiled values of:

- `OPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM`;
- `OPENA8DJ_PLAYBACK_CAPTURE_PACED`;
- `OPENA8DJ_OUTPUT_START_BYTE`;
- `OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING`;
- HAL USB-zero-timestamp selection and zero-timestamp period;
- supported-rate mask;
- active input/output channels and stream partition;
- public buffer/default normalization policy;
- capture/playback queue geometry; and
- client sample format.

The three safety-critical invariants below are mandatory:

| Invariant | Required value | Failure reason |
| --- | ---: | --- |
| Reset-style `AUDIO_PARAMS` before stream | enabled | `audio_params_reset_disabled` |
| Playback paced by completed capture | enabled | `capture_paced_output_disabled` |
| Mode-2 output start byte | `4` | `output_start_byte_not_4` |

The MVP also fails preflight if explicit future-frame scheduling or the forced
USB HAL zero timestamp is enabled, the 16384-frame host timestamp contract is
not present, the current buffer is not 512, or the four-rate allowlist/rate
codes do not agree. Those failures use stable individual reasons, never a
generic “unsupported” string.

Known differences which do not make the safe subset unusable are capabilities
with false values and gap reasons: `rate192000`,
`legacyStereoStreamPartition`, `legacyClientInt24`,
`legacyCaptureDepth64`, and `legacyOutputSlots128`. They force conformance to
remain `partial`; they must not be reported as mandatory preflight successes.

Preflight is run:

1. for an idle set before committing effective state;
2. when promoting a pending request at the last-stop/next-start boundary; and
3. immediately before each Vintage stream snapshot.

If the third check fails, stream start rejects Vintage and atomically falls
back to `balanced`, reporting the complete reason set and an apply failure. It
must not start with a policy described as Vintage. The prior hardware profile
and Core Audio configuration remain untouched.

Production does not expose build-invariant or failure injection knobs.
Offline harnesses construct descriptors directly to test every pass/fail path.

## Conformance status

The state enum is stable and has exactly these meanings:

| Status | Meaning |
| --- | --- |
| `unverified` | Preflight has not completed for the requested/effective Vintage policy, or a mandatory invariant failed. The mode is not effective. |
| `partial` | Mandatory offline/runtime invariants pass and the safe Vintage policy is effective or ready, but one or more declared legacy capabilities/comparison rows are absent or failed. This is the maximum automatic MVP status. |
| `compatible` | Every required row in a named, versioned physical/application conformance scope passed, with provenance and artifacts. It is scoped to that matrix, not universal parity. |

`compatible` is reserved in the MVP and cannot be selected through the public
API, an environment variable, defaults, a raw file path or untrusted IPC.
Offline tests may construct a complete synthetic evidence matrix to prove the
pure status evaluator reaches it only when every required row passes. The
shipping HAL reports at most `partial` until a separately reviewed attestation
design can bind a locked campaign report to the exact binary and device.

The state reports all reasons as a stable ordered array derived from a bitmask.
At minimum:

```text
not_requested
preflight_not_run
audio_params_reset_disabled
capture_paced_output_disabled
output_start_byte_not_4
explicit_usb_scheduling_enabled
usb_hal_timestamp_enabled
timestamp_period_mismatch
buffer_not_512
rate_surface_mismatch
rate_192000_not_implemented
legacy_stream_partition_mismatch
legacy_client_format_mismatch
legacy_queue_geometry_mismatch
timecode_mode_conflict
apply_failed
physical_matrix_missing
```

Unknown future reason bits are a backend protocol error in a same-schema
payload; an older shorter payload yields `vintageCompatible: null`, not a
fabricated status.

## Transaction and rollback model

The existing driver-mode rules remain authoritative:

- initial state after process restart is
  `requested=balanced/effective=balanced/pending=false`;
- an idle Vintage request commits only after preflight succeeds;
- a streaming Vintage request is visible as pending and does not affect the
  active stream;
- promotion occurs at the existing safe stop/start boundary;
- any failure retains or restores the previous complete effective policy;
- leaving Vintage for balanced or performance removes fixed-512 normalization
  at the same boundary; and
- no state is stored in a plist, defaults database, launch agent, NVRAM,
  firmware or user configuration.

Rollback includes the policy descriptor and conformance snapshot. It must not
leave `effectiveMode=vintage-compatible` with balanced normalization, or
`effectiveMode=balanced` with fixed-512 normalization. Requested/effective,
pending, stream snapshot, capabilities and reasons are read under the same
driver-mode mutex. Hot paths read an immutable stream snapshot and do not take
that mutex.

## Composition with profiles and other modes

Hardware profiles remain independent. Selecting or leaving Vintage Compatible
does not read, write, infer, reset or restore input mode, ground lift, software
lock, source routing, transforms or input decode. Any canonical hardware
profile may coexist with Vintage, and profile changes cannot silently select a
driver mode.

Driver modes remain mutually exclusive:

- `performance -> vintage-compatible`: explicit set; applies/pends with full
  Vintage preflight. Leaving Vintage for performance restores performance
  values at the safe boundary.
- `vintage-compatible -> balanced`: explicit immediate/pending reversion using
  the normal transactional rules.
- request Vintage while Timecode Optimized is armed, qualified, pending or
  active: reject with `timecode_mode_conflict` and no mutation.
- arm Timecode Optimized while Vintage is requested, pending or effective:
  reject with the existing public conflict code and no mutation.

The conflict is symmetric because the timecode fallback contract currently
allows only balanced/performance. Neither mode silently disarms the other or
rewrites its fallback. The user explicitly returns to balanced/performance
before changing families. Rejections preserve all mode and arm state.

## Private IPC and public API

Private IPC remains version 1. Existing message IDs, request payloads and
payload prefixes are unchanged. Add dedicated message IDs after the current
timecode messages:

- `VintageCompatibleGet`;
- `VintageCompatibleState`.

`VintageCompatibleGet` has zero payload. `VintageCompatibleState` is a
fixed-width, packed, schema-v1 payload containing:

- conformance status;
- complete reason bitmask;
- complete capability bitmask and known capability mask;
- actual preflight values needed to diagnose each invariant;
- supported/effective sample-rate mask;
- effective buffer normalization enum/value;
- preflight generation and failure counter; and
- the complete current `OpenA8DJDriverModeStatePayload`.

Reserved bytes are zero. Length, schema, enum, boolean, bitmask and cross-field
contradictions are rejected. Get is read-only and does not wake/start I/O.
Stream stats append the same payload at the tail. Older stats prefixes and
existing timecode tails retain their offsets.

Public API v1 adds the catalog entry after `timecode-optimized`:

```json
{
  "id": "vintage-compatible",
  "name": "Vintage Compatible (Experimental — Unverified)",
  "default": false,
  "experimental": true,
  "conformanceVerified": false,
  "requiresIdleBoundary": true
}
```

Commands are additive:

```text
opena8dj-control api driver-modes
opena8dj-control api driver-mode
opena8dj-control api driver-mode set vintage-compatible
```

The existing `driver_mode.get` and `driver_mode.set` data continue to report
requested/effective/pending. Add:

```json
{
  "vintageCompatible": {
    "schemaVersion": 1,
    "status": "partial",
    "experimental": true,
    "claim": "unverified",
    "reasons": [
      "rate_192000_not_implemented",
      "legacy_stream_partition_mismatch",
      "legacy_client_format_mismatch",
      "legacy_queue_geometry_mismatch",
      "physical_matrix_missing"
    ],
    "capabilities": {
      "audioParamsReset": true,
      "capturePacedOutput": true,
      "outputStartByte4": true,
      "hostMonotonicTimestamp": true,
      "fixedBuffer512": true,
      "rate44100": true,
      "rate48000": true,
      "rate88200": true,
      "rate96000": true,
      "rate192000": false
    }
  }
}
```

The object is present when the new HAL payload is available, even if Vintage
is not selected; then status is `unverified` with `not_requested`. A legacy HAL
produces `vintageCompatible:null`. The client validates the HAL's state and
does not derive capabilities from its own build.

`api stats` adds the same object from the append-only stats tail. It reports
the active stream snapshot, not a newly pending policy. `api version` appends
`driver-mode.vintage-compatible.read` and
`driver-mode.vintage-compatible.write`.

Stable public failures include:

| Code | Meaning | Retryable | Exit |
| --- | --- | --- | ---: |
| `driver_mode_not_allowed` | Unknown canonical mode | false | 2 |
| `driver_mode_conflict` | Timecode/Vintage family conflict | false | 3 |
| `driver_mode_apply_failed` | Preflight/promotion failed; old policy retained | true | 5 |
| `backend_protocol_error` | HAL/CLI schema, enum, length, reason or read-back mismatch | true | 4 |

Set succeeds only after the existing set/get read-back agrees and the Vintage
payload agrees with the same driver-mode generation. A pending set reports
status/reasons for the requested preflight and the unchanged effective policy;
it never prints the request as effective.

## Core Audio property contract

Offline smoke/parity tests treat these as observable, independent of names:

| Property | Vintage MVP expectation |
| --- | --- |
| Device/channel surface | 8 input channels, 8 output channels |
| Stream partition | Current OpenA8DJ one input/four output streams; reported legacy gap |
| Client format | Current interleaved `Float32`; reported legacy gap |
| Available nominal rates | exactly 44100, 48000, 88200, 96000 |
| Current nominal rate | one member of the available set |
| Buffer frame size on entry/effective | 512 |
| Buffer frame request 1/512/513/1024/4096 | read-back 512 in Vintage; normal shipping table after leaving |
| Buffer bytes at 512 | 16384 for 8-channel interleaved `Float32` |
| Zero timestamp period | 16384 frames |
| Timestamp samples/host time | monotonic across repeated reads; seed changes on accepted configuration reset |
| Identity | remains OpenA8DJ bundle/device/manufacturer values |

Sample-rate or buffer property changes use
`RequestDeviceConfigurationChange`, `PerformDeviceConfigurationChange` and
`AbortDeviceConfigurationChange` correctly. Vintage rejects them while
streaming. No private IPC mutation directly calls the apply helpers. Current
no-op property notification stubs are not expanded as part of this MVP; any
future advertised-surface change must first repair and test notification
delivery.

## Conformance matrix

The versioned matrix begins with:

| ID | Observable expectation | Provenance | MVP role | Offline method | Physical/reference method |
| --- | --- | --- | --- | --- | --- |
| `identity-open` | OpenA8DJ name, UID and bundle remain unchanged | `repo-implementation`, `docs/LEGAL.md` | required safety | bundle/HAL smoke | verify loaded bundle/hash |
| `surface-8x8` | 8 input and 8 output channels | `repo-implementation` | required subset | parity smoke | Core Audio enumeration |
| `stream-partition` | documented old stereo-pair partition | `repo-observation`, old-driver plan | known gap | report mismatch | compare app-visible stream list |
| `format-int24` | documented old 24-bit client format | `repo-observation`, old-driver plan | known gap | report mismatch | compare formats |
| `rate-44100` | present and configurable | both repo classes | required subset | HAL property/config test | locked start/stop |
| `rate-48000` | present and configurable | both repo classes | required subset | HAL property/config test | locked start/stop |
| `rate-88200` | present and configurable | both repo classes | required subset | HAL property/config test | locked start/stop |
| `rate-96000` | present and configurable | both repo classes | required subset | HAL property/config test | locked start/stop |
| `rate-192000` | documented on old surface | `repo-observation`, old-driver plan | known gap | report unsupported | deferred transport campaign |
| `buffer-default` | 512 frames | both repo classes | required | smoke/read-back | application launch/read-back |
| `buffer-normalize` | Vintage requests normalize to 512 | MVP policy only | required policy | property harness | app requests/read-back |
| `timestamp` | monotonic host/sample projection, period 16384 | current safety evidence; old plan requires monotonic timestamps | required observable | repeated-read harness | long-run drift/jump trace |
| `audio-reset` | reset-style request precedes actual params | `repo-observation` and current source | required invariant | synthetic preflight/source-independent helper | USB trace from owned hardware |
| `capture-paced` | completed IN supplies OUT transaction lengths | `repo-observation` and current source | required invariant | synthetic preflight/helper | locked USB trace |
| `output-byte-4` | playback starts at byte 4 | `repo-observation` and current source | required invariant | packing test/preflight | locked output trace |
| `queue-64-128` | documented deep legacy geometry | `repo-observation` | known gap | report mismatch | deferred transport campaign |
| `app-start-stop` | declared app starts, streams, stops and reopens without hang | future owned-hardware observation | physical required | not claimable | per-app scripted/manual case |
| `audio-quality` | no accepted regression against declared fixture/tolerance | future owned-hardware observation | physical required | not claimable | physical capture plus listening notes |

A matrix row is `not_run`, `pass`, `fail` or `not_applicable`, and names its
tolerance. Required `not_run`/`fail` rows prevent `compatible`.
`not_applicable` requires an explanation and cannot waive a known full-parity
gap while still claiming full scope.

## Offline test contract

Add `make vintage-compatible-offline-test` and include it in the relevant
driver-mode aggregate. Tests use pure state/preflight helpers plus the mock IPC
and HAL bundle harnesses. They must cover:

1. catalog ordering, exact experimental/unverified display name and session
   default balanced;
2. every mandatory preflight invariant passing and failing individually;
3. idle apply, unchanged request, streaming pending, cancellation, safe-boundary
   promotion and leaving for balanced/performance;
4. failure rollback with requested/effective/policy/conformance snapshot never
   torn;
5. entry rejection when current buffer is not 512;
6. fixed-512 normalization for 1, 512, 513, 1024 and 4096, and restoration of
   normal 512/1024/2048/4096 normalization after leaving;
7. exact rate allowlist, successful 88.2 handling, and rejection of 192/unknown/
   non-finite rates;
8. sample-rate/buffer configuration requests at idle and rejection while
   streaming, including perform/abort bookkeeping;
9. timecode arm/requested/effective/pending conflict in both directions with no
   mutation, and independent hardware profile state;
10. `unverified`, `partial` and synthetic-complete `compatible` status evaluator
    paths, reason ordering and unknown-bit rejection;
11. HAL/CLI schema, enum, length, generation, requested/effective/pending,
    capability and reason mismatch failures;
12. restart/default behavior by constructing a fresh process state;
13. legacy shorter IPC/stats payload yielding null rather than fabricated
    Vintage state;
14. observable HAL property contract, buffer bytes and monotonic timestamp
    behavior;
15. mode-2 packing validator with output start 4;
16. shipping `smoke-hal` and `parity-smoke-hal` bundle tests; and
17. existing driver-mode, timecode, public API, USB-quality and profiler suites.

No offline test uses the production socket, changes defaults, starts Core
Audio, installs a bundle or touches USB hardware.

## Build, live and comparison gates

No-hardware evidence:

```text
make vintage-compatible-offline-test
make driver-mode-offline-test
make public-api-offline-test
make usb-quality-offline-test
make hardware-profiler-offline-test
make hal
make build/opena8dj-control
make smoke-hal
make parity-smoke-hal
```

Any live/Core Audio/USB/install/application/reference comparison must run under:

```text
./scripts/shared-hardware-lock-run \
  --gate vintage-compatible \
  --run-dir <unique-run-directory> \
  -- <command>
```

The wrapper's manifest, stdout, stderr and result remain with the report. Do not
use `sudo`, reset USB, replace the installed driver, delete/override locks, or
operate proprietary software automatically without separate authorization.
Human comparison with lawfully installed software is a later supervised
campaign, not an MVP prerequisite.

A future campaign first records the OpenA8DJ binary hash/signature, bundle and
device identity, hardware/firmware profiler output, macOS build, selected rate,
buffer and effective mode/conformance snapshot. It then runs a declared
application matrix with identical fixtures and captures jitter, ISO errors,
xruns, underruns, timestamps, stream start/stop/reopen, Core Audio responsiveness
and physical audio results. All fallbacks and skipped rows are failures or
explicit gaps, never passes.

## Acceptance criteria

- `vintage-compatible` is an explicit stable mode ID with the exact
  experimental/unverified user-facing label.
- It is session-only, transactional and not effective during an active-stream
  mutation.
- It applies fixed-512 normalization and a real immutable Vintage policy; it is
  not a cosmetic alias for balanced.
- Reset `AUDIO_PARAMS`, capture-paced playback and output byte 4 are preflighted
  as compiled invariants. Failure rejects/falls back and reports each reason.
- 88.2 kHz remains supported; 192 kHz remains an explicit gap.
- Hardware profiles are untouched. Timecode conflicts are symmetric,
  deterministic and reversible through an explicit balanced/performance step.
- IPC/stats/API extensions are append-only and report
  requested/effective/pending plus status, reasons and capabilities.
- A fresh process defaults to balanced with Vintage unverified/not requested.
- Offline mode, property, mismatch, rollback, restart, smoke and parity tests
  pass with warnings enabled.
- No identity, proprietary asset, firmware, binary, branding or incompatible
  code enters the repository.
- No result claims exactness, parity or application compatibility without a
  named physical/application matrix and retained evidence.
