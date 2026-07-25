#define INITGUID
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\include\OpenA8DJShared.h"

static HANDLE OpenDevice(void)
{
    HDEVINFO infoSet;
    SP_DEVICE_INTERFACE_DATA interfaceData;
    const char *requestedInstanceId = getenv("OPENA8DJ_INSTANCE_ID");
    DWORD interfaceIndex = 0;
    HANDLE device = INVALID_HANDLE_VALUE;

    infoSet = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_OPENA8DJ_USB,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (infoSet == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    while (TRUE) {
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = NULL;
        SP_DEVINFO_DATA deviceInfoData;
        DWORD requiredSize = 0;
        char instanceId[512];

        ZeroMemory(&interfaceData, sizeof(interfaceData));
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(
                infoSet,
                NULL,
                &GUID_DEVINTERFACE_OPENA8DJ_USB,
                interfaceIndex++,
                &interfaceData)) {
            break;
        }

        ZeroMemory(&deviceInfoData, sizeof(deviceInfoData));
        deviceInfoData.cbSize = sizeof(deviceInfoData);
        (void)SetupDiGetDeviceInterfaceDetailA(
            infoSet,
            &interfaceData,
            NULL,
            0,
            &requiredSize,
            &deviceInfoData);
        if (requiredSize == 0) {
            continue;
        }

        detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)calloc(1, requiredSize);
        if (detailData == NULL) {
            break;
        }

        detailData->cbSize = sizeof(*detailData);
        if (!SetupDiGetDeviceInterfaceDetailA(
                infoSet,
                &interfaceData,
                detailData,
                requiredSize,
                NULL,
                &deviceInfoData)) {
            free(detailData);
            continue;
        }

        if (requestedInstanceId != NULL && requestedInstanceId[0] != '\0') {
            ZeroMemory(instanceId, sizeof(instanceId));
            if (!SetupDiGetDeviceInstanceIdA(
                    infoSet,
                    &deviceInfoData,
                    instanceId,
                    (DWORD)sizeof(instanceId),
                    NULL) ||
                _stricmp(instanceId, requestedInstanceId) != 0) {
                free(detailData);
                continue;
            }
        }

        device = CreateFileA(
            detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        free(detailData);
        if (device != INVALID_HANDLE_VALUE) {
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(infoSet);
    return device;
}

static BOOL DeviceIo(
    HANDLE device,
    DWORD code,
    void *inBuffer,
    DWORD inSize,
    void *outBuffer,
    DWORD outSize)
{
    DWORD returned = 0;
    return DeviceIoControl(device, code, inBuffer, inSize, outBuffer, outSize, &returned, NULL);
}

static const char *InputModeName(UCHAR mode)
{
    switch (mode) {
    case 0:
        return "timecode-vinyl";
    case 1:
        return "timecode-cd-line";
    case 2:
        return "phono";
    default:
        return "unknown";
    }
}

static const char *ComponentStateName(ULONG state)
{
    switch (state) {
    case OPENA8DJ_COMPONENT_ABSENT:
        return "absent";
    case OPENA8DJ_COMPONENT_PLANNED:
        return "planned";
    case OPENA8DJ_COMPONENT_STUB:
        return "stub";
    case OPENA8DJ_COMPONENT_READY:
        return "ready";
    default:
        return "unknown";
    }
}

static const char *EndpointModelName(ULONG model)
{
    switch (model) {
    case OPENA8DJ_ENDPOINT_MODEL_NONE:
        return "none";
    case OPENA8DJ_ENDPOINT_MODEL_SINGLE_8CH:
        return "single-8ch";
    case OPENA8DJ_ENDPOINT_MODEL_FOUR_STEREO_PAIRS:
        return "four-stereo-pairs";
    case OPENA8DJ_ENDPOINT_MODEL_DUAL_PROTOTYPE:
        return "dual-prototype";
    case OPENA8DJ_ENDPOINT_MODEL_PRIMARY_8CH_PLUS_STEREO:
        return "primary-8ch-plus-stereo";
    default:
        return "unknown";
    }
}

static const char *DirectionName(ULONG direction)
{
    switch (direction) {
    case OPENA8DJ_CHANNEL_DIRECTION_RENDER:
        return "render";
    case OPENA8DJ_CHANNEL_DIRECTION_CAPTURE:
        return "capture";
    default:
        return "unknown";
    }
}

static void PrintRateFlags(ULONG flags)
{
    BOOL printed = FALSE;

    if (flags & OPENA8DJ_RATE_FLAG_44100) {
        printf("%s44100", printed ? ", " : "");
        printed = TRUE;
    }
    if (flags & OPENA8DJ_RATE_FLAG_48000) {
        printf("%s48000", printed ? ", " : "");
        printed = TRUE;
    }
    if (flags & OPENA8DJ_RATE_FLAG_88200) {
        printf("%s88200", printed ? ", " : "");
        printed = TRUE;
    }
    if (flags & OPENA8DJ_RATE_FLAG_96000) {
        printf("%s96000", printed ? ", " : "");
        printed = TRUE;
    }
    if (!printed) {
        printf("none");
    }
}

static void PrintSurface(const OPENA8DJ_WINDOWS_SURFACE *surface)
{
    printf("OpenA8DJ Windows surface\n");
    printf("  api-version:       %lu\n", surface->ApiVersion);
    printf("  driver-model:      %s\n", surface->DriverModel);
    printf("  audio-model:       %s\n", surface->AudioModel);
    printf("  streaming-model:   %s\n", surface->StreamingModel);
    printf("  safety-policy:     %s\n", surface->SafetyPolicy);
    printf("  endpoint-model:    %s\n", EndpointModelName(surface->EndpointModel));
    printf("  stable-rates:      ");
    PrintRateFlags(surface->StableSampleRateFlags);
    printf("\n");
    printf("  planned-rates:     ");
    PrintRateFlags(surface->PlannedSampleRateFlags);
    printf("\n");
    printf("  usb-transport:     %s\n", ComponentStateName(surface->UsbTransportState));
    printf("  controls:          %s\n", ComponentStateName(surface->ControlState));
    printf("  audio-endpoints:   %s\n", ComponentStateName(surface->AudioEndpointState));
    printf("  isoch-engine:      %s\n", ComponentStateName(surface->IsochronousEngineState));
    printf("  midi:              %s\n", ComponentStateName(surface->MidiState));
    printf("  asio:              %s\n", ComponentStateName(surface->AsioState));
    printf("  flags:             0x%08lx\n", surface->SurfaceFlags);
}

static void PrintUsbInfo(const OPENA8DJ_USB_INFO *info)
{
    printf("OpenA8DJ USB transport\n");
    printf("  usb-id:            %04x:%04x\n", info->VendorId, info->ProductId);
    printf("  interfaces-total:  %u\n", info->TotalInterfaceCount);
    printf("  interfaces-config: %u\n", info->ConfiguredInterfaceCount);
    printf("  pipes-config:      %u\n", info->ConfiguredPipeCount);
    printf("  alt-settings:      %u\n", info->AlternateSettingCount);
    printf("  alt-selected:      %u\n", info->SelectedAlternateSetting);
    printf("  bulk-out:          %s\n", info->HasBulkOut ? "yes" : "no");
    printf("  bulk-in:           %s\n", info->HasBulkIn ? "yes" : "no");
    printf("  iso-in:            %s\n", info->HasIsoIn ? "yes" : "no");
    printf("  iso-out:           %s\n", info->HasIsoOut ? "yes" : "no");
    printf("  ep1-reader-config: 0x%08lx\n", info->Ep1ReaderConfigNtStatus);
    printf("  ep1-reader-start:  0x%08lx\n", info->Ep1ReaderStartNtStatus);
    printf("  ep1-reader-reads:  %lu\n", info->Ep1ReaderCompletions);
    printf("  ep1-reader-zero:   %lu\n", info->Ep1ReaderZeroReads);
    printf("  ep1-reader-bytes:  %lu\n", info->Ep1ReaderBytes);
}

static void PrintIsoCaptureSnapshot(const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *snapshot)
{
    unsigned int index;

    printf("OpenA8DJ ISO capture snapshot\n");
    printf("  nt-status:         0x%08lx\n", snapshot->NtStatus);
    printf("  max-packet:        %lu\n", snapshot->MaximumPacketSize);
    printf("  transfer-bytes:    %lu\n", snapshot->TransferBufferLength);
    printf("  packet-count:      %lu\n", snapshot->PacketCount);
    printf("  error-count:       %lu\n", snapshot->ErrorCount);
    for (index = 0; index < snapshot->PacketCount && index < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; index++) {
        printf("  packet[%u]:         offset=%lu requested=%lu completed=%lu usbd=0x%08lx\n",
               index,
               snapshot->Packets[index].Offset,
               snapshot->Packets[index].RequestedLength,
               snapshot->Packets[index].CompletedLength,
               snapshot->Packets[index].UsbdStatus);
    }
    printf("  first-bytes:       ");
    for (index = 0; index < sizeof(snapshot->FirstBytes); index++) {
        printf("%02x", snapshot->FirstBytes[index]);
        if ((index + 1u) < sizeof(snapshot->FirstBytes)) {
            printf(" ");
        }
    }
    printf("\n");
}

static void PrintIsoSilencePulse(const OPENA8DJ_ISO_SILENCE_PULSE *pulse)
{
    printf("OpenA8DJ ISO silence pulse\n");
    printf("  nt-status:         0x%08lx\n", pulse->NtStatus);
    printf("  playback-status:   0x%08lx\n", pulse->PlaybackNtStatus);
    printf("  playback-errors:   %lu\n", pulse->PlaybackErrorCount);
    printf("  playback-bytes:    %lu\n", pulse->PlaybackTransferBufferLength);
    PrintIsoCaptureSnapshot(&pulse->Capture);
}

static void PrintIsoToneBurst(const OPENA8DJ_ISO_TONE_BURST *burst)
{
    printf("OpenA8DJ ISO tone burst\n");
    printf("  nt-status:         0x%08lx\n", burst->NtStatus);
    printf("  requested:         %lu\n", burst->RequestedTransfers);
    printf("  completed:         %lu\n", burst->CompletedTransfers);
    printf("  pair-index:        %lu\n", burst->PairIndex);
    printf("  amplitude-q15:     %lu\n", burst->AmplitudeQ15);
    printf("  packet-bytes:      %lu\n", burst->PacketBytes);
    printf("  period-samples:    %lu\n", burst->PeriodSamples);
    printf("  first-capture:     0x%08lx\n", burst->FirstCaptureNtStatus);
    printf("  first-playback:    0x%08lx\n", burst->FirstPlaybackNtStatus);
    printf("  last-capture:      0x%08lx\n", burst->LastCaptureNtStatus);
    printf("  last-playback:     0x%08lx\n", burst->LastPlaybackNtStatus);
    printf("  capture-errors:    %lu\n", burst->CaptureErrorCount);
    printf("  playback-errors:   %lu\n", burst->PlaybackErrorCount);
    printf("  playback-bytes:    %lu\n", burst->PlaybackBytes);
    PrintIsoCaptureSnapshot(&burst->LastCapture);
}

static void PrintBytes(const UCHAR *bytes, ULONG length)
{
    ULONG index;
    for (index = 0; index < length; index++) {
        printf("%02x", bytes[index]);
        if ((index + 1u) < length) {
            printf(" ");
        }
    }
}

static void PrintAudioParamsResult(const OPENA8DJ_AUDIO_PARAMS_RESULT *result)
{
    printf("OpenA8DJ audio params\n");
    printf("  sample-rate:       %lu\n", result->SampleRate);
    printf("  rate-code:         %u\n", result->RateCode);
    printf("  depth:             %u\n", result->Depth);
    printf("  bytes-per-packet:  %u\n", result->BytesPerPacket);
    printf("  info-write:        0x%08lx\n", result->DeviceInfoWriteNtStatus);
    printf("  info-read:         0x%08lx\n", result->DeviceInfoReadNtStatus);
    printf("  info-reply:        ");
    PrintBytes(result->DeviceInfoReply, result->DeviceInfoReplyLength < sizeof(result->DeviceInfoReply) ?
               result->DeviceInfoReplyLength : (ULONG)sizeof(result->DeviceInfoReply));
    printf("\n");
    printf("  reset-write:       0x%08lx\n", result->ResetWriteNtStatus);
    printf("  reset-read:        0x%08lx\n", result->ResetReadNtStatus);
    printf("  reset-status:      0x%08lx\n", result->ResetNtStatus);
    printf("  reset-reply:       ");
    PrintBytes(result->ResetReply, result->ResetReplyLength < sizeof(result->ResetReply) ?
               result->ResetReplyLength : (ULONG)sizeof(result->ResetReply));
    printf("\n");
    printf("  set-write:         0x%08lx\n", result->SetWriteNtStatus);
    printf("  set-read:          0x%08lx\n", result->SetReadNtStatus);
    printf("  set-status:        0x%08lx\n", result->SetNtStatus);
    printf("  set-reply:         ");
    PrintBytes(result->SetReply, result->SetReplyLength < sizeof(result->SetReply) ?
               result->SetReplyLength : (ULONG)sizeof(result->SetReply));
    printf("\n");
}

static void PrintControlState(const OPENA8DJ_CONTROL_STATE *state)
{
    printf("Audio 8 DJ controls\n");
    printf("  input-mode:        %u (%s)\n", state->InputMode, InputModeName(state->InputMode));
    printf("  gnd-vinyl:         %s\n", state->GndLiftTCVinyl ? "on" : "off");
    printf("  gnd-cd-line:       %s\n", state->GndLiftTCCDLine ? "on" : "off");
    printf("  gnd-phono:         %s\n", state->GndLiftPhono ? "on" : "off");
    printf("  software-lock:     %s\n", state->SoftwareLock ? "on" : "off");
}

static void PrintCapabilities(const OPENA8DJ_CAPABILITIES *capabilities)
{
    DWORD i;
    BOOL printedRate = FALSE;

    printf("OpenA8DJ Windows capabilities\n");
    printf("  api-version:       %lu\n", capabilities->ApiVersion);
    printf("  usb-id:            %04x:%04x\n", capabilities->VendorId, capabilities->ProductId);
    printf("  inputs:            %u\n", capabilities->InputChannels);
    printf("  outputs:           %u\n", capabilities->OutputChannels);
    printf("  stereo-pairs:      %u\n", capabilities->StereoPairs);
    printf("  hardware-midi:     %u in / %u out\n", capabilities->MidiInputs, capabilities->MidiOutputs);
    printf("  buffer-frames:     %lu-%lu default=%lu\n",
           capabilities->MinBufferFrames,
           capabilities->MaxBufferFrames,
           capabilities->DefaultBufferFrames);
    printf("  sample-rates:      ");
    for (i = 0; i < OPENA8DJ_SAMPLE_RATE_COUNT; i++) {
        if (capabilities->SampleRates[i] == 0) {
            continue;
        }
        printf("%s%lu", printedRate ? ", " : "", capabilities->SampleRates[i]);
        printedRate = TRUE;
    }
    if (!printedRate) {
        printf("none");
    }
    printf("\n");
    printf("  experimental:      %s\n", capabilities->Experimental ? "yes" : "no");
    printf("  audio-endpoint:    %s\n", capabilities->WindowsAudioEndpointExposed ? "yes" : "not-yet");
    printf("  usb-transport:     %s\n", capabilities->UsbTransportReady ? "ready" : "not-ready");
    printf("  controls-hardware: %s\n", capabilities->ControlsReady ? "ready" : "not-ready");
    printf("  midi-driver:       %s\n", capabilities->MidiReady ? "ready" : "not-yet");
    for (i = 0; i < OPENA8DJ_STEREO_PAIRS; i++) {
        printf("  input-pair-%lu:      %s\n", i + 1, capabilities->InputPairNames[i]);
    }
    for (i = 0; i < OPENA8DJ_STEREO_PAIRS; i++) {
        printf("  output-pair-%lu:     %s\n", i + 1, capabilities->OutputPairNames[i]);
    }
}

static void PrintFormat(const OPENA8DJ_AUDIO_FORMAT *format)
{
    printf("Audio format\n");
    printf("  sample-rate:       %lu\n", format->SampleRate);
    printf("  inputs:            %lu\n", format->InputChannels);
    printf("  outputs:           %lu\n", format->OutputChannels);
    printf("  buffer-frames:     %lu\n", format->BufferFrames);
}

static void PrintStreamState(const OPENA8DJ_STREAM_STATE *state)
{
    printf("Stream state\n");
    printf("  streaming:         %s\n", state->Streaming ? "yes" : "no");
    printf("  engine-ready:      %s\n", state->StreamingEngineReady ? "yes" : "no");
    printf("  sample-rate:       %lu\n", state->SampleRate);
    printf("  buffer-frames:     %lu\n", state->BufferFrames);
    printf("  render-frames:     %llu\n", state->RenderFramesSubmitted);
    printf("  capture-frames:    %llu\n", state->CaptureFramesDelivered);
    printf("  underruns:         %llu\n", state->UsbUnderruns);
    printf("  overruns:          %llu\n", state->UsbOverruns);
    printf("  usb-in-packets:    %llu\n", state->UsbInPacketsCompleted);
    printf("  usb-out-packets:   %llu\n", state->UsbOutPacketsCompleted);
    printf("  packet-errors:     %llu\n", state->UsbPacketErrors);
    printf("  late-completions:  %llu\n", state->LateCompletions);
}

static void PrintTopology(const OPENA8DJ_TOPOLOGY *topology)
{
    DWORD i;

    printf("OpenA8DJ Windows topology\n");
    printf("  api-version:       %lu\n", topology->ApiVersion);
    printf("  endpoint-model:    %s\n", EndpointModelName(topology->EndpointModel));
    printf("  render-endpoints:  %lu\n", topology->RenderEndpointCount);
    printf("  capture-endpoints: %lu\n", topology->CaptureEndpointCount);
    printf("  render-channels:   %lu\n", topology->RenderChannelCount);
    printf("  capture-channels:  %lu\n", topology->CaptureChannelCount);
    printf("  render-name:       %s\n", topology->RenderEndpointName);
    printf("  capture-name:      %s\n", topology->CaptureEndpointName);
    for (i = 0; i < OPENA8DJ_TOTAL_CHANNELS; i++) {
        printf("  channel-%02lu:       %-7s pair=%lu pair-channel=%lu name=%s\n",
               i + 1,
               DirectionName(topology->Channels[i].Direction),
               topology->Channels[i].PairIndex + 1,
               topology->Channels[i].PairChannelIndex + 1,
               topology->Channels[i].Name);
    }
}

static void PrintDiagnostics(const OPENA8DJ_DIAGNOSTICS *diagnostics)
{
    DWORD pairIndex;
    DWORD byteIndex;

    printf("OpenA8DJ Windows diagnostics\n");
    printf("  api-version:       %lu\n", diagnostics->ApiVersion);
    printf("  start-requests:    %llu\n", diagnostics->StartRequests);
    printf("  rejected-starts:   %llu\n", diagnostics->RejectedStartRequests);
    printf("  stop-requests:     %llu\n", diagnostics->StopRequests);
    printf("  format-changes:    %llu\n", diagnostics->FormatChanges);
    printf("  control-writes:    %llu\n", diagnostics->ControlWrites);
    printf("  profile-applies:   %llu\n", diagnostics->ProfileApplies);
    printf("  controls-hardware: %s\n", diagnostics->ControlsHardwareReady ? "ready" : "not-ready");
    printf("  ctl-read-status:   0x%08lx\n", diagnostics->LastControlReadNtStatus);
    printf("  ctl-write-status:  0x%08lx\n", diagnostics->LastControlWriteNtStatus);
    printf("  ctl-rdbk-status:   0x%08lx\n", diagnostics->LastControlReadbackNtStatus);
    printf("  ctl-rdbk-mismatch: %s\n", diagnostics->LastControlWriteMismatch ? "yes" : "no");
    printf("  ctl-raw:           ");
    for (byteIndex = 0; byteIndex < sizeof(diagnostics->RawControlState); byteIndex++) {
        printf("%02x%s", diagnostics->RawControlState[byteIndex], (byteIndex + 1u) < sizeof(diagnostics->RawControlState) ? " " : "\n");
    }
    printf("  ctl-write-req:     ");
    for (byteIndex = 0; byteIndex < sizeof(diagnostics->LastControlWriteRequest); byteIndex++) {
        printf("%02x%s", diagnostics->LastControlWriteRequest[byteIndex], (byteIndex + 1u) < sizeof(diagnostics->LastControlWriteRequest) ? " " : "\n");
    }
    printf("  ctl-write-rdbk:    ");
    for (byteIndex = 0; byteIndex < sizeof(diagnostics->LastControlWriteReadBack); byteIndex++) {
        printf("%02x%s", diagnostics->LastControlWriteReadBack[byteIndex], (byteIndex + 1u) < sizeof(diagnostics->LastControlWriteReadBack) ? " " : "\n");
    }
    PrintStreamState(&diagnostics->StreamState);
    printf("  acx-create-stream: %llu\n", diagnostics->AcxCreateStreamCallbacks);
    printf("  acx-prepare:       %llu\n", diagnostics->AcxPrepareCallbacks);
    printf("  acx-release:       %llu\n", diagnostics->AcxReleaseCallbacks);
    printf("  acx-run:           %llu\n", diagnostics->AcxRunCallbacks);
    printf("  acx-pause:         %llu\n", diagnostics->AcxPauseCallbacks);
    printf("  acx-latency:       %llu\n", diagnostics->AcxLatencyCallbacks);
    printf("  acx-alloc-packets: %llu\n", diagnostics->AcxAllocatePacketCallbacks);
    printf("  acx-free-packets:  %llu\n", diagnostics->AcxFreePacketCallbacks);
    printf("  acx-set-render:    %llu\n", diagnostics->AcxSetRenderPacketCallbacks);
    printf("  acx-get-current:   %llu\n", diagnostics->AcxGetCurrentPacketCallbacks);
    printf("  acx-get-capture:   %llu\n", diagnostics->AcxGetCapturePacketCallbacks);
    printf("  acx-get-position:  %llu\n", diagnostics->AcxGetPresentationPositionCallbacks);
    printf("  acx-rt-frames:     %llu\n", diagnostics->AcxRtFramesRead);
    printf("  acx-rt-nonzero:    %llu\n", diagnostics->AcxRtNonZeroFrames);
    printf("  acx-rt-peak-s24:   %lu\n", diagnostics->AcxRtPeakAbsS24);
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        printf("  acx-render-pair-%lu: nonzero=%llu peak-s24=%lu\n",
               pairIndex + 1u,
               diagnostics->AcxRtRenderPairNonZeroFrames[pairIndex],
               diagnostics->AcxRtRenderPairPeakAbsS24[pairIndex]);
    }
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        printf("  acx-capture-pair-%lu: nonzero=%llu peak-s16=%lu\n",
               pairIndex + 1u,
               diagnostics->AcxRtCapturePairNonZeroFrames[pairIndex],
               diagnostics->AcxRtCapturePairPeakAbsS16[pairIndex]);
    }
    printf("  acx-rt-channels:   %lu\n", diagnostics->AcxRtChannels);
    printf("  acx-rt-blockalign: %lu\n", diagnostics->AcxRtBlockAlign);
    printf("  acx-rt-bits:       %lu\n", diagnostics->AcxRtBitsPerSample);
    printf("  acx-rt-float:      %lu\n", diagnostics->AcxRtIsFloat);
    printf("  acx-rt-packets:    %lu\n", diagnostics->AcxRtPacketCount);
    printf("  acx-rt-packet-size:%lu\n", diagnostics->AcxRtPacketSize);
    printf("  acx-rt-frame-count:%lu\n", diagnostics->AcxRtFrameCount);
    printf("  acx-last-render-pkt:%lu flags=0x%08lx eos=%lu\n",
           diagnostics->AcxLastSetRenderPacket,
           diagnostics->AcxLastSetRenderFlags,
           diagnostics->AcxLastSetRenderEosPacketLength);
    printf("  acx-render-pkt-done:%llu\n", diagnostics->AcxRtRenderPacketCompletions);
    printf("  acx-render-pkt-notify:%llu\n", diagnostics->AcxRtRenderPacketNotifications);
    printf("  acx-render-pkt-notify-fail:%llu\n", diagnostics->AcxRtRenderPacketNotificationFailures);
    printf("  acx-render-current:%lu\n", diagnostics->AcxRtRenderCurrentPacket);
    printf("  acx-render-notify-status:0x%08lx\n", diagnostics->AcxRtRenderLastNotificationNtStatus);
    printf("  worker-iterations: %llu\n", diagnostics->StreamWorkerIterations);
    printf("  worker-cap-bytes:  %llu\n", diagnostics->StreamWorkerCaptureBytes);
    printf("  worker-play-bytes: %llu\n", diagnostics->StreamWorkerPlaybackBytes);
    printf("  worker-no-render:  %llu\n", diagnostics->StreamWorkerNoRenderIterations);
    printf("  worker-last-cap:   %lu\n", diagnostics->StreamWorkerLastCaptureBytes);
    printf("  worker-last-play:  %lu\n", diagnostics->StreamWorkerLastPlaybackBytes);
    printf("  worker-render-mask:0x%08lx\n", diagnostics->StreamWorkerLastRenderMask);
    printf("  worker-capt-mask:  0x%08lx\n", diagnostics->StreamWorkerLastCaptureMask);
    printf("  worker-max-cap:    %lu\n", diagnostics->StreamWorkerMaxCaptureBytes);
    printf("  worker-max-play:   %lu\n", diagnostics->StreamWorkerMaxPlaybackBytes);
    printf("  iso-packets/slots: %lu/%lu\n",
           diagnostics->PersistentIsoPacketCount,
           diagnostics->PersistentIsoSlotCount);
    printf("  iso-output-lead:   %lu frames\n", diagnostics->PersistentIsoOutputLeadFrames);
    printf("  iso-qpc-frequency: %llu\n", diagnostics->IsoQpcFrequency);
    printf("  iso-out-empty:     %llu\n", diagnostics->IsoOutputQueueEmptyTransitions);
    printf("  iso-out-late:      %llu\n", diagnostics->IsoOutputLatePackets);
    printf("  iso-out-bad-start: %llu\n", diagnostics->IsoOutputBadStartFrames);
    printf("  iso-out-other-err: %llu\n", diagnostics->IsoOutputOtherPacketErrors);
    printf("  iso-output-panic:  %llu\n", diagnostics->IsoOutputPanicFlags);
    printf("  iso-cap-late:      %llu\n", diagnostics->IsoCaptureLatePackets);
    printf("  iso-cap-bad-start: %llu\n", diagnostics->IsoCaptureBadStartFrames);
    printf("  iso-cap-other-err: %llu\n", diagnostics->IsoCaptureOtherPacketErrors);
    printf("  iso-cap-last:      urb=0x%08lx packet=0x%08lx errors=%lu\n",
           diagnostics->IsoLastCaptureUrbStatus,
           diagnostics->IsoLastCapturePacketStatus,
           diagnostics->IsoLastCaptureErrorCount);
    printf("  iso-cap-out-maxqpc:%llu\n", diagnostics->IsoCaptureToOutputSubmitMaxQpc);
    printf("  iso-last-frames:   capture=%lu output=%lu\n",
           diagnostics->IsoLastCaptureStartFrame,
           diagnostics->IsoLastOutputStartFrame);
    printf("  rate-settle-runs:  %llu\n", diagnostics->AudioRateSettleRuns);
    printf("  rate-settle-snaps: %llu\n", diagnostics->AudioRateSettleSnapshots);
    printf("  rate-settle-mismatch:%llu\n", diagnostics->AudioRateSettleMismatchedPackets);
    printf("  rate-settle-fails: %llu\n", diagnostics->AudioRateSettleFailures);
    printf("  rate-settle-last:  %lu bytes\n", diagnostics->AudioRateSettleLastObservedBytes);
    printf("  iso-frame-query:   runs=%llu failures=%llu current=%lu seed=%lu next=%lu\n",
           diagnostics->IsoCaptureFrameQueries,
           diagnostics->IsoCaptureFrameQueryFailures,
           diagnostics->IsoCaptureFrameQueryCurrent,
           diagnostics->IsoCaptureSeedStartFrame,
           diagnostics->IsoCaptureNextStartFrame);
    printf("  iso-input-reset:   runs=%llu failures=%llu status=0x%08lx\n",
           diagnostics->IsoInputPipeResetRuns,
           diagnostics->IsoInputPipeResetFailures,
           diagnostics->IsoInputPipeResetLastNtStatus);
    printf("  iso-output-reset:  runs=%llu failures=%llu status=0x%08lx\n",
           diagnostics->IsoOutputPipeResetRuns,
           diagnostics->IsoOutputPipeResetFailures,
           diagnostics->IsoOutputPipeResetLastNtStatus);
    printf("  iso-transport:     healthy=%lu generation=%ld one-shot=%lu input-start=0x%08lx output-start=0x%08lx\n",
           diagnostics->IsoTransportHealthy,
           diagnostics->IsoTransportHealthyGeneration,
           diagnostics->IsoOneShotActive,
           diagnostics->IsoInputTargetStartLastNtStatus,
           diagnostics->IsoOutputTargetStartLastNtStatus);
    printf("  iso-cap-error-slot:%lu generation=%ld sequence=%llu frame=%lu snapshot=%lu\n",
           diagnostics->IsoLastCaptureErrorSlot,
           diagnostics->IsoLastCaptureErrorGeneration,
           diagnostics->IsoLastCaptureErrorSubmitSequence,
           diagnostics->IsoLastCaptureErrorScheduledStartFrame,
           diagnostics->IsoCaptureErrorSnapshotSequence);
    printf("  iso-cap-error-pkts:first=%lu last=%lu count=%lu\n",
           diagnostics->IsoLastCaptureErrorFirstPacket,
           diagnostics->IsoLastCaptureErrorLastPacket,
           diagnostics->IsoLastCaptureErrorPacketCount);
    printf("  iso-cap-error-qpc: submit=%llu complete=%llu\n",
           diagnostics->IsoLastCaptureErrorSubmitQpc,
           diagnostics->IsoLastCaptureErrorCompletionQpc);
}

static void PrintRenderTrace(const OPENA8DJ_RENDER_TRACE *trace)
{
    DWORD index;

    printf("OpenA8DJ render trace\n");
    printf("  size:              %lu\n", trace->Size);
    printf("  frame-count:       %lu\n", trace->FrameCount);
    printf("  write-index:       %lu\n", trace->WriteIndex);
    printf("  write-count:       %llu\n", trace->WriteCount);
    printf("  active-render-mask:0x%08lx\n", trace->ActiveRenderMask);
    printf("  rt-channels:       %lu\n", trace->RtChannels);
    printf("  rt-blockalign:     %lu\n", trace->RtBlockAlign);
    printf("  rt-bits:           %lu\n", trace->RtBitsPerSample);
    printf("  rt-frame-count:    %lu\n", trace->RtFrameCount);
    printf("csv,index,pair,rtCursor,rtFrameCount,outByte,pos,rawL,rawR,outL,outR,channels,blockAlign,bits\n");
    for (index = 0; index < trace->FrameCount && index < OPENA8DJ_RENDER_TRACE_FRAME_COUNT; index++) {
        const OPENA8DJ_RENDER_TRACE_FRAME *frame = &trace->Frames[index];

        printf(
            "csv,%lu,%lu,%lu,%lu,%lu,%llu,%ld,%ld,%ld,%ld,%lu,%lu,%lu\n",
            index,
            frame->PairIndex,
            frame->RtFrameCursor,
            frame->RtFrameCount,
            frame->OutputByteInFrame,
            frame->PositionBlocks,
            frame->RawLeftS24,
            frame->RawRightS24,
            frame->OutputLeftS24,
            frame->OutputRightS24,
            frame->RtChannels,
            frame->RtBlockAlign,
            frame->RtBitsPerSample);
    }
}

static BOOL WriteUsbPlaybackTrace(const OPENA8DJ_USB_PLAYBACK_TRACE *trace, const char *path)
{
    FILE *file;
    size_t written;
    ULONG byteCount = trace->ByteCount;

    if (byteCount > OPENA8DJ_USB_PLAYBACK_TRACE_BYTES) {
        byteCount = OPENA8DJ_USB_PLAYBACK_TRACE_BYTES;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FALSE;
    }
    written = fwrite(trace->Bytes, 1, byteCount, file);
    if (fclose(file) != 0) {
        return FALSE;
    }

    printf("OpenA8DJ USB playback trace\n");
    printf("  path:              %s\n", path);
    printf("  size:              %lu\n", trace->Size);
    printf("  bytes:             %lu\n", byteCount);
    printf("  fixed-packet-bytes:%lu\n", trace->FixedPacketBytes);
    printf("  active-render-mask:0x%08lx\n", trace->ActiveRenderMask);
    printf("  worker-last-play:  %lu\n", trace->WorkerLastPlaybackBytes);
    printf("  write-count:       %llu\n", trace->WorkerIteration);
    return written == byteCount;
}

static BOOL ParseBool(const char *text, UCHAR *value)
{
    if (strcmp(text, "on") == 0 || strcmp(text, "1") == 0 || strcmp(text, "true") == 0) {
        *value = 1;
        return TRUE;
    }
    if (strcmp(text, "off") == 0 || strcmp(text, "0") == 0 || strcmp(text, "false") == 0) {
        *value = 0;
        return TRUE;
    }
    return FALSE;
}

static BOOL ParseInputMode(const char *text, UCHAR *mode)
{
    if (strcmp(text, "0") == 0 || strcmp(text, "timecode-vinyl") == 0) {
        *mode = 0;
        return TRUE;
    }
    if (strcmp(text, "1") == 0 || strcmp(text, "timecode-cd-line") == 0) {
        *mode = 1;
        return TRUE;
    }
    if (strcmp(text, "2") == 0 || strcmp(text, "phono") == 0) {
        *mode = 2;
        return TRUE;
    }
    return FALSE;
}

static BOOL ParseProfile(const char *text, ULONG *profile)
{
    if (strcmp(text, "timecode-vinyl") == 0) {
        *profile = OPENA8DJ_PROFILE_TIMECODE_VINYL;
        return TRUE;
    }
    if (strcmp(text, "timecode-cd-line") == 0) {
        *profile = OPENA8DJ_PROFILE_TIMECODE_CD_LINE;
        return TRUE;
    }
    if (strcmp(text, "phono") == 0) {
        *profile = OPENA8DJ_PROFILE_PHONO;
        return TRUE;
    }
    if (strcmp(text, "unlock") == 0) {
        *profile = OPENA8DJ_PROFILE_UNLOCK;
        return TRUE;
    }
    return FALSE;
}

static BOOL ParseCanaryPhase(const char *text, ULONG *phase)
{
    if (strcmp(text, "control-read") == 0) {
        *phase = OPENA8DJ_CANARY_PHASE_CONTROL_READ;
    } else if (strcmp(text, "iso-capture") == 0) {
        *phase = OPENA8DJ_CANARY_PHASE_ISO_CAPTURE;
    } else if (strcmp(text, "iso-output") == 0) {
        *phase = OPENA8DJ_CANARY_PHASE_ISO_OUTPUT;
    } else if (strcmp(text, "streaming") == 0) {
        *phase = OPENA8DJ_CANARY_PHASE_STREAMING;
    } else {
        return FALSE;
    }
    return TRUE;
}

static void PrintSafetyState(const OPENA8DJ_SAFETY_STATE *state)
{
    printf("OpenA8DJ physical safety state\n");
    printf("  api-version:       %lu\n", state->ApiVersion);
    printf("  build-fingerprint: %s\n", state->BuildFingerprint);
    printf("  armed-phase:       %lu\n", state->ArmedPhase);
    printf("  operations-left:   %lu\n", state->OperationsRemaining);
    printf("  nonce:             %016llx%016llx\n", state->NonceHigh, state->NonceLow);
    printf("  checkpoint:        %lu\n", state->LastCheckpoint);
    printf("  checkpoint-seq:    %lu\n", state->LastCheckpointSequence);
    printf("  checkpoint-status: 0x%08lx\n", state->LastNtStatus);
    printf("  prepared/stopping/worker: %lu/%lu/%lu\n",
        state->DevicePrepared, state->DeviceStopping, state->StreamWorkerActive);
}

static void Usage(const char *argv0)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s\n", argv0);
    fprintf(stderr, "  %s status\n", argv0);
    fprintf(stderr, "  %s surface\n", argv0);
    fprintf(stderr, "  %s usb\n", argv0);
    fprintf(stderr, "  %s topology\n", argv0);
    fprintf(stderr, "  %s diagnostics\n", argv0);
    fprintf(stderr, "  %s caps|capabilities\n", argv0);
    fprintf(stderr, "  %s controls\n", argv0);
    fprintf(stderr, "  %s format\n", argv0);
    fprintf(stderr, "  %s stream\n", argv0);
    fprintf(stderr, "  %s render-trace\n", argv0);
    fprintf(stderr, "  %s usb-playback-trace PATH\n", argv0);
    fprintf(stderr, "  %s safety\n", argv0);
    fprintf(stderr, "  %s arm control-read|iso-capture|iso-output|streaming NONCE_HIGH NONCE_LOW\n", argv0);
    fprintf(stderr, "  %s disarm\n", argv0);
    fprintf(stderr, "  %s profile timecode-vinyl|timecode-cd-line|phono|unlock\n", argv0);
    fprintf(stderr, "  %s input-mode 0|1|2|timecode-vinyl|timecode-cd-line|phono\n", argv0);
    fprintf(stderr, "  %s gnd-vinyl on|off\n", argv0);
    fprintf(stderr, "  %s gnd-cd-line on|off\n", argv0);
    fprintf(stderr, "  %s gnd-phono on|off\n", argv0);
    fprintf(stderr, "  %s software-lock on|off\n", argv0);
    fprintf(stderr, "  %s set-format 44100|48000 15..4096\n", argv0);
    fprintf(stderr, "  %s iso-capture\n", argv0);
    fprintf(stderr, "  %s iso-silence\n", argv0);
    fprintf(stderr, "  %s iso-tone [transfers 1..250] [pair 0..3] [amplitudeQ15 1..8192] [packetBytes 0|1..512] [period 40|48|56|64]\n", argv0);
    fprintf(stderr, "      iso-tone defaults: transfers=50 pair=0 amplitudeQ15=4096 packetBytes=352 period=40\n");
    fprintf(stderr, "  %s start|stop\n", argv0);
}

int main(int argc, char **argv)
{
    HANDLE device;
    OPENA8DJ_CONTROL_STATE controls;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        Usage(argv[0]);
        return 0;
    }

    device = OpenDevice();
    if (device == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "OpenA8DJ Windows driver interface not found. Install OpenA8DJUsb first.\n");
        return 1;
    }

    if (argc == 1 || strcmp(argv[1], "status") == 0) {
        OPENA8DJ_WINDOWS_SURFACE surface;
        OPENA8DJ_USB_INFO usbInfo;
        OPENA8DJ_CAPABILITIES caps;
        OPENA8DJ_AUDIO_FORMAT format;
        OPENA8DJ_STREAM_STATE stream;

        ZeroMemory(&surface, sizeof(surface));
        ZeroMemory(&usbInfo, sizeof(usbInfo));
        ZeroMemory(&caps, sizeof(caps));
        ZeroMemory(&format, sizeof(format));
        ZeroMemory(&stream, sizeof(stream));
        ZeroMemory(&controls, sizeof(controls));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_SURFACE, NULL, 0, &surface, sizeof(surface));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_USB_INFO, NULL, 0, &usbInfo, sizeof(usbInfo));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_CAPABILITIES, NULL, 0, &caps, sizeof(caps));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_AUDIO_FORMAT, NULL, 0, &format, sizeof(format));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_STREAM_STATE, NULL, 0, &stream, sizeof(stream));
        controls.Size = sizeof(controls);
        PrintSurface(&surface);
        PrintUsbInfo(&usbInfo);
        PrintCapabilities(&caps);
        PrintFormat(&format);
        PrintControlState(&controls);
        PrintStreamState(&stream);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "safety") == 0 && argc == 2) {
        OPENA8DJ_SAFETY_STATE safety;
        ZeroMemory(&safety, sizeof(safety));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_SAFETY_STATE, NULL, 0, &safety, sizeof(safety))) {
            fprintf(stderr, "Could not read safety state: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintSafetyState(&safety);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "arm") == 0 && argc == 5) {
        OPENA8DJ_CANARY_ARM_REQUEST arm;
        ZeroMemory(&arm, sizeof(arm));
        arm.Size = sizeof(arm);
        arm.ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
        arm.MaxOperations = 1;
        if (!ParseCanaryPhase(argv[2], &arm.Phase)) {
            Usage(argv[0]);
            CloseHandle(device);
            return 2;
        }
        arm.NonceHigh = _strtoui64(argv[3], NULL, 0);
        arm.NonceLow = _strtoui64(argv[4], NULL, 0);
        if ((arm.NonceHigh == 0 && arm.NonceLow == 0) ||
            !DeviceIo(device, IOCTL_OPENA8DJ_ARM_PHYSICAL_CANARY, &arm, sizeof(arm), NULL, 0)) {
            fprintf(stderr, "Could not arm one-shot canary: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "disarm") == 0 && argc == 2) {
        if (!DeviceIo(device, IOCTL_OPENA8DJ_DISARM_PHYSICAL_CANARY, NULL, 0, NULL, 0)) {
            fprintf(stderr, "Could not disarm physical canary: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "usb") == 0) {
        OPENA8DJ_USB_INFO usbInfo;
        ZeroMemory(&usbInfo, sizeof(usbInfo));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_USB_INFO, NULL, 0, &usbInfo, sizeof(usbInfo))) {
            fprintf(stderr, "Could not read USB transport: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintUsbInfo(&usbInfo);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "controls") == 0) {
        ZeroMemory(&controls, sizeof(controls));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_CONTROL_STATE, NULL, 0, &controls, sizeof(controls))) {
            fprintf(stderr, "Could not read controls: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintControlState(&controls);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "iso-capture") == 0) {
        OPENA8DJ_ISO_CAPTURE_SNAPSHOT snapshot;
        ZeroMemory(&snapshot, sizeof(snapshot));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_ISO_CAPTURE_SNAPSHOT, NULL, 0, &snapshot, sizeof(snapshot))) {
            fprintf(stderr, "Could not read ISO capture snapshot: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintIsoCaptureSnapshot(&snapshot);
        CloseHandle(device);
        return snapshot.NtStatus == 0 ? 0 : 2;
    }

    if (strcmp(argv[1], "iso-silence") == 0) {
        OPENA8DJ_ISO_SILENCE_PULSE pulse;
        ZeroMemory(&pulse, sizeof(pulse));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_ISO_SILENCE_PULSE, NULL, 0, &pulse, sizeof(pulse))) {
            fprintf(stderr, "Could not send ISO silence pulse: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintIsoSilencePulse(&pulse);
        CloseHandle(device);
        return pulse.PlaybackNtStatus == 0 ? 0 : 2;
    }

    if (strcmp(argv[1], "iso-tone") == 0) {
        OPENA8DJ_ISO_TONE_BURST burst;
        ZeroMemory(&burst, sizeof(burst));
        burst.Size = sizeof(burst);
        burst.RequestedTransfers = argc > 2 ? strtoul(argv[2], NULL, 0) : 50u;
        burst.PairIndex = argc > 3 ? strtoul(argv[3], NULL, 0) : 0u;
        burst.AmplitudeQ15 = argc > 4 ? strtoul(argv[4], NULL, 0) : 4096u;
        burst.PacketBytes = argc > 5 ? strtoul(argv[5], NULL, 0) : 352u;
        burst.PeriodSamples = argc > 6 ? strtoul(argv[6], NULL, 0) : 40u;
        if (!DeviceIo(device, IOCTL_OPENA8DJ_ISO_TONE_BURST, &burst, sizeof(burst), &burst, sizeof(burst))) {
            fprintf(stderr, "Could not send ISO tone burst: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintIsoToneBurst(&burst);
        CloseHandle(device);
        return burst.NtStatus == 0 ? 0 : 2;
    }

    if (strcmp(argv[1], "audio-params") == 0) {
        OPENA8DJ_AUDIO_PARAMS_RESULT result;
        ZeroMemory(&result, sizeof(result));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_APPLY_AUDIO_PARAMS, NULL, 0, &result, sizeof(result))) {
            fprintf(stderr, "Could not apply audio params: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintAudioParamsResult(&result);
        CloseHandle(device);
        return result.SetNtStatus == 0 ? 0 : 2;
    }

    if (strcmp(argv[1], "surface") == 0) {
        OPENA8DJ_WINDOWS_SURFACE surface;
        ZeroMemory(&surface, sizeof(surface));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_SURFACE, NULL, 0, &surface, sizeof(surface))) {
            fprintf(stderr, "Could not read Windows surface: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintSurface(&surface);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "topology") == 0) {
        OPENA8DJ_TOPOLOGY topology;
        ZeroMemory(&topology, sizeof(topology));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_TOPOLOGY, NULL, 0, &topology, sizeof(topology))) {
            fprintf(stderr, "Could not read topology: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintTopology(&topology);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "diagnostics") == 0) {
        OPENA8DJ_DIAGNOSTICS diagnostics;
        ZeroMemory(&diagnostics, sizeof(diagnostics));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_DIAGNOSTICS, NULL, 0, &diagnostics, sizeof(diagnostics))) {
            fprintf(stderr, "Could not read diagnostics: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintDiagnostics(&diagnostics);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "render-trace") == 0) {
        OPENA8DJ_RENDER_TRACE trace;
        ZeroMemory(&trace, sizeof(trace));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_RENDER_TRACE, NULL, 0, &trace, sizeof(trace))) {
            fprintf(stderr, "Could not read render trace: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintRenderTrace(&trace);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "usb-playback-trace") == 0) {
        OPENA8DJ_USB_PLAYBACK_TRACE trace;
        const char *path = argc > 2 ? argv[2] : "open-a8dj-usb-playback-trace.bin";
        ZeroMemory(&trace, sizeof(trace));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_USB_PLAYBACK_TRACE, NULL, 0, &trace, sizeof(trace))) {
            fprintf(stderr, "Could not read USB playback trace: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        if (!WriteUsbPlaybackTrace(&trace, path)) {
            fprintf(stderr, "Could not write USB playback trace to %s\n", path);
            CloseHandle(device);
            return 1;
        }
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "caps") == 0 || strcmp(argv[1], "capabilities") == 0) {
        OPENA8DJ_CAPABILITIES caps;
        ZeroMemory(&caps, sizeof(caps));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_CAPABILITIES, NULL, 0, &caps, sizeof(caps))) {
            fprintf(stderr, "Could not read capabilities: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintCapabilities(&caps);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "format") == 0) {
        OPENA8DJ_AUDIO_FORMAT format;
        ZeroMemory(&format, sizeof(format));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_AUDIO_FORMAT, NULL, 0, &format, sizeof(format))) {
            fprintf(stderr, "Could not read format: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintFormat(&format);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "stream") == 0) {
        OPENA8DJ_STREAM_STATE stream;
        ZeroMemory(&stream, sizeof(stream));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_STREAM_STATE, NULL, 0, &stream, sizeof(stream))) {
            fprintf(stderr, "Could not read stream state: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintStreamState(&stream);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "profile") == 0 && argc == 3) {
        OPENA8DJ_PROFILE_REQUEST request;
        ULONG profile = 0;

        if (!ParseProfile(argv[2], &profile)) {
            Usage(argv[0]);
            CloseHandle(device);
            return 2;
        }
        ZeroMemory(&request, sizeof(request));
        request.Size = sizeof(request);
        request.Profile = profile;
        ZeroMemory(&controls, sizeof(controls));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_APPLY_PROFILE, &request, sizeof(request), &controls, sizeof(controls))) {
            fprintf(stderr, "Could not apply profile: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintControlState(&controls);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "set-format") == 0 && argc == 4) {
        OPENA8DJ_AUDIO_FORMAT inFormat;
        OPENA8DJ_AUDIO_FORMAT outFormat;

        ZeroMemory(&inFormat, sizeof(inFormat));
        ZeroMemory(&outFormat, sizeof(outFormat));
        inFormat.Size = sizeof(inFormat);
        inFormat.SampleRate = strtoul(argv[2], NULL, 10);
        inFormat.InputChannels = OPENA8DJ_INPUT_CHANNELS;
        inFormat.OutputChannels = OPENA8DJ_OUTPUT_CHANNELS;
        inFormat.BufferFrames = strtoul(argv[3], NULL, 10);
        if (!DeviceIo(device, IOCTL_OPENA8DJ_SET_AUDIO_FORMAT, &inFormat, sizeof(inFormat), &outFormat, sizeof(outFormat))) {
            fprintf(stderr, "Could not set format: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintFormat(&outFormat);
        CloseHandle(device);
        return 0;
    }

    if ((strcmp(argv[1], "start") == 0 || strcmp(argv[1], "stop") == 0) && argc == 2) {
        OPENA8DJ_STREAM_STATE stream;
        DWORD code = strcmp(argv[1], "start") == 0 ? IOCTL_OPENA8DJ_START_STREAMING : IOCTL_OPENA8DJ_STOP_STREAMING;

        ZeroMemory(&stream, sizeof(stream));
        if (!DeviceIo(device, code, NULL, 0, &stream, sizeof(stream))) {
            fprintf(stderr, "Could not change stream state: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        PrintStreamState(&stream);
        CloseHandle(device);
        return 0;
    }

    if (argc == 3) {
        UCHAR value = 0;
        ZeroMemory(&controls, sizeof(controls));
        if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_CONTROL_STATE, NULL, 0, &controls, sizeof(controls))) {
            fprintf(stderr, "Could not read controls: error=%lu\n", GetLastError());
            CloseHandle(device);
            return 1;
        }
        if (strcmp(argv[1], "input-mode") == 0) {
            if (!ParseInputMode(argv[2], &value)) {
                Usage(argv[0]);
                CloseHandle(device);
                return 2;
            }
            controls.InputMode = value;
        } else {
            if (!ParseBool(argv[2], &value)) {
                Usage(argv[0]);
                CloseHandle(device);
                return 2;
            }
            if (strcmp(argv[1], "gnd-vinyl") == 0) {
                fprintf(stderr, "Ground-lift controls are hardware readback-only on this Audio 8 DJ: error=%lu\n", (DWORD)ERROR_NOT_SUPPORTED);
                PrintControlState(&controls);
                CloseHandle(device);
                return 1;
            } else if (strcmp(argv[1], "gnd-cd-line") == 0) {
                fprintf(stderr, "Ground-lift controls are hardware readback-only on this Audio 8 DJ: error=%lu\n", (DWORD)ERROR_NOT_SUPPORTED);
                PrintControlState(&controls);
                CloseHandle(device);
                return 1;
            } else if (strcmp(argv[1], "gnd-phono") == 0) {
                fprintf(stderr, "Ground-lift controls are hardware readback-only on this Audio 8 DJ: error=%lu\n", (DWORD)ERROR_NOT_SUPPORTED);
                PrintControlState(&controls);
                CloseHandle(device);
                return 1;
            } else if (strcmp(argv[1], "software-lock") == 0) {
                controls.SoftwareLock = value;
            } else {
                Usage(argv[0]);
                CloseHandle(device);
                return 2;
            }
        }
        controls.Size = sizeof(controls);
        if (!DeviceIo(device, IOCTL_OPENA8DJ_SET_CONTROL_STATE, &controls, sizeof(controls), &controls, sizeof(controls))) {
            DWORD error = GetLastError();
            if (error == ERROR_NOT_SUPPORTED && strncmp(argv[1], "gnd-", 4) == 0) {
                fprintf(stderr, "Ground-lift controls are hardware readback-only on this Audio 8 DJ: error=%lu\n", error);
            } else {
                fprintf(stderr, "Could not write controls: error=%lu\n", error);
            }
            CloseHandle(device);
            return 1;
        }
        PrintControlState(&controls);
        CloseHandle(device);
        return 0;
    }

    Usage(argv[0]);
    CloseHandle(device);
    return 2;
}
