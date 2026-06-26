#!/usr/bin/env python3
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "windows" / "driver" / "OpenA8DJUsb.c"
HEADER = ROOT / "windows" / "include" / "OpenA8DJShared.h"
CTL = ROOT / "windows" / "tools" / "opena8djctl.c"
INF = ROOT / "windows" / "driver" / "OpenA8DJUsb.inf"
CONTROL_MATRIX = ROOT / "windows" / "tests" / "run-a8dj-control-readback-matrix.ps1"
FULL_SUMMARY = ROOT / "windows" / "tests" / "summarize-a8dj-full-validation.ps1"
FULL_RUNNER = ROOT / "windows" / "tests" / "run-a8dj-full-validation-round.ps1"
TRAKTOR_SMOKE = ROOT / "windows" / "tests" / "run-traktor-active-smoke.ps1"
MIDI_SMOKE = ROOT / "windows" / "tests" / "run-a8dj-midi-endpoint-smoke.ps1"
MIDI_PROBE = ROOT / "windows" / "tests" / "a8dj_midi_endpoint_probe.py"
ASIO_SMOKE = ROOT / "windows" / "tests" / "run-a8dj-asio-endpoint-smoke.ps1"
VERSION_PREFLIGHT = ROOT / "windows" / "tests" / "run-a8dj-version-preflight.ps1"
PAIR_PROBE = ROOT / "windows" / "tests" / "a8dj_pair_matrix_probe.py"
LOAD_CANARY = ROOT / "windows" / "tests" / "run-a8dj-driver-load-canary.ps1"
READONLY_CANARY = ROOT / "windows" / "tests" / "run-a8dj-readonly-control-canary.ps1"


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
    inf = read(INF)
    control_matrix = read(CONTROL_MATRIX)
    full_summary = read(FULL_SUMMARY)
    full_runner = read(FULL_RUNNER)
    traktor_smoke = read(TRAKTOR_SMOKE)
    midi_smoke = read(MIDI_SMOKE)
    midi_probe = read(MIDI_PROBE)
    asio_smoke = read(ASIO_SMOKE)
    version_preflight = read(VERSION_PREFLIGHT)
    pair_probe = read(PAIR_PROBE)
    load_canary = read(LOAD_CANARY)
    readonly_canary = read(READONLY_CANARY)

    require_contains(header, "#define OPENA8DJ_DRIVER_API_VERSION 27", "API 27 diagnostics contract")
    require_contains(header, "#define OPENA8DJ_STABLE_SAMPLE_RATE_COUNT 2", "stable rate count")
    require_contains(header, "ULONG64 StreamWorkerIterations;", "worker diagnostics contract")
    require_contains(header, "ULONG64 StreamWorkerCaptureBytes;", "worker diagnostics contract")
    require_contains(header, "ULONG StreamWorkerLastRenderMask;", "worker diagnostics contract")

    require_contains(inf, "DriverVer=06/25/2026,0.0.134.0", "driver package version")
    require_contains(driver, "#define OPENA8DJ_ENABLE_ASYNC_OUTPUT 0", "async output disabled")
    require_contains(driver, "#if OPENA8DJ_ENABLE_ASYNC_OUTPUT\ntypedef struct _OPENA8DJ_ASYNC_ISO_OUTPUT_SLOT", "async slot compile gate")
    require_contains(driver, "#if OPENA8DJ_ENABLE_ASYNC_OUTPUT\nstatic VOID\nNTAPI\nOpenA8DJ_EvtIsoOutputRequestComplete", "async completion compile gate")
    require_contains(driver, "#if OPENA8DJ_ENABLE_ASYNC_OUTPUT\nstatic NTSTATUS\nOpenA8DJ_QueueAsyncIsoOutputBuffer", "async queue compile gate")
    require_contains(driver, "UCHAR *outputBuffer = playbackBuffer;", "sync output buffer path")
    require_not_contains(driver, "useAsyncOutput ? outputSlots[nextOutputSlot].Buffer : playbackBuffer", "async ternary leakage")
    require_contains(inf, "AddInterface=%KSCATEGORY_REALTIME%,%KSNAME_OpenA8DJRenderA%", "render realtime interface")
    for capture_name in (
        "KSNAME_OpenA8DJCaptureA",
        "KSNAME_OpenA8DJCaptureB",
        "KSNAME_OpenA8DJCaptureC",
        "KSNAME_OpenA8DJCaptureD",
    ):
        require_not_contains(
            inf,
            f"AddInterface=%KSCATEGORY_REALTIME%,%{capture_name}%",
            "capture WDM-KS compatibility",
        )

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

    profiles = extract_function(driver, "OpenA8DJ_ApplyProfile")
    require_contains(profiles, "state.InputMode = 0;", "vinyl profile")
    require_contains(profiles, "state.InputMode = 1;", "cd-line profile")
    require_contains(profiles, "state.InputMode = 2;", "phono profile")
    require_contains(profiles, "state.SoftwareLock = 1;", "locked input profiles")
    require_contains(profiles, "state.SoftwareLock = 0;", "unlock profile")

    capabilities = extract_function(driver, "OpenA8DJ_FillCapabilities")
    require_contains(capabilities, "Capabilities->ControlsReady = Context->ControlsHardwareReady;", "capabilities truth")
    require_contains(capabilities, "Capabilities->WindowsAudioEndpointExposed = TRUE;", "endpoint truth")
    require_contains(capabilities, "Capabilities->MidiReady = FALSE;", "midi truth")
    require_contains(capabilities, "OPENA8DJ_STABLE_SAMPLE_RATE_COUNT", "capabilities rates")

    surface = extract_function(driver, "OpenA8DJ_FillSurface")
    require_contains(surface, "Surface->ControlState = Context->ControlsHardwareReady", "surface controls")
    require_contains(surface, "OPENA8DJ_COMPONENT_READY", "surface controls")
    require_contains(surface, "OPENA8DJ_COMPONENT_STUB", "surface controls")
    require_contains(surface, "Surface->AudioEndpointState = OPENA8DJ_COMPONENT_READY;", "surface endpoint")
    require_contains(surface, "OPENA8DJ_ENDPOINT_MODEL_PRIMARY_8CH_PLUS_STEREO", "surface endpoint model")
    require_contains(surface, "Context->StreamWorkItem != NULL", "surface stream")
    require_contains(surface, "OPENA8DJ_COMPONENT_READY", "surface stream")
    require_contains(surface, "Surface->MidiState = OPENA8DJ_COMPONENT_PLANNED;", "surface midi")
    require_contains(surface, "Surface->AsioState = OPENA8DJ_COMPONENT_PLANNED;", "surface asio")
    require_contains(surface, "OPENA8DJ_SURFACE_FLAG_CONTROLS", "surface flags")
    require_contains(surface, "falls back to silence", "surface safety policy")

    ioctls = extract_function(driver, "OpenA8DJ_EvtIoDeviceControl")
    start_block = ioctls[ioctls.find("IOCTL_OPENA8DJ_START_STREAMING"):]
    start_block = start_block[:start_block.find("IOCTL_OPENA8DJ_STOP_STREAMING")]
    require(
        re.search(r"WdfWorkItemEnqueue\s*\(\s*context->StreamWorkItem\s*\)\s*;", start_block) is not None,
        "start stream worker: missing WdfWorkItemEnqueue(context->StreamWorkItem)",
    )
    require_contains(start_block, "context->StreamState.Streaming = TRUE;", "start truth")
    require_contains(start_block, "context->StreamState.StreamingEngineReady = TRUE;", "start truth")

    require_contains(ctl, "set-format 44100|48000 15..4096", "CLI usage")
    usage_tail = ctl[ctl.find("static void Usage"):]
    require_not_contains(usage_tail, "set-format 44100|48000|88200|96000", "CLI usage")
    require_contains(ctl, "controls-hardware:", "CLI control label")
    require_contains(ctl, "hardware-midi:", "CLI MIDI hardware label")
    require_contains(ctl, "caps|capabilities", "CLI capabilities alias")
    require_contains(ctl, 'strcmp(argv[1], "capabilities") == 0', "CLI capabilities alias")
    require_contains(ctl, "ctl-raw:", "CLI raw controls")
    require_contains(ctl, "ctl-rdbk-mismatch:", "CLI control write readback")
    require_contains(ctl, "worker-iterations:", "CLI worker diagnostics")
    require_contains(ctl, "worker-render-mask:", "CLI worker diagnostics")
    require_contains(control_matrix, "ExpectedUnsupportedPhonoGround", "control matrix phono-ground classification")
    require_contains(control_matrix, "UnexpectedMismatch", "control matrix mismatch classification")
    require_contains(full_runner, "midi-endpoint-smoke", "full validation MIDI smoke")
    require_contains(full_runner, "asio-endpoint-smoke", "full validation ASIO smoke")
    require_contains(full_runner, "version-preflight", "full validation version preflight")
    require_contains(full_runner, "Invoke-Cooldown", "full validation hardware cooldowns")
    require_contains(full_runner, "cooldowns.log", "full validation hardware cooldown evidence")
    require_contains(full_runner, "TraktorTrimWheelNotches", "full validation Traktor trim passthrough")
    require_contains(full_runner, "TraktorTrackDirectory", "full validation Traktor music directory passthrough")
    require_contains(traktor_smoke, "top-process-samples.json", "Traktor CPU attribution artifact")
    require_contains(traktor_smoke, "Get-TopProcessCpuDelta", "Traktor CPU attribution")
    require_contains(traktor_smoke, "top_cpu_processes", "Traktor CPU attribution summary")
    require_contains(traktor_smoke, "PerformanceCounter", "Traktor low-overhead CPU sampler")
    require_contains(traktor_smoke, "IrigCpuSampleIntervalSeconds", "Traktor iRig CPU sample throttle")
    require_contains(traktor_smoke, "capture_process_priority", "Traktor iRig capture priority evidence")
    require_contains(traktor_smoke, "system_cpu_source", "Traktor CPU sampler source evidence")
    require_contains(traktor_smoke, "TrackDirectory", "Traktor music directory default")
    require_contains(traktor_smoke, "000_santxez_spring_25_select", "Traktor Sanchez music folder default")
    require_contains(traktor_smoke, "PnpMonitorIntervalSeconds", "Traktor PnP monitor throttle")
    require_contains(traktor_smoke, "TraktorTrimWheelNotches", "Traktor capture-level trim evidence")
    require_contains(traktor_smoke, "traktor-playback-gesture.json", "Traktor playback proof artifact")
    require_contains(traktor_smoke, "browser-compact-all-tracks-row1", "Traktor browser load fallback")
    require_contains(traktor_smoke, "playback_gesture_active", "Traktor playback proof summary")
    require_contains(version_preflight, "source_api_version", "version preflight source API")
    require_contains(version_preflight, "loaded_api_version", "version preflight loaded API")
    require_contains(version_preflight, "loaded_matches_source_api", "version preflight API comparison")
    require_contains(midi_smoke, "does_not_open_midi_devices=1", "MIDI smoke safety")
    require_contains(midi_probe, "midiInGetNumDevs", "MIDI winmm probe")
    require_contains(midi_probe, "midiOutGetNumDevs", "MIDI winmm probe")
    require_contains(asio_smoke, "does_not_create_com_objects=1", "ASIO smoke safety")
    require_contains(asio_smoke, "HKLM:\\SOFTWARE\\ASIO", "ASIO registry probe")
    require_contains(asio_smoke, "opena8dj_asio_ready", "ASIO readiness metric")
    require_contains(pair_probe, "run_case_with_status_retries", "pair matrix PortAudio status retry")
    require_contains(pair_probe, "--status-retries", "pair matrix PortAudio status retry option")
    require_contains(pair_probe, "status_retry_attempts", "pair matrix PortAudio retry evidence")
    require_contains(pair_probe, "--cpu-sample-interval", "pair matrix CPU sample throttle")
    require_contains(pair_probe, "cpu_sample_interval_seconds", "pair matrix CPU sample interval evidence")
    require_contains(load_canary, "driver_load_canary_only", "load canary safety policy")
    require_contains(load_canary, "does_not_open_audio_endpoints=1", "load canary safety policy")
    require_contains(load_canary, "does_not_start_streaming=1", "load canary safety policy")
    require_contains(load_canary, "does_not_run_opena8djctl=1", "load canary safety policy")
    require_contains(load_canary, "Refusing canary for DriverVer", "load canary version gate")
    require_contains(load_canary, "bugcheck_events_since_start", "load canary bugcheck evidence")
    require_contains(readonly_canary, "readonly_control_canary_only", "read-only canary safety policy")
    require_contains(readonly_canary, "does_not_open_audio_endpoints=1", "read-only canary safety policy")
    require_contains(readonly_canary, "does_not_start_streaming=1", "read-only canary safety policy")
    require_contains(readonly_canary, 'Invoke-CtlReadOnly -Command "surface"', "read-only canary commands")
    require_contains(readonly_canary, 'Invoke-CtlReadOnly -Command "capabilities"', "read-only canary commands")
    require_contains(readonly_canary, 'Invoke-CtlReadOnly -Command "stream"', "read-only canary commands")
    require_contains(readonly_canary, 'Invoke-CtlReadOnly -Command "diagnostics"', "read-only canary commands")
    require_contains(readonly_canary, "Worker iterations changed during read-only canary", "read-only canary no streaming gate")
    require_contains(full_runner, "CooldownSeconds", "full validation cooldown option")
    require_contains(full_summary, "control_expected_unsupported_phono_ground", "candidate summary control metrics")
    require_contains(full_summary, "control_unexpected_mismatches", "candidate summary control metrics")
    require_contains(full_summary, "midi_matching_input_count", "candidate summary MIDI metrics")
    require_contains(full_summary, "asio_matching_opena8dj_count", "candidate summary ASIO metrics")
    require_contains(full_summary, "loaded_matches_source_api", "candidate summary version metrics")
    require_contains(full_summary, "traktor_top_cpu_process_names", "candidate summary Traktor CPU attribution")
    require_contains(full_summary, "traktor_playback_gesture_active", "candidate summary Traktor playback proof")

    diagnostics = extract_function(driver, "OpenA8DJ_FillDiagnostics")
    require_contains(diagnostics, "Diagnostics->StreamWorkerIterations", "driver worker diagnostics")
    require_contains(diagnostics, "Diagnostics->StreamWorkerCaptureBytes", "driver worker diagnostics")
    require_contains(diagnostics, "Diagnostics->StreamWorkerLastRenderMask", "driver worker diagnostics")

    worker = extract_function(driver, "OpenA8DJ_EvtStreamWorkItem")
    require_contains(worker, "InterlockedIncrement64(&context->StreamWorkerIterations);", "worker iteration counter")
    require_contains(worker, "InterlockedAdd64(&context->StreamWorkerCaptureBytes", "worker capture bytes counter")
    require_contains(worker, "context->StreamWorkerLastRenderMask = activeRenderMask;", "worker active render mask")
    require_contains(worker, "WDFMEMORY outputUrbMemory = NULL;", "stream output URB reuse")
    require_contains(worker, "OpenA8DJ_SendIsoOutputBufferPrepared(", "stream output URB reuse")
    require_not_contains(worker, "OpenA8DJ_SendIsoOutputBuffer(\n                context,", "stream output URB reuse")
    require_contains(worker, "if (context->IsoOutPipe == NULL)", "worker output pipe readiness")

    print("PASS: Windows surface contract is truthful for offline/macOS validation")


if __name__ == "__main__":
    main()
