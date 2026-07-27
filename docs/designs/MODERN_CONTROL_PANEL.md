# Modern macOS Control Panel

Status: implementation contract for the API 1.1 dashboard
Scope: `macos/OpenA8DJControlCenter`, its tests, build, package, uninstall, and
user documentation

## Goal and non-goals

Replace the synchronous preset-only panel with a native macOS dashboard that
truthfully presents the state already exposed by the integrated OpenA8DJ
features:

- exact device, firmware, and USB power known-state;
- USB stability, bounded jitter, isochronous-error and hard-xrun deltas, with
  the backend reasons that produced the classification;
- active electrical profile;
- requested, effective, and pending driver mode;
- Timecode Optimized arm, qualification evidence, and wait/fail-open reason;
- Vintage Compatible experimental/conformance state;
- loopback enablement, source, publishing state, and gap/overrun counters; and
- snapshot age, partial evidence, and backend/schema mismatch.

The panel is a client of the versioned process interfaces. It does not add a
daemon, network listener, telemetry, auto-update, direct USB access, Core Audio
I/O, firmware operation, private socket client, or new driver policy. It does
not claim that Performance reduces measured latency, that Vintage is verified,
or that a hardware/USB state is healthy when evidence is unavailable.

The implementation remains an independent OpenA8DJ work. Do not use vendor
logos, artwork, binaries, firmware, private headers, or claims of endorsement.

## Existing surfaces and compatibility baseline

The current deployment target is macOS 13.0, declared by
`macos/OpenA8DJControlCenter/Info.plist`. Use SwiftUI with AppKit only for
macOS-native facilities that SwiftUI does not cover (for example open/save
panels). Do not add a package dependency, web view, embedded server, or network
entitlement.

The current app is compiled directly by `swiftc` from one source and embeds
`opena8dj-control`. The new implementation may split sources under
`macos/OpenA8DJControlCenter/`; the Makefile must enumerate those sources
deterministically and must also embed the matching
`opena8dj-hardware-profiler` and bundled known-issues catalog. Installed tools
remain available in `/usr/local/bin`, but the app must execute its signed,
bundled sibling tools so that UI and decoder contracts cannot drift.

The only runtime command surfaces the app may invoke are:

```text
opena8dj-control api version
opena8dj-control api profiles
opena8dj-control api profile
opena8dj-control api profile set <allowlisted-id>
opena8dj-control api driver-modes
opena8dj-control api driver-mode
opena8dj-control api driver-mode set <allowlisted-id>
opena8dj-control api driver-mode arm timecode-optimized --input-pairs A,B
opena8dj-control api driver-mode disarm timecode-optimized
opena8dj-control api stats
opena8dj-control api loopback get
opena8dj-control api loopback enable <A|B|C|D>
opena8dj-control api loopback disable
opena8dj-control usb-quality --json --interval-ms 1000 --count 2
opena8dj-hardware-profiler --json
```

Do not invoke `export-config`, `import-config`, `apply-preset`, the private
`/tmp/opena8dj-control.sock`, or a shell. The public CLI remains the sole owner
of socket authentication, mutation locking, payload validation, and
same-connection set/read-back.

## Architecture

Keep I/O, decoding, reduction, and views separate:

```text
SwiftUI/AppKit views
        |
ControlCenterStore (@MainActor, UI intents and presentation state)
        |
DashboardReducer (pure snapshots, deltas, staleness, action outcomes)
        |
RefreshCoordinator actor (visibility, cadence, backoff, cancellation)
        |
BoundedProcessRunner actor (one child at a time, argument vectors only)
        |
bundled opena8dj-control / bundled opena8dj-hardware-profiler
```

Suggested source boundaries are `Models.swift`, `Decoders.swift`,
`ProcessRunner.swift`, `DashboardReducer.swift`, `ControlCenterStore.swift`,
`Views.swift`, and the small `@main` file. Exact filenames are not an API, but
production models/reducer/process code must be independently compilable by the
fixture test target.

### State model

Do not use `nil`, zero, green, or an empty string as a generic unknown value.
Use an explicit evidence type in the domain model:

```swift
enum Evidence<Value> {
    case known(Value)
    case unavailable(reason: EvidenceReason)
}
```

`EvidenceReason` must distinguish at least `backendUnavailable`,
`permissionDenied`, `protocolMismatch`, `unsupportedTail`, `notStreaming`,
`insufficientData`, `profilerUnknown(code:)`, `timedOut`, `truncated`,
`stale`, and `notYetObserved`. Preserve stable backend reason/code strings for
display and support copy; map unknown future strings to an explicit
`unrecognized(code:)`, never to success.

`DashboardSnapshot` is immutable and contains:

- API semantic version and capability set;
- profile snapshot;
- stream/quality, driver-mode, timecode, Vintage, and loopback snapshots;
- profiler report summary/checks;
- monotonic `capturedAt` per source;
- current refresh phase and last source-specific error; and
- a backend generation/fingerprint sufficient to detect disagreement between
  the version, stats, mode, profile, loopback, and profiler observations.

The reducer retains the last valid snapshot after a refresh failure only as
`stale(lastUpdated:, reason:)`. Views must show its age and failure beside it;
they must not relabel it current. Counter decreases, generation rollback,
schema disagreement, or incompatible same-cycle state are a backend mismatch,
not negative deltas.

### Process execution and trust boundary

Resolve tools only from `Bundle.main.resourceURL`. Reject a missing tool,
symbolic link, non-regular file, or non-executable file. Never search `PATH`,
honor an environment/config override, interpolate a command string, or accept
an executable path from JSON. Launch with `Process.executableURL` and a literal
argument array selected from an internal operation enum.

The runner must:

- serialize all app-created child processes; there is never an overlapping
  refresh or mutation process;
- capture stdout and stderr separately and drain both while the process runs;
- cap stdout at 512 KiB and stderr at 32 KiB per invocation, terminate on
  overflow, discard bytes beyond the cap, and report `truncated`;
- use a 3-second timeout for single API calls, 7 seconds for the two-sample
  USB-quality command, and 8 seconds for the profiler;
- on timeout or cancellation send termination, wait a short bounded grace
  period, then interrupt if still running, and always reap the child;
- accept exactly one newline-terminated UTF-8 JSON object for `api`, exactly
  two individually bounded newline-delimited objects for the quality command,
  and exactly one profiler object;
- treat non-UTF-8, missing final newline, multiple API/profiler values, trailing
  non-whitespace, JSON duplicate-key ambiguity if detectable, or a nonmatching
  exit/envelope as protocol failure; and
- never include arbitrary stderr, paths, environment, or raw JSON in the
  normal dashboard. A bounded, user-initiated diagnostic detail may expose
  stable error codes and redacted messages only.

All API reads must set `OPENA8DJ_CONTROL_NO_WAKE=1`. The public API already does
not wake the driver; the environment flag makes the intent explicit and
protects against accidental fallback to a legacy command.

## Exact JSON-to-UI map

Every public API response must have schema
`org.opena8dj.public-api.response.v1`, API major `1`, `ok` matching process
status, the expected operation, and exactly one of `data` or `error`. Accept
additive members and API minor versions at least 1.1; reject a different major,
schema, operation, required type, or enum/cross-field invariant. A version
newer than the panel's tested minor is shown as `newer backend — partially
verified`, not hidden.

### Bootstrap and refresh

| UI/domain value | Command and JSON source |
| --- | --- |
| backend/API badge | `api version`: top-level schema/version plus `data.apiVersion`, `data.schema`, `data.transport == "process-json"`, capabilities |
| profile choices | `api profiles`: `data.profiles[].id/title/surface/summary` |
| mode choices/flags | `api driver-modes`: `data.schemaVersion == 1`, `data.driverModes[]` |
| electrical profile | `api profile`: `data.activeProfile`, mode/boolean/source/transform members |
| stream and cumulative quality | `api stats`: `data.stream`, `data.quality`, `data.output`, `data.health` |
| mode requested/effective/pending | `api driver-mode`: `data.requestedMode`, `effectiveMode`, `pending`, `streaming`, `lastResult`, `rejectionReason`, `generation`, `effectivePolicy` |
| Timecode arm/evidence/wait | `api driver-mode`: `data.timecodeOptimized` |
| Vintage status | `api driver-mode`: `data.vintageCompatible` |
| loopback configuration | `api loopback get`: all `data` members |
| USB window classification | `usb-quality ... --count 2`: second sample |
| device/firmware/power | `opena8dj-hardware-profiler --json`: overall and required checks |

Bootstrap `api version`, profiles, and driver modes once per activation and
again after an executable/version change. Required capabilities are checked
individually. Missing read capability makes that card `unsupported`; missing
write capability disables only its action.

The normal refresh cycle is serial: quality two-sample window, stats, profile,
driver mode, and loopback. The profiler runs on activation, on explicit
refresh, and at most once per 60 seconds; it is inserted into the same serial
queue. The quality command supplies the one-second foreground cadence and its
second observation is the dashboard value. Never present its first
`warming-up` observation as the final window.

### USB quality card

Map the second quality sample as follows:

- title state: `stability.classification`, preserving `stable`, `degraded`,
  `unstable`, `not-streaming`, `insufficient-data`, or `warming-up`;
- explanation: every string in `stability.reasons`, in backend order;
- jitter: `jitter.capture/playback.p95` and `.p99` using
  `upperBoundUs`/`overflow`; do not invent a point estimate from a histogram;
- isochronous errors: capture and playback `totalEvents`, plus component
  deltas on demand;
- xruns: `xruns.totalHardXruns`, active underruns, ring overruns, and late-write
  batch/frame deltas;
- sampling context: `windowMilliseconds`, `streaming`, `sampleRateHz`,
  `instrumentationAvailable`, and active-direction/sample counts.

Cross-check its stream/rate/instrumentation values against the adjacent
`stats.get` snapshot. A disagreement that cannot be explained by capture time
is shown as `backend mismatch`. Do not show `stable` when instrumentation is
false, the relevant active direction has fewer than 20 samples, classification
is insufficient/not-streaming/warming, or schema validation failed.

Counter deltas are computed only between monotonic same-generation snapshots.
On the first sample or after reset/reconnect, show `baseline`, not zero. A
counter decrease displays `counter reset` and starts a new baseline.

### Device, firmware, power, and profiler card

Validate profiler schema `org.opena8dj.hardware-profiler.report.v1` and
`schemaVersion == 1`. Display `overall.status` and `summaryCodes` without
weakening precedence. Required rows map by exact `checks[].id`:

- device presence/identity: `usb.identity` and `usb.enumeration`;
- link state: `usb.link-speed`;
- power known-state: `usb.power`;
- firmware: `device.firmware`;
- Core Audio pairing: `coreaudio.device`;
- panel/backend pairing: `driver.api-pairing`; and
- profiler USB quality corroboration: `usb.stream-quality`.

Each row shows status (`PASS`, `WARN`, `FAIL`, or `UNKNOWN`), stable `code`,
summary, and optional remediation. `UNKNOWN` remains a first-class label.
Firmware values are shown only from available evidence for
`device.firmware`; never reinterpret USB `bcdDevice` as firmware. Power is
never inferred from device presence, absence of an error, or xruns.

Cross-check profiler API pairing and firmware evidence with `api version`,
`api hardware`, or stats-derived availability when present. Any conflict is a
visible backend mismatch and neither source wins silently.

### Electrical profile card

Show `activeProfile` exactly, including `custom`, plus input mode, decode,
software lock, lifts, sources, and transforms. Choices are the intersection of
the compile-time canonical IDs and exact IDs from `profiles.list`; an additive
unknown backend profile can be displayed but not applied by this panel build.
After set, distinguish `requestedProfile` from truthfully inferred
`activeProfile`.

### Driver modes, Timecode, and Vintage card

Show requested and effective mode on separate labelled rows and always show
`pending`. Include streaming, `lastResult`, `rejectionReason`, effective
latency-frame policy, QoS, and generation in disclosure details. Never rewrite
effective mode to the requested value.

For Timecode Optimized, map:

- `armed`, `armState`, `allowedInputPairs`;
- `electricalProfile`, `profileVerified`;
- `evidenceKind`, `windowFresh`, `qualified`, `optimizedActive`;
- `eligibleWindows` / `requiredEligibleWindows`, dropout windows;
- A/B/C/D `pairWindows` or explicit unavailable evidence; and
- `lastFailOpenReason`, input lead/ceiling, and counters.

The primary wait reason is derived without concealment: wrong/unverified
profile; stale/missing evidence; insufficient qualifying windows; pending safe
boundary; outside-pair activity; transport/xrun/input-lead trip; apply failure;
or explicit disarm. `armed` is never rendered as `active`.

Vintage always carries the literal `Experimental — Unverified` label. Display
`status` (`unverified`, `partial`, or compatible), `claim`, all reasons, and
preflight/capability disclosure. The UI must not shorten `partial` to
compatible or omit the experimental label.

### Loopback card and privacy

Loopback is disabled by driver default and must stay disabled until the user
explicitly enables it. Display `enabled`, exact `sourcePair`,
`sessionOnly`, `physicalPlaybackPublishing`, readers, generation, and
published/delivered/silence/gap/overrun values. Compute gap/overrun deltas with
the same baseline/reset rules as quality counters.

Do not call a pair “master”. Enabling requires an explicit confirmation that
the selected output pair will be exposed as an application-readable virtual
input for the current session. Changing source while enabled is a distinct
confirmed enable action. Disable is always immediately available unless
another action is busy.

## Polling and lifecycle

The store starts refresh only while the app is active and the dashboard window
is visible. Observe both scene phase and AppKit window visibility/key lifecycle.
On resign-active, close, miniaturize, or navigation away from the dashboard,
cancel the current process and cadence task. Resume with a fresh baseline; do
not accumulate a hidden background history.

There is one `RefreshCoordinator` actor and one cancellable cycle. Manual
refresh coalesces with the current cycle; it never starts another process.
Mutations suspend polling, use the same process queue, and trigger a full
read-back cycle before polling resumes.

When the backend returns `backend_unavailable`, retain explicitly stale
evidence and back off refresh attempts to 2, 4, 8, then 15 seconds maximum.
Successful API contact resets backoff. Permission/protocol mismatch does not
spin; pause automatic retries at 15 seconds and present a manual retry.
Offline catalog operations may still populate choices while the HAL is absent.

Every card displays age from a monotonic capture timestamp. At 3 seconds it is
`aging`; at 10 seconds it is `stale`. Wall-clock changes cannot make evidence
fresh. VoiceOver text must include the same age/state.

## Mutations, confirmation, read-back, and rollback

Expose only these typed intents:

- set one canonical profile;
- set `balanced`, `performance`, or `vintage-compatible`;
- arm/disarm Timecode Optimized for the exact A,B pair;
- enable loopback for exact A/B/C/D; and
- disable loopback.

No view passes arbitrary arguments. The operation enum constructs the complete
argument array and enforces length/character allowlists before launch. The
whole dashboard has one visible busy action; disable conflicting controls and
offer cancellation only before a mutation process begins.

For every mutation:

1. capture the latest valid pre-action state;
2. show a confirmation summarizing requested effect, pending semantics, privacy
   or experimental status as applicable;
3. execute the public mutation and validate its successful read-back envelope;
4. perform a separate public get and compare the relevant generation and
   fields; and
5. show applied, pending, rolled back, rollback failed, or indeterminate.

The CLI already keeps the prior effective state on a failed transactional
apply. If the mutation succeeds but the separate get disagrees, attempt one
compensating public mutation to the previously requested allowlisted state,
then read back again. Profile compensation is allowed only when the previous
`activeProfile` is canonical; a previous `custom` state is not reconstructible,
so show `rollback unavailable` rather than guessing. Loopback rollback restores
the previous enabled/source state. Timecode rollback restores its previous arm
boolean. Never loop rollback attempts.

Do not erase the prior snapshot or error banner after failure. Error details
must show the stable API code, whether retryable, action phase, and rollback
outcome. Pending is a successful truthful outcome, not a failure.

## UI, accessibility, and error/offline states

Use a sidebar or toolbar-backed dashboard with compact system-native cards:
Overview, USB Quality, Driver Modes, Loopback, and Diagnostics. Preserve useful
profile descriptions/cabling guidance from the current app without making a
known state depend on color.

Every status combines text, an SF Symbol, and optional system color. Use system
background/label/semantic colors with sufficient contrast in light, dark, and
increased-contrast modes. Never use only red/amber/green, animation, or a chart
shape to communicate state.

Required accessibility behavior:

- every metric has a concise VoiceOver label, value, unit, evidence state, and
  age; grouped cards have useful summaries and traversal order;
- buttons, popups, disclosures, confirmations, and retry are keyboard
  reachable, with visible focus and nonconflicting shortcuts;
- values do not update the accessibility focus on every poll; announce only
  meaningful state transitions, action results, and new failures;
- respect Reduce Motion; numeric updates do not slide/pulse, and any optional
  transition becomes immediate;
- Dynamic Type/system text styles are used and content remains readable when
  the window narrows; and
- jitter and counters have textual/tabular equivalents. A sparkline is optional
  and can never be the only representation.

Explicit top-level states are:

- `starting`: no validated snapshot yet;
- `online`: compatible current evidence;
- `partial`: compatible backend with absent/old optional tails;
- `offline`: retryable backend absence;
- `permission denied`;
- `protocol/backend mismatch`;
- `stale`: last good evidence retained with age/reason; and
- `action failed/indeterminate`: current state and rollback outcome visible.

Never collapse `UNKNOWN`, `unverified`, `partial`, `insufficient-data`,
`not-streaming`, `warming-up`, experimental, pending, or stale into a generic
healthy state.

## Tests and verification

Add fixture-driven offline tests that compile and exercise production
decoders/reducer/process policy. Fixtures must cover at least:

1. a fully compatible API 1.1/profiler report and stable quality window;
2. an older compatible partial stats tail with null mode/timecode/Vintage/
   loopback and profiler `UNKNOWN`;
3. wrong schema, API major, operation, required type, enum, cross-field value,
   and profiler schema/version;
4. truncated output, oversized output, invalid UTF-8, missing newline, trailing
   second object, timeout, and cancellation;
5. initial/baseline, valid deltas, counter reset, stale/aging thresholds, and
   backend reconnect;
6. action error envelopes, nonzero/success mismatch, read-back disagreement,
   rollback success/failure/unavailable, and preserved error visibility;
7. requested/effective disagreement with `pending: true`;
8. Timecode armed-but-waiting, stale evidence, wrong profile, qualified pending,
   active, and fail-open reasons;
9. loopback disabled default, explicit enable/source, gap/overrun deltas, and
   privacy confirmation policy; and
10. backend unavailable backoff, no overlapping process launch, visibility
    cancellation, and refresh coalescing.

Provide representative checked-in JSON/NDJSON fixtures; tests must never use
the private socket, hardware, installed driver, network, or mutable global
state. Keep fixtures small and assert user-facing evidence labels, not only
decoder success.

Add `make control-center-offline-test` for all fixtures/model/reducer tests and
`make control-center-smoke-test` for:

- a release build of the app at deployment target 13.0;
- bundle structure, embedded regular/executable tools and catalog;
- `plutil -lint` and `codesign --verify --deep --strict`;
- construction of every top-level SwiftUI view using fixtures; and
- source-policy checks for no shell, private socket, network API, direct USB,
  vendor assets, or unbounded process reads.

Pure compilation, fixtures, bundle inspection, and ad-hoc signature validation
do not need the shared hardware lock. Launching the app, invoking it against a
live Core Audio/API backend, running the live profiler, installing, packaging
with installation side effects, or taking a UI screenshot must use:

```sh
./scripts/shared-hardware-lock-run \
  --gate modern-control-panel \
  --run-dir <unique-run-directory> \
  -- <command>
```

Do not use `sudo`, reset/reload hardware, change branch, remove lock files, or
weaken an existing offline test. A compile/build smoke test is not evidence of
live metrics or final visual quality. Those claims require a locked live launch
and reviewed screenshots at normal, dark, increased-contrast, large-text, and
Reduce Motion settings.

## Build, packaging, uninstall, and documentation

The `control-center` target must depend on the profiler, catalog, production
Swift sources, plist, and control tool, and embed the exact built artifacts in
`Contents/Resources`. Package-root tests must verify executable/catalog hashes
against the built inputs and verify there are no vendor assets.

Update the tools package and manual install so that the app, both bundled
tools, installed tools, and catalog are consistent. Update preinstall,
postinstall, and the tools-only uninstaller to remove/permission both tools and
catalog while preserving unrelated `/Library/Application Support/OpenA8DJ`
content. Do not broaden removal beyond the documented tools package.

Update `docs/INSTALL.md`, `docs/AUDIO8DJ_CONTROL_SURFACES_USER_GUIDE.md`, and
verification documentation with:

- dashboard scope and evidence limitations;
- foreground-only polling and offline/backoff behavior;
- exact mutation confirmations/pending/read-back/rollback semantics;
- loopback privacy/default-disabled behavior;
- experimental/unverified Vintage wording;
- profiler privacy and `UNKNOWN` meaning;
- offline test/build commands; and
- the shared-lock command for any live launch or screenshot.

## Acceptance criteria

- The shipping panel uses only bundled, trusted public process APIs and never
  connects to private IPC or the network.
- Processes are bounded, timed out, cancellable, non-overlapping, and parsed
  with strict schema/operation/type/cross-field validation.
- The minimum dashboard truthfully exposes every state and metric listed in the
  goal, including age and backend mismatch.
- Mutations are compile-time allowlisted, single-flight, confirmed, separately
  read back, and have visible bounded compensation outcomes.
- Loopback remains disabled until explicit informed action.
- Unknown, partial, insufficient, pending, stale, experimental, and unverified
  states remain visible in text and accessibility output.
- Polling stops whenever the dashboard is not foreground-visible and backs off
  when the backend is absent.
- Fixture tests cover good, partial, mismatch, truncated, stale, action-failure,
  pending, and lifecycle cases; compile, UI construction, bundle, package, and
  uninstall contracts pass offline.
- Build/package/docs are updated without external dependencies or vendor
  branding.
- Any live evidence is captured only under the shared hardware lock; absent a
  locked launch and screenshots, the result is reported as offline-verified
  only, with live metrics and final visual QA still unverified.
