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
    PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = NULL;
    DWORD requiredSize = 0;
    HANDLE device = INVALID_HANDLE_VALUE;

    infoSet = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_OPENA8DJ_USB,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (infoSet == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    ZeroMemory(&interfaceData, sizeof(interfaceData));
    interfaceData.cbSize = sizeof(interfaceData);

    if (!SetupDiEnumDeviceInterfaces(infoSet, NULL, &GUID_DEVINTERFACE_OPENA8DJ_USB, 0, &interfaceData)) {
        SetupDiDestroyDeviceInfoList(infoSet);
        return INVALID_HANDLE_VALUE;
    }

    (void)SetupDiGetDeviceInterfaceDetailA(infoSet, &interfaceData, NULL, 0, &requiredSize, NULL);
    if (requiredSize == 0) {
        SetupDiDestroyDeviceInfoList(infoSet);
        return INVALID_HANDLE_VALUE;
    }

    detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)calloc(1, requiredSize);
    if (detailData == NULL) {
        SetupDiDestroyDeviceInfoList(infoSet);
        return INVALID_HANDLE_VALUE;
    }

    detailData->cbSize = sizeof(*detailData);
    if (SetupDiGetDeviceInterfaceDetailA(infoSet, &interfaceData, detailData, requiredSize, NULL, NULL)) {
        device = CreateFileA(
            detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }

    free(detailData);
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

    printf("OpenA8DJ Windows capabilities\n");
    printf("  api-version:       %lu\n", capabilities->ApiVersion);
    printf("  usb-id:            %04x:%04x\n", capabilities->VendorId, capabilities->ProductId);
    printf("  inputs:            %u\n", capabilities->InputChannels);
    printf("  outputs:           %u\n", capabilities->OutputChannels);
    printf("  stereo-pairs:      %u\n", capabilities->StereoPairs);
    printf("  midi:              %u in / %u out\n", capabilities->MidiInputs, capabilities->MidiOutputs);
    printf("  buffer-frames:     %lu-%lu default=%lu\n",
           capabilities->MinBufferFrames,
           capabilities->MaxBufferFrames,
           capabilities->DefaultBufferFrames);
    printf("  sample-rates:      ");
    for (i = 0; i < OPENA8DJ_SAMPLE_RATE_COUNT; i++) {
        printf("%s%lu", i == 0 ? "" : ", ", capabilities->SampleRates[i]);
    }
    printf("\n");
    printf("  experimental:      %s\n", capabilities->Experimental ? "yes" : "no");
    printf("  audio-endpoint:    %s\n", capabilities->WindowsAudioEndpointExposed ? "yes" : "not-yet");
    printf("  usb-transport:     %s\n", capabilities->UsbTransportReady ? "ready" : "not-ready");
    printf("  controls:          %s\n", capabilities->ControlsReady ? "ready" : "not-ready");
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
    printf("  sample-rate:       %lu\n", state->SampleRate);
    printf("  buffer-frames:     %lu\n", state->BufferFrames);
    printf("  render-frames:     %llu\n", state->RenderFramesSubmitted);
    printf("  capture-frames:    %llu\n", state->CaptureFramesDelivered);
    printf("  underruns:         %llu\n", state->UsbUnderruns);
    printf("  overruns:          %llu\n", state->UsbOverruns);
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

static void Usage(const char *argv0)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s\n", argv0);
    fprintf(stderr, "  %s status\n", argv0);
    fprintf(stderr, "  %s caps\n", argv0);
    fprintf(stderr, "  %s format\n", argv0);
    fprintf(stderr, "  %s stream\n", argv0);
    fprintf(stderr, "  %s profile timecode-vinyl|timecode-cd-line|phono|unlock\n", argv0);
    fprintf(stderr, "  %s input-mode 0|1|2|timecode-vinyl|timecode-cd-line|phono\n", argv0);
    fprintf(stderr, "  %s gnd-vinyl on|off\n", argv0);
    fprintf(stderr, "  %s gnd-cd-line on|off\n", argv0);
    fprintf(stderr, "  %s gnd-phono on|off\n", argv0);
    fprintf(stderr, "  %s software-lock on|off\n", argv0);
    fprintf(stderr, "  %s set-format 44100|48000|88200|96000 15..4096\n", argv0);
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

    ZeroMemory(&controls, sizeof(controls));
    if (!DeviceIo(device, IOCTL_OPENA8DJ_GET_CONTROL_STATE, NULL, 0, &controls, sizeof(controls))) {
        fprintf(stderr, "Could not read controls: error=%lu\n", GetLastError());
        CloseHandle(device);
        return 1;
    }

    if (argc == 1 || strcmp(argv[1], "status") == 0) {
        OPENA8DJ_CAPABILITIES caps;
        OPENA8DJ_AUDIO_FORMAT format;
        OPENA8DJ_STREAM_STATE stream;

        ZeroMemory(&caps, sizeof(caps));
        ZeroMemory(&format, sizeof(format));
        ZeroMemory(&stream, sizeof(stream));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_CAPABILITIES, NULL, 0, &caps, sizeof(caps));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_AUDIO_FORMAT, NULL, 0, &format, sizeof(format));
        (void)DeviceIo(device, IOCTL_OPENA8DJ_GET_STREAM_STATE, NULL, 0, &stream, sizeof(stream));
        PrintCapabilities(&caps);
        PrintFormat(&format);
        PrintControlState(&controls);
        PrintStreamState(&stream);
        CloseHandle(device);
        return 0;
    }

    if (strcmp(argv[1], "caps") == 0) {
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
                controls.GndLiftTCVinyl = value;
            } else if (strcmp(argv[1], "gnd-cd-line") == 0) {
                controls.GndLiftTCCDLine = value;
            } else if (strcmp(argv[1], "gnd-phono") == 0) {
                controls.GndLiftPhono = value;
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
            fprintf(stderr, "Could not write controls: error=%lu\n", GetLastError());
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
