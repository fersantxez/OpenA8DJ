# Firmware/Hardware Profiler design

Status: implementation-ready MVP design
Scope: macOS, read-only support diagnostics for the Audio 8 DJ
Command: `opena8dj-hardware-profiler [--json] [--catalog PATH]`

## Goal

Provide a truthful, reproducible support report for the physical interface,
the OpenA8DJ/Core Audio pairing, cached device information, USB link and power
evidence, stream quality, and locally documented known issues. The report is
useful both to a person and to support automation, without changing any device,
driver, Core Audio, or profile state.

OpenA8DJ is an independent compatibility project. The profiler uses original
OpenA8DJ code, public macOS APIs, public USB descriptors, the public OpenA8DJ
process API, and observations from lawfully owned hardware. It must not read,
copy, download, install, upload, or redistribute Native Instruments firmware,
drivers, applications, or other proprietary payloads.

## Non-goals and hard safety boundary

The MVP must never:

- open, claim, seize, reset, suspend, or reconfigure a USB device or interface;
- send `GET_DEVICE_INFO` or any other command directly to an endpoint;
- write EEPROM, upload/download firmware, or offer firmware recovery;
- start an audio stream, wake an inactive HAL bridge, change sample rate or
  buffer size, change a default device, or reload/install a driver;
- read or write an OpenA8DJ profile or hardware control;
- infer an external power-supply failure. The Audio 8 DJ is USB 2.0
  bus-powered, so absence of an external supply is expected;
- use a zero-valued placeholder, a missing property, a denied query, or an old
  HAL as evidence of health;
- contact a network service or silently update its known-issues catalog; or
- describe OpenA8DJ as an official, endorsed, or certified NI driver.

The probe's `--claim`, `--seize`, `--iso-test`, `AUDIO_PARAMS`, and direct EP1
paths are deliberately not reused. The reset utility is also out of scope.

## Existing evidence surfaces

The implementation should reuse these facts and paths without changing their
semantics:

- USB identity is exactly vendor `0x17cc`, product `0x1978`.
  `opena8dj-probe.m` and `OpenA8DJUSB.m` already use that pair.
- The probe already reads device/configuration descriptors, registry speed,
  link speed, `bcdDevice`, and configuration `MaxPower`. Its default
  enumeration path is read-only, but its firmware query is not.
- `OpenA8DJUSBEngine` already queries `GET_DEVICE_INFO` as part of its normal
  open path and retains the decoded `CaiaqDeviceSpec` in `_spec`, including
  firmware version and hardware subtype.
- The HAL's authenticated private IPC already returns an append-compatible
  stream snapshot. `opena8dj-control api stats` exposes the non-destructive
  public subset, including explicit quality-instrumentation availability.
- The public API authenticates the local socket/server, does not wake Core
  Audio, and defines additive minor-version evolution. The profiler must
  consume this public process API; it must not add a second unauthenticated
  private-IPC client.
- The expected Core Audio identity is device UID
  `org.opena8dj.Audio8DJ`, normally named `Open Audio 8 DJ`.
- The hardware dossier records USB 2.0 bus power and the legal document
  prohibits redistribution of firmware without separate licensing review.

## MVP architecture

### 1. Additive cached device-information surface

Append a fixed device-information tail to `OpenA8DJStreamStatsPayload` in both
the HAL and control tool. Do not reorder, resize, rename, or reinterpret any
existing field, and do not change private IPC version 1. The tail is:

```c
uint64_t deviceInfoAvailable; /* exact marker: 1 means all following fields valid */
uint64_t deviceFirmwareVersion;
uint64_t deviceHardwareSubtype;
uint64_t deviceNumAnalogAudioOut;
uint64_t deviceNumAnalogAudioIn;
uint64_t deviceNumDigitalAudioOut;
uint64_t deviceNumDigitalAudioIn;
uint64_t deviceNumMidiOut;
uint64_t deviceNumMidiIn;
uint64_t deviceDataAlignment;
```

The HAL fills it from the already-decoded, immutable `_spec`. It must set
`deviceInfoAvailable` to exactly `1` only after a complete, command-matched
`GET_DEVICE_INFO` reply was parsed. An absent/short/invalid reply leaves the
marker `0`; zero firmware with marker `0` means unavailable, not firmware 0.
Reading the snapshot must not issue a new USB request.

Add the public operation:

```text
opena8dj-control api hardware
```

Its success envelope remains
`org.opena8dj.public-api.response.v1` / API `1.0`, operation
`hardware.get`. `data` contains:

```json
{
  "deviceInfoAvailable": true,
  "firmwareVersion": 31,
  "hardwareSubtype": 0,
  "capabilities": {
    "analogAudioOutputs": 8,
    "analogAudioInputs": 8,
    "digitalAudioOutputs": 0,
    "digitalAudioInputs": 0,
    "midiOutputs": 1,
    "midiInputs": 1,
    "dataAlignment": 2
  }
}
```

When the append-only tail or exact marker is absent, `hardware.get` succeeds
with `deviceInfoAvailable: false`, all unavailable values represented as
`null`, and no health claim. A legacy/mismatched reply that is shorter than the
existing minimum through `sampleRate` remains `backend_protocol_error`.
`version.get.data.capabilities` additively gains `hardware.read`. Existing
operations, members, types, units, side effects, error codes, and schema stay
unchanged.

This public operation is intentionally cached observation, not a firmware
management API. It cannot accept arguments and cannot initiate USB I/O.

### 2. Standalone report command

Build and package `opena8dj-hardware-profiler` as a Swift command-line tool.
Swift is suitable here because Foundation provides bounded process execution
and strict JSON decoding, while IOKit and Core Audio provide the read-only live
collectors. The tool:

1. enumerates IORegistry USB device services and reads properties/descriptors
   without opening an `IOUSBHostDevice`;
2. enumerates Core Audio devices and reads UID/name/manufacturer/transport
   properties without starting I/O;
3. invokes the sibling `opena8dj-control` executable with argument arrays
   (`api version`, `api hardware`, `api stats`), never through a shell;
4. evaluates checks against a bundled local catalog; and
5. emits either a concise human report or one newline-terminated JSON document.

Each child command has a hard timeout and captured stdout/stderr size limit.
The production executable searches only its own directory and the fixed
installed `/usr/local/bin/opena8dj-control`; it reports the selected executable
as redacted provenance. It must reject non-regular files and must not use
`PATH`, an environment variable, a config file, or a response field to redirect
the production control client. Test builds bypass process execution with
fixtures.

`--catalog PATH` is an explicit operator override for offline support work.
The report must label an override as `operator-supplied` and include its
SHA-256, never imply it is the bundled release catalog, and never execute
catalog content. Normal production operation uses the packaged catalog.

### 3. Providers and deterministic evaluation

Keep collection separate from evaluation:

```text
USBProvider -----\
CoreAudioProvider +--> Observation --> Evaluator --> ReportRenderer
PublicAPIProvider/
CatalogProvider --/
```

The evaluator accepts a complete immutable `Observation` and performs no I/O.
Every production source maps absence, permission denial, malformed data, and
timeout explicitly. A test-only build flag
`OPENA8DJ_HARDWARE_PROFILER_TESTING` enables `--fixture PATH`; the shipping
binary must neither recognize that option nor read a fixture environment
variable. Fixtures use the same observation decoder and evaluator as live
data.

## Check model

Every check has a stable `id`, one status (`PASS`, `WARN`, `FAIL`, `UNKNOWN`),
one stable result `code`, a human summary, zero or more evidence records, and
zero or more remediations. `UNKNOWN` means the profiler cannot establish the
fact; it must never be rendered as pass, warning-free, or healthy.

Evidence records contain:

- `source`: stable source ID such as `ioregistry.usb`,
  `coreaudio.device`, `opena8dj.api.hardware`, or
  `opena8dj.api.stats`;
- `key`: stable fact name;
- `available`: boolean;
- `value`: typed value or `null`;
- optional `unit`;
- `confidence`: `direct`, `derived`, or `unavailable`; and
- optional `reasonCode` for unavailable evidence.

Required checks and result rules:

| Check ID | PASS | WARN | FAIL | UNKNOWN |
| --- | --- | --- | --- | --- |
| `usb.identity` | Exactly one `17cc:1978` service | More than one exact service | An observed candidate has the wrong VID/PID, or no exact device is present | USB enumeration itself failed/was denied |
| `usb.enumeration` | Exact device has a readable device descriptor/current configuration | Descriptor/configuration is incomplete but identity is exact | Exact device is present but cannot enumerate as a usable USB device | No exact device or query unavailable |
| `usb.link-speed` | Direct link evidence is high speed (480 Mbit/s) or faster | Direct evidence is full/low speed | A reliable stack error says the link is unusable | Speed property unavailable/unrecognized |
| `usb.power` | Reliable same-scope current-available/allocation evidence is at least reliable current-required evidence and no failure flag exists | Exact device enumerates but only partial non-failure power evidence exists | `kUSBFailedRequestedPower` is asserted, or comparable same-scope available/allocation mA is reliably less than required mA | No comparable reliable current-available and current-required evidence |
| `device.firmware` | Cached device info exists and firmware is recognized by the local catalog | Cached info exists but firmware is not cataloged | Cached structure is internally impossible or an explicit catalog rule marks it unsupported | HAL/device-info evidence unavailable, old, denied, timed out, or malformed |
| `coreaudio.device` | Exactly one device has UID `org.opena8dj.Audio8DJ` | Exact UID exists but non-identity metadata is unexpected | USB device is exact/present but expected Core Audio UID is absent | Core Audio query failed/was denied, or USB device is absent |
| `driver.api-pairing` | Version, hardware, and stats calls have the expected v1 schema and the exact Core Audio UID exists | Additive unknown members or a newer compatible minor version are present | UID exists but API is unavailable/incompatible, schema/operation is wrong, or device-info structures contradict each other | Both driver surfaces are absent, or evidence is insufficient to pair them |
| `usb.stream-quality` | Instrumentation is explicitly available, stream has enough samples, and no quality threshold/error is violated | Concrete jitter degradation, ISO error, or xrun evidence exists | Reserved for structurally inconsistent counters or an explicitly unusable stream | Stats/instrumentation unavailable, not streaming, or insufficient samples |
| `known-issues.catalog` | Catalog is valid and no rule matches | One or more valid WARN issue rules match | One or more valid FAIL/unsupported rules match | Catalog missing/invalid/conflicting, or a potentially applicable rule lacks required evidence |

For `usb.identity`, a live run normally has no notion of a "candidate" when the
exact device is absent, so the result is `FAIL/USB_DEVICE_NOT_FOUND`. The
separate stable `USB_IDENTITY_MISMATCH` result is exercised when a fixture or
future public candidate signal explicitly identifies a nonmatching device.
The profiler must not select a device by product-name substring.

`bcdDevice` is reported as USB descriptor release evidence, not silently
relabeled as firmware. Firmware comes only from the cached, marker-qualified
device info. Speed uses the public `UsbLinkSpeed`/USB speed property and known
Apple link-speed constants. Configuration `MaxPower` is descriptor demand, not
proof of actual allocation.

For power, comparisons are allowed only when units and scope are known.
`CurrentAvailable`/allocation and `CurrentRequired`/descriptor demand must both
be reliable and comparable before a numerical conclusion is made. Public
evidence can include the configuration descriptor's required mA,
`kUSBFailedRequestedPower`, `UsbPowerSinkAllocation`,
`kUSBBusCurrentAllocation`, or `Bus Power Available`. A descriptor `MaxPower`
alone, a missing failure flag, enumeration symptoms, xruns, or a slow link are
not proof of sufficient or insufficient power. If reliable current-available
and current-required evidence cannot be paired, the result is `UNKNOWN`.
Missing external power is never a signal.

The quality check reuses public stats semantics. It requires
`quality.instrumentationAvailable: true`, a streaming snapshot, internally
consistent histograms (`sum(bins) == samples`), and at least 20 samples for each
direction whose transfer count is nonzero. It uses the documented quality
thresholds and treats cumulative counters as scoped to the current stream.
Zero placeholders, an inactive stream, and an old HAL are `UNKNOWN`.

## Stable result codes

The MVP treats these codes as a compatibility surface:

```text
USB_IDENTITY_EXACT
USB_DEVICE_MULTIPLE
USB_DEVICE_NOT_FOUND
USB_IDENTITY_MISMATCH
USB_IDENTITY_UNKNOWN
USB_ENUMERATION_OK
USB_DESCRIPTOR_INCOMPLETE
USB_ENUMERATION_FAILED
USB_ENUMERATION_UNKNOWN
USB_LINK_HIGH_SPEED
USB_LINK_DEGRADED
USB_LINK_UNUSABLE
USB_LINK_UNKNOWN
USB_POWER_SUFFICIENT
USB_POWER_PARTIAL_EVIDENCE
USB_POWER_INSUFFICIENT
USB_POWER_UNKNOWN
DEVICE_FIRMWARE_RECOGNIZED
DEVICE_FIRMWARE_UNKNOWN
DEVICE_INFO_INVALID
DEVICE_INFO_UNAVAILABLE
COREAUDIO_DEVICE_MATCHED
COREAUDIO_METADATA_UNEXPECTED
COREAUDIO_DEVICE_MISSING
COREAUDIO_QUERY_UNAVAILABLE
DRIVER_API_MATCHED
DRIVER_API_NEWER_COMPATIBLE
DRIVER_API_MISMATCH
DRIVER_PAIRING_UNKNOWN
USB_QUALITY_HEALTHY
USB_QUALITY_DEGRADED
USB_QUALITY_INVALID
USB_QUALITY_UNAVAILABLE
KNOWN_ISSUES_NONE
KNOWN_ISSUE_MATCHED
KNOWN_ISSUE_UNSUPPORTED
KNOWN_ISSUES_CATALOG_INVALID
KNOWN_ISSUES_CATALOG_CONFLICT
KNOWN_ISSUES_EVIDENCE_MISSING
```

Future releases may add codes but must not change the meaning of an existing
code within report schema v1.

## Overall status and process exit

Overall precedence is `FAIL`, then `WARN`, then `UNKNOWN`, then `PASS`:

- any failed check produces overall `FAIL`;
- otherwise any warning produces overall `WARN`;
- otherwise any unknown check produces overall `UNKNOWN`;
- only an all-pass report produces overall `PASS`.

Exit status is stable and independent of output format:

| Exit | Meaning |
| --- | --- |
| `0` | report overall `PASS` |
| `1` | report overall `WARN` |
| `2` | report overall `FAIL` |
| `3` | report overall `UNKNOWN`/incomplete evidence |
| `64` | invalid arguments |
| `70` | report could not be constructed due to an internal error |

Catalog parse/conflict normally produces a complete report with an `UNKNOWN`
catalog check and exit `3`, not an internal-error exit. JSON output is still
emitted for report exits 0 through 3. Argument/internal errors go to stderr and
must not emit a success-looking partial JSON report.

## Versioned JSON report

JSON mode emits exactly one UTF-8, newline-terminated document:

```json
{
  "schema": "org.opena8dj.hardware-profiler.report.v1",
  "schemaVersion": 1,
  "toolVersion": "0.3.135",
  "generatedAt": "2026-07-26T12:00:00Z",
  "overall": {
    "status": "UNKNOWN",
    "exitStatus": 3,
    "summaryCodes": ["USB_QUALITY_UNAVAILABLE"]
  },
  "subject": {
    "expectedUsb": {"vendorId": 6092, "productId": 6520},
    "expectedCoreAudioUid": "org.opena8dj.Audio8DJ",
    "platform": "macOS"
  },
  "privacy": {
    "mode": "support-redacted",
    "omitted": ["usbSerial", "usbLocationId", "registryPath", "hostName", "userName", "socketPath"]
  },
  "checks": [
    {
      "id": "usb.identity",
      "status": "PASS",
      "code": "USB_IDENTITY_EXACT",
      "summary": "One exact Audio 8 DJ USB identity is present.",
      "evidence": [
        {
          "source": "ioregistry.usb",
          "key": "vendorId",
          "available": true,
          "value": 6092,
          "confidence": "direct"
        }
      ],
      "remediation": []
    }
  ],
  "knownIssues": {
    "catalog": {
      "schema": "org.opena8dj.hardware-known-issues.v1",
      "version": "2026.07.1",
      "provenance": "bundled",
      "sha256": "lowercase-hex"
    },
    "matches": [],
    "unresolved": [],
    "conflicts": []
  },
  "collectorErrors": []
}
```

Required top-level members and their types are stable in v1. Minor revisions
may add object members/checks/codes but must not remove or rename fields, change
their JSON types, or weaken redaction. Object ordering and whitespace are not
contractual. Human output shows overall status first, then one line per check,
evidence availability, and remediation; it must retain the same statuses and
codes as JSON.

Collector errors contain stable source/reason codes and a redacted message.
They explain `UNKNOWN` results but are not a second hidden health model.

## Privacy and redaction

The default and only MVP report mode is `support-redacted`. Never emit:

- USB serial number;
- USB `locationID`, registry entry/path, port chain, or exclusive-owner PID;
- Unix socket path, inode, owner UID, user name, home directory, or host name;
- arbitrary Core Audio properties or process stderr that could include paths;
- raw device-info or IPC bytes.

The collector may inspect a property only when required for a check. It must
discard sensitive values before building `Observation`, so both renderers and
fixtures exercise the redacted model. Product/driver versions, macOS version,
USB VID/PID, USB speed/power values, firmware number, hardware subtype, channel
counts, public counters, and stable error codes are support-safe evidence.

An operator-supplied catalog path is never echoed; only provenance and content
SHA-256 are reported. Catalog source URLs are permitted only as explicit
documentation fields authored in the repository.

## Local known-issues catalog

Ship `resources/hardware-profiler-known-issues-v1.json` in the main package and
tools package. It is data, not executable policy fetched from a server:

```json
{
  "schema": "org.opena8dj.hardware-known-issues.v1",
  "catalogVersion": "2026.07.1",
  "issues": [
    {
      "id": "org.opena8dj.example",
      "status": "WARN",
      "summary": "Example only; the initial production catalog should not invent an issue.",
      "source": {
        "title": "OpenA8DJ validated issue reference",
        "version": "1",
        "url": "https://github.com/fersantxez/OpenA8DJ/issues/000"
      },
      "all": [
        {"fact": "usb.vendorId", "op": "eq", "value": 6092},
        {"fact": "usb.productId", "op": "eq", "value": 6520}
      ],
      "remediation": ["Use a remediation validated and documented by OpenA8DJ."],
      "exclusiveGroup": null
    }
  ]
}
```

The example above documents shape only and must not be shipped as a real issue.
The initial catalog may validly contain an empty `issues` array. Every real
rule requires a stable ID, `WARN` or `FAIL`, factual summary, explicit source
title/version (and URL when public), one or more predicates, and at least one
safe remediation. Do not cite or bundle vendor binaries, firmware, internal
documents, or copied proprietary tables.

Supported facts are allowlisted:

```text
usb.vendorId
usb.productId
usb.bcdDevice
usb.linkSpeedBitsPerSecond
usb.requiredPowerMilliAmps
usb.availablePowerMilliAmps
device.firmwareVersion
device.hardwareSubtype
driver.version
api.version
os.version
```

Supported operators are `eq`, `ne`, `lt`, `lte`, `gt`, `gte`, and
`version-in-range` with explicit inclusive minimum/maximum. All predicates in
`all` must match. No regex, script, expression language, nested path, implicit
coercion, network include, environment substitution, or arbitrary JSON pointer
is allowed. Integer facts compare as integers; dotted versions compare
component-wise after strict parsing.

If a required fact is unavailable, the rule is `unresolved`, not unmatched.
Any unresolved rule whose known predicates match yields
`KNOWN_ISSUES_EVIDENCE_MISSING`/`UNKNOWN`.

Duplicate IDs, unknown facts/operators, invalid status/source/remediation, or
schema/version errors invalidate the catalog. If two matched rules in the same
non-null `exclusiveGroup` have different status or remediation, the catalog is
conflicting. Conflicting rules produce no hardware conclusion and yield
`KNOWN_ISSUES_CATALOG_CONFLICT`/`UNKNOWN`; array order never chooses a winner.
Matches retain their rule ID, source/version, status, summary, and remediation
in the report.

## Build, packaging, and documentation

Add:

- `src/tools/opena8dj-hardware-profiler.swift`;
- `resources/hardware-profiler-known-issues-v1.json`;
- a Makefile build target and `hardware-profiler-offline-test`;
- installation of the profiler to `/usr/local/bin`;
- installation of the catalog to
  `/Library/Application Support/OpenA8DJ/`;
- removal of both files in uninstall/preinstall flows; and
- concise CLI usage and privacy/limitations documentation in the relevant
  install/support guide.

The profiler executable version must come from the Makefile release version,
not a separately maintained literal. Package and tools-package must carry the
same catalog bytes used in tests. Package verification should assert the
binary/catalog presence and catalog SHA-256.

## Required offline tests

Offline fixture tests compile the test-enabled profiler, never touch IOKit,
Core Audio, USB, or the installed control socket, and need no hardware lock.
They must assert both human/JSON semantics and exit codes for:

1. exact device absent (`USB_DEVICE_NOT_FOUND`);
2. explicit wrong VID/PID candidate (`USB_IDENTITY_MISMATCH`);
3. recognized firmware from cached device info;
4. unknown firmware (`DEVICE_FIRMWARE_UNKNOWN`, never fabricated healthy);
5. power evidence unavailable (`USB_POWER_UNKNOWN`);
6. reliable insufficient power when fixture facts use comparable mA or the
   failure flag (`USB_POWER_INSUFFICIENT`);
7. Core Audio UID present with API unavailable/incompatible
   (`DRIVER_API_MISMATCH`);
8. stats unavailable or instrumentation marker absent
   (`USB_QUALITY_UNAVAILABLE`);
9. malformed/inconsistent quality histograms (`USB_QUALITY_INVALID`);
10. duplicate/invalid catalog rules and conflicting matched exclusive rules;
11. a potentially applicable known issue with required evidence missing;
12. privacy fixtures containing serial/location/path/user values, proving none
    appear in human or JSON output;
13. production build rejection of `--fixture`;
14. version/schema/member type validation and exactly one newline-terminated
    JSON document;
15. append-only private payload parity between HAL and control definitions,
    legacy-tail behavior, and unchanged pre-existing public API groups; and
16. package manifest/root verification for the profiler and exact catalog.

Fixtures must not make production collection paths redirectable. Test time and
child API results are injected through the test-only provider.

## Live verification and shared hardware lock

Building and running fixture tests does not need the shared lock. Any test that
queries live USB, Core Audio, the installed HAL/API, or physical hardware must
be read-only and run exactly under:

```sh
./scripts/shared-hardware-lock-run \
  --gate hardware-profiler \
  --run-dir <evidence-directory> \
  -- ./build/opena8dj-hardware-profiler --json
```

The live verification must save the JSON report, stderr, exit status, build
commit, catalog SHA-256, and control/HAL versions. It must not use `sudo`,
install/reload a driver, reset USB, change a profile, or remove/override a
shared lock. A busy lock means defer the live test; it is not a profiler
failure.

## Acceptance criteria

The MVP is complete when:

- all required fixture tests pass offline;
- the report never mutates USB/Core Audio/driver/profile state;
- exact identity is `17cc:1978` and no name-based fallback exists;
- cached firmware is marker-qualified and missing data stays `UNKNOWN`;
- Core Audio UID and authenticated public API pairing are independently
  checked;
- power failure requires a reliable explicit signal and bus power is modeled
  correctly;
- quality availability and sample sufficiency prevent false health;
- catalog evaluation is deterministic, versioned, sourced, conflict-safe, and
  network-free;
- human and JSON output agree on status/code/exit;
- redaction tests prove support output contains no sensitive fixture values;
- existing public API tests retain all previous compatibility assertions;
- packages contain matching binary/catalog/docs and no firmware/vendor payload;
  and
- any live smoke evidence was gathered read-only under the shared hardware
  lock.
