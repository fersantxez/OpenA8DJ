#include <initguid.h>
#include "OpenA8DJUsb.h"

static const ULONG kOpenA8DJStableSampleRates[OPENA8DJ_STABLE_SAMPLE_RATE_COUNT] = {
    44100,
    48000
};

static VOID
OpenA8DJ_SetRawFlag(_Inout_ UCHAR *Value, _In_ UCHAR Mask, _In_ BOOLEAN Enabled)
{
    if (Enabled) {
        *Value = (UCHAR)(*Value | Mask);
    } else {
        *Value = (UCHAR)(*Value & (UCHAR)~Mask);
    }
}

static VOID
OpenA8DJ_CopyString(_Out_writes_(OPENA8DJ_CHANNEL_NAME_LENGTH) CHAR *Destination, _In_z_ const CHAR *Source)
{
    SIZE_T index;

    RtlZeroMemory(Destination, OPENA8DJ_CHANNEL_NAME_LENGTH);
    for (index = 0; index + 1 < OPENA8DJ_CHANNEL_NAME_LENGTH && Source[index] != '\0'; index++) {
        Destination[index] = Source[index];
    }
}

static VOID
OpenA8DJ_CopyFixedString(_Out_writes_(Length) CHAR *Destination, _In_ SIZE_T Length, _In_z_ const CHAR *Source)
{
    SIZE_T index;

    RtlZeroMemory(Destination, Length);
    for (index = 0; index + 1 < Length && Source[index] != '\0'; index++) {
        Destination[index] = Source[index];
    }
}

static VOID
OpenA8DJ_InitializeDefaults(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    RtlZeroMemory(Context->RawControlState, sizeof(Context->RawControlState));
    Context->RawControlState[0] = 0;
    Context->RawControlState[1] = 2;
    Context->RawControlState[2] = 3;
    Context->RawControlState[3] = 0;
    Context->RawControlState[4] = 2;
    Context->RawControlState[5] = 0;

    RtlZeroMemory(&Context->CurrentFormat, sizeof(Context->CurrentFormat));
    Context->CurrentFormat.Size = sizeof(Context->CurrentFormat);
    Context->CurrentFormat.SampleRate = OPENA8DJ_DEFAULT_SAMPLE_RATE;
    Context->CurrentFormat.InputChannels = OPENA8DJ_INPUT_CHANNELS;
    Context->CurrentFormat.OutputChannels = OPENA8DJ_OUTPUT_CHANNELS;
    Context->CurrentFormat.BufferFrames = OPENA8DJ_DEFAULT_BUFFER_FRAMES;

    RtlZeroMemory(&Context->StreamState, sizeof(Context->StreamState));
    Context->StreamState.Size = sizeof(Context->StreamState);
    Context->StreamState.StreamingEngineReady = FALSE;
    Context->StreamState.SampleRate = Context->CurrentFormat.SampleRate;
    Context->StreamState.BufferFrames = Context->CurrentFormat.BufferFrames;

    Context->StartRequests = 0;
    Context->RejectedStartRequests = 0;
    Context->StopRequests = 0;
    Context->FormatChanges = 0;
    Context->ControlWrites = 0;
    Context->ProfileApplies = 0;
}

static BOOLEAN
OpenA8DJ_IsSupportedSampleRate(_In_ ULONG SampleRate)
{
    ULONG index;

    for (index = 0; index < OPENA8DJ_STABLE_SAMPLE_RATE_COUNT; index++) {
        if (kOpenA8DJStableSampleRates[index] == SampleRate) {
            return TRUE;
        }
    }

    return FALSE;
}

static NTSTATUS
OpenA8DJ_ValidateAudioFormat(_In_ const OPENA8DJ_AUDIO_FORMAT *Format)
{
    if (Format == NULL || Format->Size < sizeof(OPENA8DJ_AUDIO_FORMAT)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!OpenA8DJ_IsSupportedSampleRate(Format->SampleRate)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (Format->InputChannels != OPENA8DJ_INPUT_CHANNELS ||
        Format->OutputChannels != OPENA8DJ_OUTPUT_CHANNELS) {
        return STATUS_INVALID_PARAMETER;
    }
    if (Format->BufferFrames < OPENA8DJ_MIN_BUFFER_FRAMES ||
        Format->BufferFrames > OPENA8DJ_MAX_BUFFER_FRAMES) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static VOID
OpenA8DJ_LoadControlState(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_CONTROL_STATE ControlState)
{
    RtlZeroMemory(ControlState, sizeof(*ControlState));
    ControlState->Size = sizeof(*ControlState);
    ControlState->InputMode = Context->RawControlState[0];
    ControlState->GndLiftTCVinyl = (Context->RawControlState[3] & (1u << 0)) ? 1 : 0;
    ControlState->GndLiftTCCDLine = (Context->RawControlState[3] & (1u << 1)) ? 1 : 0;
    ControlState->GndLiftPhono = (Context->RawControlState[3] & (1u << 2)) ? 1 : 0;
    ControlState->SoftwareLock = (Context->RawControlState[5] & (1u << 0)) ? 1 : 0;
}

static NTSTATUS
OpenA8DJ_StoreControlState(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ const OPENA8DJ_CONTROL_STATE *ControlState)
{
    if (ControlState == NULL || ControlState->Size < sizeof(OPENA8DJ_CONTROL_STATE)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (ControlState->InputMode > OPENA8DJ_PROFILE_PHONO) {
        return STATUS_INVALID_PARAMETER;
    }

    Context->RawControlState[0] = ControlState->InputMode;
    Context->RawControlState[1] = 2;
    Context->RawControlState[2] = 3;
    Context->RawControlState[4] = 2;

    OpenA8DJ_SetRawFlag(&Context->RawControlState[3], (UCHAR)(1u << 0), ControlState->GndLiftTCVinyl != 0);
    OpenA8DJ_SetRawFlag(&Context->RawControlState[3], (UCHAR)(1u << 1), ControlState->GndLiftTCCDLine != 0);
    OpenA8DJ_SetRawFlag(&Context->RawControlState[3], (UCHAR)(1u << 2), ControlState->GndLiftPhono != 0);
    OpenA8DJ_SetRawFlag(&Context->RawControlState[5], (UCHAR)(1u << 0), ControlState->SoftwareLock != 0);
    Context->ControlWrites++;

    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_ApplyProfile(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG Profile,
    _Out_ POPENA8DJ_CONTROL_STATE ControlState)
{
    OPENA8DJ_CONTROL_STATE state;

    OpenA8DJ_LoadControlState(Context, &state);

    switch (Profile) {
    case OPENA8DJ_PROFILE_TIMECODE_VINYL:
        state.InputMode = 0;
        state.GndLiftTCVinyl = 1;
        state.SoftwareLock = 1;
        break;
    case OPENA8DJ_PROFILE_TIMECODE_CD_LINE:
        state.InputMode = 1;
        state.GndLiftTCCDLine = 1;
        state.SoftwareLock = 1;
        break;
    case OPENA8DJ_PROFILE_PHONO:
        state.InputMode = 2;
        state.GndLiftPhono = 1;
        state.SoftwareLock = 1;
        break;
    case OPENA8DJ_PROFILE_UNLOCK:
        state.SoftwareLock = 0;
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    OpenA8DJ_StoreControlState(Context, &state);
    Context->ProfileApplies++;
    OpenA8DJ_LoadControlState(Context, ControlState);
    return STATUS_SUCCESS;
}

static VOID
OpenA8DJ_FillCapabilities(_In_ POPENA8DJ_DEVICE_CONTEXT Context, _Out_ POPENA8DJ_CAPABILITIES Capabilities)
{
    ULONG index;

    RtlZeroMemory(Capabilities, sizeof(*Capabilities));
    Capabilities->Size = sizeof(*Capabilities);
    Capabilities->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
    Capabilities->VendorId = OPENA8DJ_VENDOR_ID;
    Capabilities->ProductId = OPENA8DJ_PRODUCT_ID;
    Capabilities->InputChannels = OPENA8DJ_INPUT_CHANNELS;
    Capabilities->OutputChannels = OPENA8DJ_OUTPUT_CHANNELS;
    Capabilities->StereoPairs = OPENA8DJ_STEREO_PAIRS;
    Capabilities->MidiInputs = 1;
    Capabilities->MidiOutputs = 1;
    Capabilities->MinBufferFrames = OPENA8DJ_MIN_BUFFER_FRAMES;
    Capabilities->MaxBufferFrames = OPENA8DJ_MAX_BUFFER_FRAMES;
    Capabilities->DefaultBufferFrames = OPENA8DJ_DEFAULT_BUFFER_FRAMES;
    Capabilities->Experimental = TRUE;
    Capabilities->WindowsAudioEndpointExposed = FALSE;
    Capabilities->UsbTransportReady = Context->BulkOutPipe != NULL &&
                                      Context->BulkInPipe != NULL &&
                                      Context->IsoInPipe != NULL &&
                                      Context->IsoOutPipe != NULL;
    Capabilities->MidiReady = FALSE;
    Capabilities->ControlsReady = FALSE;

    for (index = 0; index < OPENA8DJ_STABLE_SAMPLE_RATE_COUNT; index++) {
        Capabilities->SampleRates[index] = kOpenA8DJStableSampleRates[index];
    }

    OpenA8DJ_CopyString(Capabilities->InputPairNames[0], "Input A L/R");
    OpenA8DJ_CopyString(Capabilities->InputPairNames[1], "Input B L/R");
    OpenA8DJ_CopyString(Capabilities->InputPairNames[2], "Input C L/R");
    OpenA8DJ_CopyString(Capabilities->InputPairNames[3], "Input D L/R");
    OpenA8DJ_CopyString(Capabilities->OutputPairNames[0], "Output A L/R");
    OpenA8DJ_CopyString(Capabilities->OutputPairNames[1], "Output B L/R");
    OpenA8DJ_CopyString(Capabilities->OutputPairNames[2], "Output C L/R");
    OpenA8DJ_CopyString(Capabilities->OutputPairNames[3], "Output D L/R");
}

static VOID
OpenA8DJ_FillSurface(_In_ POPENA8DJ_DEVICE_CONTEXT Context, _Out_ POPENA8DJ_WINDOWS_SURFACE Surface)
{
    RtlZeroMemory(Surface, sizeof(*Surface));
    Surface->Size = sizeof(*Surface);
    Surface->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
    Surface->SurfaceFlags = OPENA8DJ_SURFACE_FLAG_EXPERIMENTAL;
    Surface->StableSampleRateFlags = OPENA8DJ_RATE_FLAG_44100 | OPENA8DJ_RATE_FLAG_48000;
    Surface->PlannedSampleRateFlags = OPENA8DJ_RATE_FLAG_88200 | OPENA8DJ_RATE_FLAG_96000;
    Surface->AudioEndpointState = OPENA8DJ_COMPONENT_PLANNED;
    Surface->UsbTransportState = (Context->BulkOutPipe != NULL &&
                                  Context->BulkInPipe != NULL &&
                                  Context->IsoInPipe != NULL &&
                                  Context->IsoOutPipe != NULL) ?
                                  OPENA8DJ_COMPONENT_READY : OPENA8DJ_COMPONENT_STUB;
    Surface->IsochronousEngineState = OPENA8DJ_COMPONENT_PLANNED;
    Surface->MidiState = OPENA8DJ_COMPONENT_PLANNED;
    Surface->ControlState = OPENA8DJ_COMPONENT_STUB;
    Surface->AsioState = OPENA8DJ_COMPONENT_PLANNED;
    Surface->EndpointModel = OPENA8DJ_ENDPOINT_MODEL_DUAL_PROTOTYPE;
    if (Surface->UsbTransportState == OPENA8DJ_COMPONENT_READY) {
        Surface->SurfaceFlags |= OPENA8DJ_SURFACE_FLAG_USB_TRANSPORT;
    }
    OpenA8DJ_CopyFixedString(Surface->DriverModel,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "KMDF USB function driver surface v2");
    OpenA8DJ_CopyFixedString(Surface->AudioModel,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "ACX 1.1 planned; no Windows audio endpoint yet");
    OpenA8DJ_CopyFixedString(Surface->StreamingModel,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "capture-paced CAIAQ isochronous engine planned");
    OpenA8DJ_CopyFixedString(Surface->SafetyPolicy,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "start rejected; controls are local-only diagnostics");
}

static VOID
OpenA8DJ_FillChannel(
    _Out_ POPENA8DJ_CHANNEL_DESCRIPTOR Channel,
    _In_ ULONG Direction,
    _In_ ULONG ChannelIndex,
    _In_ ULONG PairIndex,
    _In_ ULONG PairChannelIndex,
    _In_z_ const CHAR *Name)
{
    RtlZeroMemory(Channel, sizeof(*Channel));
    Channel->Size = sizeof(*Channel);
    Channel->Direction = Direction;
    Channel->ChannelIndex = ChannelIndex;
    Channel->PairIndex = PairIndex;
    Channel->PairChannelIndex = PairChannelIndex;
    OpenA8DJ_CopyString(Channel->Name, Name);
}

static VOID
OpenA8DJ_FillTopology(_Out_ POPENA8DJ_TOPOLOGY Topology)
{
    RtlZeroMemory(Topology, sizeof(*Topology));
    Topology->Size = sizeof(*Topology);
    Topology->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
    Topology->EndpointModel = OPENA8DJ_ENDPOINT_MODEL_DUAL_PROTOTYPE;
    Topology->RenderEndpointCount = 4;
    Topology->CaptureEndpointCount = 4;
    Topology->RenderChannelCount = OPENA8DJ_OUTPUT_CHANNELS;
    Topology->CaptureChannelCount = OPENA8DJ_INPUT_CHANNELS;
    OpenA8DJ_CopyFixedString(Topology->RenderEndpointName,
                             OPENA8DJ_ENDPOINT_NAME_LENGTH,
                             "Open Audio 8 DJ Output A/B/C/D");
    OpenA8DJ_CopyFixedString(Topology->CaptureEndpointName,
                             OPENA8DJ_ENDPOINT_NAME_LENGTH,
                             "Open Audio 8 DJ Input A/B/C/D");

    OpenA8DJ_FillChannel(&Topology->Channels[0], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 0, 0, 0, "Output A Left");
    OpenA8DJ_FillChannel(&Topology->Channels[1], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 1, 0, 1, "Output A Right");
    OpenA8DJ_FillChannel(&Topology->Channels[2], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 2, 1, 0, "Output B Left");
    OpenA8DJ_FillChannel(&Topology->Channels[3], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 3, 1, 1, "Output B Right");
    OpenA8DJ_FillChannel(&Topology->Channels[4], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 4, 2, 0, "Output C Left");
    OpenA8DJ_FillChannel(&Topology->Channels[5], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 5, 2, 1, "Output C Right");
    OpenA8DJ_FillChannel(&Topology->Channels[6], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 6, 3, 0, "Output D Left");
    OpenA8DJ_FillChannel(&Topology->Channels[7], OPENA8DJ_CHANNEL_DIRECTION_RENDER, 7, 3, 1, "Output D Right");
    OpenA8DJ_FillChannel(&Topology->Channels[8], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 0, 0, 0, "Input A Left");
    OpenA8DJ_FillChannel(&Topology->Channels[9], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 1, 0, 1, "Input A Right");
    OpenA8DJ_FillChannel(&Topology->Channels[10], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 2, 1, 0, "Input B Left");
    OpenA8DJ_FillChannel(&Topology->Channels[11], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 3, 1, 1, "Input B Right");
    OpenA8DJ_FillChannel(&Topology->Channels[12], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 4, 2, 0, "Input C Left");
    OpenA8DJ_FillChannel(&Topology->Channels[13], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 5, 2, 1, "Input C Right");
    OpenA8DJ_FillChannel(&Topology->Channels[14], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 6, 3, 0, "Input D Left");
    OpenA8DJ_FillChannel(&Topology->Channels[15], OPENA8DJ_CHANNEL_DIRECTION_CAPTURE, 7, 3, 1, "Input D Right");
}

static VOID
OpenA8DJ_FillDiagnostics(_In_ POPENA8DJ_DEVICE_CONTEXT Context, _Out_ POPENA8DJ_DIAGNOSTICS Diagnostics)
{
    RtlZeroMemory(Diagnostics, sizeof(*Diagnostics));
    Diagnostics->Size = sizeof(*Diagnostics);
    Diagnostics->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
    Diagnostics->StartRequests = Context->StartRequests;
    Diagnostics->RejectedStartRequests = Context->RejectedStartRequests;
    Diagnostics->StopRequests = Context->StopRequests;
    Diagnostics->FormatChanges = Context->FormatChanges;
    Diagnostics->ControlWrites = Context->ControlWrites;
    Diagnostics->ProfileApplies = Context->ProfileApplies;
    Diagnostics->StreamState = Context->StreamState;
}

static NTSTATUS
OpenA8DJ_RetrieveOutput(
    _In_ WDFREQUEST Request,
    _In_ size_t RequiredLength,
    _Outptr_ PVOID *Buffer)
{
    return WdfRequestRetrieveOutputBuffer(Request, RequiredLength, Buffer, NULL);
}

static NTSTATUS
OpenA8DJ_RetrieveInput(
    _In_ WDFREQUEST Request,
    _In_ size_t RequiredLength,
    _Outptr_ PVOID *Buffer)
{
    return WdfRequestRetrieveInputBuffer(Request, RequiredLength, Buffer, NULL);
}

static VOID
OpenA8DJ_ResetPipeMap(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    Context->BulkOutPipe = NULL;
    Context->BulkInPipe = NULL;
    Context->IsoInPipe = NULL;
    Context->IsoOutPipe = NULL;
}

static VOID
OpenA8DJ_MapPipe(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ WDFUSBPIPE Pipe,
    _In_ PWDF_USB_PIPE_INFORMATION PipeInfo)
{
    switch (PipeInfo->EndpointAddress) {
    case OPENA8DJ_EP_BULK_OUT:
        Context->BulkOutPipe = Pipe;
        break;
    case OPENA8DJ_EP_BULK_IN:
        Context->BulkInPipe = Pipe;
        break;
    case OPENA8DJ_EP_ISO_IN:
        Context->IsoInPipe = Pipe;
        break;
    case OPENA8DJ_EP_ISO_OUT:
        Context->IsoOutPipe = Pipe;
        break;
    default:
        break;
    }
}

static VOID
OpenA8DJ_MapConfiguredPipes(_In_ WDFDEVICE Device)
{
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(Device);
    UCHAR pipeCount;
    UCHAR pipeIndex;

    OpenA8DJ_ResetPipeMap(context);

    if (context->UsbInterface == NULL) {
        return;
    }

    pipeCount = WdfUsbInterfaceGetNumConfiguredPipes(context->UsbInterface);

    for (pipeIndex = 0; pipeIndex < pipeCount; pipeIndex++) {
        WDF_USB_PIPE_INFORMATION pipeInfo;
        WDFUSBPIPE pipe;
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
        pipe = WdfUsbInterfaceGetConfiguredPipe(
            context->UsbInterface,
            pipeIndex,
            &pipeInfo);

        OpenA8DJ_MapPipe(context, pipe, &pipeInfo);

        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   "OpenA8DJUsb: pipe %u endpoint 0x%02x type %d maxPacket %u\n",
                   pipeIndex,
                   pipeInfo.EndpointAddress,
                   pipeInfo.PipeType,
                   pipeInfo.MaximumPacketSize));
    }
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(&config, OpenA8DJ_EvtDeviceAdd);

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: DriverEntry\n"));

    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE);
}

NTSTATUS
OpenA8DJ_EvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_IO_QUEUE_CONFIG queueConfig;

    UNREFERENCED_PARAMETER(Driver);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit, FALSE);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = OpenA8DJ_EvtDevicePrepareHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, OPENA8DJ_DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: WdfDeviceCreate failed 0x%08x\n",
                   status));
        return status;
    }

    OpenA8DJ_InitializeDefaults(OpenA8DJGetDeviceContext(device));

    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_OPENA8DJ_USB,
        NULL);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: WdfDeviceCreateDeviceInterface failed 0x%08x\n",
                   status));
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = OpenA8DJ_EvtIoDeviceControl;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: WdfIoQueueCreate failed 0x%08x\n",
                   status));
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
OpenA8DJ_EvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    NTSTATUS status;
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(Device);
    WDF_USB_DEVICE_CREATE_CONFIG usbCreateConfig;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    if (context->UsbDevice == NULL) {
        WDF_USB_DEVICE_CREATE_CONFIG_INIT(
            &usbCreateConfig,
            USBD_CLIENT_CONTRACT_VERSION_602);

        status = WdfUsbTargetDeviceCreateWithParameters(
            Device,
            &usbCreateConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &context->UsbDevice);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "OpenA8DJUsb: WdfUsbTargetDeviceCreateWithParameters failed 0x%08x\n",
                       status));
            return status;
        }
    }

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

    status = WdfUsbTargetDeviceSelectConfig(
        context->UsbDevice,
        WDF_NO_OBJECT_ATTRIBUTES,
        &configParams);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: WdfUsbTargetDeviceSelectConfig failed 0x%08x\n",
                   status));
        return status;
    }

    context->UsbInterface = configParams.Types.SingleInterface.ConfiguredUsbInterface;
    context->InterfaceCount = WdfUsbTargetDeviceGetNumInterfaces(context->UsbDevice);

    OpenA8DJ_MapConfiguredPipes(Device);

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: prepared Audio 8 DJ USB transport interfaces=%u bulkOut=%p bulkIn=%p isoIn=%p isoOut=%p\n",
               context->InterfaceCount,
               context->BulkOutPipe,
               context->BulkInPipe,
               context->IsoInPipe,
               context->IsoOutPipe));

    return STATUS_SUCCESS;
}

VOID
OpenA8DJ_EvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(device);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    size_t bytesReturned = 0;
    POPENA8DJ_USB_INFO info = NULL;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    if (IoControlCode == IOCTL_OPENA8DJ_GET_USB_INFO) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_USB_INFO), (PVOID *)&info);
        if (NT_SUCCESS(status)) {
            RtlZeroMemory(info, sizeof(*info));
            info->Size = sizeof(*info);
            info->VendorId = OPENA8DJ_VENDOR_ID;
            info->ProductId = OPENA8DJ_PRODUCT_ID;
            info->ConfiguredInterfaceCount = context->InterfaceCount;
            info->HasBulkOut = (context->BulkOutPipe != NULL);
            info->HasBulkIn = (context->BulkInPipe != NULL);
            info->HasIsoIn = (context->IsoInPipe != NULL);
            info->HasIsoOut = (context->IsoOutPipe != NULL);
            bytesReturned = sizeof(*info);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_CAPABILITIES) {
        POPENA8DJ_CAPABILITIES capabilities = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_CAPABILITIES), (PVOID *)&capabilities);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_FillCapabilities(context, capabilities);
            bytesReturned = sizeof(*capabilities);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_CONTROL_STATE) {
        POPENA8DJ_CONTROL_STATE controlState = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_CONTROL_STATE), (PVOID *)&controlState);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_LoadControlState(context, controlState);
            bytesReturned = sizeof(*controlState);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_SET_CONTROL_STATE) {
        POPENA8DJ_CONTROL_STATE inputState = NULL;
        POPENA8DJ_CONTROL_STATE outputState = NULL;

        status = OpenA8DJ_RetrieveInput(Request, sizeof(OPENA8DJ_CONTROL_STATE), (PVOID *)&inputState);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_StoreControlState(context, inputState);
        }
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_CONTROL_STATE), (PVOID *)&outputState);
        }
        if (NT_SUCCESS(status)) {
            OpenA8DJ_LoadControlState(context, outputState);
            bytesReturned = sizeof(*outputState);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_APPLY_PROFILE) {
        POPENA8DJ_PROFILE_REQUEST profile = NULL;
        POPENA8DJ_CONTROL_STATE outputState = NULL;

        status = OpenA8DJ_RetrieveInput(Request, sizeof(OPENA8DJ_PROFILE_REQUEST), (PVOID *)&profile);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_CONTROL_STATE), (PVOID *)&outputState);
        }
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_ApplyProfile(context, profile->Profile, outputState);
        }
        if (NT_SUCCESS(status)) {
            bytesReturned = sizeof(*outputState);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_AUDIO_FORMAT) {
        POPENA8DJ_AUDIO_FORMAT format = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_AUDIO_FORMAT), (PVOID *)&format);
        if (NT_SUCCESS(status)) {
            *format = context->CurrentFormat;
            bytesReturned = sizeof(*format);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_SET_AUDIO_FORMAT) {
        POPENA8DJ_AUDIO_FORMAT inputFormat = NULL;
        POPENA8DJ_AUDIO_FORMAT outputFormat = NULL;

        status = OpenA8DJ_RetrieveInput(Request, sizeof(OPENA8DJ_AUDIO_FORMAT), (PVOID *)&inputFormat);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_ValidateAudioFormat(inputFormat);
        }
        if (NT_SUCCESS(status)) {
            context->CurrentFormat = *inputFormat;
            context->CurrentFormat.Size = sizeof(context->CurrentFormat);
            context->StreamState.SampleRate = context->CurrentFormat.SampleRate;
            context->StreamState.BufferFrames = context->CurrentFormat.BufferFrames;
            context->FormatChanges++;
            status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_AUDIO_FORMAT), (PVOID *)&outputFormat);
        }
        if (NT_SUCCESS(status)) {
            *outputFormat = context->CurrentFormat;
            bytesReturned = sizeof(*outputFormat);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_START_STREAMING) {
        POPENA8DJ_STREAM_STATE streamState = NULL;

        context->StartRequests++;
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        context->StreamState.SampleRate = context->CurrentFormat.SampleRate;
        context->StreamState.BufferFrames = context->CurrentFormat.BufferFrames;
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_STREAM_STATE), (PVOID *)&streamState);
        if (NT_SUCCESS(status)) {
            *streamState = context->StreamState;
            bytesReturned = sizeof(*streamState);
            context->RejectedStartRequests++;
            status = STATUS_NOT_SUPPORTED;
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_STOP_STREAMING) {
        POPENA8DJ_STREAM_STATE streamState = NULL;

        context->StopRequests++;
        context->StreamState.Streaming = FALSE;
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_STREAM_STATE), (PVOID *)&streamState);
        if (NT_SUCCESS(status)) {
            *streamState = context->StreamState;
            bytesReturned = sizeof(*streamState);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_STREAM_STATE) {
        POPENA8DJ_STREAM_STATE streamState = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_STREAM_STATE), (PVOID *)&streamState);
        if (NT_SUCCESS(status)) {
            *streamState = context->StreamState;
            bytesReturned = sizeof(*streamState);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_SURFACE) {
        POPENA8DJ_WINDOWS_SURFACE surface = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_WINDOWS_SURFACE), (PVOID *)&surface);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_FillSurface(context, surface);
            bytesReturned = sizeof(*surface);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_TOPOLOGY) {
        POPENA8DJ_TOPOLOGY topology = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_TOPOLOGY), (PVOID *)&topology);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_FillTopology(topology);
            bytesReturned = sizeof(*topology);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_DIAGNOSTICS) {
        POPENA8DJ_DIAGNOSTICS diagnostics = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_DIAGNOSTICS), (PVOID *)&diagnostics);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_FillDiagnostics(context, diagnostics);
            bytesReturned = sizeof(*diagnostics);
        }
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
