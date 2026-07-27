# Performance driver mode design

Status: implementation contract for the MVP.

## Purpose and scope

The driver-mode layer is a session-scoped, transactional policy surface for
choosing conservative runtime behavior. The MVP exposes two canonical modes:

- `balanced` (default): the exact shipping runtime policy before this feature;
- `performance`: a conservative low-latency candidate which may spend more CPU.

The word "performance" is a policy name, not a measured result. The product
must not claim lower latency, acceptable CPU cost, or equivalent stability
until the benchmark and hardware gates in this document pass.

This layer deliberately does not expose compile-time USB experiments. It must
not change USB isochronous framing, transfer coalescing, queue depth, explicit
scheduling, packet layout, sample rate, channel routing, input decoding, or
hardware controls.

## Driver mode is not an input profile

`driverMode` controls runtime scheduling and buffering policy. It is independent
of the existing hardware input `profile`, whose canonical values select such
things as `timecode-vinyl`, `timecode-cd-line`, phono grounding, input mapping,
and software lock.

The two axes compose:

| Axis | Examples | Owns | Must not own |
| --- | --- | --- | --- |
| Driver mode | `balanced`, `performance`; future `timecode-optimized`, `vintage-compatible` | Safe runtime scheduling/buffering policy | Electrical input state, routing, sample rate |
| Hardware input profile | `playback`, `timecode-vinyl`, `phono-ab` | Device controls and input mapping | Runtime queue QoS or latency policy |

In particular, the existing profile ID `timecode-vinyl` is not the future
driver mode `timecode-optimized`. Neither selecting a timecode hardware profile
nor observing two phono inputs may silently select a driver mode in this MVP.

The mode catalog is an explicit allowlist. Future modes add a stable ID and an
immutable policy descriptor; they do not add free-form runtime knobs.

## Existing timing and lifecycle constraints

The macOS HAL currently:

- advertises and defaults to a 512-frame Core Audio buffer, normalizing valid
  requests to 512, 1024, 2048, or 4096 frames;
- starts USB on the first `StartIO` client and stops at the last `StopIO`
  client, serialized by `gIOMutex`;
- creates a serial USB dispatch queue when `OpenA8DJUSBEngine` is constructed;
- launches its USB worker when the engine starts;
- uses internal output start/target latency of 8192 frames and a restart
  latency of 4096 frames;
- keeps USB transfer geometry and queue depths as compile-time constants; and
- serves authenticated local IPC while the engine is open.

The 512-frame Core Audio minimum is the lowest validated public buffer in this
HAL and remains unchanged. The existing 4096-frame restart watermark is the
lowest already-shipping internal recovery watermark and is the floor for the
MVP performance policy. This is a conservative implementation boundary, not
evidence that 4096 is optimal.

## Runtime policy

Policy descriptors are internal, immutable, and selected only by canonical
mode ID:

| Policy field | `balanced` | `performance` |
| --- | ---: | ---: |
| Core Audio preferred/minimum buffer | 512 / 512 | unchanged |
| Output start latency frames | 8192 | 4096 |
| Output restart latency frames | 4096 | 4096 |
| Output target latency frames | 8192 | 4096 |
| USB worker block QoS | inherited/default | `QOS_CLASS_USER_INTERACTIVE` |
| USB framing/queue depth/coalescing | shipping compile-time values | unchanged |

The worker QoS override belongs on the dispatched worker block for a newly
started stream, not in a mutation of the already-created serial queue and not
on IPC, MIDI, or diagnostic workers. The output policy is snapshotted once at
the same start boundary. Hot paths read that per-stream snapshot; they do not
take the driver-mode mutex.

`performance` never changes sample rate, Core Audio buffer size, stream count,
routing, electrical/input profile, control payload, or unsafe compile-time
flags. A future hardware campaign may reject or revise the candidate values.

## State model and transitions

The process-wide state survives an engine close/re-open but is not persisted
across the Core Audio host process:

- `requested`: last valid requested canonical mode;
- `effective`: policy used by the current stream or selected for the next one;
- `pending`: `requested` differs from `effective` because a stream is active;
- `lastResult`: `unchanged`, `applied`, `pending`, `cancelled`,
  `invalid`, or `apply_failed`;
- monotonically increasing session counters and a generation.

Initial state is `requested=balanced`, `effective=balanced`, `pending=false`.

| Current condition | Request | Result |
| --- | --- | --- |
| Idle | current effective mode | `unchanged`; no generation change |
| Idle | different valid mode; policy preflight succeeds | Commit requested and effective together; `applied` |
| Idle | different valid mode; policy preflight fails | Keep prior effective policy; expose `apply_failed`; no partial update |
| Streaming | effective mode while another request is pending | Cancel pending request; `requested=effective`; `cancelled` |
| Streaming | different valid mode | Change only requested; `pending=true`; effective stream is untouched |
| Any | unknown/reserved ID or incompatible schema | Reject without changing requested/effective |
| Stop/next safe start | pending mode; preflight succeeds | Atomically promote requested to effective before the next worker launches |
| Stop/next safe start | pending mode; preflight fails | Keep prior effective; keep request visible as pending with `apply_failed` |

An implementation may promote pending state when the last USB stream stops or
immediately before the next stream starts. It must use the same mode-state
mutex in either case and snapshot the resulting immutable policy before
launching the worker. It must never report the requested mode as effective
until that commit occurs.

The transition helper takes a test-only/future preflight or apply callback.
Failure leaves the previous effective descriptor intact. No production
environment variable or public argument may activate failure injection.

Multiple Core Audio clients do not create separate modes. The policy is per
driver process. A mutation during any active USB stream is pending until the
safe boundary.

## Persistence and reversion

The MVP is session-only by design:

- no plist, defaults database, launch agent, or root-owned configuration file;
- no mode is inferred from a hardware input profile;
- `balanced` is restored when the hosting Core Audio process is restarted;
- explicit `driver-mode set balanced` is the immediate idle reversion path and
  the pending/cancel path while streaming.

Persistence is deferred until hardware evidence shows that carrying a
performance selection across OS/driver upgrades is safe and a migration and
recovery UX exists. Session-only state also prevents a stale experimental
choice from making later troubleshooting ambiguous.

## Private HAL IPC

Private IPC remains version 1 and preserves all existing numeric message IDs.
Append three new message types:

- `DriverModeGet`;
- `DriverModeSet`;
- `DriverModeState`.

The request and state payloads each begin with `schemaVersion=1` and use fixed
width integer fields. Reserved bytes must be zero on send and ignored on read.
The set request contains only a canonical numeric mode ID. The HAL validates
the schema and allowlist even though the public CLI validates them first.

The state response includes at least:

- payload schema version;
- requested and effective canonical IDs;
- `pending`, `streaming`, `lastResult`, and rejection reason;
- generation;
- accepted requests, rejected requests, applied transitions, apply failures,
  and pending transitions;
- effective output start/restart/target frame values and effective worker QoS.

Malformed or unsupported requests receive a `DriverModeState` rejection
response and increment the HAL rejection counter; they must not be silently
treated as a set. A getter is read-only and does not wake or start Core Audio.

The HAL state mutex serializes all raw private-IPC mutations. The public client
also reuses the existing per-user advisory mutation lock so separate CLI
processes cannot interleave set/read-back transactions. Existing peer
credential checks, fixed production socket path, socket type/owner/mode checks,
bounded I/O, and test-only compile-time socket overrides remain mandatory.

## Public JSON API and CLI

This is an additive extension to public API v1. Existing operations, JSON
members, exit statuses, and hardware `profile` behavior remain compatible.
Every driver-mode response has `data.schemaVersion: 1`.

Commands:

```text
opena8dj-control api driver-modes
opena8dj-control api driver-mode
opena8dj-control api driver-mode set balanced
opena8dj-control api driver-mode set performance
```

Operations:

- `driver_modes.list`;
- `driver_mode.get`;
- `driver_mode.set`.

`driver_modes.list` is offline and returns catalog entries in stable order.
Each entry has a canonical `id`, display `name`, `default`, and
`requiresIdleBoundary`. The allowlist contains exactly `balanced` and
`performance` in the MVP.

Get/set data uses canonical strings and includes:

```json
{
  "schemaVersion": 1,
  "requestedMode": "performance",
  "effectiveMode": "balanced",
  "pending": true,
  "streaming": true,
  "lastResult": "pending",
  "generation": 0,
  "counters": {
    "acceptedRequests": 1,
    "rejectedRequests": 0,
    "appliedTransitions": 0,
    "applyFailures": 0,
    "pendingTransitions": 1
  },
  "effectivePolicy": {
    "outputStartLatencyFrames": 8192,
    "outputRestartLatencyFrames": 4096,
    "outputTargetLatencyFrames": 8192,
    "workerQoS": "default"
  }
}
```

A successful set must parse the HAL's set response and perform a get/read-back
on the same authenticated connection. It succeeds only if read-back reports
the requested canonical mode and either:

- it is effective and not pending; or
- it is explicitly pending with the previous effective mode.

It must never rewrite `effectiveMode` client-side. Unsupported payload version,
unknown enum, truncation, disagreement between set response and read-back, or
an impossible state is `backend_protocol_error`. A HAL `apply_failed` is
`driver_mode_apply_failed`.

Add stable public errors:

| Code | Condition | Retryable | Exit |
| --- | --- | --- | ---: |
| `driver_mode_not_allowed` | ID is not in the public allowlist | false | 2 |
| `driver_mode_busy` | Reserved for a caller that explicitly disallows pending; not used by normal MVP set | true | 3 |
| `driver_mode_apply_failed` | HAL kept/rolled back to prior effective policy | true | 5 |

Backend unavailable/authentication/protocol errors reuse the existing API
codes and exit statuses. Add `driver-mode.read` and `driver-mode.write` to the
version capability list without removing or renaming existing capabilities.

The human CLI help must call these driver modes, never profiles, and explain
that a set during streaming may be accepted as pending.

## Observability

Driver mode state is available from `api driver-mode` even when not streaming,
provided the HAL backend is open. Extend `api stats` additively with a
`driverMode` object containing the same requested/effective/pending/result,
generation, counters, and effective policy. Older shorter private stats
payloads remain valid and produce an explicit unavailable/null driver-mode
view rather than fabricated balanced state.

The HAL stream-stats payload remains append-only. New fields go at its tail so
older tools retain their offsets. A stream stats snapshot reports the
per-stream effective policy used by that stream, not a newly pending request.
Outside streaming it reports the selected next-stream effective policy.

Required metrics:

- accepted and rejected mutation requests;
- applied and pending transitions;
- apply/preflight failures;
- requested and effective mode;
- last result/rejection reason;
- effective output policy and worker QoS.

Existing USB jitter, ISO error, xrun, output underrun/overrun/late-write, frame,
and quality instrumentation is reused for comparison. Do not reset cumulative
quality counters just because a mode changes; per-stream counters retain their
existing lifecycle.

## Compatibility and safety

- `balanced` must be bit-for-bit policy-equivalent to the pre-feature shipping
  defaults at first startup.
- Existing input profiles and control payload layout stay unchanged.
- Existing private IPC IDs and payload prefixes stay unchanged; append only.
- Existing API operations and non-API commands retain behavior.
- Unknown future mode IDs fail closed in older clients and HALs.
- Neither list nor get wakes the device or starts a stream.
- Public writes remain allowlisted, authenticated, bounded, and locked.
- No API accepts numeric QoS, buffer, latency, USB, routing, sample-rate,
  firmware, file-path, or raw-IPC parameters.
- Performance cannot lower Core Audio below 512 or internal output policy below
  4096 in this MVP.

## Offline implementation and test contract

Keep the state machine and policy lookup small and independently testable.
Prefer a shared pure-C header/module used by the Objective-C HAL and an offline
C harness; do not make correctness depend only on source-text assertions.
Production synchronization is a mutex, while tests may exercise the transition
helper directly and through mock IPC.

Add `make driver-mode-offline-test` and include:

1. Default state is balanced/balanced, not pending, with shipping policy.
2. Idle set to performance applies and read-back matches exact policy.
3. Unknown string, unknown numeric ID, bad schema, and non-zero/invalid
   reserved input are rejected without state mutation.
4. Streaming set is truthfully pending; cancelling back to effective works;
   safe-boundary promotion changes effective only once.
5. Injected preflight/apply failure leaves the old effective descriptor intact,
   records failure, and returns the stable API error.
6. HAL/CLI schema or enum mismatch, malformed/truncated state, contradictory
   pending/effective values, and set/get disagreement fail as protocol errors.
7. Two concurrent public writers are serialized by the existing mutation lock;
   final state is one complete valid transaction, never a torn mixture.
8. API stats and mode get expose requested/effective/rejections/counters and
   effective policy; a legacy short stats payload remains readable.
9. Existing profile set/read-back tests still pass and cannot affect mode.
10. Shipping tool and HAL build with warnings enabled; CLI version/list work
    without a backend.

Offline tests use temporary private socket and lock paths through existing
test-only compile-time overrides. They must not use the production socket,
install a driver, access Core Audio, or touch hardware.

## Build, smoke, and hardware evidence

Required no-hardware evidence:

```text
make driver-mode-offline-test
make public-api-offline-test
make usb-quality-offline-test
make hardware-profiler-offline-test
make hal
make build/opena8dj-control
```

Any installed-driver, Core Audio, live USB, smoke, or benchmark command must be
serialized:

```text
./scripts/shared-hardware-lock-run \
  --gate performance-mode \
  --run-dir <unique-run-directory> \
  -- <command>
```

Do not use `sudo`, reset USB, replace the installed driver, or delete/override
coordination locks without separate authorization.

A future comparable balanced/performance benchmark uses the same machine,
interface, port, sample rate, 512-frame Core Audio buffer, channels, fixture,
duration, warm-up, background-load condition, driver build, and instrumentation
setting. Randomize or alternate order and run enough repetitions to report a
distribution, not one sample. Record at least:

- end-to-end or stimulus-to-output latency distribution (median, p95, p99);
- `coreaudiod`/driver CPU distribution and system load;
- xruns, active underruns, ring overruns, late writes;
- capture/playback completion jitter histograms and ISO errors;
- requested/effective mode and exact effective policy for every run.

Promotion requires no regression in routing/profile state, no ISO errors or
hard xruns under the agreed workload, a statistically credible latency
improvement, and an explicitly accepted CPU delta. Until then UI/docs label
performance as an experimental session policy and make no marketing claim.

## Acceptance criteria

- Balanced is the session default and preserves prior behavior.
- Performance is an allowlisted, real runtime policy, not a renamed no-op.
- Mode and hardware input profile are independently readable and writable.
- Streaming mutation is pending and never falsely reported as effective.
- Policy commit is atomic and failure keeps the old effective policy.
- API set performs authenticated locked read-back verification.
- Requested/effective/rejection/failure metrics are visible in mode state and
  additively in stats.
- No mode changes USB framing, compile-time experiments, Core Audio minimum,
  sample rate, routing, or electrical controls.
- Offline transition, IPC/API, concurrency, compatibility, build, and smoke
  evidence passes; any live evidence is lock-serialized and honestly reported.
