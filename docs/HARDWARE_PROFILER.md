# Hardware profiler

`opena8dj-hardware-profiler` creates a read-only support report for an Audio 8
DJ and its OpenA8DJ/Core Audio pairing:

```sh
opena8dj-hardware-profiler
opena8dj-hardware-profiler --json
opena8dj-hardware-profiler --json --catalog /path/to/offline-catalog.json
```

Exit status is `0` for PASS, `1` for WARN, `2` for FAIL, `3` when evidence is
incomplete/UNKNOWN, `64` for invalid arguments, and `70` for an internal
construction error. UNKNOWN is not a health claim.

The profiler enumerates public IORegistry and Core Audio properties and calls
the authenticated, non-waking `opena8dj-control api` process interface. It
does not open or reset USB devices, start audio, query firmware directly,
change profiles, install/reload a driver, or contact the network. Firmware is
reported only from marker-qualified device information already cached by the
running HAL. USB power passes or fails numerically only when same-scope,
same-unit available and required current evidence is present; otherwise it is
UNKNOWN. The Audio 8 DJ is USB bus-powered, so no external supply is expected.

Reports are always support-redacted. They omit USB serial/location/registry
path, socket path, host and user identity, and arbitrary child-process stderr.
An explicitly supplied catalog is labelled `operator-supplied`; its path is
not emitted, only its SHA-256. The built-in catalog is local and does not
silently update.

OpenA8DJ is an independent compatibility project, not an official or endorsed
Native Instruments driver. The profiler does not contain, retrieve, or manage
vendor firmware.
