# Public API v1 design

Status: implementation-ready MVP design
Scope: macOS `opena8dj-control` and the existing local HAL control bridge

## Goal

Give local third-party applications a stable, machine-readable way to:

- discover the API version;
- read a bounded, documented snapshot of driver statistics;
- read marker-qualified device information already cached by the HAL;
- enumerate the built-in profiles;
- query the active profile and control state; and
- apply one built-in profile dynamically;
- enumerate the allowlisted driver modes;
- read requested/effective driver-mode state and policy; and
- request an allowlisted session driver mode dynamically.

The MVP is a JSON command API exposed by `opena8dj-control`. It is deliberately
not a network service, SDK ABI, or new resident daemon. A client launches the
tool with an argument vector, reads one JSON document from standard output, and
uses the process exit status as a coarse success/failure signal. This gives
applications in any language a stable integration surface without adding
listener lifecycle, authentication, or code-signing policy to the real-time
driver.

This implementation remains independent: it uses only OpenA8DJ source and
public macOS interfaces. No Native Instruments binary, header, or proprietary
source is used.

## Existing surface and constraints

The HAL USB engine currently owns `/tmp/opena8dj-control.sock`. The CLI and MIDI
bridge use an internal packed-binary protocol with:

- magic `A8DJ`, protocol version `1`, a type, and a 16-bit payload length;
- control get/set/state messages;
- input statistics that are reset when read; and
- stream statistics that are copied without resetting them.

That protocol is an implementation detail. Its duplicated C structures,
host-endian numbers, packed layout, and append-only length compatibility are
appropriate for the matching bundled tools, but are not a public ABI.

The existing control tool already has a catalog of ten canonical presets and
can infer four states (`playback-4out`, `traktor-dvs-vinyl`,
`traktor-dvs-cd-line`, and `vinyl-recording`). Several workflow presets map to
the same hardware state, so a later query cannot always recover the name that
was requested. In that case the truthful active profile is `custom`.

The HAL is hosted by the `_coreaudiod` account while the bundled clients run as
the console user. The socket therefore has to remain connectable across those
UIDs. Public mutation safety must come from authenticating Unix peer
credentials, not from making the socket owner-only.

## Invocation contract

The public entry point is:

```text
opena8dj-control api version
opena8dj-control api stats
opena8dj-control api hardware
opena8dj-control api profiles
opena8dj-control api profile
opena8dj-control api profile set <canonical-profile-id>
opena8dj-control api driver-modes
opena8dj-control api driver-mode
opena8dj-control api driver-mode set balanced
opena8dj-control api driver-mode set performance
```

Exactly one UTF-8 JSON object, terminated by a newline, is written to standard
output for every `api` invocation, including errors. Public API commands must
not write human prose to standard output. Diagnostic text may go to standard
error only when it cannot leak control payloads or private paths.

`api version`, `api profiles`, and `api driver-modes` are offline operations.
The other reads do not wake or start Core Audio. `api profile set` and
`api driver-mode set` also operate only when the matching HAL bridge is already
available; the MVP does not let a third-party call silently start an audio
stream. Existing non-API CLI commands retain their current wake behavior.

### Success envelope

```json
{
  "schema": "org.opena8dj.public-api.response.v1",
  "apiVersion": "1.0",
  "ok": true,
  "operation": "profiles.list",
  "data": {}
}
```

Required top-level members and their types are stable for API major version 1:

- `schema`: the exact string above;
- `apiVersion`: semantic API version string, initially `1.0`;
- `ok`: boolean;
- `operation`: one of `version.get`, `stats.get`, `hardware.get`,
  `profiles.list`, `profile.get`, `profile.set`, `driver_modes.list`,
  `driver_mode.get`, or `driver_mode.set`; and
- `data`: an object on success.

Minor releases may add object members and new operations. They must not remove
or rename members, change their JSON types or units, change counter semantics,
or add new side effects. A breaking change requires a new `api` major version
and schema identifier. Object member ordering and whitespace are not part of
the contract.

### Error envelope and exit status

```json
{
  "schema": "org.opena8dj.public-api.response.v1",
  "apiVersion": "1.0",
  "ok": false,
  "operation": "profile.set",
  "error": {
    "code": "profile_not_allowed",
    "message": "The requested profile is not in the built-in allowlist.",
    "retryable": false
  }
}
```

The stable v1 error codes are:

| Code | Meaning | Retryable | Exit |
| --- | --- | --- | --- |
| `invalid_request` | Wrong arity or unknown API operation | false | 2 |
| `profile_not_allowed` | Profile is not a canonical built-in ID | false | 2 |
| `backend_unavailable` | HAL control bridge is not running | true | 3 |
| `backend_permission_denied` | Socket path, owner, or peer credentials failed local authentication | false | 4 |
| `backend_protocol_error` | Private IPC reply is invalid, truncated, or incompatible | true | 4 |
| `profile_apply_failed` | Set or read-back verification failed | true | 5 |
| `driver_mode_not_allowed` | Driver mode is not an exact public allowlist ID | false | 2 |
| `driver_mode_busy` | Reserved for an explicit future caller that disallows pending; normal v1 set does not emit it | true | 3 |
| `driver_mode_apply_failed` | HAL preflight/apply kept the previous effective policy | true | 5 |

Messages are for people and may be clarified; clients branch only on `code`.
Unknown operations use the literal operation value `unknown`.

Public API v1 therefore currently defines exactly nine operations, eight
capabilities, and nine stable error codes. Compatible minor releases may add
to these sets but must not remove or redefine existing entries.

## Response data

### `version.get`

`data` contains:

- `apiVersion`: `"1.0"`;
- `schema`: `"org.opena8dj.public-api.response.v1"`;
- `transport`: `"process-json"`;
- `privateIPCVersion`: `1` (diagnostic only; not a promise that clients may use
  the private IPC); and
- `capabilities`: an array containing `stats.read`, `usb-quality.read`,
  `hardware.read`, `profiles.list`, `profile.read`, and `profile.write`.
  It also contains `driver-mode.read` and `driver-mode.write`, for a total of
  eight v1 capabilities.

### `hardware.get`

This operation takes no arguments and reads only the already cached
device-information tail in the non-resetting HAL statistics snapshot. It does
not open or wake a USB device, issue `GET_DEVICE_INFO`, start Core Audio, change
a profile or hardware control, or perform firmware management.

`data.deviceInfoAvailable` is `true` only when the HAL received and completely
decoded the command-matched device-information response during its existing
open path. When true, `firmwareVersion` and `hardwareSubtype` are integers and
`capabilities` contains integer `analogAudioOutputs`, `analogAudioInputs`,
`digitalAudioOutputs`, `digitalAudioInputs`, `midiOutputs`, `midiInputs`, and
`dataAlignment`.

For an older append-compatible HAL, an absent tail, or any marker other than
exactly `1`, the operation still succeeds with `deviceInfoAvailable: false`.
`firmwareVersion`, `hardwareSubtype`, and every member of
`data.capabilities` are then JSON `null`; these placeholders are unavailable
evidence and must not be interpreted as firmware zero or zero capabilities. A
private reply shorter than the existing base through `sampleRate` remains a
`backend_protocol_error`.

### `profiles.list`

`data.profiles` is an array in catalog order. Every element contains the
canonical `id`, `title`, `surface`, and `summary` already defined in the
OpenA8DJ built-in preset table. Aliases accepted by the legacy CLI are not
listed and are not accepted by the public API.

### `profile.get`

`data` contains:

- `activeProfile`: inferred canonical ID or `"custom"`;
- `inputMode`: `timecode-vinyl`, `timecode-cd-line`, `phono`, or `unknown`;
- `inputModeValue`: integer;
- `inputDecode`, `softwareLock`, `groundLiftVinyl`,
  `groundLiftCDLine`, and `groundLiftPhono`: booleans;
- `inputSources`: object with string members `A` through `D`; and
- `inputTransforms`: object with string members `A` through `D`.

### `profile.set`

Only an exact canonical ID in the built-in preset table is accepted. No aliases,
paths, free-form control maps, raw USB commands, or numeric profile identifiers
are accepted.

The tool reads the current state, applies the existing built-in transformation
in memory, sends the complete bounded control payload, reads state back, and
compares the read-back payload with the expected payload. It returns
`profile_apply_failed` on a mismatch.

On success, `data` contains:

- `requestedProfile`: the canonical requested ID;
- `activeProfile`: the truthfully inferred ID or `"custom"`;
- `applied`: `true`; and
- the same control state fields as `profile.get`.

Because some catalog entries intentionally produce indistinguishable hardware
states, `requestedProfile` and `activeProfile` may differ. The API must not
persist a cosmetic active name or claim that an ambiguous state is identifiable.

### `driver_modes.list`

This offline operation returns `data.schemaVersion: 1` and
`data.driverModes` in stable catalog order. The MVP allowlist contains exactly:

1. `balanced`, display name `Balanced`, `default: true`;
2. `performance`, display name `Performance`, `default: false`.

Every entry also has `requiresIdleBoundary: true`: requesting a different mode
while a stream is active is accepted as pending, not applied to that stream.
The catalog exposes named immutable policies, never free-form latency, QoS,
USB, routing, sample-rate, or hardware-control knobs.

### `driver_mode.get` and `driver_mode.set`

Every successful response has `data.schemaVersion: 1` and:

- `requestedMode` and `effectiveMode`: canonical `balanced` or `performance`;
- `pending` and `streaming`: booleans;
- `lastResult`: `unchanged`, `applied`, `pending`, `cancelled`, `invalid`, or
  `apply_failed`;
- `rejectionReason`: `none`, `bad_length`, `unsupported_schema`,
  `reserved_nonzero`, or `unknown_mode`;
- `generation`: effective-policy generation;
- `counters`: `acceptedRequests`, `rejectedRequests`, `appliedTransitions`,
  `applyFailures`, and `pendingTransitions`; and
- `effectivePolicy`: `outputStartLatencyFrames`,
  `outputRestartLatencyFrames`, `outputTargetLatencyFrames`, and string
  `workerQoS`.

`balanced` is the session default and reports policy
`8192 / 4096 / 8192 / default`. `performance` reports
`4096 / 4096 / 4096 / user-interactive`. The selection is process-session
state only: it is not persisted, does not change the 512-frame Core Audio
minimum, and is independent of the electrical/routing hardware `profile`.
Production preflight requires every output watermark to be at least 4096 and
strictly below the fixed 32768-frame output ring capacity, with
`restart <= target <= start` and a known worker QoS. This keeps future catalog
entries from turning an immutable descriptor into an unsafe ring policy.

`driver_mode.set` accepts only the exact strings `balanced` and `performance`.
When idle, a valid different policy is committed atomically after preflight.
While streaming, it changes only `requestedMode`; `effectiveMode` and the
current stream policy remain unchanged and `pending` is true until a safe
stop/next-start boundary. Requesting the effective mode cancels a pending
request. A preflight/apply failure preserves the previous effective descriptor
and returns `driver_mode_apply_failed`.

The client sends the versioned private set request, validates the HAL state
response, then performs a getter/read-back on the same authenticated
connection. Success requires exact agreement between set response and
read-back, with the requested mode either effective or truthfully pending.
Unknown schemas/enums, truncation, contradictory pending/effective state, or
set/get disagreement produce `backend_protocol_error`; the client never
rewrites `effectiveMode`.

### `stats.get`

The response is a non-resetting snapshot. Existing counters reset at stream
start, and are monotonically increasing only within that stream. The stable
subset is grouped as follows:

- `stream`: `streaming` (boolean), `sampleRate` (number, Hz),
  `outputRingFrames`, and `outputTargetLatencyFrames`;
- `clock`: `anchorValid` (boolean), `acceptedAnchors`, `rejectedAnchors`,
  `anchorResets`, and `usbFrameResyncs`;
- `capture`: `transfers`, `transactions`, `bytes`,
  `transactionFailures`, `shortTransfers`, and `queueFailures`;
- `playback`: `transfers`, `transactions`, `bytes`,
  `transactionFailures`, `shortTransfers`, and `queueFailures`;
- `output`: `framesWritten`, `framesRead`, `underruns`,
  `activeUnderruns`, `ringOverruns`, `timelineResets`,
  `lateWriteFrames`, and `lateWriteBatches`; and
- `health`: `inputCheckErrors` and `outputPanicFlags`.
- `quality`: additive USB completion-cadence histograms and isochronous error
  classes as specified in [USB_QUALITY_METER.md](USB_QUALITY_METER.md).
- `driverMode`: the same schema version, requested/effective/pending/result,
  generation, counters, and effective policy exposed by `driver_mode.get`.

`quality.instrumentationAvailable` is `false` when the connected HAL predates
the complete append-only quality tail or explicitly reports that the build-time
instrumentation is disabled. In that case its counters are zero only as
placeholders and must not be interpreted as a healthy link.

All counters and frame values are JSON integers. The private payload must
contain its base through `sampleRate`; an empty or shorter payload is a
`backend_protocol_error`. If a later value does not exist in an older
append-compatible payload, it is reported as `0`; it is not omitted or
fabricated from a different metric.

The destructive input meter statistics are intentionally excluded from v1.
Reading public statistics must never change later observations.

When the private stream payload predates the complete append-only driver-mode
tail, `stats.get` remains readable and reports `data.driverMode: null`. It must
not fabricate a balanced state. A complete but invalid driver-mode tail is a
`backend_protocol_error`.

## Security and concurrency

1. The HAL socket remains `0666` so the console-user clients can connect to the
   `_coreaudiod` host. Immediately after `accept`, the HAL obtains the Unix peer
   credentials with `getpeereid` and accepts only UID 0, its own effective UID,
   or the current `/dev/console` owner. It closes every other peer before adding
   it to the client set, sending state, or dispatching a request. Mode `0666`
   is only cross-UID DAC permission to attempt a connection; it grants no
   authorization to read or mutate controls. Other local accounts are closed
   based on their authenticated peer UID.
2. Before any public API operation connects, it uses `lstat` without following
   symlinks and rejects a path that is not a Unix socket, has any executable
   permission bit, or is not owned by UID 0 or the `_coreaudiod` account. After
   connecting it obtains the server credentials with `getpeereid`, repeats
   `lstat`, verifies that device/inode and owner did not change, and requires
   the peer UID to equal the observed socket owner.
3. Production uses the fixed existing socket path. A test-only socket override
   may be compiled into a dedicated test harness, but the shipping binary must
   not accept an environment variable, config file, or request field that
   redirects control traffic.
4. API reads and writes never wake Core Audio. They have bounded connect/read
   behavior and must not wait indefinitely.
5. Profile and driver-mode writes share the same per-user advisory mutation
   lock created with mode `0600`, owner/type validation, and a bounded wait.
   This prevents compliant concurrent API writers of either kind from
   interleaving mutation/read-back transactions. Driver-mode set/get state is
   additionally serialized in the HAL by one process-wide mode mutex covering
   raw IPC mutation and stream start/stop boundaries. Read-back remains
   authoritative because legacy/private clients do not take the public lock.
6. JSON strings are emitted through a real escaping helper. Catalog fields and
   error strings must not be inserted unescaped.
7. Requests are argument-vector tokens, not shell strings. Profile and
   driver-mode IDs have a maximum length of 64 bytes and must exactly equal
   their respective catalog IDs.
8. The public API has no raw IPC, arbitrary file import, arbitrary state patch,
   firmware, USB, MIDI, installation, reload, or privilege-elevation operation.
9. Statistics can reveal device activity to the logged-in user but are not
   exposed cross-user or over a network.
10. Server-side peer enforcement requires the matching updated HAL. Updating
    only `opena8dj-control` can authenticate a server from the client side, but
    cannot add peer authorization to an already-installed legacy HAL.

## Implementation boundaries

- Reuse the existing private IPC readers and preset catalog; do not create a
  second driver-side JSON parser.
- Keep existing human CLI output and command behavior backward compatible.
- Factor JSON envelope/state/stats emitters enough to test them without
  hardware. Do not make the packed private structs a public installed header.
- HAL additions preserve authentication and cross-UID socket policy, append
  versioned driver-mode IPC IDs/payloads, serialize session mode state, and
  append mode observability to stream stats. They do not expose JSON inside the
  driver or change existing private IDs/payload prefixes.
- Add the public API usage to the control-surfaces user guide and CLI help.
- Add an offline contract test target to `Makefile`; it must not connect to
  Core Audio, USB, `/tmp/opena8dj-control.sock`, or installed components.

## Required offline tests

The contract suite must exercise the built shipping CLI code (or the same
factored functions linked into a harness), not just grep source:

1. Parse every success and error response as JSON and assert exactly one JSON
   document is emitted.
2. Assert the schema, API version, operation names, required members, JSON
   types, and stats units/field names.
3. Verify version and profile enumeration work with no backend.
4. Verify all ten canonical profile IDs are enumerated and accepted by the
   allowlist; aliases and hostile/oversized strings are rejected.
5. Exercise `invalid_request`, `profile_not_allowed`, `backend_unavailable`,
   `backend_permission_denied`, `backend_protocol_error`, and
   `profile_apply_failed`, including stable exit statuses.
6. Use a private temporary-directory mock of private IPC v1 to test profile
   query, stats conversion, cached hardware conversion including legacy-null
   semantics, successful set/read-back, and mismatched read-back.
   The test binary may receive a compile-time socket path and an explicit
   test-only allowance for a mock owned by the current UID; the shipping binary
   may not have a runtime redirect or current-UID owner bypass.
7. Verify JSON escaping with quotes, backslashes, and control characters.
8. Verify the HAL source/build artifact keeps the IPC socket at `0666` and
   authenticates every accepted peer before registering or dispatching it.
9. Compile and execute the factored HAL UID policy with both allowed UIDs and
   a denied unrelated local UID, and exercise pathname inode replacement
   between connect and the client's second `lstat`.
10. Run the existing control tool build with warnings enabled and the new
   contract test through a single documented offline command.
11. Verify the two-entry driver-mode catalog and eight version capabilities
    without a backend.
12. Exercise driver-mode default, idle set/read-back, pending/cancel/promote,
    preflight rollback, malformed schema/enum/truncation/contradiction,
    set/get disagreement, the active `driver_mode_not_allowed` and
    `driver_mode_apply_failed` paths, and the reserved `driver_mode_busy`
    definition.
13. Run two concurrent public mode writers and prove the shared mutation lock
    yields complete set/get transactions rather than torn state.
14. Verify `stats.driverMode` counters/policy and legacy `null`, profile/mode
    independence, fixed ring capacity, safe policy invariants, and worker-block
    QoS scope.

No hardware lock is required for compilation or the mock contract suite. Any
manual call to a live control command, Core Audio, USB, installation/reload, or
performance measurement must instead run through:

```sh
./scripts/shared-hardware-lock-run \
  --gate public-api \
  --run-dir <unique-run-directory> \
  -- <command>
```

The `public-api` gate is only for general live API work. Any live driver-mode
or performance-mode command, Core Audio or USB exercise, or benchmark must use
`--gate performance-mode` as required by
[PERFORMANCE_MODE.md](PERFORMANCE_MODE.md).

## Acceptance criteria

- All nine public operations return the documented v1 JSON envelopes.
- `version.get` reports the documented eight capabilities and the error table
  defines all nine stable codes.
- Existing non-API commands remain behaviorally compatible.
- A caller can mutate only a canonical built-in profile or one of the two
  allowlisted driver modes.
- A successful mutation is backed by exact state read-back.
- Streaming driver-mode mutation is truthfully pending; safe-boundary
  promotion is atomic and a failed preflight preserves the old effective
  policy.
- Driver mode and hardware profile remain independently readable and writable.
- The public path does not wake audio or perform destructive stats reads.
- Unsafe socket ownership/type/mode or mismatched peer credentials are
  rejected, and the HAL authenticates every accepted client.
- Contract tests cover success, every active stable error path, reserved error
  definitions, hostile inputs, mock private IPC, schema/types, and socket-mode
  policy without hardware.
- Driver-mode tests cover policy invariants, rollback, concurrency, stats
  compatibility, and fixed-capacity/no-allocation safety without hardware.
- The user guide contains a copy/paste integration example and compatibility
  guidance.
- Build and contract tests pass offline with no warnings introduced.

## Explicit non-goals

- Remote access, TCP/HTTP/WebSocket, Bonjour, or cross-user access.
- A long-running broker, launch agent, SDK framework, Swift package, or C ABI.
- Streaming/subscription updates; clients poll `api stats` in v1.
- Input-meter statistics, because their current read resets accumulation.
- Per-application authorization UI, entitlements, or signed-client allowlists.
- Raw controls, USB commands, firmware operations, arbitrary imported configs,
  or profile creation.
- Solving concurrent writes made through legacy CLI versions.
- Persisting driver mode across Core Audio host process restarts.
- User-supplied latency/QoS/ring/USB/sample-rate/routing policy knobs.
- Windows/Linux parity in this macOS MVP; those platforms may implement the
  same public JSON schema over their native backends later.
