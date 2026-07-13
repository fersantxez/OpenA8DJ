#!/usr/bin/env python3
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "windows" / "driver" / "OpenA8DJUsb.c"
DRIVER_HEADER = ROOT / "windows" / "driver" / "OpenA8DJUsb.h"
HEADER = ROOT / "windows" / "include" / "OpenA8DJShared.h"
CTL = ROOT / "windows" / "tools" / "opena8djctl.c"
INF = ROOT / "windows" / "driver" / "OpenA8DJUsb.inf"
PROJECT = ROOT / "windows" / "driver" / "OpenA8DJUsb.vcxproj"
CONTROL_MATRIX = ROOT / "windows" / "tests" / "run-a8dj-control-readback-matrix.ps1"
FULL_SUMMARY = ROOT / "windows" / "tests" / "summarize-a8dj-full-validation.ps1"
FULL_RUNNER = ROOT / "windows" / "tests" / "run-a8dj-full-validation-round.ps1"
BUILD_DRIVER = ROOT / "windows" / "scripts" / "build-driver.ps1"
BUILD_VIRTUAL_ACX = ROOT / "windows" / "scripts" / "build-virtual-acx.ps1"
WINDOWS_WORKFLOW = ROOT / ".github" / "workflows" / "windows-driver.yml"
VIRTUAL_PROJECT = ROOT / "windows" / "driver" / "OpenA8DJVirtual.vcxproj"
VIRTUAL_SOLUTION = ROOT / "windows" / "OpenA8DJVirtual.sln"
VIRTUAL_INF = ROOT / "windows" / "driver" / "OpenA8DJVirtual.inf"
VIRTUAL_CANARY = ROOT / "windows" / "tests" / "run-open-a8dj-virtual-endpoint-canary.ps1"
VIRTUAL_OUTPUT_CANARY = ROOT / "windows" / "tests" / "run-open-a8dj-virtual-output-canary.ps1"
VIRTUAL_PROBE = ROOT / "windows" / "tests" / "run-open-a8dj-virtual-endpoint-probe.ps1"
VIRTUAL_RECOVERY = ROOT / "windows" / "tests" / "analyze-open-a8dj-virtual-canary-recovery.ps1"
TRAKTOR_SMOKE = ROOT / "windows" / "tests" / "run-traktor-active-smoke.ps1"
MIDI_SMOKE = ROOT / "windows" / "tests" / "run-a8dj-midi-endpoint-smoke.ps1"
MIDI_PROBE = ROOT / "windows" / "tests" / "a8dj_midi_endpoint_probe.py"
ASIO_SMOKE = ROOT / "windows" / "tests" / "run-a8dj-asio-endpoint-smoke.ps1"
VERSION_PREFLIGHT = ROOT / "windows" / "tests" / "run-a8dj-version-preflight.ps1"
PAIR_PROBE = ROOT / "windows" / "tests" / "a8dj_pair_matrix_probe.py"
LOAD_CANARY = ROOT / "windows" / "tests" / "run-a8dj-driver-load-canary.ps1"
INPUT_ENDPOINT_PROBE = ROOT / "windows" / "tests" / "a8dj_input_endpoint_probe.py"
OUTPUT_ENDPOINT_PROBE = ROOT / "windows" / "tests" / "a8dj_output_endpoint_probe.py"
PHYSICAL_RECOVERY = ROOT / "windows" / "tests" / "analyze-a8dj-driver-load-canary-recovery.ps1"
READONLY_CANARY = ROOT / "windows" / "tests" / "run-a8dj-readonly-control-canary.ps1"
RECOVERY_ACTION = ROOT / "windows" / "tests" / "recover-a8dj-physical-canary.ps1"
WINDOWS_COMMON = ROOT / "windows" / "scripts" / "OpenA8DJ.WindowsCommon.psm1"
INSTALL_DRIVER = ROOT / "windows" / "scripts" / "install-driver.ps1"
PACKAGE_INSTALLER = ROOT / "windows" / "scripts" / "package-installer.ps1"


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
    while start >= 0:
        brace = text.find("{", start)
        semicolon = text.find(";", start)
        if brace >= 0 and (semicolon < 0 or brace < semicolon):
            break
        start = text.find(marker, semicolon + 1)
    require(start >= 0, f"missing function body {name}")
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
    driver_header = read(DRIVER_HEADER)
    header = read(HEADER)
    ctl = read(CTL)
    inf = read(INF)
    project = read(PROJECT)
    control_matrix = read(CONTROL_MATRIX)
    full_summary = read(FULL_SUMMARY)
    full_runner = read(FULL_RUNNER)
    build_driver = read(BUILD_DRIVER)
    windows_workflow = read(WINDOWS_WORKFLOW)
    traktor_smoke = read(TRAKTOR_SMOKE)
    midi_smoke = read(MIDI_SMOKE)
    midi_probe = read(MIDI_PROBE)
    asio_smoke = read(ASIO_SMOKE)
    version_preflight = read(VERSION_PREFLIGHT)
    pair_probe = read(PAIR_PROBE)
    load_canary = read(LOAD_CANARY)
    input_endpoint_probe = read(INPUT_ENDPOINT_PROBE)
    output_endpoint_probe = read(OUTPUT_ENDPOINT_PROBE)
    physical_recovery = read(PHYSICAL_RECOVERY)
    readonly_canary = read(READONLY_CANARY)
    recovery_action = read(RECOVERY_ACTION)
    windows_common = read(WINDOWS_COMMON)
    install_driver = read(INSTALL_DRIVER)
    package_installer = read(PACKAGE_INSTALLER)
    build_virtual_acx = read(BUILD_VIRTUAL_ACX)
    virtual_project = read(VIRTUAL_PROJECT)
    virtual_solution = read(VIRTUAL_SOLUTION)
    virtual_inf = read(VIRTUAL_INF)
    virtual_canary = read(VIRTUAL_CANARY)
    virtual_output_canary = read(VIRTUAL_OUTPUT_CANARY)
    virtual_probe = read(VIRTUAL_PROBE)
    virtual_recovery = read(VIRTUAL_RECOVERY)

    require_contains(header, "#define OPENA8DJ_DRIVER_API_VERSION 44", "API 44 safety contract")
    require_contains(header, "IOCTL_OPENA8DJ_ARM_PHYSICAL_CANARY", "one-shot physical authorization")
    require_contains(header, "OPENA8DJ_SAFETY_STATE", "driver safety state contract")
    require_contains(header, "#define OPENA8DJ_STABLE_SAMPLE_RATE_COUNT 2", "stable rate count")
    require_contains(header, "ULONG64 StreamWorkerIterations;", "worker diagnostics contract")
    require_contains(header, "ULONG64 StreamWorkerCaptureBytes;", "worker diagnostics contract")
    require_contains(header, "ULONG StreamWorkerLastRenderMask;", "worker diagnostics contract")

    require_contains(inf, "DriverVer=07/13/2026,0.0.183.0", "driver package version")

    control_store = extract_function(driver, "OpenA8DJ_StoreControlState")
    require_contains(control_store, "OPENA8DJ_CONTROL_FLAGS_MASK", "ground-lift hardware readback guard")
    require_contains(control_store, "Context->LastControlWriteStatus = STATUS_NOT_SUPPORTED", "truthful unsupported ground write status")
    require_contains(control_store, "return STATUS_NOT_SUPPORTED", "ground write rejected before USB")
    require_contains(ctl, "Ground-lift controls are hardware readback-only", "truthful ground control CLI")
    require_contains(ctl, "(DWORD)ERROR_NOT_SUPPORTED", "ground control CLI error contract")

    apply_profile = extract_function(driver, "OpenA8DJ_ApplyProfile")
    require_contains(apply_profile, "OpenA8DJ_RefreshHardwareControlState(Context)", "profile refreshes hardware readback")
    require(
        apply_profile.index("OpenA8DJ_RefreshHardwareControlState(Context)")
        < apply_profile.index("OpenA8DJ_LoadControlState(Context, &state)"),
        "profile refresh before mutation",
    )

    bulk_command = extract_function(driver, "OpenA8DJ_SendBulkCommand")
    require_contains(driver, "#define OPENA8DJ_BULK_WRITE_RETRY_LIMIT 3u", "bounded bulk retry limit")
    require_contains(driver, "#define OPENA8DJ_BULK_WRITE_RETRY_DELAY_MS 100u", "bounded bulk retry delay")
    require_contains(bulk_command, "OpenA8DJ_IsIdempotentBulkCommand(Command)", "idempotent bulk retry allowlist")
    require_contains(bulk_command, "KeDelayExecutionThread(KernelMode, FALSE, &retryDelay)", "passive bulk retry backoff")
    require_contains(bulk_command, "Context->DeviceStopping", "bulk retry teardown abort")
    require_contains(bulk_command, "bulkOutPipe", "stable bulk pipe handle")

    release_hardware = extract_function(driver, "OpenA8DJ_EvtDeviceReleaseHardware")
    require_contains(release_hardware, "WdfWaitLockAcquire(context->BulkCommandLock, NULL)", "release drains bulk command lock")
    require_contains(release_hardware, "OpenA8DJ_StopPipeTarget(context->BulkInPipe)", "release stops bulk input under lock")
    require_contains(header, "#define OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT 8", "high-speed ISO packet group")
    require_contains(inf, "StartType=3", "PnP demand-start service")
    require_contains(driver, "#define OPENA8DJ_ENABLE_ASYNC_OUTPUT 0", "async output disabled")
    require_contains(driver, "#define OPENA8DJ_ENABLE_PERSISTENT_ASYNC_ISO 1", "persistent async engine enabled")
    require_contains(driver_header, "#define OPENA8DJ_ISO_ENGINE_SLOT_COUNT 8", "persistent async slot count")
    require_contains(driver, "#define OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT 64u", "Mac-aligned persistent isochronous transfer depth")
    require_contains(driver, "#define OPENA8DJ_ISO_OUTPUT_LEAD_FRAMES 8u", "bounded explicit output lead")
    require_contains(driver, "#define OPENA8DJ_ISO_CAPTURE_START_LEAD_FRAMES 16u", "bounded explicit capture start lead")
    require_contains(header, "ULONG64 IsoCaptureLatePackets;", "capture late-packet diagnostics contract")
    require_contains(driver, "OPENA8DJ_HW_LATENCY_MILLISECONDS", "truthful scheduled output latency")
    latency_callback = extract_function(driver, "OpenA8DJ_EvtAcxStreamGetHwLatency")
    require_contains(latency_callback, "*FifoSize = 0", "scheduled lead is not misreported as hardware FIFO storage")
    require_contains(latency_callback, "*Delay = streamContext->IsRender", "scheduled lead is reported only for render")
    require_contains(latency_callback, "? OPENA8DJ_HW_LATENCY_MILLISECONDS * 10000u", "render lead is reported as presentation delay")
    require_contains(latency_callback, ": 0u", "capture does not report the render-only presentation lead")
    ensure_transport = extract_function(driver, "OpenA8DJ_EnsureIsoTransportResources")
    require_contains(
        ensure_transport,
        "WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED",
        "persistent ISO rejects non-high-speed USB geometry",
    )
    require_contains(driver_header, "LONG64 CompletionQpc;", "persistent completion timestamp")
    require_contains(header, "ULONG64 IsoOutputLatePackets;", "late output diagnostics contract")
    require_contains(header, "ULONG64 IsoOutputPanicFlags;", "hardware output-panic diagnostics contract")
    require_contains(header, "ULONG64 AudioRateSettleMismatchedPackets;", "sample-rate transition diagnostics contract")
    require_contains(driver_header, "KEVENT IsoEngineDrainedEvent;", "persistent async drain barrier")
    require_contains(driver, "OpenA8DJ_PreparePersistentOutputSlotFromCapture", "capture-paced persistent output")
    prepare_hardware = extract_function(driver, "OpenA8DJ_EvtDevicePrepareHardware")
    physical_prepare = prepare_hardware.split("OpenA8DJ_MapConfiguredPipes(Device)", 1)[1]
    require(
        physical_prepare.index("InterlockedExchange(&context->DevicePrepared, 1)")
        < physical_prepare.index("OpenA8DJ_EnsureIsoTransportResources(context)"),
        "PrepareHardware must publish mapped transport readiness before guarded ISO allocation",
    )
    require_contains(prepare_hardware, "InterlockedExchange(&context->DevicePrepared, 0)", "failed prepare revokes readiness")
    rate_settle = extract_function(driver, "OpenA8DJ_WaitForAudioRateSettle")
    require_contains(rate_settle, "OPENA8DJ_AUDIO_RATE_SETTLE_MAX_SNAPSHOTS", "bounded persistent rate settle")
    require_contains(rate_settle, "OPENA8DJ_AUDIO_RATE_SETTLE_BUDGET_MS", "wall-clock bounded persistent rate settle")
    require_contains(rate_settle, "KeQueryPerformanceCounter", "rate settle wall-clock budget")
    require_contains(rate_settle, "Context->DeviceStopping", "rate settle aborts during PnP teardown")
    require_contains(rate_settle, "Context->DevicePrepared", "rate settle requires prepared hardware")
    require_contains(rate_settle, "AudioRateSettleActive, 1", "persistent rate settle published after initialization")
    normalize_capture = extract_function(driver, "OpenA8DJ_NormalizePersistentCaptureSlot")
    require_contains(normalize_capture, "if (length != 0 && length != configuredPacketBytes)", "old-rate packets always discarded")
    require_contains(normalize_capture, "OPENA8DJ_AUDIO_RATE_SETTLE_REQUIRED_CONSECUTIVE", "consecutive persistent rate settle proof")
    require_contains(normalize_capture, "AudioRateSettleAttemptsRemaining", "completion-count bounded persistent rate settle")
    require_contains(normalize_capture, "STATUS_IO_TIMEOUT", "persistent rate settle fails closed")
    require_contains(normalize_capture, "Never decode a transfer containing mixed old/new geometry", "mixed-rate transfer discarded as a unit")
    require(
        normalize_capture.index("attemptsRemaining <= 0")
        < normalize_capture.index("AudioRateSettleConsecutiveClean) >="),
        "rate-settle deadline must win over a late clean completion",
    )
    configure_audio = extract_function(driver, "OpenA8DJ_ConfigureAudioParams")
    require_contains(configure_audio, "OpenA8DJ_WaitForAudioRateSettle(Context, bytesPerPacket)", "stream waits for physical rate settle")
    require_contains(driver, "IsoPacket[packetIndex].Length = 0;", "isochronous IN lengths initialized as host-controller output fields")
    require_contains(driver, "OpenA8DJ_NormalizePersistentCaptureSlot", "capture descriptors normalized before decode")
    require_contains(driver, "capturePipeInfo.MaximumPacketSize != packetStride", "capture packet stride bounded by endpoint maximum")
    require_contains(driver, "transferSucceeded && captureHasPayload", "empty capture completion skips USB output")
    require_contains(driver, "else if (Slot->Direction == OpenA8DJIsoDirectionOutput)", "empty capture completion does not count phantom output")
    require_contains(driver, "OPENA8DJ_STREAM_NO_DATA_COMPLETION_LIMIT", "bounded consecutive empty capture completions")
    require_contains(
        driver,
        "CaptureSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length",
        "capture-paced packet lengths",
    )
    paired_output = extract_function(driver, "OpenA8DJ_PreparePersistentOutputSlotFromCapture")
    require_contains(paired_output, "offset > CaptureSlot->TransferLength", "capture-paced offset bound")
    require_contains(paired_output, "length > CaptureSlot->TransferLength - offset", "capture-paced length bound")
    require_contains(paired_output, "length > outputPipeInfo.MaximumPacketSize", "capture-paced endpoint bound")
    require_contains(paired_output, "OPENA8DJ_PERSISTENT_ISO_TRANSFER_FRAMES", "output scheduled after source capture duration")
    require_contains(paired_output, "OPENA8DJ_ISO_OUTPUT_LEAD_FRAMES", "output scheduled with bounded lead")
    require_not_contains(paired_output, "USBD_START_ISO_TRANSFER_ASAP", "output uses explicit frame scheduling")
    persistent_capture = extract_function(driver, "OpenA8DJ_PreparePersistentCaptureSlot")
    require_contains(persistent_capture, "USBD_START_ISO_TRANSFER_ASAP", "persistent capture uses native USB stack cadence")
    require_contains(
        persistent_capture,
        "Slot->Urb->UrbIsochronousTransfer.StartFrame = 0",
        "ASAP capture leaves start-frame selection to the USB stack",
    )
    purge_wrapper = extract_function(driver, "OpenA8DJ_PurgeIsoTargets")
    purge_iso = extract_function(driver, "OpenA8DJ_PurgeIsoTargetsLocked")
    require_contains(purge_wrapper, "WdfWaitLockAcquire(Context->IsoPurgeLock", "purge lifecycle serialization")
    require_contains(purge_wrapper, "Context->DeviceStopping", "teardown blocks stale restart requests")
    require_contains(purge_wrapper, "Context->DevicePrepared", "restart requires prepared hardware")
    require_contains(purge_iso, "WdfIoTargetPurgeIoAndWait", "pipe reset follows a synchronous drain")
    require_contains(purge_iso, "WdfUsbTargetPipeResetSynchronously", "stale ASAP tracking is reset between generations")
    require_contains(purge_iso, "WDF_REQUEST_SEND_OPTION_IGNORE_TARGET_STATE", "reset may run against the deliberately purged target")
    require_contains(purge_iso, "WDF_REL_TIMEOUT_IN_SEC(1)", "pipe reset is time bounded")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_INPUT_RESET_START", "pipe reset start checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_INPUT_RESET_COMPLETE", "pipe reset completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RESET_START", "output pipe reset start checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RESET_COMPLETE", "output pipe reset completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_INPUT_START_ENTER", "input target start enter checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_INPUT_START_COMPLETE", "input target start completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_START_ENTER", "output target start enter checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_START_COMPLETE", "output target start completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_REPURGE_INPUT_ENTER", "input repurge enter checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_REPURGE_INPUT_COMPLETE", "input repurge completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_REPURGE_OUTPUT_ENTER", "output repurge enter checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_REPURGE_OUTPUT_COMPLETE", "output repurge completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_PURGE_INPUT_ENTER", "initial input purge enter checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_PURGE_INPUT_COMPLETE", "initial input purge completion checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_PURGE_OUTPUT_ENTER", "initial output purge enter checkpoint")
    require_contains(purge_iso, "OPENA8DJ_CHECKPOINT_ISO_PURGE_OUTPUT_COMPLETE", "initial output purge completion checkpoint")
    require(
        purge_iso.index("WdfIoTargetPurgeIoAndWait")
        < purge_iso.index("WdfUsbTargetPipeResetSynchronously")
        < purge_iso.index("WdfIoTargetStart(inputTarget)"),
        "input pipe must be drained, reset, then restarted",
    )
    require_contains(purge_iso, "NT_SUCCESS(inputResetStatus)", "failed reset prevents target restart")
    require_contains(purge_iso, "inputStartStatus = WdfIoTargetStart(inputTarget)", "input target restart status is captured")
    require_contains(purge_iso, "outputStartStatus = WdfIoTargetStart(outputTarget)", "output target restart status is captured")
    require_contains(purge_iso, "IsoTransportHealthyGeneration", "successful restart publishes its exact generation")
    require_contains(purge_iso, "A partial restart is never exposed to the next stream", "partial target restart is purged closed")
    persistent_start = extract_function(driver, "OpenA8DJ_StartPersistentIsoEngine")
    require_contains(persistent_start, "IsoTransportHealthy", "stream start requires a healthy transport latch")
    require_contains(persistent_start, "IsoTransportHealthyGeneration", "stream start requires the current healthy generation")
    require_contains(persistent_start, "IsoOneShotActive", "stream start excludes one-shot USB producers")
    require_contains(persistent_start, "WdfIoTargetGetState", "stream start verifies both target states")
    require_contains(persistent_start, "WdfUsbTargetDeviceRetrieveCurrentFrameNumber", "stream start seeds capture schedule from current USB frame")
    require_contains(persistent_start, "OPENA8DJ_CHECKPOINT_ISO_FRAME_QUERY_START", "frame query start checkpoint")
    require_contains(persistent_start, "OPENA8DJ_CHECKPOINT_ISO_FRAME_QUERY_COMPLETE", "frame query completion checkpoint")
    require_contains(
        persistent_start,
        "currentFrameNumber + OPENA8DJ_ISO_CAPTURE_START_LEAD_FRAMES",
        "capture seed is exactly current USB frame plus bounded start lead",
    )
    require(
        persistent_start.index("WdfUsbTargetDeviceRetrieveCurrentFrameNumber")
        < persistent_start.index("currentFrameNumber + OPENA8DJ_ISO_CAPTURE_START_LEAD_FRAMES")
        < persistent_start.index("OpenA8DJ_PreparePersistentCaptureSlot")
        < persistent_start.index("OpenA8DJ_SubmitPersistentIsoSlot"),
        "query, seed, prepare, and initial submit order must be deterministic",
    )
    require(
        persistent_start.index("if (!NT_SUCCESS(status))")
        < persistent_start.index("IsoEngineRunning, 1"),
        "frame-query failure must abort before publishing the engine running",
    )
    require_not_contains(
        persistent_start,
        "OPENA8DJ_CHECKPOINT_STREAM_WORKER_ENTER",
        "persistent start must not perform durable worker-enter logging after frame query",
    )
    safety_checkpoint = extract_function(driver, "OpenA8DJ_RecordSafetyCheckpoint")
    require_not_contains(
        safety_checkpoint,
        "Checkpoint == OPENA8DJ_CHECKPOINT_ISO_FRAME_QUERY_COMPLETE",
        "frame-query completion is durably committed at PASSIVE_LEVEL",
    )
    require(
        safety_checkpoint.index("if (realtimeOnly || sampledHotPath)")
        < safety_checkpoint.index("KdPrintEx"),
        "realtime checkpoints must return before KdPrint and registry I/O",
    )
    capture_processing = extract_function(driver, "OpenA8DJ_ProcessPersistentCaptureSlot")
    require_contains(
        capture_processing,
        "for (checkOffset = 0u; checkOffset + 3u < length; checkOffset += 16u)",
        "Mode2 output-panic flags use status bytes 0..3",
    )
    require_not_contains(
        capture_processing,
        "for (checkOffset = 8u;",
        "Mode2 audio bytes are not misclassified as output-panic flags",
    )
    output_completion = extract_function(driver, "OpenA8DJ_ProcessPersistentIsoSlot")
    require_contains(
        output_completion,
        "if (Slot->Direction == OpenA8DJIsoDirectionOutput)",
        "output USBD statuses classified even when ErrorCount is nonzero",
    )
    require(
        output_completion.count(
            "Slot->UrbStatus != USBD_STATUS_SUCCESS &&\n            packetErrorCount == 0"
        ) == 2,
        "URB header errors must be deduplicated from packet errors in both directions",
    )
    require_contains(output_completion, "IsoCaptureErrorSnapshotSequence", "capture error snapshot writer seqlock")
    fill_diagnostics = extract_function(driver, "OpenA8DJ_FillDiagnostics")
    require_contains(fill_diagnostics, "errorSequenceBefore == errorSequenceAfter", "diagnostics snapshot seqlock validation")
    require_contains(driver, "Context->IsoLastCaptureErrorSlot = MAXULONG", "capture error slot sentinel")
    require_contains(driver, "Context->IsoLastCaptureErrorFirstPacket = MAXULONG", "capture error packet sentinel")
    require_not_contains(
        output_completion,
        "if (transferSucceeded && Slot->Direction == OpenA8DJIsoDirectionOutput)",
        "output status classification is not gated by aggregate success",
    )
    require_contains(
        output_completion,
        "BOOLEAN expectedTeardown =",
        "expected teardown is classified before USB completion errors",
    )
    require_contains(
        output_completion,
        "} else if (!expectedTeardown) {",
        "expected teardown does not contaminate active USB error counters",
    )
    completion = extract_function(driver, "OpenA8DJ_EvtPersistentIsoComplete")
    require_contains(completion, "remaining == 0", "queue-empty transition uses atomic decrement result")
    require_contains(
        completion,
        "InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) == 0",
        "queue-empty transition excludes expected stream teardown",
    )
    require_contains(
        completion,
        "InterlockedCompareExchange(&context->DeviceStopping, 0, 0) == 0",
        "queue-empty transition excludes expected PnP teardown",
    )
    require_not_contains(
        completion,
        "InterlockedCompareExchange(&context->IsoOutstandingOutput, 0, 0) == 0",
        "queue-empty transition avoids racy global reread",
    )
    require_contains(
        output_completion,
        "if (InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0 ||",
        "capture descriptors cancelled during teardown are not reported as live USB faults",
    )
    require_contains(paired_output, "length > configuredPacketBytes", "capture-paced configured BPP bound")
    require_not_contains(paired_output, "length == 0", "capture-paced individual zero packet support")
    require_contains(
        paired_output,
        "(length % OPENA8DJ_USB_OUTPUT_FRAME_BYTES) != 0",
        "capture-paced frame alignment",
    )
    require_contains(paired_output, "length > MAXULONG - transferLength", "capture-paced sum overflow")
    find_completed = extract_function(driver, "OpenA8DJ_FindCompletedPersistentIsoSlot")
    require_contains(
        find_completed,
        "OpenA8DJIsoSlotProcessing,\n                    OpenA8DJIsoSlotIdle",
        "capture-paced output reservation",
    )
    require_contains(
        driver,
        "OpenA8DJIsoDirectionOutput);\n        OpenA8DJ_ProcessPersistentIsoDirection(\n            context,\n            OpenA8DJIsoDirectionCapture);",
        "output completion reclaimed before paired capture",
    )
    require_not_contains(
        driver,
        "OpenA8DJ_PreparePersistentOutputSlot(outputSlot, OutputPacketBytes)",
        "fixed maximum-size persistent output",
    )
    require_contains(driver, "if ((sequence & 0x3ffu) != 0)", "steady-state IFR sampling")
    require_contains(driver, "if (realtimeOnly || sampledHotPath)", "realtime and sampled checkpoints skip persistent I/O")
    require_contains(driver_header, "WDFWAITLOCK BulkCommandLock;", "bulk command serialization contract")
    require_not_contains(driver_header, "StreamCaptureUrbMemory", "persistent isochronous input URB lifetime")
    require_not_contains(driver_header, "StreamOutputUrbMemory", "persistent isochronous output URB lifetime")
    require_contains(driver_header, "UCHAR Ep1Reply[64];", "CAIAQ EP1 single-packet reader contract")
    require_contains(driver_header, "WDFWAITLOCK AudioConfigLock;", "audio configuration serialization contract")
    require_contains(driver_header, "volatile LONG AudioParamsConfigured;", "audio configuration state contract")
    require_contains(driver_header, "BOOLEAN RenderCircuitAdded[OPENA8DJ_STEREO_PAIRS];", "ACX per-circuit teardown state")
    require_contains(driver_header, "BOOLEAN CaptureCircuitAdded[OPENA8DJ_STEREO_PAIRS];", "ACX per-circuit teardown state")
    require_contains(driver, "WdfWaitLockAcquire(Context->BulkCommandLock", "bulk command serialization")
    require_contains(driver, "WdfWaitLockRelease(Context->BulkCommandLock)", "bulk command serialization")
    require_contains(driver, "WdfWaitLockCreate(&attributes, &context->BulkCommandLock)", "bulk command lock initialization")
    require_contains(driver, "WdfWaitLockCreate(&attributes, &context->AudioConfigLock)", "audio configuration lock initialization")
    require_contains(driver, "WdfIoTargetStop(pipeTarget, WdfIoTargetCancelSentIo);", "EP1 reader stop-before-configure")
    require_contains(driver, "WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(BulkInPipe);", "EP1 64-byte CAIAQ reader exception")
    require_contains(driver, "status = WdfIoTargetStart(pipeTarget);", "EP1 reader configure-then-start")
    require_contains(driver, "InterlockedExchange(&context->StreamStopRequested, 0);", "legacy START stopped-to-running transition")
    require_contains(driver, "attributes.ExecutionLevel = WdfExecutionLevelPassive;", "passive-level device execution")
    require_contains(project, "<ACX_VERSION_MAJOR>1</ACX_VERSION_MAJOR>", "ACX build configuration")
    require_contains(project, "acxstub.lib", "ACX link configuration")
    require_contains(project, "<WppRecorderEnabled>true</WppRecorderEnabled>", "IFR crash tracing")
    require_contains(driver, "AcxDeviceInitInitialize(DeviceInit, &acxDeviceInitConfig)", "ACX device initialization")
    require_contains(driver, "AcxDeviceInitialize(device, &acxDeviceConfig)", "ACX device initialization")
    require_contains(driver, "AcxDeviceAddCircuit(Device, Context->RenderCircuits[pairIndex])", "ACX render circuit registration")
    require_contains(driver, "AcxDeviceAddCircuit(Device, Context->CaptureCircuits[pairIndex])", "ACX capture circuit registration")
    require_contains(driver, "AcxDeviceRemoveCircuit(Device, Context->RenderCircuits[pairIndex])", "ACX render circuit removal")
    require_contains(driver, "AcxDeviceRemoveCircuit(Device, Context->CaptureCircuits[pairIndex])", "ACX capture circuit removal")
    require_contains(driver, "OpenA8DJ_AddAcxCircuits(Device, context)", "ACX circuit transaction")
    require_contains(driver, "OpenA8DJ_RemoveAcxCircuits(Device, context)", "ACX circuit teardown")
    require_contains(
        driver,
        "pinConfig.Communication = Render ?\n                              AcxPinCommunicationSink :\n                              AcxPinCommunicationSource;",
        "ACX directional pin communication",
    )
    add_circuits = extract_function(driver, "OpenA8DJ_AddAcxCircuits")
    require_contains(add_circuits, "goto rollback;", "ACX circuit rollback")
    require_contains(add_circuits, "AcxDeviceRemoveCircuit(Device, Context->CaptureCircuits[index])", "ACX circuit rollback")
    require_contains(add_circuits, "AcxDeviceRemoveCircuit(Device, Context->RenderCircuits[index])", "ACX circuit rollback")
    release_hardware = extract_function(driver, "OpenA8DJ_EvtDeviceReleaseHardware")
    require_contains(release_hardware, "circuitStatus = OpenA8DJ_RemoveAcxCircuits(Device, context);", "ACX circuit teardown")
    require_contains(release_hardware, "return circuitStatus;", "ACX circuit teardown status")
    require_contains(add_circuits, "Context->RenderCircuitAdded[pairIndex] = TRUE;", "ACX per-circuit registration state")
    require_contains(add_circuits, "Context->CaptureCircuitAdded[pairIndex] = TRUE;", "ACX per-circuit registration state")
    require_contains(add_circuits, "Context->AcxCircuitsAdded = circuitsRemain;", "ACX rollback state preservation")
    require_contains(driver, "OpenA8DJ_RunVirtualStreamWorker(context);", "virtual ACX stream worker")
    require_contains(driver, "OPENA8DJ_VIRTUAL_TICK_FRAMES", "virtual ACX deterministic cadence")
    require_contains(driver, "OpenA8DJVirtual: ACX circuit registration failed", "virtual ACX readiness")
    require_contains(virtual_project, "<TargetName>OpenA8DJVirtual</TargetName>", "virtual ACX target")
    require_contains(virtual_project, "OPENA8DJ_VIRTUAL_MODE=1", "virtual ACX compile isolation")
    require_contains(virtual_project, "OpenA8DJVirtual.inf", "virtual ACX package")
    require_contains(virtual_solution, "OpenA8DJVirtual.vcxproj", "virtual ACX solution")
    require_contains(virtual_inf, "Class=MEDIA", "virtual ACX INF class")
    require_contains(virtual_inf, "ROOT\\OpenA8DJVirtual", "virtual ACX root device")
    require_not_contains(virtual_inf, "USB\\VID_", "virtual ACX hardware isolation")
    require_contains(build_virtual_acx, "OpenA8DJVirtual.sln", "virtual ACX build gate")
    require_contains(build_virtual_acx, "infverif.exe", "virtual ACX INF verification")
    require_contains(build_virtual_acx, "Inf2Cat.exe", "virtual ACX catalog verification")
    require_contains(virtual_canary, "does_not_target_audio_8_dj_usb=1", "virtual canary hardware isolation")
    require_contains(virtual_canary, "if (-not $AllowVirtualInstall)", "virtual canary dry-run gate")
    require_contains(virtual_canary, "-AllowTestSigned", "virtual canary signing gate")
    require_contains(virtual_canary, "ROOT\\OpenA8DJVirtual", "virtual canary device isolation")
    require_contains(virtual_canary, "does_not_run_opena8djctl=1", "virtual canary no control traffic")
    require_not_contains(virtual_canary, "& $CtlPath", "virtual canary no control traffic")
    require_contains(virtual_canary, 'Invoke-DevconBounded', "virtual canary bounded install action")
    require_contains(virtual_canary, "PnpUtilTimeoutSeconds", "virtual canary bounded pnputil timeout")
    require_contains(virtual_canary, "ProcessStartInfo", "virtual canary bounded pnputil process")
    require_contains(virtual_canary, "WaitForExit", "virtual canary bounded pnputil timeout")
    require_contains(virtual_canary, "Stop-Process", "virtual canary bounded pnputil timeout")
    require_contains(virtual_canary, "CreateNoWindow", "virtual canary hidden pnputil process")
    require_contains(virtual_canary, "Resolve-DevconPath", "virtual canary root device creation")
    require_contains(virtual_canary, "Invoke-DevconBounded", "virtual canary bounded root device creation")
    require_contains(virtual_canary, "/remove-device", "virtual canary rollback")
    require_contains(virtual_canary, 'Invoke-PnpUtilBounded -Arguments @("/delete-driver", [string]$publishedName, "/uninstall", "/force")', "virtual canary bounded rollback")
    require_contains(virtual_canary, "-ProbeCtlPath", "virtual canary read-only probe integration")
    require_contains(virtual_canary, "-ProbeCtlPath requires -RemoveAfter", "virtual canary cleanup gate")
    require_contains(virtual_canary, "run-open-a8dj-virtual-endpoint-probe.ps1", "virtual canary read-only probe integration")
    require_contains(virtual_canary, "Find-PublishedDriverName", "virtual canary package cleanup")
    require_contains(virtual_canary, "remaining_virtual_devices", "virtual canary package cleanup")
    require_contains(virtual_canary, "Virtual driver package cleanup failed", "virtual canary package cleanup")
    require_contains(virtual_canary, "Virtual device cleanup failed", "virtual canary cleanup ordering")
    require_contains(virtual_output_canary, "isolated_virtual_output_only", "virtual output canary safety policy")
    require_contains(virtual_output_canary, "does_not_target_audio_8_dj_usb=1", "virtual output canary hardware isolation")
    require_contains(virtual_output_canary, "AllowVirtualInstall", "virtual output canary install gate")
    require_contains(virtual_output_canary, "AllowTestSigned", "virtual output canary signing gate")
    require_contains(virtual_output_canary, "Try-ApproveWindowsSecurityDriverPrompt", "virtual output prompt safety")
    require_contains(virtual_output_canary, "ExpectedInfHash", "virtual output prompt hash gate")
    require_contains(virtual_output_canary, "output-stream-start", "virtual output durable checkpoints")
    require_contains(virtual_output_canary, "output-stream-returned", "virtual output durable checkpoints")
    require_contains(virtual_output_canary, "process_timeout_seconds", "virtual output bounded timeout")
    require_contains(virtual_output_canary, "/remove-device", "virtual output rollback")
    require_contains(virtual_output_canary, "/delete-driver", "virtual output package cleanup")
    require_contains(virtual_output_canary, "bugcheck_events_since_start", "virtual output bugcheck evidence")
    require_contains(virtual_canary, "Write-CanaryCheckpoint", "virtual canary durable checkpoints")
    require_contains(virtual_canary, "install-start", "virtual canary durable checkpoints")
    require_contains(virtual_canary, "probe-start", "virtual canary durable checkpoints")
    require_contains(virtual_canary, "cleanup-start", "virtual canary durable checkpoints")
    require_contains(virtual_canary, "checkpoints.jsonl", "virtual canary durable checkpoints")
    require_contains(virtual_canary, "Get-PhysicalAudio8DjUsbDevices", "virtual canary physical USB isolation")
    require_contains(virtual_canary, "physical_audio8dj_usb_device_count", "virtual canary physical USB isolation")
    require_contains(virtual_probe, "OpenA8DJ Virtual ACX Proof Endpoint", "virtual probe exact hardware identity")
    require_contains(virtual_canary, "OpenA8DJ Virtual ACX Proof Endpoint", "virtual canary exact hardware identity")
    require_contains(virtual_recovery, "last_checkpoint", "virtual canary recovery analysis")
    require_contains(virtual_recovery, "expected_next_stage", "virtual canary recovery analysis")
    require_contains(virtual_recovery, "system_events_since_first_checkpoint", "virtual canary recovery analysis")
    require_contains(virtual_recovery, "physical_audio8dj_usb_devices_now", "virtual canary physical USB evidence")
    require_not_contains(virtual_recovery, "/add-driver", "virtual canary recovery read-only policy")
    require_not_contains(virtual_recovery, "/remove-device", "virtual canary recovery read-only policy")
    require_contains(virtual_probe, "virtual_endpoint_read_only_probe", "virtual endpoint probe safety")
    require_contains(virtual_probe, "Invoke-CtlReadOnly", "virtual endpoint probe read-only IOCTLs")
    require_contains(virtual_probe, '"audio-endpoints"', "virtual endpoint probe readiness")
    require_contains(virtual_probe, '"usb-transport"', "virtual endpoint probe USB isolation")
    require_contains(virtual_probe, '"streaming"', "virtual endpoint probe no streaming")
    require_not_contains(virtual_probe, "/install", "virtual endpoint probe no installation")
    require_not_contains(virtual_probe, "IOCTL_OPENA8DJ_SET", "virtual endpoint probe no writes")
    require_contains(driver, "AcxRtStreamCreate(Device, Circuit", "ACX RT stream creation")
    require_contains(driver, "AcxStreamInitAssignAcxRtStreamCallbacks", "ACX RT stream callbacks")
    require_not_contains(driver, "OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES", "rate-dependent packet sizing")
    require_contains(driver, "case 44100:", "rate-code mapping")
    require_contains(driver, "*RateCode = 0;", "44.1 kHz rate-code mapping")
    require_contains(driver, "case 48000:", "rate-code mapping")
    require_contains(driver, "*RateCode = 1;", "48 kHz rate-code mapping")
    require_contains(driver, "OPENA8DJ_CLOCK_DRIFT_TOLERANCE", "rate-dependent packet sizing")
    require_contains(driver, "OPENA8DJ_AUDIO_STREAM_COUNT", "rate-dependent packet sizing")
    require_contains(driver, "OPENA8DJ_USB_OUTPUT_FRAME_BYTES", "rate-dependent frame accounting")
    require_contains(driver, "_In_ ULONG ChannelMask,\n    _In_ ULONG SampleRate", "ACX sample-rate formats")
    require_contains(
        driver,
        "static NTSTATUS\nOpenA8DJ_AddPcmFormatToPin(",
        "ACX format helper return type",
    )
    require_contains(
        driver,
        "bytesPerPacket = ((SampleRate / 8000u) + OPENA8DJ_CLOCK_DRIFT_TOLERANCE) *\n                     OPENA8DJ_USB_BYTES_PER_SAMPLE *\n                     2u *\n                     OPENA8DJ_AUDIO_STREAM_COUNT;",
        "CAIAQ packet-size formula",
    )
    default_formats = extract_function(driver, "OpenA8DJ_AddDefaultFormatsToPin")
    require_contains(default_formats, "44100", "ACX 44.1 kHz format")
    require_contains(default_formats, "48000", "ACX 48 kHz format")
    require_contains(
        default_formats,
        "KSAUDIO_SPEAKER_7POINT1_SURROUND",
        "ACX aggregate endpoint 7.1 channel mask",
    )
    require_contains(
        driver,
        "AcxDataFormatListAssignDefaultDataFormat(formatList, acxFormat)",
        "explicit ACX default data format",
    )
    require_contains(inf, "PKEY_AudioEngine_OEMFormat", "ACX OEM endpoint format")
    require_contains(inf, "OpenA8DJUsb.Format8ch48k.AddReg", "ACX 8-channel 48 kHz OEM format")
    require_contains(inf, "3F,06,00,00", "ACX 7.1 OEM channel mask")
    require_contains(inf, 'KSNAME_OpenA8DJRenderA="OpenA8DJ Render A v2"', "fresh ACX interface identity")
    require_contains(driver, "OPENA8DJ_STREAM_TRANSIENT_RETRY_LIMIT", "bounded USB stream retries")
    store_controls = extract_function(driver, "OpenA8DJ_StoreControlState")
    require_contains(store_controls, "StreamWorkerActive", "live control write rejection")
    require_contains(store_controls, "STATUS_DEVICE_BUSY", "live control write rejection status")
    require(
        default_formats.index("48000") < default_formats.index("44100"),
        "ACX default format order must prefer 48 kHz",
    )
    run_callback = extract_function(driver, "OpenA8DJ_EvtAcxStreamRun")
    require_contains(run_callback, "NTSTATUS status = STATUS_DEVICE_NOT_READY;", "ACX run readiness")
    require_contains(run_callback, "OpenA8DJ_ConfigureAudioParams(", "ACX rate configuration")
    require_contains(run_callback, "streamContext->RtSampleRate", "ACX rate configuration")
    require_contains(run_callback, "OpenA8DJ_IsTransportReady(deviceContext)", "ACX transport readiness")
    require_contains(run_callback, "deviceContext->DevicePrepared", "ACX run revalidates prepared hardware after rate settle")
    require_contains(run_callback, "deviceContext->DeviceStopping", "ACX run revalidates PnP teardown after rate settle")
    require_contains(driver, "Context->IsoInPipe != NULL", "ACX USB transport readiness")
    require_contains(driver, "Context->IsoOutPipe != NULL", "ACX USB transport readiness")
    require_contains(driver, "OPENA8DJ_VIRTUAL_MODE", "ACX virtual transport readiness")
    require_contains(run_callback, "OpenA8DJ_RecordAcxStage(412, status);", "ACX run status")
    require_contains(run_callback, "return status;", "ACX run status")
    packet_alloc = extract_function(driver, "OpenA8DJ_EvtAcxStreamAllocateRtPackets")
    require_contains(packet_alloc, "rawBytes > MAXULONG - (PAGE_SIZE - 1u)", "ACX packet allocation overflow guard")
    require_contains(build_driver, "function Find-InfVerif", "WDK INF verification")
    require_contains(build_driver, "& $infVerif /w /v $infDest", "WDK INF verification")
    require_contains(build_driver, "& $infVerif /h /v $infDest", "WDK INF hardware-signature verification")
    require_contains(windows_workflow, "permissions:\n  contents: read", "GitHub workflow least privilege")
    require_contains(windows_workflow, "persist-credentials: false", "GitHub checkout credential safety")
    require_contains(windows_workflow, "pull_request:", "GitHub fork build trigger")
    require_not_contains(windows_workflow, "pull_request_target", "GitHub fork build isolation")
    require_not_contains(windows_workflow, "secrets.", "GitHub fork secret isolation")
    require_not_contains(windows_workflow, "git push", "GitHub fork write isolation")
    require_contains(windows_workflow, "virtual-acx-proof:", "virtual ACX CI gate")
    require_contains(windows_workflow, "build-virtual-acx.ps1", "virtual ACX CI gate")
    require_contains(ctl, "OPENA8DJ_INSTANCE_ID", "device-specific control selection")
    require_contains(ctl, "SetupDiGetDeviceInstanceIdA", "device-specific control selection")
    require_contains(virtual_probe, "$env:OPENA8DJ_INSTANCE_ID", "virtual probe device isolation")
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
    require_contains(capabilities, "Capabilities->WindowsAudioEndpointExposed = endpointReady;", "endpoint truth")
    require_contains(capabilities, "endpointReady = transportReady && Context->AcxCircuitsAdded;", "endpoint truth")
    require_contains(capabilities, "Capabilities->MidiReady = FALSE;", "midi truth")
    require_contains(capabilities, "OPENA8DJ_STABLE_SAMPLE_RATE_COUNT", "capabilities rates")

    surface = extract_function(driver, "OpenA8DJ_FillSurface")
    require_contains(surface, "Surface->ControlState = Context->ControlsHardwareReady", "surface controls")
    require_contains(surface, "OPENA8DJ_COMPONENT_READY", "surface controls")
    require_contains(surface, "OPENA8DJ_COMPONENT_STUB", "surface controls")
    require_contains(surface, "Surface->AudioEndpointState = endpointReady ?", "surface endpoint")
    require_contains(surface, "endpointReady = transportReady && Context->AcxCircuitsAdded;", "surface endpoint")
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
    require_contains(start_block, "audioParamsStatus = OpenA8DJ_ConfigureAudioParams(", "start hardware configuration")
    require_contains(start_block, "if (!NT_SUCCESS(audioParamsStatus))", "start hardware configuration")
    require_contains(start_block, "InterlockedExchange(&context->StreamWorkerActive, 0);", "start rollback")
    require_contains(start_block, "status = STATUS_DEVICE_BUSY;", "start/stop serialization")
    stop_block = ioctls[ioctls.find("IOCTL_OPENA8DJ_STOP_STREAMING"):]
    require_contains(stop_block, "workerStopped = InterlockedCompareExchange(&context->StreamWorkerActive, 0, 0) == 0;", "stop completion truth")
    require_contains(stop_block, "status = STATUS_IO_TIMEOUT;", "stop completion truth")
    require_contains(stop_block, "InterlockedExchange(&context->StreamStopRequested, 0);", "stop state reset")
    apply_params = extract_function(driver, "OpenA8DJ_EvtIoDeviceControl")
    require_contains(apply_params, "status = OpenA8DJ_ConfigureAudioParamsIsolated(\n                context,\n                context->CurrentFormat.SampleRate,\n                audioParams);", "audio parameter status propagation")
    require_not_contains(apply_params, "(void)OpenA8DJ_ApplyAudioParams(context, audioParams);", "audio parameter status propagation")
    audio_params = extract_function(driver, "OpenA8DJ_ApplyAudioParams")
    require_contains(audio_params, "if (!NT_SUCCESS(status)) {\n        return status;\n    }", "audio parameter transaction gate")
    require_contains(audio_params, "Result->DeviceInfoReply[0] != 0x01", "audio parameter protocol gate")
    require_contains(audio_params, "Result->ResetReply[1] != 0", "audio parameter reset protocol gate")
    require_contains(audio_params, "Result->SetNtStatus = (ULONG)STATUS_DEVICE_PROTOCOL_ERROR;", "audio parameter set protocol gate")
    format_ioctl = extract_function(driver, "OpenA8DJ_EvtIoDeviceControl")
    require_contains(format_ioctl, "status = STATUS_DEVICE_BUSY;", "live format change safety")
    require_contains(format_ioctl, "OpenA8DJ_InvalidateAudioParamsConfiguration(context);", "format reconfiguration")
    control_read = extract_function(driver, "OpenA8DJ_ReadHardwareControlState")
    require_not_contains(control_read, "(void)OpenA8DJ_SendBulkCommand", "control read status propagation")
    require_contains(control_read, "status = OpenA8DJ_SendBulkCommand(", "control read status propagation")
    require_contains(control_read, "if (!NT_SUCCESS(status)) {\n            continue;", "control read status propagation")

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
    require_contains(control_matrix, "expected_unsupported_cases", "control matrix readback-only classification")
    require_contains(control_matrix, "Ground-lift controls are hardware readback-only", "control matrix ground CLI contract")
    require_contains(control_matrix, "ControlWritesBefore", "control matrix no-write invariant")
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
    require_contains(load_canary, "ValidateSet('Stage', 'LoadInert', 'ControlRead', 'IsoCapture', 'IsoOutput', 'IsoStress', 'Streaming')", "phased physical canary")
    require_contains(load_canary, "AllowPhysicalLoad", "physical load authorization gate")
    require_contains(load_canary, "AcknowledgeCrashRisk", "physical crash-risk acknowledgement")
    require_contains(load_canary, "CM_PROB_DISABLED", "disabled-device precondition")
    require_contains(load_canary, "Get-OpenA8DJDriverStoreMatches", "DriverStore identity gate")
    require_contains(load_canary, "loaded-binary-verified-inert", "loaded build identity gate")
    require_contains(load_canary, "boot-recovery-armed", "automatic boot recovery")
    require_contains(load_canary, "arm', $phaseMap[$Phase]", "one-shot operation authorization")
    require_contains(load_canary, "checkpoints.jsonl", "physical load durable checkpoints")
    require_contains(load_canary, "Invoke-Bounded", "physical load bounded process")
    require_contains(load_canary, "Try-ApproveExactDriverPrompt", "unsigned driver prompt safety")
    require_contains(load_canary, "ExpectedInfHash", "unsigned driver prompt hash gate")
    require_contains(load_canary, "pnputil.exe /disable-device", "fail-closed device cleanup")
    require_contains(recovery_action, "driver_checkpoint_before_cleanup", "post-crash checkpoint preservation")
    require_contains(recovery_action, "/disable-device", "boot recovery disables hardware")
    require_contains(recovery_action, "/delete-driver", "boot recovery removes candidate")
    require_contains(windows_common, "New-OpenA8DJPackageManifest", "single package identity manifest")
    require_contains(windows_common, "Test-OpenA8DJPackageManifest", "package identity verification")
    require_contains(build_driver, "buildFingerprint", "build fingerprint generation")
    require_contains(build_driver, "Remove-Item -LiteralPath $cleanPath -Recurse -Force", "clean physical build")
    require_contains(install_driver, "Staging driver package without binding or loading it", "stage-only installer default")
    require_contains(package_installer, "Test-OpenA8DJPackageManifest", "installer manifest verification")
    require_contains(output_endpoint_probe, "silence_only", "physical output probe silence-only policy")
    require_contains(output_endpoint_probe, "OutputStream", "physical output probe endpoint open")
    require_contains(output_endpoint_probe, "status_event_count", "physical output probe status evidence")
    require_contains(output_endpoint_probe, "timeout_seconds", "physical output control probe timeout")
    require_contains(output_endpoint_probe, "diagnostics-before-start", "physical output probe stage checkpoints")
    require_contains(output_endpoint_probe, "diagnostics-before-returned", "physical output probe stage checkpoints")
    require_contains(output_endpoint_probe, "ROOT\\\\ instance probes must explicitly target", "output probe instance isolation")
    require_contains(output_endpoint_probe, "OPENA8DJ_INSTANCE_ID", "output probe instance isolation")
    require_contains(output_endpoint_probe, 'target["hostapi"] in ("MME", "Windows DirectSound")', "physical output host-rate normalization")
    require_contains(output_endpoint_probe, "target_rate = 44100", "physical output host-rate normalization")
    require_contains(physical_recovery, "checkpoints.jsonl", "physical reboot recovery checkpoints")
    require_contains(physical_recovery, "0x0000007e", "physical reboot recovery diagnosis")
    require_contains(physical_recovery, "physical_device_now", "physical reboot recovery PnP snapshot")
    require_contains(physical_recovery, "driver_checkpoint", "driver checkpoint recovery")
    require_contains(physical_recovery, "wdflogdump", "IFR dump extraction")
    require_contains(load_canary, "delete-driver", "physical load package cleanup")
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
    require_contains(worker, "OpenA8DJ_SendIsoOutputBuffer(\n                context,", "stream output temporary URB")
    require_not_contains(worker, "OpenA8DJ_SendIsoOutputBufferPrepared(", "stream output temporary URB")
    require_not_contains(worker, "outputUrbMemory", "stream output temporary URB")
    require_contains(worker, "if (context->IsoOutPipe == NULL)", "worker output pipe readiness")
    require_contains(driver, "OpenA8DJ_AbortIsoOutputPipe", "isochronous output abort safety")
    require_contains(driver, "OpenA8DJ_AbortIsoInputPipe", "isochronous input abort safety")
    require_contains(driver_header, "WDFMEMORY StreamCaptureBufferMemory;", "parent-owned streaming buffer lifetime")
    require_contains(driver_header, "WDFMEMORY StreamPlaybackBufferMemory;", "parent-owned streaming buffer lifetime")
    require_contains(driver_header, "volatile LONG CanaryPhase;", "one-shot physical authorization state")
    require_contains(driver, "OpenA8DJ_ConsumeCanaryAuthorization", "one-shot physical authorization enforcement")
    begin_one_shot = extract_function(driver, "OpenA8DJ_BeginIsoOneShot")
    end_one_shot = extract_function(driver, "OpenA8DJ_EndIsoOneShot")
    ioctl = extract_function(driver, "OpenA8DJ_EvtIoDeviceControl")
    require_contains(begin_one_shot, "IsoOneShotActive", "one-shot producer exclusion claim")
    require_contains(begin_one_shot, "OpenA8DJIsoEngineStopped", "one-shot denied while engine is active")
    require_contains(begin_one_shot, "StreamWorkerActive", "one-shot denied after stream worker publication")
    require_contains(begin_one_shot, "OpenA8DJ_HasActiveAcxStreams", "one-shot denied after ACX stream publication")
    require_contains(begin_one_shot, "IsoTransportHealthy", "one-shot requires healthy transport")
    require_contains(begin_one_shot, "OpenA8DJ_ConsumeCanaryAuthorization", "one-shot consumes physical authorization")
    require_contains(end_one_shot, "OpenA8DJ_PurgeIsoTargetsLocked(Context, TRUE)", "one-shot always purges and resets ASAP tracking")
    require_contains(begin_one_shot, "WdfWaitLockAcquire(Context->IsoPurgeLock", "one-shot owns lifecycle lock through cleanup")
    require_contains(end_one_shot, "WdfWaitLockRelease(Context->IsoPurgeLock)", "one-shot releases lifecycle lock after cleanup")
    require_contains(ioctl, "OPENA8DJ_CANARY_PHASE_ISO_CAPTURE", "capture snapshot authorization phase")
    require_contains(ioctl, "OPENA8DJ_CANARY_PHASE_ISO_OUTPUT", "output one-shot authorization phase")
    require_contains(ioctl, "status = !NT_SUCCESS(cleanupStatus) ? cleanupStatus : operationStatus", "one-shot operation and cleanup errors propagate fail-closed")
    record_checkpoint = extract_function(driver, "OpenA8DJ_RecordSafetyCheckpoint")
    require_not_contains(record_checkpoint, "Checkpoint == OPENA8DJ_CHECKPOINT_ISO_FRAME_QUERY_COMPLETE", "frame-query completion persists as the registry commit marker")
    require_contains(record_checkpoint, "WdfWaitLockAcquire(", "passive checkpoint persistence is serialized")
    stream_run = extract_function(driver, "OpenA8DJ_EvtAcxStreamRun")
    require_contains(stream_run, "WdfWaitLockAcquire(deviceContext->IsoPurgeLock", "ACX run owns lifecycle lock before hardware configuration")
    require_contains(stream_run, "IsoOneShotActive", "ACX run excludes one-shot producers")
    require(
        stream_run.index("WdfWaitLockAcquire(deviceContext->IsoPurgeLock")
        < stream_run.index("OpenA8DJ_ConfigureAudioParams(")
        < stream_run.index("WdfWorkItemEnqueue(deviceContext->StreamWorkItem)"),
        "ACX run lifecycle lock covers configure and worker publication",
    )
    require_contains(driver, "OpenA8DJ_ArmNextBootFailSafe(RegistryPath)", "load-once next-boot fail-safe")
    require_contains(driver, "status = ZwFlushKey(serviceKey);", "durable next-boot fail-safe")
    require_contains(driver, "WdfIoTargetPurgeIoAndWait", "synchronous isochronous drain barrier")
    require_contains(release_hardware, "OpenA8DJ_PurgeIsoTargetsLocked(context, FALSE);", "teardown drain before object destruction")
    require_not_contains(worker, "context->StreamCaptureUrb", "stream input temporary URB")
    require_not_contains(worker, "context->StreamOutputUrb", "stream output temporary URB")
    require_contains(driver, "UrbIsochronousTransfer.Hdr.Length", "isochronous URB header initialization")
    require_contains(driver, "GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT)", "isochronous URB header initialization")
    require_contains(driver, "status = WdfUsbTargetDeviceCreate(\n", "legacy KMDF USB client without XRB contract")
    require_not_contains(driver, "WdfUsbTargetDeviceCreateWithParameters", "no USBD 602 XRB client contract")
    require_not_contains(driver, "WdfUsbTargetDeviceCreateIsochUrb", "no FxUsbUrb/XRB allocation path")
    require_not_contains(driver, "USBD_UrbAllocate", "no direct XRB allocation path")
    require_not_contains(driver, "USBD_UrbFree", "no XRB destruction path")
    ensure_iso = extract_function(driver, "OpenA8DJ_EnsureIsoTransportResources")
    capture_iso = extract_function(driver, "OpenA8DJ_CaptureIsoSnapshotWithPayload")
    output_iso = extract_function(driver, "OpenA8DJ_SendIsoOutputBuffer")
    output_iso_locked = extract_function(driver, "OpenA8DJ_SendIsoOutputBufferLocked")
    require_contains(ensure_iso, "Context->IsoCaptureRequest", "persistent capture request allocation")
    require_contains(ensure_iso, "Context->IsoOutputRequest", "persistent output request allocation")
    require_contains(ensure_iso, "WdfMemoryCreate(", "plain URB memory allocation")
    require_contains(capture_iso, "Context->IsoCaptureRequest", "persistent capture slot use")
    require_contains(ensure_iso, "Context->DeviceStopping", "transport allocation blocked during PnP teardown")
    require_contains(ensure_iso, "Context->DevicePrepared", "transport allocation requires prepared hardware")
    require_contains(capture_iso, "Context->DeviceStopping", "snapshot use blocked during PnP teardown")
    require_contains(capture_iso, "Context->DevicePrepared", "snapshot use requires prepared hardware")
    require_contains(output_iso_locked, "Context->IsoOutputRequest", "persistent output slot use")
    require_contains(output_iso_locked, "Context->DeviceStopping", "locked output use blocked during PnP teardown")
    require_contains(output_iso_locked, "Context->DevicePrepared", "locked output use requires prepared hardware")
    require_not_contains(capture_iso, "WdfRequestCreate", "no capture hot-path request allocation")
    require_not_contains(output_iso, "WdfRequestCreate", "no output hot-path request allocation")
    require_not_contains(capture_iso, "WdfObjectDelete", "no capture hot-path object destruction")
    require_not_contains(output_iso, "WdfObjectDelete", "no output hot-path object destruction")
    require_contains(release_hardware, "WdfWorkItemFlush(context->StreamWorkItem);", "stream worker drain before slot teardown")
    require_contains(release_hardware, "OpenA8DJ_DestroyIsoTransportResources(context);", "persistent slot teardown")
    require_contains(driver, "OpenA8DJ_SendIsoUrbWithRequest", "explicit isochronous request completion")
    require_not_contains(driver, "WdfUsbTargetPipeSendUrbSynchronously", "no internal isochronous request lifetime")
    require_contains(worker, "context->StreamCaptureBuffer", "parent-owned streaming buffer reuse")
    require_contains(worker, "context->StreamPlaybackBuffer", "parent-owned streaming buffer reuse")
    require_not_contains(driver, "WdfObjectDelete(urbMemory);", "request-owned isochronous URB cleanup")
    require_not_contains(driver, "WdfObjectDelete(Slots[slotIndex].UrbMemory);", "async request-owned isochronous URB cleanup")
    require_contains(driver, "attributes.ParentObject = Context->IsoCaptureRequest;", "capture request-owned URB lifetime")
    require_contains(driver, "attributes.ParentObject = Context->IsoOutputRequest;", "output request-owned URB lifetime")
    require_contains(driver, "attributes.ParentObject = Context->UsbDevice;", "async request rooted at USB device")
    require_contains(driver, "WdfRequestReuse(request, &reuseParams)", "formatted URB reference release")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_RECLAIM_START", "capture reclaim checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_RECLAIM_COMPLETE", "capture reclaim completion checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_SLOT_IDLE", "capture slot idle checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RECLAIM_START", "output reclaim checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RECLAIM_COMPLETE", "output reclaim completion checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SLOT_IDLE", "output slot idle checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_REQUEST_CREATED", "capture request-create checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_BUFFER_CREATED", "capture buffer-create checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_URB_CREATED", "capture URB-create checkpoint")
    require_contains(driver, "OPENA8DJ_CHECKPOINT_ISO_CAPTURE_FORMATTED", "capture format checkpoint")
    require_not_contains(driver, "retaining worker output isochronous URB memory", "no parent-owned output URB retention")
    require_not_contains(driver, "retaining worker input isochronous URB memory", "no parent-owned input URB retention")
    require_not_contains(worker, "InterlockedExchange(&context->DeviceStopping, 1);", "stream USB failure must not poison PnP device state")
    require_contains(worker, "InterlockedExchange(&context->StreamStopRequested, 1);", "stop stream after failed synchronous USB I/O")
    require_contains(driver, "cancelled failed isochronous input request", "isochronous URB failure containment")

    print("PASS: Windows surface contract is truthful for offline/macOS validation")


if __name__ == "__main__":
    main()
