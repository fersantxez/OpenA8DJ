#!/usr/bin/env python3
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "windows" / "driver" / "OpenA8DJUsb.c"
HEADER = ROOT / "windows" / "include" / "OpenA8DJShared.h"
CTL = ROOT / "windows" / "tools" / "opena8djctl.c"


def read(path):
    return path.read_text(encoding="ascii")


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def require_contains(text, needle, label):
    require(needle in text, f"{label}: missing `{needle}`")


def require_not_contains(text, needle, label):
    require(needle not in text, f"{label}: unexpected `{needle}`")


def extract_function(text, name):
    marker = f"{name}("
    start = text.find(marker)
    require(start >= 0, f"missing function {name}")
    brace = text.find("{", start)
    require(brace >= 0, f"missing body for {name}")
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace:index + 1]
    raise SystemExit(f"FAIL: unterminated function {name}")


def main():
    driver = read(DRIVER)
    header = read(HEADER)
    ctl = read(CTL)

    require_contains(header, "#define OPENA8DJ_STABLE_SAMPLE_RATE_COUNT 2", "stable rate count")

    stable_table = re.search(
        r"kOpenA8DJStableSampleRates\s*\[[^\]]+\]\s*=\s*\{(?P<body>[^}]*)\}",
        driver,
        re.MULTILINE | re.DOTALL,
    )
    require(stable_table is not None, "stable sample-rate table missing")
    stable_body = stable_table.group("body")
    require_contains(stable_body, "44100", "stable sample-rate table")
    require_contains(stable_body, "48000", "stable sample-rate table")
    require_not_contains(stable_body, "88200", "stable sample-rate table")
    require_not_contains(stable_body, "96000", "stable sample-rate table")

    validator = extract_function(driver, "OpenA8DJ_IsSupportedSampleRate")
    require_contains(validator, "OPENA8DJ_STABLE_SAMPLE_RATE_COUNT", "sample-rate validator")
    require_not_contains(validator, "OPENA8DJ_SAMPLE_RATE_COUNT", "sample-rate validator")

    capabilities = extract_function(driver, "OpenA8DJ_FillCapabilities")
    require_contains(capabilities, "Capabilities->ControlsReady = FALSE;", "capabilities truth")
    require_contains(capabilities, "Capabilities->WindowsAudioEndpointExposed = FALSE;", "endpoint truth")
    require_contains(capabilities, "Capabilities->MidiReady = FALSE;", "midi truth")
    require_contains(capabilities, "OPENA8DJ_STABLE_SAMPLE_RATE_COUNT", "capabilities rates")

    surface = extract_function(driver, "OpenA8DJ_FillSurface")
    require_contains(surface, "Surface->ControlState = OPENA8DJ_COMPONENT_STUB;", "surface controls")
    require_contains(surface, "Surface->AudioEndpointState = OPENA8DJ_COMPONENT_PLANNED;", "surface endpoint")
    require_contains(surface, "Surface->IsochronousEngineState = OPENA8DJ_COMPONENT_PLANNED;", "surface stream")
    require_contains(surface, "Surface->MidiState = OPENA8DJ_COMPONENT_PLANNED;", "surface midi")
    require_contains(surface, "Surface->AsioState = OPENA8DJ_COMPONENT_PLANNED;", "surface asio")
    require_not_contains(surface, "OPENA8DJ_SURFACE_FLAG_CONTROLS |", "surface flags")
    require_contains(surface, "controls are local-only diagnostics", "surface safety policy")

    ioctls = extract_function(driver, "OpenA8DJ_EvtIoDeviceControl")
    start_block = ioctls[ioctls.find("IOCTL_OPENA8DJ_START_STREAMING"):]
    start_block = start_block[:start_block.find("} else if", 1)]
    require_contains(start_block, "context->StreamState.Streaming = FALSE;", "start truth")
    require_contains(start_block, "context->StreamState.StreamingEngineReady = FALSE;", "start truth")
    require_contains(start_block, "status = STATUS_NOT_SUPPORTED;", "start truth")

    require_contains(ctl, "set-format 44100|48000 15..4096", "CLI usage")
    usage_tail = ctl[ctl.find("static void Usage"):]
    require_not_contains(usage_tail, "set-format 44100|48000|88200|96000", "CLI usage")
    require_contains(ctl, "controls-hardware:", "CLI control label")

    print("PASS: Windows surface contract is truthful for offline/macOS validation")


if __name__ == "__main__":
    main()
