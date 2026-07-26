# Linux Tools

Current readiness:

```text
diagnostic only, sound quality not validated
```

This directory contains Linux-only user-space tools.

## `opena8dj-linuxctl`

`opena8dj-linuxctl` is currently a read-only diagnostics prototype. It does not
load modules, change mixer controls, reset USB, restart services, play audio, or
record audio.

Examples:

```sh
linux/tools/opena8dj-linuxctl status
linux/tools/opena8dj-linuxctl diagnostics --json
linux/tools/opena8dj-linuxctl hardware-model --json
linux/tools/opena8dj-linuxctl self-test
linux/tools/opena8dj-linuxctl list-profiles
linux/tools/opena8dj-linuxctl verify --report-dir local-analysis/linux-verify/manual
```

The tool is safe to run on non-Linux hosts; it will report unavailable Linux
interfaces instead of failing destructively.

Hardware-control writes are explicit:

```sh
opena8dj-linuxctl apply-profile traktor-dvs-vinyl --yes
opena8dj-linuxctl apply-profile timecode-vinyl --yes
opena8dj-linuxctl set-control input-mode phono --yes
```

Package installation never runs those commands.

`hardware-model` is static reference data derived from the macOS and Windows
workstreams. It records USB endpoints, A/B/C/D topology, profile semantics,
hot-path rules, and validation policy so Linux diagnostics can be compared
against the same hardware contract.
