# Timecode Optimized driver mode design

Status: implementation contract for an experimental, opt-in MVP.

## Purpose and claim boundary

`timecode-optimized` is a session-scoped driver mode for a two-deck DVS
workflow on the two phono-capable stereo input pairs of Audio 8 DJ. It may use
a more aggressive buffering policy only after the operator explicitly arms it
and the driver observes sustained evidence that the requested electrical and
signal conditions hold.

The name is a policy name, not a measured result. Until a locked physical
campaign establishes the result, the product must not claim:

- lower end-to-end, input, output, or DVS control latency;
- improved timecode scope, tracking quality, stability, or CPU use; or
- compatibility with a particular Traktor release or control medium.

OpenA8DJ remains an independent compatibility project. This feature does not
change the project's legal status or imply endorsement by Native Instruments.

## Findings in the current implementation

The implementation must be based on these observed constraints:

- The HAL preserves one 8-channel Core Audio input stream containing the named
  stereo pairs A, B, C, and D, and four stereo output streams.
- `OPENA8DJ_INPUT_STREAM_COUNT` defaults to `1`. Therefore input stream usage
  has no per-pair granularity even if the optional stream-usage property is
  enabled.
- `OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY` defaults to `0`. There is no reliable
  production observation of client intent for A/B/C/D.
- USB mode-2 decode produces all four physical stereo pairs before routing and
  already accumulates frames, RMS energy, peak, and correlation per pair.
- The public `input-stats` read is destructive. The classifier must not consume
  or reset that operator-facing accumulator.
- The input `FloatRing` and output timeline each allocate 32,768 frames for all
  eight channels. Allocation and Core Audio topology are fixed for a stream.
- `balanced` uses output start/restart/target `8192/4096/8192` frames with
  default worker QoS. `performance` uses `4096/4096/4096` with
  user-interactive worker QoS.
- Driver modes are process-session state with requested/effective/pending
  transitions and an immutable policy snapshot at a safe stream boundary.
- Hardware profiles independently control input mode, the applicable
  ground-lift flag, software lock, transforms/sources, and input decode.

Consequently, this feature can detect signal activity, not application intent.
Any API, log, UI, or documentation that says it “detected that Traktor uses
only A/B” is incorrect. The truthful statement is: the operator explicitly
allowed A/B and the driver observed sustained A/B activity with no observed
C/D activity.

## Exact meaning of “two phono inputs”

Audio 8 DJ has two phono-capable stereo deck inputs:

| Deck | Pair | Channels | MVP allowlist bit |
| --- | --- | --- | ---: |
| Deck A | Input A | 1/2, left/right | `0x1` |
| Deck B | Input B | 3/4, left/right | `0x2` |

“Two inputs” means exactly the two stereo pairs A and B, not two mono channels.
Both channels of each pair must satisfy the allowed-pair activity rule. Input C
(5/6, or the front MIC source selected by the physical switch) and Input D
(7/8) are outside the allowlist.

The MVP arm request must contain the explicit pair mask `A,B` (`0x3`). The HAL
rejects zero, one pair, C/D, unknown bits, duplicates, and masks with more or
fewer than A and B. Keeping the mask explicit is intentional: it acknowledges
the caller's routing intent without pretending the current HAL can infer it.
The fixed MVP mask can be generalized only after a future topology provides
equally verifiable electrical capability and client intent.

## Independent electrical-profile gate

Driver mode and hardware profile remain separate axes. Arming or applying
`timecode-optimized` must never write a profile, input mode, ground lift,
software lock, input transform/source, sample rate, or input-decode setting.

Eligibility requires a fresh, internally read hardware-control snapshot that
exactly matches one canonical A/B profile:

| Canonical profile | Required state |
| --- | --- |
| `traktor-dvs-vinyl` | input mode `0`, vinyl ground lift on, the other two ground lifts off, software lock on, input decode on, identity A/B/C/D sources, no transforms |
| `traktor-dvs-cd-line` | input mode `1`, CD/line ground lift on, the other two ground lifts off, software lock on, input decode on, identity sources, no transforms |
| `vinyl-recording` | input mode `2`, phono ground lift on, the other two ground lifts off, software lock on, input decode on, identity sources, no transforms |

These states are the verifiable electrical DVS/phono profiles already modeled
by the control tool. A partial match, `custom`, a failed/stale control read, a
physical/front-panel change, disabled decode, non-identity source routing, or
any transform is `wrong_profile`.

The mode does not infer or select one of these profiles. `api profile set` and
`api driver-mode arm` remain separate authenticated, serialized transactions.
A profile change while armed resets qualification. A profile change while
optimized triggers fail-open deoptimization and disarms the mode.

## Explicit arm and session lifecycle

The mode is never enabled by silence, a profile selection alone, or a previous
run. It requires:

1. an authenticated, allowlisted `arm` request;
2. explicit `A,B` pair acknowledgement;
3. a matching fresh electrical-profile snapshot;
4. a live decoded-input statistics source;
5. sustained qualifying activity; and
6. a safe stream boundary before the optimized policy becomes effective.

Arm state is process-session only. A Core Audio host-process restart starts in
`balanced`, disarmed, with no remembered allowlist or evidence. It is not
written to defaults, plist, disk, NVRAM, firmware, or a launch agent.

Arming stores the current non-timecode mode as `fallbackMode` (`balanced` or
`performance`). A later disarm or fail-open transition returns to that exact
fallback. A normal request to set `balanced` or `performance` disarms and
clears all timecode evidence. Re-arming replaces no active arm silently: an
identical request is `unchanged`; a different or malformed request is rejected.

Arm states are:

- `disarmed`;
- `waiting_profile`;
- `qualifying`;
- `qualified_pending_boundary`;
- `active`;
- `deopt_pending_boundary`; and
- `faulted` (disarmed, with the latched cause visible).

`armed=true` is not synonymous with `effectiveMode=timecode-optimized`.
`qualified=true` is not synonymous with application intent or DVS quality.

## Non-destructive signal evidence

Add a private classifier accumulator beside, but independent from,
`_inputStats`. The decoder feeds it from the four physical pairs after sample
decode and before configurable input-source routing. Public `input-stats`
continues to have its existing destructive-read behavior and must not affect
classification.

The classifier uses fixed 250 ms windows derived from the active sample rate.
It records per channel:

- decoded frame count;
- sum of squares;
- absolute peak; and
- finite-value/decoder validity.

The initial internal thresholds are conservative candidates, immutable through
the public API:

- allowed-channel entry: RMS at least `0.002` (about -54 dBFS) and peak at
  least `0.007943` (about -42 dBFS);
- allowed-channel hold: RMS at least `0.001` (about -60 dBFS) or peak at least
  `0.003981` (about -48 dBFS);
- forbidden-channel trip: RMS at least `0.001` or peak at least `0.003981`.

A pair is entry-active only when both of its channels meet both entry
conditions in the same complete window. A and B must both be entry-active.
Any channel of C or D meeting the forbidden trip is outside-allowlist activity.
NaN, infinity, an incomplete window, no fresh frame, or a frame-count mismatch
is missing/invalid evidence, never silence.

These thresholds are engineering starting points, not a claim that every
timecode medium has this level. They may be revised only with recorded physical
noise-floor and timecode-level evidence; they are not caller-provided knobs.

### Hysteresis

- Activation needs eight consecutive eligible windows: 2.0 seconds of
  sustained A+B evidence with C+D below the trip threshold.
- A single forbidden C/D window resets qualification immediately.
- While active, one forbidden C/D window requests fail-open deoptimization
  immediately.
- While active, four consecutive windows in which A or B fails the hold rule
  (1.0 second) dequalify. This tolerates a brief needle lift without allowing
  silence to qualify or remain indefinitely.
- More than 500 ms without a complete fresh classifier window is
  `stats_missing` and requests immediate fail-open deoptimization.
- Profile, configuration, transport, and error faults bypass hysteresis.

The state exposes consecutive eligible and dropout counts so noisy-threshold
behavior can be tested without wall-clock sleeps.

## Safe-boundary state machine

The existing requested/effective mode machine remains the owner of policy
commit. Timecode qualification is a guard feeding that machine, not a second
writer of hot-path policy.

Activation while idle may commit immediately after preflight. Qualification
that completes during streaming sets:

```text
requestedMode=timecode-optimized
effectiveMode=<fallbackMode>
pending=true
armState=qualified_pending_boundary
```

It promotes only after the last active stream stops or immediately before a
new stream starts, using the existing driver-mode mutex and production policy
preflight. It must never report timecode policy as effective before that
commit. A warm-up stream followed by a real stop/start is therefore expected
in the MVP.

Fail-open conditions are detected immediately. At detection time, the state
must stop claiming `active`, latch the exact cause, increment a counter, clear
qualification, and request `fallbackMode`. The actual policy swap follows one
of these truthful paths:

- if idle, commit fallback immediately;
- if the implementation proves a monotonic expansion-only policy swap at a
  serialized Core Audio cycle/USB-worker boundary, commit there atomically; or
- otherwise set `deopt_pending_boundary`, keep the old effective policy visible
  as the actual snapshot, and promote fallback at the next stop/start boundary.

The MVP should use the existing stop/start boundary unless the in-stream
boundary is implemented and tested as a real atomic boundary. Merely changing
the JSON state while the worker uses the old policy is forbidden. During
`deopt_pending_boundary`, `optimizedActive=false`, `pending=true`, the actual
`effectiveMode` and policy remain truthful, and the fail-open cause is visible.

## Fail-open and rollback conditions

Each condition below clears qualification. Conditions marked “disarm” also
require a new explicit arm before any future optimization:

| Condition | Timing | Result |
| --- | --- | --- |
| C or D crosses trip threshold | one window | deopt, disarm |
| Three or four active pairs | one window | deopt, disarm |
| A or B absent below hold | four windows | deopt, remain armed and requalify only after fallback boundary |
| classifier stats missing/invalid | immediate at 500 ms | deopt, disarm |
| electrical profile/control change or mismatch | immediate | deopt, disarm |
| sample-rate request/change | before configuration commit | deopt, disarm |
| Core Audio buffer request/change | before configuration commit | deopt, disarm |
| input decode disabled or routing/transform changed | immediate | deopt, disarm |
| capture/playback queue, completion, transaction, zero-length, or short-transfer error | first new error | deopt, disarm |
| input check error or output panic flag | first new error | deopt, disarm |
| input ring overrun/trim/short read, output active underrun/overrun/late write/timeline reset | first new event | deopt, disarm |
| policy preflight/apply failure | at boundary | rollback to fallback, disarm, `apply_failed` |
| explicit disarm or balanced/performance set | transaction | fallback at safe boundary |

Error tests use deltas from the snapshot taken at arm/activation, not historical
cumulative nonzero counters. No counter is reset merely to arm the mode.

If fallback promotion itself fails, keep the actual effective descriptor,
surface `apply_failed`, and retry fallback at each safe boundary. Never replace
the descriptor with an unvalidated or partially applied policy.

## Concrete buffering policy

The fixed ring allocations remain 32,768 frames and eight channels. The mode
does not realloc, resize, change channel count, or introduce a two/four-channel
USB path.

Extend the immutable driver policy with:

- `inputLeadCeilingFrames`;
- `inputLeadGuardEnabled`; and
- `timecodeEvidenceRequired`.

Initial policies are:

| Field | `balanced` | `performance` | `timecode-optimized` |
| --- | ---: | ---: | ---: |
| output start frames | 8192 | 4096 | 4096 |
| output restart frames | 4096 | 4096 | 4096 |
| output target frames | 8192 | 4096 | 4096 |
| worker QoS | default | user-interactive | user-interactive |
| input lead guard | off | off | on |
| input lead ceiling | ring capacity | ring capacity | 2048 |
| evidence required | no | no | yes |

The 4096-frame output values reuse the lowest already-shipping, preflighted
performance descriptor; this mode does not invent a lower unvalidated output
watermark. The non-cosmetic difference is the mandatory A/B evidence state
machine and the input lead guard.

The 2048-frame input ceiling is four times the minimum 512-frame Core Audio
buffer and remains below the 4096-frame public maximum while far inside the
32,768-frame allocation. At each input read boundary, read ring occupancy
without allocating. If occupancy would exceed 2048, request fail-open fallback
before dropping or trimming any input. Balanced semantics then consume the
oldest frames. The MVP must not discard DVS samples merely to satisfy a latency
target. `inputLeadCeilingFrames` is thus a guard for retaining optimized status,
not permission to conceal an xrun.

Because the existing input reader already drains immediately and has no
startup watermark, the design does not fabricate a lower input watermark. Any
future trim-to-latest or smaller ceiling requires a physical campaign proving
the tradeoff and a separately reviewed discontinuity policy.

Policy preflight verifies:

- all frame fields are nonzero where applicable and below ring capacity;
- output ordering invariants from performance mode;
- timecode output values are no lower than 4096;
- input ceiling is at least four current Core Audio buffers and below capacity;
- the current buffer/sample rate match the qualification snapshot;
- arm, profile, classifier freshness, and error deltas remain valid; and
- all allocations and stream topology are unchanged.

## Unchanged transport and public audio surface

`timecode-optimized` must not change:

- the 8-input/8-output Core Audio channel surface or names;
- stream count, channel order, routing, sample format, or source transforms;
- sample rate, public Core Audio buffer size, safety offset, or framing;
- USB isochronous frame size, transfers per request, queue depths, coalescing,
  capture-paced ordering, check bytes, packet layout, or scheduling mode;
- hardware controls, firmware, MIDI, or power state; or
- fixed input/output ring allocation.

Optimizing by disabling C/D decode is forbidden: C/D must remain decoded so
outside-allowlist activity can trigger fail-open behavior, and all eight Core
Audio inputs must remain available.

## Private IPC and append-only compatibility

Preserve private IPC version 1 and every existing message ID and payload
prefix. Append dedicated fixed-width messages after the current driver-mode
messages:

- `TimecodeOptimizedGet`;
- `TimecodeOptimizedArm`;
- `TimecodeOptimizedDisarm`; and
- `TimecodeOptimizedState`.

Do not reinterpret the existing `DriverModeSet` reserved bytes. Arm schema v1
contains only:

- `schemaVersion=1`;
- canonical mode ID `timecode-optimized`;
- `allowedInputPairMask=0x3`; and
- zero reserved bytes.

The state contains arm/evidence/fallback fields and the complete current
driver-mode state. Unknown schema, length, enum, mask, boolean, or nonzero
reserved input is rejected without mutation. Getter is read-only. Arm/disarm
use the existing authenticated peer policy, bounded I/O, driver-mode mutex, and
per-user public mutation lock.

Extend stream stats only at the tail. Older shorter payloads yield
`timecodeOptimized: null`; they do not fabricate a disarmed state. New clients
must continue accepting the former exact driver-mode tail.

## Public API and CLI

Extend public API v1 additively:

```text
opena8dj-control api driver-modes
opena8dj-control api driver-mode
opena8dj-control api driver-mode arm timecode-optimized --input-pairs A,B
opena8dj-control api driver-mode disarm timecode-optimized
```

The catalog adds `timecode-optimized` after `performance` with
`default=false`, `requiresArm=true`, and `requiresIdleBoundary=true`. Plain
`driver-mode set timecode-optimized` is rejected with
`driver_mode_arm_required`; numeric masks and free-form thresholds are not
public API.

Add capabilities `timecode-optimized.read` and
`timecode-optimized.arm`. Do not remove or rename existing capabilities.
Profile members and commands remain unchanged.

`driver_mode.get`, successful arm/disarm, and `stats` add a
`timecodeOptimized` object:

```json
{
  "armed": true,
  "armState": "qualifying",
  "allowedInputPairs": ["A", "B"],
  "evidenceKind": "observed_activity",
  "intentObserved": false,
  "electricalProfile": "traktor-dvs-vinyl",
  "profileVerified": true,
  "qualified": false,
  "optimizedActive": false,
  "fallbackMode": "balanced",
  "eligibleWindows": 5,
  "requiredEligibleWindows": 8,
  "dropoutWindows": 0,
  "windowFrames": 12000,
  "lastFailOpenReason": "none",
  "inputLeadCeilingFrames": 2048,
  "counters": {
    "arms": 1,
    "disarms": 0,
    "qualifications": 0,
    "activations": 0,
    "deoptimizations": 0,
    "outsideAllowlistTrips": 0,
    "missingEvidenceTrips": 0,
    "profileTrips": 0,
    "configurationTrips": 0,
    "xrunErrorTrips": 0
  }
}
```

Expose per-pair window state (`active`, RMS and peak per channel) in stats, not
as proof of intent. Values are null when no fresh complete window exists.
Every arm response performs set/read-back verification on one authenticated
connection. A mismatch is `backend_protocol_error`.

Stable new errors:

| Code | Condition | Retryable | Exit |
| --- | --- | --- | ---: |
| `driver_mode_arm_required` | plain set attempted | false | 2 |
| `timecode_pair_allowlist_invalid` | mask is not exactly A,B | false | 2 |
| `timecode_profile_not_eligible` | no fresh exact electrical profile | true | 5 |
| `timecode_arm_apply_failed` | transactional arm/preflight failed | true | 5 |

Wrong profile may be represented truthfully as an armed waiting state only if
the caller explicitly allows waiting; the canonical CLI arm defaults to
rejecting before mutation when the current profile is already known wrong.

## Observability requirements

The mode state and stream-stats tail expose:

- requested/effective/fallback mode and actual effective policy;
- arm state, allowed mask, profile snapshot/generation, sample rate, buffer;
- evidence kind `observed_activity` and `intentObserved=false`;
- window length, thresholds, fresh/complete status, pair classifications, and
  hysteresis counters;
- input ring occupancy/ceiling and guard trips;
- activation, deoptimization, rejection, profile/configuration, missing-stats,
  outside-pair, and xrun/error counters;
- the exact fail-open cause and the generation at which it occurred; and
- whether policy rollback is applied or pending a boundary.

Logs are rate-limited and contain transitions, not per-frame samples. No log or
JSON field may call activity “usage” or “intent”.

## Deterministic offline test contract

Extend the pure-C mode harness or add one small pure-C timecode classifier/state
module used by both HAL and tests. Tests feed synthetic window summaries and
counter deltas; they do not sleep or access Core Audio/USB.

Required cases:

1. Restart initializes balanced/disarmed with empty evidence.
2. Silence for any duration never arms, qualifies, requests, or activates.
3. A correct profile without an explicit arm never activates.
4. Arm with wrong/custom/stale profile rejects or waits truthfully; no mode or
   profile state changes.
5. Invalid schema, reserved data, mode ID, empty/one/C/D/three/four-pair mask,
   and API/HAL enum mismatch are rejected without mutation.
6. Exactly A+B active for seven windows does not qualify; the eighth qualifies.
7. Only A, only B, or one channel of a stereo pair never qualifies.
8. A+B active qualifies; A+B+C or all four trips immediately.
9. Values immediately below/at/above RMS and peak thresholds exercise both
   conditions and hysteresis deterministically.
10. A brief allowed-pair dropout does not deopt; four windows do.
11. One forbidden-pair window, missing/invalid stats, wrong profile, profile
    change, rate change, buffer change, input routing/transform/decode change,
    xrun, USB/ISO error, panic/check error, or input-lead violation requests
    deoptimization immediately and records the exact cause.
12. Qualification while streaming is pending and cannot alter the policy
    snapshot; the next stop/start boundary promotes it once.
13. Deoptimization reports the old actual policy while rollback is pending,
    then commits the exact fallback at the safe boundary.
14. Injected activation or rollback preflight failure preserves the previous
    effective descriptor and is observable as `apply_failed`.
15. Balanced/performance remain behaviorally unchanged and timecode policy
    satisfies the fixed 32,768-frame ring invariants.
16. Profile and driver-mode transactions remain independent and serialized.
17. Public API accepts legacy stats/mode payloads with
    `timecodeOptimized:null`, rejects malformed tails, and detects arm/read-back
    disagreement.
18. Public `input-stats` reads cannot reset or perturb classifier evidence.
19. Concurrent arm/disarm/base-mode writers produce one complete transaction.
20. The timecode smoke gate restores the exact pre-gate control configuration
    on PASS, failure, signal, and early exit; it must not always force playback.

Required no-hardware evidence:

```text
make driver-mode-offline-test
make public-api-offline-test
make usb-quality-offline-test
make hardware-profiler-offline-test
make hal
make build/opena8dj-control
```

## Live gate and physical campaign

Any live Core Audio, USB, installed-driver, Traktor, smoke, or benchmark command
must be serialized:

```text
./scripts/shared-hardware-lock-run \
  --gate timecode-optimized \
  --run-dir <unique-run-directory> \
  -- <command>
```

The timecode gate must first export the exact current control configuration and
install an EXIT/signal trap that imports and verifies it. It may temporarily
apply a DVS profile only inside the shared lock. Failure to capture a restorable
baseline aborts before mutation. It must not use `sudo`, reset USB, replace the
installed driver, restart audio services, delete locks, or change branches.

After offline acceptance, a separately authorized physical campaign compares
fallback and optimized modes on the same build, port, media, cartridges/CD
players, mixer, Traktor version, sample rate, 512-frame public buffer, duration,
warm-up, load, and instrumentation. Alternate order and record distributions:

- stimulus/control-signal to DVS response latency, median/p95/p99;
- timecode scope/tracking results captured by an operator-approved method;
- input ring occupancy, short reads, guard trips, and callback timing;
- output underruns/overruns/late writes/timeline resets;
- capture/playback completion jitter and all ISO/queue/transaction errors;
- CPU/system load; and
- requested/effective/arm/profile/evidence state for each run.

Any fail-open event invalidates that optimized sample. Promotion requires no
routing/profile regression, no additional hard xrun or USB error, a credible
latency improvement, acceptable DVS behavior, and an explicitly accepted CPU
delta. Until then the UI and docs label the mode experimental.

## Acceptance criteria

- Default/restart is balanced and disarmed; silence cannot activate it.
- “Two inputs” means the two stereo phono-capable deck pairs A+B.
- Intent is explicit through the A+B arm allowlist; automatic evidence is
  labeled decoded signal activity only.
- A fresh exact DVS/phono electrical profile and sustained A+B-only activity
  are both required.
- Profile, routing, sample rate, buffer, errors, missing evidence, or C/D
  activity fail open with an observable cause.
- Activation and rollback use a real safe boundary and never misreport the
  actual effective policy.
- Policy reuses the existing mode state and validated 4096-frame output floor,
  adds an input lead guard, and stays within fixed ring allocations.
- All eight Core Audio inputs and outputs, USB framing, sample format, routing,
  and electrical controls remain unchanged.
- API/IPC evolution is authenticated, allowlisted, fixed-width, append-only,
  locked, read-back verified, and legacy-readable.
- Offline tests cover false activation, profiles, one/two/three-plus pairs,
  threshold noise, hysteresis, immediate deopt decision, safe-boundary pending,
  mismatches, rollback/errors, restart, and exact gate restoration.
- No latency or DVS-quality claim is made without locked physical evidence.
