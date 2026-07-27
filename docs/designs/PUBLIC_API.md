# Public API v1 design

Status: implementation-ready MVP design
Scope: macOS `opena8dj-control` and the existing local HAL control bridge

## Goal

Give local third-party applications a stable, machine-readable way to:

- discover the API version;
- read a bounded, documented snapshot of driver statistics;
- enumerate the built-in profiles;
- query the active profile and control state; and
- apply one built-in profile dynamically.

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

The current HAL changes the socket mode to `0666`. That is not acceptable for a
public mutation surface on a multi-user Mac and must be tightened as part of
this MVP.

## Invocation contract

The public entry point is:

```text
opena8dj-control api version
opena8dj-control api stats
opena8dj-control api profiles
opena8dj-control api profile
opena8dj-control api profile set <canonical-profile-id>
```

Exactly one UTF-8 JSON object, terminated by a newline, is written to standard
output for every `api` invocation, including errors. Public API commands must
not write human prose to standard output. Diagnostic text may go to standard
error only when it cannot leak control payloads or private paths.

`api version` and `api profiles` are offline operations. The other reads do not
wake or start Core Audio. `api profile set` also operates only when the matching
HAL bridge is already available; the MVP does not let a third-party call
silently start an audio stream. Existing non-API CLI commands retain their
current wake behavior.

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
- `operation`: one of `version.get`, `stats.get`, `profiles.list`,
  `profile.get`, or `profile.set`; and
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
| `backend_permission_denied` | Socket is not a safe local owner-only socket | false | 4 |
| `backend_protocol_error` | Private IPC reply is invalid, truncated, or incompatible | true | 4 |
| `profile_apply_failed` | Set or read-back verification failed | true | 5 |

Messages are for people and may be clarified; clients branch only on `code`.
Unknown operations use the literal operation value `unknown`.

## Response data

### `version.get`

`data` contains:

- `apiVersion`: `"1.0"`;
- `schema`: `"org.opena8dj.public-api.response.v1"`;
- `transport`: `"process-json"`;
- `privateIPCVersion`: `1` (diagnostic only; not a promise that clients may use
  the private IPC); and
- `capabilities`: an array containing `stats.read`, `profiles.list`,
  `profile.read`, and `profile.write`.

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

All counters and frame values are JSON integers. If a value does not exist in
an older append-compatible private payload, it is reported as `0`; it is not
omitted or fabricated from a different metric.

The destructive input meter statistics are intentionally excluded from v1.
Reading public statistics must never change later observations.

## Security and concurrency

1. The HAL socket mode changes from `0666` to `0600` before `listen`. Existing
   bundled clients run in the same user session and remain compatible.
2. Before any public API operation connects, it uses `lstat` and rejects a path
   that is not a Unix socket, is not owned by the effective user, or has any
   group/other permission bits. It does not follow a symlink.
3. Production uses the fixed existing socket path. A test-only socket override
   may be compiled into a dedicated test harness, but the shipping binary must
   not accept an environment variable, config file, or request field that
   redirects control traffic.
4. API reads and writes never wake Core Audio. They have bounded connect/read
   behavior and must not wait indefinitely.
5. A profile write uses a per-user advisory mutation lock created with mode
   `0600`, owner/type validation, and a bounded wait. This prevents compliant
   concurrent API writers from interleaving read-modify-write operations.
   The read-back remains authoritative because legacy CLI clients do not take
   this new lock.
6. JSON strings are emitted through a real escaping helper. Catalog fields and
   error strings must not be inserted unescaped.
7. Requests are argument-vector tokens, not shell strings. The profile ID has a
   maximum length of 64 bytes and must exactly equal a catalog ID.
8. The public API has no raw IPC, arbitrary file import, arbitrary state patch,
   firmware, USB, MIDI, installation, reload, or privilege-elevation operation.
9. Statistics can reveal device activity to the logged-in user but are not
   exposed cross-user or over a network.

## Implementation boundaries

- Reuse the existing private IPC readers and preset catalog; do not create a
  second driver-side JSON parser.
- Keep existing human CLI output and command behavior backward compatible.
- Factor JSON envelope/state/stats emitters enough to test them without
  hardware. Do not make the packed private structs a public installed header.
- The HAL change is limited to socket permissions unless a narrowly necessary
  fix is found while implementing the safety checks.
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
   query, stats conversion, successful set/read-back, and mismatched read-back.
   The test binary may receive a compile-time socket path; the shipping binary
   may not have a runtime redirect.
7. Verify JSON escaping with quotes, backslashes, and control characters.
8. Verify the HAL source/build artifact creates the IPC socket as `0600`.
9. Run the existing control tool build with warnings enabled and the new
   contract test through a single documented offline command.

No hardware lock is required for compilation or the mock contract suite. Any
manual call to a live control command, Core Audio, USB, installation/reload, or
performance measurement must instead run through:

```sh
./scripts/shared-hardware-lock-run \
  --gate public-api \
  --run-dir <unique-run-directory> \
  -- <command>
```

## Acceptance criteria

- All five public operations return the documented v1 JSON envelopes.
- Existing non-API commands remain behaviorally compatible.
- A caller cannot use the public API to apply anything except a canonical
  built-in preset.
- A successful mutation is backed by exact state read-back.
- The public path does not wake audio or perform destructive stats reads.
- Unsafe socket ownership/type/mode is rejected, and the HAL socket is owner
  read/write only.
- Contract tests cover success, all stable errors, hostile inputs, mock private
  IPC, schema/types, and socket-mode policy without hardware.
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
- Windows/Linux parity in this macOS MVP; those platforms may implement the
  same public JSON schema over their native backends later.
