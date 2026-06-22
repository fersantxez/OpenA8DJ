# Linux Driver Scaffold

This directory is a placeholder for future Linux driver work.

It intentionally does not contain a buildable kernel module yet. The first
required engineering step is to complete the `snd-usb-caiaq` audit against a
chosen kernel baseline and decide whether the implementation should extend
CAIAQ or create a focused `snd-opena8dj` module.

Current state:

```text
diagnostic only, sound quality not validated
```

Allowed current actions:

- Read this scaffold.
- Run `make help`.
- Add audit notes and design constraints.

Not allowed from this scaffold yet:

- Installing a module.
- Binding to live Audio 8 DJ hardware.
- Loading or unloading kernel modules.
- Running playback/capture tests.
- Claiming enumeration, routing, latency, or sound quality.

Any future hardware-facing target must use:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```
