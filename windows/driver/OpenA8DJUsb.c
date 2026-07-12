#include <initguid.h>
#include "OpenA8DJUsb.h"

static const ULONG kOpenA8DJStableSampleRates[OPENA8DJ_STABLE_SAMPLE_RATE_COUNT] = {
    44100,
    48000
};

#define OPENA8DJ_CAIAQ_COMMAND_READ_IO 0x04
#define OPENA8DJ_CAIAQ_COMMAND_WRITE_IO 0x05
#define OPENA8DJ_CAIAQ_COMMAND_AUTO_MSG 0x0b
#define OPENA8DJ_CONTROL_STATE_BYTES 6
#define OPENA8DJ_CONTROL_FLAGS_MASK 0x07
#define OPENA8DJ_CONTROL_LOCK_MASK 0x01

static NTSTATUS
OpenA8DJ_SendBulkCommand(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ UCHAR Command,
    _In_reads_bytes_(PayloadLength) const UCHAR *Payload,
    _In_ ULONG PayloadLength,
    _Out_writes_bytes_(ReplyCapacity) UCHAR *Reply,
    _In_ ULONG ReplyCapacity,
    _Out_ PULONG ReplyLength,
    _Out_opt_ PULONG WriteStatus,
    _Out_opt_ PULONG ReadStatus);

static NTSTATUS
OpenA8DJ_RefreshHardwareControlState(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);

static NTSTATUS
OpenA8DJ_WriteHardwareControlState(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_reads_bytes_(OPENA8DJ_CONTROL_STATE_BYTES) const UCHAR *State);

DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_A,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x11);
DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_B,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x12);
DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_C,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x13);
DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_D,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x14);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_A,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x38);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_B,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x39);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_C,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x3a);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_D,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x3b);

static const GUID kOpenA8DJKsCategoryAudio = { STATIC_KSCATEGORY_AUDIO };
static const GUID kOpenA8DJKsDataFormatTypeAudio = { STATIC_KSDATAFORMAT_TYPE_AUDIO };
static const GUID kOpenA8DJKsDataFormatSubtypePcm = { STATIC_KSDATAFORMAT_SUBTYPE_PCM };
static const GUID kOpenA8DJKsDataFormatSubtypeIeeeFloat =
    { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID kOpenA8DJKsDataFormatSpecifierWaveFormatEx = { STATIC_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX };
static const GUID kOpenA8DJKsNodeTypeSpeaker = { STATIC_KSNODETYPE_SPEAKER };
static const GUID kOpenA8DJKsNodeTypeMicrophone = { STATIC_KSNODETYPE_MICROPHONE };
static const GUID kOpenA8DJKsNodeTypeLineConnector = { STATIC_KSNODETYPE_LINE_CONNECTOR };

DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameA, L"OpenA8DJ Render A");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameB, L"OpenA8DJ Render B");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameC, L"OpenA8DJ Render C");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameD, L"OpenA8DJ Render D");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameA, L"OpenA8DJ Capture A");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameB, L"OpenA8DJ Capture B");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameC, L"OpenA8DJ Capture C");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameD, L"OpenA8DJ Capture D");

static const GUID * const gOpenA8DJRenderCircuitIds[OPENA8DJ_STEREO_PAIRS] = {
    &GUID_OPENA8DJ_RENDER_CIRCUIT_A,
    &GUID_OPENA8DJ_RENDER_CIRCUIT_B,
    &GUID_OPENA8DJ_RENDER_CIRCUIT_C,
    &GUID_OPENA8DJ_RENDER_CIRCUIT_D
};

static const GUID * const gOpenA8DJCaptureCircuitIds[OPENA8DJ_STEREO_PAIRS] = {
    &GUID_OPENA8DJ_CAPTURE_CIRCUIT_A,
    &GUID_OPENA8DJ_CAPTURE_CIRCUIT_B,
    &GUID_OPENA8DJ_CAPTURE_CIRCUIT_C,
    &GUID_OPENA8DJ_CAPTURE_CIRCUIT_D
};

static PCUNICODE_STRING const gOpenA8DJRenderCircuitNames[OPENA8DJ_STEREO_PAIRS] = {
    &gOpenA8DJRenderCircuitNameA,
    &gOpenA8DJRenderCircuitNameB,
    &gOpenA8DJRenderCircuitNameC,
    &gOpenA8DJRenderCircuitNameD
};

static PCUNICODE_STRING const gOpenA8DJCaptureCircuitNames[OPENA8DJ_STEREO_PAIRS] = {
    &gOpenA8DJCaptureCircuitNameA,
    &gOpenA8DJCaptureCircuitNameB,
    &gOpenA8DJCaptureCircuitNameC,
    &gOpenA8DJCaptureCircuitNameD
};

DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderEndpointNameA, L"Audio 8 DJ (8ch Out)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderEndpointNameB, L"Audio 8 DJ (Ch B, Out 3|4)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderEndpointNameC, L"Audio 8 DJ (Ch C, Out 5|6)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderEndpointNameD, L"Audio 8 DJ (Ch D, Out 7|8)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureEndpointNameA, L"Audio 8 DJ (8ch In)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureEndpointNameB, L"Audio 8 DJ (Ch B, In 3|4)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureEndpointNameC, L"Audio 8 DJ (Ch C, In 5|6)");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureEndpointNameD, L"Audio 8 DJ (Ch D, In 7|8)");

static PCUNICODE_STRING const gOpenA8DJRenderEndpointNames[OPENA8DJ_STEREO_PAIRS] = {
    &gOpenA8DJRenderEndpointNameA,
    &gOpenA8DJRenderEndpointNameB,
    &gOpenA8DJRenderEndpointNameC,
    &gOpenA8DJRenderEndpointNameD
};

static PCUNICODE_STRING const gOpenA8DJCaptureEndpointNames[OPENA8DJ_STEREO_PAIRS] = {
    &gOpenA8DJCaptureEndpointNameA,
    &gOpenA8DJCaptureEndpointNameB,
    &gOpenA8DJCaptureEndpointNameC,
    &gOpenA8DJCaptureEndpointNameD
};

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#define OPENA8DJ_ASYNC_OUTPUT_SLOTS 2
#define OPENA8DJ_ENABLE_ASYNC_OUTPUT 0
#define OPENA8DJ_ISO_OUTPUT_PACKET_BYTES_CAPTURE_SHAPE MAXULONG
#define OPENA8DJ_HW_LATENCY_FRAMES 0u
#define OPENA8DJ_RENDER_TRANSFER_BLEND_FRAMES 0u
#define OPENA8DJ_RT_RENDER_START_OFFSET_FRAMES 0u
#define OPENA8DJ_RT_RENDER_PREFILL_FRAMES 480u
#define OPENA8DJ_DEBUG_STREAM_TONE 0
#define OPENA8DJ_OUTPUT_NATIVE_I24 0
#define OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES 192u

typedef struct _OPENA8DJ_ACX_STREAM_CONTEXT {
    POPENA8DJ_DEVICE_CONTEXT DeviceContext;
    ACXSTREAM Stream;
    BOOLEAN IsRender;
    ULONG PairIndex;
    PMDL RtMdl;
    PVOID RtKernelAddress;
    PACX_RTPACKET RtPackets;
    ULONG RtPacketCount;
    ULONG RtPacketSize;
    ULONG RtBufferBytes;
    ULONG RtFrameCount;
    ULONG RtChannels;
    ULONG RtBlockAlign;
    ULONG RtBitsPerSample;
    BOOLEAN RtIsFloat;
    UCHAR OutputFrameBytes[4][6];
    UCHAR InputFrameBytes[4][6];
    LONG LastOutputSamples[OPENA8DJ_STEREO_PAIRS][2];
    UCHAR OutputByteInFrame;
    UCHAR InputByteInFrame;
    BOOLEAN OutputFrameLoaded;
    BOOLEAN HasLastOutputSamples;
    ULONG CurrentPacket;
    ULONG LastCompletedPacket;
    ULONG RenderFrameCursor;
    ULONG CaptureFrameCursor;
    ULONG RenderTransferFrameIndex;
    ULONGLONG PositionBlocks;
    ULONGLONG PositionQpc;
    ULONG RenderPrefillFramesRemaining;
    EX_RUNDOWN_REF WorkerRundown;
    BOOLEAN WorkerRundownCompleted;
} OPENA8DJ_ACX_STREAM_CONTEXT, *POPENA8DJ_ACX_STREAM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(OPENA8DJ_ACX_STREAM_CONTEXT, OpenA8DJGetAcxStreamContext)

static POPENA8DJ_ACX_STREAM_CONTEXT
OpenA8DJ_AcquireActiveStream(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ volatile PVOID *Slot)
{
    KIRQL oldIrql;
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;

    KeAcquireSpinLock(&Context->ActiveStreamLock, &oldIrql);
    streamContext = (POPENA8DJ_ACX_STREAM_CONTEXT)*Slot;
    if (streamContext != NULL &&
        !ExAcquireRundownProtection(&streamContext->WorkerRundown)) {
        streamContext = NULL;
    }
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
    return streamContext;
}

static VOID
OpenA8DJ_ReleaseActiveStream(_In_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    ExReleaseRundownProtection(&StreamContext->WorkerRundown);
}

static VOID
OpenA8DJ_SetActiveStream(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ volatile PVOID *Slot,
    _In_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&Context->ActiveStreamLock, &oldIrql);
    *Slot = StreamContext;
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
}

static VOID
OpenA8DJ_ClearActiveStream(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ volatile PVOID *Slot,
    _In_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&Context->ActiveStreamLock, &oldIrql);
    if (*Slot == StreamContext) {
        *Slot = NULL;
    }
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
}

static ULONG
OpenA8DJ_GetActiveRenderMask(_In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    KIRQL oldIrql;
    ULONG mask = 0;
    ULONG pairIndex;

    KeAcquireSpinLock(&Context->ActiveStreamLock, &oldIrql);
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (Context->ActiveRenderStreams[pairIndex] != NULL) {
            mask |= (1u << pairIndex);
        }
    }
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
    return mask;
}

static VOID
OpenA8DJ_ClearAllActiveStreams(_In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&Context->ActiveStreamLock, &oldIrql);
    RtlZeroMemory((PVOID)Context->ActiveRenderStreams, sizeof(Context->ActiveRenderStreams));
    RtlZeroMemory((PVOID)Context->ActiveCaptureStreams, sizeof(Context->ActiveCaptureStreams));
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
}

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
typedef struct _OPENA8DJ_ASYNC_ISO_OUTPUT_SLOT {
    WDFREQUEST Request;
    WDFMEMORY UrbMemory;
    PURB Urb;
    UCHAR *Buffer;
    ULONG BufferLength;
    volatile LONG InFlight;
    NTSTATUS CompletionStatus;
    ULONG ErrorCount;
    KEVENT CompleteEvent;
} OPENA8DJ_ASYNC_ISO_OUTPUT_SLOT, *POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT;
#endif

typedef struct _OPENA8DJ_ACX_PIN_CONTEXT {
    PCUNICODE_STRING Name;
} OPENA8DJ_ACX_PIN_CONTEXT, *POPENA8DJ_ACX_PIN_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(OPENA8DJ_ACX_PIN_CONTEXT, OpenA8DJGetAcxPinContext)

static EVT_ACX_CIRCUIT_CREATE_STREAM OpenA8DJ_EvtCircuitCreateStream;
static EVT_ACX_PIN_RETRIEVE_NAME OpenA8DJ_EvtAcxPinRetrieveName;
static EVT_ACX_STREAM_PREPARE_HARDWARE OpenA8DJ_EvtAcxStreamPrepareHardware;
static EVT_ACX_STREAM_RELEASE_HARDWARE OpenA8DJ_EvtAcxStreamReleaseHardware;
static EVT_ACX_STREAM_RUN OpenA8DJ_EvtAcxStreamRun;
static EVT_ACX_STREAM_PAUSE OpenA8DJ_EvtAcxStreamPause;
static EVT_ACX_STREAM_ALLOCATE_RTPACKETS OpenA8DJ_EvtAcxStreamAllocateRtPackets;
static EVT_ACX_STREAM_FREE_RTPACKETS OpenA8DJ_EvtAcxStreamFreeRtPackets;
static EVT_ACX_STREAM_GET_HW_LATENCY OpenA8DJ_EvtAcxStreamGetHwLatency;
static EVT_ACX_STREAM_SET_RENDER_PACKET OpenA8DJ_EvtAcxStreamSetRenderPacket;
static EVT_ACX_STREAM_GET_CURRENT_PACKET OpenA8DJ_EvtAcxStreamGetCurrentPacket;
static EVT_ACX_STREAM_GET_CAPTURE_PACKET OpenA8DJ_EvtAcxStreamGetCapturePacket;
static EVT_ACX_STREAM_GET_PRESENTATION_POSITION OpenA8DJ_EvtAcxStreamGetPresentationPosition;

static VOID
OpenA8DJ_RecordAcxStage(_In_ ULONG Stage, _In_ NTSTATUS Status)
{
    UNICODE_STRING path;
    ULONG statusValue = (ULONG)Status;

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return;
    }

    RtlInitUnicodeString(&path, L"OpenA8DJUsbAcx\\Parameters");
    (VOID)RtlWriteRegistryValue(
        RTL_REGISTRY_SERVICES,
        path.Buffer,
        L"LastAcxStage",
        REG_DWORD,
        &Stage,
        sizeof(Stage));
    (VOID)RtlWriteRegistryValue(
        RTL_REGISTRY_SERVICES,
        path.Buffer,
        L"LastAcxStatus",
        REG_DWORD,
        &statusValue,
        sizeof(statusValue));
}

static VOID
OpenA8DJ_UpdateStreamPositionQpc(_Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    LARGE_INTEGER qpc = KeQueryPerformanceCounter(NULL);

    StreamContext->PositionQpc = (ULONGLONG)qpc.QuadPart;
}

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
    RtlZeroMemory(Context->LastControlWriteRequest, sizeof(Context->LastControlWriteRequest));
    RtlZeroMemory(Context->LastControlWriteReadBack, sizeof(Context->LastControlWriteReadBack));
    Context->LastControlWriteMismatch = FALSE;
    Context->LastControlReadStatus = STATUS_NOT_SUPPORTED;
    Context->LastControlWriteStatus = STATUS_NOT_SUPPORTED;
    Context->LastControlReadbackStatus = STATUS_NOT_SUPPORTED;

    RtlZeroMemory(&Context->CurrentFormat, sizeof(Context->CurrentFormat));
    Context->CurrentFormat.Size = sizeof(Context->CurrentFormat);
    Context->CurrentFormat.SampleRate = OPENA8DJ_DEFAULT_SAMPLE_RATE;
    Context->CurrentFormat.InputChannels = OPENA8DJ_INPUT_CHANNELS;
    Context->CurrentFormat.OutputChannels = OPENA8DJ_OUTPUT_CHANNELS;
    Context->CurrentFormat.BufferFrames = OPENA8DJ_DEFAULT_BUFFER_FRAMES;

    RtlZeroMemory(&Context->StreamState, sizeof(Context->StreamState));
    Context->StreamState.Size = sizeof(Context->StreamState);
    Context->StreamState.StreamingEngineReady = TRUE;
    Context->StreamState.SampleRate = Context->CurrentFormat.SampleRate;
    Context->StreamState.BufferFrames = Context->CurrentFormat.BufferFrames;

    Context->StreamWorkItem = NULL;
    RtlZeroMemory(Context->RenderCircuits, sizeof(Context->RenderCircuits));
    RtlZeroMemory(Context->CaptureCircuits, sizeof(Context->CaptureCircuits));
    KeInitializeSpinLock(&Context->ActiveStreamLock);
    RtlZeroMemory((PVOID)Context->ActiveRenderStreams, sizeof(Context->ActiveRenderStreams));
    RtlZeroMemory((PVOID)Context->ActiveCaptureStreams, sizeof(Context->ActiveCaptureStreams));
    Context->StreamStopRequested = 1;
    Context->StreamWorkerActive = 0;
    Context->AcxCircuitsAdded = FALSE;
    Context->StartRequests = 0;
    Context->RejectedStartRequests = 0;
    Context->StopRequests = 0;
    Context->FormatChanges = 0;
    Context->ControlWrites = 0;
    Context->ProfileApplies = 0;
    Context->AcxCreateStreamCallbacks = 0;
    Context->AcxPrepareCallbacks = 0;
    Context->AcxReleaseCallbacks = 0;
    Context->AcxRunCallbacks = 0;
    Context->AcxPauseCallbacks = 0;
    Context->AcxLatencyCallbacks = 0;
    Context->AcxAllocatePacketCallbacks = 0;
    Context->AcxFreePacketCallbacks = 0;
    Context->AcxSetRenderPacketCallbacks = 0;
    Context->AcxGetCurrentPacketCallbacks = 0;
    Context->AcxGetCapturePacketCallbacks = 0;
    Context->AcxGetPresentationPositionCallbacks = 0;
    Context->AcxRtFramesRead = 0;
    Context->AcxRtNonZeroFrames = 0;
    Context->AcxRtPeakAbsS24 = 0;
    RtlZeroMemory((PVOID)Context->AcxRtRenderPairNonZeroFrames, sizeof(Context->AcxRtRenderPairNonZeroFrames));
    RtlZeroMemory(Context->AcxRtRenderPairPeakAbsS24, sizeof(Context->AcxRtRenderPairPeakAbsS24));
    RtlZeroMemory((PVOID)Context->AcxRtCapturePairNonZeroFrames, sizeof(Context->AcxRtCapturePairNonZeroFrames));
    RtlZeroMemory(Context->AcxRtCapturePairPeakAbsS16, sizeof(Context->AcxRtCapturePairPeakAbsS16));
    Context->AcxRtChannels = 0;
    Context->AcxRtBlockAlign = 0;
    Context->AcxRtBitsPerSample = 0;
    Context->AcxRtIsFloat = 0;
    Context->AcxRtPacketCount = 0;
    Context->AcxRtPacketSize = 0;
    Context->AcxRtFrameCount = 0;
    Context->AcxLastSetRenderPacket = 0;
    Context->AcxLastSetRenderFlags = 0;
    Context->AcxLastSetRenderEosPacketLength = 0;
    Context->StreamWorkerIterations = 0;
    Context->StreamWorkerCaptureBytes = 0;
    Context->StreamWorkerPlaybackBytes = 0;
    Context->StreamWorkerNoRenderIterations = 0;
    Context->StreamWorkerLastCaptureBytes = 0;
    Context->StreamWorkerLastPlaybackBytes = 0;
    Context->StreamWorkerLastRenderMask = 0;
    Context->StreamWorkerLastCaptureMask = 0;
    Context->StreamWorkerMaxCaptureBytes = 0;
    Context->StreamWorkerMaxPlaybackBytes = 0;
    Context->RenderTraceWriteIndex = 0;
    Context->RenderTraceWriteCount = 0;
    RtlZeroMemory(Context->RenderTraceFrames, sizeof(Context->RenderTraceFrames));
    Context->UsbPlaybackTraceWriteCount = 0;
    Context->UsbPlaybackTraceByteCount = 0;
    Context->UsbPlaybackTraceFixedPacketBytes = 0;
    Context->UsbPlaybackTraceActiveRenderMask = 0;
    Context->UsbPlaybackTraceWorkerLastPlaybackBytes = 0;
    RtlZeroMemory(Context->UsbPlaybackTraceBytes, sizeof(Context->UsbPlaybackTraceBytes));
    KeInitializeSpinLock(&Context->Ep1Lock);
    KeInitializeEvent(&Context->Ep1Event, NotificationEvent, FALSE);
    Context->Ep1PendingActive = FALSE;
    Context->Ep1ReplyReady = FALSE;
    Context->Ep1PendingCommand = 0;
    Context->Ep1ReplyLength = 0;
    Context->Ep1ReaderConfigStatus = STATUS_NOT_SUPPORTED;
    Context->Ep1ReaderStartStatus = STATUS_NOT_SUPPORTED;
    Context->Ep1ReaderCompletions = 0;
    Context->Ep1ReaderZeroReads = 0;
    Context->Ep1ReaderBytes = 0;
}

static BOOLEAN
OpenA8DJ_HasActiveAcxStreams(_In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    KIRQL oldIrql;
    BOOLEAN active = FALSE;
    ULONG pairIndex;

    KeAcquireSpinLock(&Context->ActiveStreamLock, &oldIrql);
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (Context->ActiveRenderStreams[pairIndex] != NULL ||
            Context->ActiveCaptureStreams[pairIndex] != NULL) {
            active = TRUE;
            break;
        }
    }
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
    return active;
}

static VOID
OpenA8DJ_InitPcm48Format(
    _Out_ KSDATAFORMAT_WAVEFORMATEXTENSIBLE *Format,
    _In_ USHORT Channels,
    _In_ ULONG ChannelMask)
{
    USHORT blockAlign = (USHORT)(Channels * sizeof(SHORT));

    RtlZeroMemory(Format, sizeof(*Format));
    Format->DataFormat.FormatSize = sizeof(*Format);
    Format->DataFormat.Flags = 0;
    Format->DataFormat.SampleSize = blockAlign;
    Format->DataFormat.Reserved = 0;
    Format->DataFormat.MajorFormat = kOpenA8DJKsDataFormatTypeAudio;
    Format->DataFormat.SubFormat = kOpenA8DJKsDataFormatSubtypePcm;
    Format->DataFormat.Specifier = kOpenA8DJKsDataFormatSpecifierWaveFormatEx;

    Format->WaveFormatExt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    Format->WaveFormatExt.Format.nChannels = Channels;
    Format->WaveFormatExt.Format.nSamplesPerSec = 48000;
    Format->WaveFormatExt.Format.nAvgBytesPerSec = 48000 * blockAlign;
    Format->WaveFormatExt.Format.nBlockAlign = blockAlign;
    Format->WaveFormatExt.Format.wBitsPerSample = 16;
    Format->WaveFormatExt.Format.cbSize =
        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    Format->WaveFormatExt.Samples.wValidBitsPerSample = 16;
    Format->WaveFormatExt.dwChannelMask = ChannelMask;
    Format->WaveFormatExt.SubFormat = kOpenA8DJKsDataFormatSubtypePcm;
}

OpenA8DJ_AddPcm48FormatToPin(
    _In_ WDFDEVICE Device,
    _In_ ACXCIRCUIT Circuit,
    _In_ ACXPIN Pin,
    _In_ USHORT Channels,
    _In_ ULONG ChannelMask)
{
    NTSTATUS status;
    ACXDATAFORMAT acxFormat = NULL;
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE format;
    ACX_DATAFORMAT_CONFIG formatConfig;
    WDF_OBJECT_ATTRIBUTES attributes;
    ACXDATAFORMATLIST formatList;

    OpenA8DJ_InitPcm48Format(&format, Channels, ChannelMask);
    ACX_DATAFORMAT_CONFIG_INIT_KS(&formatConfig, &format);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = (WDFOBJECT)Circuit;
    status = AcxDataFormatCreate(Device, &attributes, &formatConfig, &acxFormat);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    formatList = AcxPinGetRawDataFormatList(Pin);
    if (formatList == NULL) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return AcxDataFormatListAddDataFormat(formatList, acxFormat);
}

static NTSTATUS
OpenA8DJ_AddDefaultFormatsToPin(
    _In_ WDFDEVICE Device,
    _In_ ACXCIRCUIT Circuit,
    _In_ ACXPIN Pin,
    _In_ ULONG PairIndex)
{
    NTSTATUS status;

    if (PairIndex == 0) {
        return OpenA8DJ_AddPcm48FormatToPin(Device, Circuit, Pin, 8, KSAUDIO_SPEAKER_DIRECTOUT);
    }

    status = OpenA8DJ_AddPcm48FormatToPin(Device, Circuit, Pin, 2, KSAUDIO_SPEAKER_STEREO);
    return status;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamPrepareHardware(
    _In_ ACXSTREAM Stream)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);

    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxPrepareCallbacks);
    }
    if (streamContext->RtKernelAddress != NULL && streamContext->RtBufferBytes != 0) {
        RtlZeroMemory(streamContext->RtKernelAddress, streamContext->RtBufferBytes);
    }
    streamContext->CurrentPacket = 0;
    streamContext->LastCompletedPacket = 0;
    streamContext->RenderFrameCursor =
        streamContext->RtFrameCount != 0 ?
        (OPENA8DJ_RT_RENDER_START_OFFSET_FRAMES % streamContext->RtFrameCount) :
        0;
    streamContext->CaptureFrameCursor = 0;
    RtlZeroMemory(streamContext->LastOutputSamples, sizeof(streamContext->LastOutputSamples));
    streamContext->OutputByteInFrame = 4u;
    streamContext->InputByteInFrame = 0;
    streamContext->OutputFrameLoaded = FALSE;
    streamContext->HasLastOutputSamples = FALSE;
    streamContext->RenderTransferFrameIndex = 0;
    streamContext->PositionBlocks = 0;
    OpenA8DJ_UpdateStreamPositionQpc(streamContext);
    streamContext->RenderPrefillFramesRemaining = 0;
    OpenA8DJ_RecordAcxStage(410, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamReleaseHardware(
    _In_ ACXSTREAM Stream)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);

    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxReleaseCallbacks);
    }
    OpenA8DJ_RecordAcxStage(411, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamRun(
    _In_ ACXSTREAM Stream)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);
    POPENA8DJ_DEVICE_CONTEXT deviceContext = streamContext->DeviceContext;

    if (deviceContext != NULL) {
        InterlockedIncrement64(&deviceContext->AcxRunCallbacks);
        if (streamContext->RtKernelAddress != NULL &&
            deviceContext->StreamWorkItem != NULL) {
            streamContext->RenderFrameCursor =
                streamContext->RtFrameCount != 0 ?
                (OPENA8DJ_RT_RENDER_START_OFFSET_FRAMES % streamContext->RtFrameCount) :
                0;
            streamContext->CaptureFrameCursor = 0;
            RtlZeroMemory(streamContext->LastOutputSamples, sizeof(streamContext->LastOutputSamples));
            streamContext->OutputByteInFrame = 4u;
            streamContext->InputByteInFrame = 0;
            streamContext->OutputFrameLoaded = FALSE;
            streamContext->HasLastOutputSamples = FALSE;
            streamContext->RenderTransferFrameIndex = 0;
            streamContext->PositionBlocks = 0;
            OpenA8DJ_UpdateStreamPositionQpc(streamContext);
            streamContext->RenderPrefillFramesRemaining =
                streamContext->IsRender ? OPENA8DJ_RT_RENDER_PREFILL_FRAMES : 0u;
            if (streamContext->IsRender) {
                OpenA8DJ_SetActiveStream(
                    deviceContext,
                    &deviceContext->ActiveRenderStreams[streamContext->PairIndex],
                    streamContext);
            } else {
                OpenA8DJ_SetActiveStream(
                    deviceContext,
                    &deviceContext->ActiveCaptureStreams[streamContext->PairIndex],
                    streamContext);
            }
            InterlockedExchange(&deviceContext->StreamStopRequested, 0);
            if (InterlockedCompareExchange(&deviceContext->StreamWorkerActive, 1, 0) == 0) {
                WdfWorkItemEnqueue(deviceContext->StreamWorkItem);
            }
        }
    }
    OpenA8DJ_RecordAcxStage(412, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamPause(
    _In_ ACXSTREAM Stream)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);

    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxPauseCallbacks);
        if (streamContext->IsRender) {
            OpenA8DJ_ClearActiveStream(
                streamContext->DeviceContext,
                &streamContext->DeviceContext->ActiveRenderStreams[streamContext->PairIndex],
                streamContext);
        } else {
            OpenA8DJ_ClearActiveStream(
                streamContext->DeviceContext,
                &streamContext->DeviceContext->ActiveCaptureStreams[streamContext->PairIndex],
                streamContext);
        }
        if (!OpenA8DJ_HasActiveAcxStreams(streamContext->DeviceContext)) {
            InterlockedExchange(&streamContext->DeviceContext->StreamStopRequested, 1);
        }
    }
    OpenA8DJ_RecordAcxStage(413, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamGetHwLatency(
    _In_ ACXSTREAM Stream,
    _Out_ ULONG *FifoSize,
    _Out_ ULONG *Delay)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);

    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxLatencyCallbacks);
    }
#if OPENA8DJ_HW_LATENCY_FRAMES == 0
    *FifoSize = 0;
    *Delay = 0;
#else
    if (streamContext->RtBlockAlign != 0) {
        *FifoSize = OPENA8DJ_HW_LATENCY_FRAMES * streamContext->RtBlockAlign;
    } else {
        *FifoSize = OPENA8DJ_HW_LATENCY_FRAMES * 16u;
    }
    *Delay = (ULONG)(((ULONGLONG)OPENA8DJ_HW_LATENCY_FRAMES * 10000000ull) / OPENA8DJ_DEFAULT_SAMPLE_RATE);
#endif
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamAllocateRtPackets(
    _In_ ACXSTREAM Stream,
    _In_ ULONG PacketCount,
    _In_ ULONG PacketSize,
    _Out_ PACX_RTPACKET *Packets)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;
    PHYSICAL_ADDRESS lowAddress;
    PHYSICAL_ADDRESS highAddress;
    PHYSICAL_ADDRESS skipBytes;
    ULONG rawBytes;
    ULONG totalBytes;
    PACX_RTPACKET packets;
    ULONG index;

    *Packets = NULL;
    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxAllocatePacketCallbacks);
    }
    if (PacketCount == 0 || PacketCount > 2 || PacketSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    if (PacketSize > (ULONG)(MAXULONG / PacketCount)) {
        return STATUS_INTEGER_OVERFLOW;
    }
    if (streamContext->WorkerRundownCompleted) {
        ExReInitializeRundownProtection(&streamContext->WorkerRundown);
        streamContext->WorkerRundownCompleted = FALSE;
    }

    rawBytes = PacketCount * PacketSize;
    totalBytes = (rawBytes + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
    packets = (PACX_RTPACKET)ExAllocatePoolZero(
        NonPagedPoolNx,
        sizeof(ACX_RTPACKET) * PacketCount,
        OPENA8DJ_POOL_TAG);
    if (packets == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    lowAddress.QuadPart = 0;
    highAddress.QuadPart = MAXLONGLONG;
    skipBytes.QuadPart = 0;
    streamContext->RtMdl = MmAllocatePagesForMdlEx(
        lowAddress,
        highAddress,
        skipBytes,
        totalBytes,
        MmCached,
        MM_ALLOCATE_FULLY_REQUIRED);
    if (streamContext->RtMdl == NULL) {
        ExFreePoolWithTag(packets, OPENA8DJ_POOL_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    streamContext->RtKernelAddress = MmGetSystemAddressForMdlSafe(
        streamContext->RtMdl,
        NormalPagePriority | MdlMappingNoExecute);
    if (streamContext->RtKernelAddress == NULL) {
        MmFreePagesFromMdl(streamContext->RtMdl);
        ExFreePool(streamContext->RtMdl);
        streamContext->RtMdl = NULL;
        ExFreePoolWithTag(packets, OPENA8DJ_POOL_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(streamContext->RtKernelAddress, totalBytes);

    for (index = 0; index < PacketCount; index++) {
        ACX_RTPACKET_INIT(&packets[index]);
        packets[index].RtPacketOffset = index * PacketSize;
        packets[index].RtPacketSize = PacketSize;
    }
    WDF_MEMORY_DESCRIPTOR_INIT_MDL(
        &packets[0].RtPacketBuffer,
        streamContext->RtMdl,
        totalBytes);

    streamContext->RtPackets = packets;
    streamContext->RtPacketCount = PacketCount;
    streamContext->RtPacketSize = PacketSize;
    streamContext->RtBufferBytes = totalBytes;
    streamContext->RtFrameCount =
        streamContext->RtBlockAlign != 0 ? rawBytes / streamContext->RtBlockAlign : 0;
    if (streamContext->IsRender && streamContext->DeviceContext != NULL) {
        streamContext->DeviceContext->AcxRtPacketCount = PacketCount;
        streamContext->DeviceContext->AcxRtPacketSize = PacketSize;
        streamContext->DeviceContext->AcxRtFrameCount = streamContext->RtFrameCount;
    }
    streamContext->CurrentPacket = 0;
    streamContext->LastCompletedPacket = 0;
    streamContext->RenderFrameCursor =
        streamContext->RtFrameCount != 0 ?
        (OPENA8DJ_RT_RENDER_START_OFFSET_FRAMES % streamContext->RtFrameCount) :
        0;
    streamContext->CaptureFrameCursor = 0;
    RtlZeroMemory(streamContext->LastOutputSamples, sizeof(streamContext->LastOutputSamples));
    streamContext->OutputByteInFrame = 4u;
    streamContext->InputByteInFrame = 0;
    streamContext->OutputFrameLoaded = FALSE;
    streamContext->HasLastOutputSamples = FALSE;
    streamContext->RenderTransferFrameIndex = 0;
    streamContext->PositionBlocks = 0;
    OpenA8DJ_UpdateStreamPositionQpc(streamContext);
    streamContext->RenderPrefillFramesRemaining = 0;
    *Packets = packets;

    return STATUS_SUCCESS;
}

static VOID
NTAPI
OpenA8DJ_EvtAcxStreamFreeRtPackets(
    _In_ ACXSTREAM Stream,
    _In_ PACX_RTPACKET Packets,
    _In_ ULONG PacketCount)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;

    UNREFERENCED_PARAMETER(PacketCount);

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxFreePacketCallbacks);
        OpenA8DJ_ClearActiveStream(
            streamContext->DeviceContext,
            &streamContext->DeviceContext->ActiveRenderStreams[streamContext->PairIndex],
            streamContext);
        OpenA8DJ_ClearActiveStream(
            streamContext->DeviceContext,
            &streamContext->DeviceContext->ActiveCaptureStreams[streamContext->PairIndex],
            streamContext);
        if (!OpenA8DJ_HasActiveAcxStreams(streamContext->DeviceContext)) {
            InterlockedExchange(&streamContext->DeviceContext->StreamStopRequested, 1);
        }
    }
    if (!streamContext->WorkerRundownCompleted) {
        ExWaitForRundownProtectionRelease(&streamContext->WorkerRundown);
        streamContext->WorkerRundownCompleted = TRUE;
    }
    if (streamContext->RtMdl != NULL) {
        MmFreePagesFromMdl(streamContext->RtMdl);
        ExFreePool(streamContext->RtMdl);
        streamContext->RtMdl = NULL;
    }
    if (Packets != NULL) {
        ExFreePoolWithTag(Packets, OPENA8DJ_POOL_TAG);
    }
    streamContext->RtPackets = NULL;
    streamContext->RtKernelAddress = NULL;
    streamContext->RtPacketCount = 0;
    streamContext->RtPacketSize = 0;
    streamContext->RtBufferBytes = 0;
    streamContext->RtFrameCount = 0;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamSetRenderPacket(
    _In_ ACXSTREAM Stream,
    _In_ ULONG Packet,
    _In_ ULONG Flags,
    _In_ ULONG EosPacketLength)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;

    UNREFERENCED_PARAMETER(Packet);

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxSetRenderPacketCallbacks);
        streamContext->DeviceContext->AcxLastSetRenderPacket = Packet;
        streamContext->DeviceContext->AcxLastSetRenderFlags = Flags;
        streamContext->DeviceContext->AcxLastSetRenderEosPacketLength = EosPacketLength;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamGetCurrentPacket(
    _In_ ACXSTREAM Stream,
    _Out_ PULONG CurrentPacket)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxGetCurrentPacketCallbacks);
    }
    *CurrentPacket = streamContext->CurrentPacket;
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamGetCapturePacket(
    _In_ ACXSTREAM Stream,
    _Out_ PULONG LastCapturePacket,
    _Out_ PULONGLONG QPCPacketStart,
    _Out_ PBOOLEAN MoreData)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;
    LARGE_INTEGER qpc;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxGetCapturePacketCallbacks);
    }
    qpc = KeQueryPerformanceCounter(NULL);
    *LastCapturePacket = streamContext->LastCompletedPacket;
    *QPCPacketStart = (ULONGLONG)qpc.QuadPart;
    *MoreData = FALSE;
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamGetPresentationPosition(
    _In_ ACXSTREAM Stream,
    _Out_ PULONGLONG PositionInBlocks,
    _Out_ PULONGLONG QPCPosition)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;
    LARGE_INTEGER qpc;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxGetPresentationPositionCallbacks);
    }
    qpc = KeQueryPerformanceCounter(NULL);
    *PositionInBlocks = streamContext->PositionBlocks;
    *QPCPosition = streamContext->PositionQpc != 0 ?
        streamContext->PositionQpc :
        (ULONGLONG)qpc.QuadPart;
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtCircuitCreateStream(
    _In_ WDFDEVICE Device,
    _In_ ACXCIRCUIT Circuit,
    _In_ ACXPIN Pin,
    _In_ PACXSTREAM_INIT StreamInit,
    _In_ ACXDATAFORMAT StreamFormat,
    _In_ const GUID *SignalProcessingMode,
    _In_ ACXOBJECTBAG VarArguments)
{
    NTSTATUS status;
    ACX_STREAM_CALLBACKS streamCallbacks;
    ACX_RT_STREAM_CALLBACKS rtCallbacks;
    WDF_OBJECT_ATTRIBUTES attributes;
    POPENA8DJ_DEVICE_CONTEXT deviceContext;
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;
    ACXSTREAM stream = NULL;
    BOOLEAN circuitMatched = FALSE;
    BOOLEAN isRender = FALSE;
    ULONG pairIndex;

    UNREFERENCED_PARAMETER(Pin);
    UNREFERENCED_PARAMETER(StreamFormat);
    UNREFERENCED_PARAMETER(SignalProcessingMode);
    UNREFERENCED_PARAMETER(VarArguments);

    OpenA8DJ_RecordAcxStage(400, STATUS_SUCCESS);
    deviceContext = OpenA8DJGetDeviceContext(Device);
    InterlockedIncrement64(&deviceContext->AcxCreateStreamCallbacks);
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (Circuit == deviceContext->RenderCircuits[pairIndex]) {
            isRender = TRUE;
            circuitMatched = TRUE;
            break;
        }
        if (Circuit == deviceContext->CaptureCircuits[pairIndex]) {
            isRender = FALSE;
            circuitMatched = TRUE;
            break;
        }
    }
    if (!circuitMatched) {
        OpenA8DJ_RecordAcxStage(405, STATUS_INVALID_DEVICE_STATE);
        return STATUS_INVALID_DEVICE_STATE;
    }

    ACX_STREAM_CALLBACKS_INIT(&streamCallbacks);
    streamCallbacks.EvtAcxStreamPrepareHardware = OpenA8DJ_EvtAcxStreamPrepareHardware;
    streamCallbacks.EvtAcxStreamReleaseHardware = OpenA8DJ_EvtAcxStreamReleaseHardware;
    streamCallbacks.EvtAcxStreamRun = OpenA8DJ_EvtAcxStreamRun;
    streamCallbacks.EvtAcxStreamPause = OpenA8DJ_EvtAcxStreamPause;

    status = AcxStreamInitAssignAcxStreamCallbacks(StreamInit, &streamCallbacks);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(401, status);
        return status;
    }

    ACX_RT_STREAM_CALLBACKS_INIT(&rtCallbacks);
    rtCallbacks.EvtAcxStreamGetHwLatency = OpenA8DJ_EvtAcxStreamGetHwLatency;
    rtCallbacks.EvtAcxStreamAllocateRtPackets = OpenA8DJ_EvtAcxStreamAllocateRtPackets;
    rtCallbacks.EvtAcxStreamFreeRtPackets = OpenA8DJ_EvtAcxStreamFreeRtPackets;
    rtCallbacks.EvtAcxStreamSetRenderPacket = OpenA8DJ_EvtAcxStreamSetRenderPacket;
    rtCallbacks.EvtAcxStreamGetCurrentPacket = OpenA8DJ_EvtAcxStreamGetCurrentPacket;
    rtCallbacks.EvtAcxStreamGetCapturePacket = OpenA8DJ_EvtAcxStreamGetCapturePacket;
    rtCallbacks.EvtAcxStreamGetPresentationPosition = OpenA8DJ_EvtAcxStreamGetPresentationPosition;

    status = AcxStreamInitAssignAcxRtStreamCallbacks(StreamInit, &rtCallbacks);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(402, status);
        return status;
    }
    AcxStreamInitSetAcxRtStreamSupportsNotifications(StreamInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, OPENA8DJ_ACX_STREAM_CONTEXT);
    attributes.ParentObject = Circuit;
    status = AcxRtStreamCreate(Device, Circuit, &attributes, &StreamInit, &stream);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(403, status);
        return status;
    }

    streamContext = OpenA8DJGetAcxStreamContext(stream);
    streamContext->DeviceContext = deviceContext;
    streamContext->Stream = stream;
    streamContext->IsRender = isRender;
    streamContext->PairIndex = pairIndex;
    ExInitializeRundownProtection(&streamContext->WorkerRundown);
    streamContext->WorkerRundownCompleted = FALSE;
    streamContext->RtChannels = AcxDataFormatGetChannelsCount(StreamFormat);
    streamContext->RtBlockAlign = AcxDataFormatGetBlockAlign(StreamFormat);
    streamContext->RtBitsPerSample = AcxDataFormatGetBitsPerSample(StreamFormat);
    streamContext->RtIsFloat = FALSE;
    {
        WAVEFORMATEX *waveFormat = (WAVEFORMATEX *)AcxDataFormatGetWaveFormatEx(StreamFormat);
        if (waveFormat != NULL) {
            streamContext->RtChannels = waveFormat->nChannels;
            streamContext->RtBlockAlign = waveFormat->nBlockAlign;
            streamContext->RtBitsPerSample = waveFormat->wBitsPerSample;
            if (waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                streamContext->RtIsFloat = TRUE;
            } else if (waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                       waveFormat->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
                WAVEFORMATEXTENSIBLE *extensible = (WAVEFORMATEXTENSIBLE *)waveFormat;
                if (RtlEqualMemory(
                        &extensible->SubFormat,
                        &kOpenA8DJKsDataFormatSubtypeIeeeFloat,
                        sizeof(GUID))) {
                    streamContext->RtIsFloat = TRUE;
                }
            }
        }
    }
    if (streamContext->RtChannels == 0) {
        streamContext->RtChannels = 2;
    }
    if (streamContext->RtBlockAlign == 0) {
        streamContext->RtBlockAlign = 8;
    }
    if (streamContext->RtBitsPerSample == 0) {
        streamContext->RtBitsPerSample = 32;
        streamContext->RtIsFloat = TRUE;
    }
    if (streamContext->IsRender) {
        deviceContext->AcxRtChannels = streamContext->RtChannels;
        deviceContext->AcxRtBlockAlign = streamContext->RtBlockAlign;
        deviceContext->AcxRtBitsPerSample = streamContext->RtBitsPerSample;
        deviceContext->AcxRtIsFloat = streamContext->RtIsFloat ? 1u : 0u;
    }

    OpenA8DJ_RecordAcxStage(404, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxPinRetrieveName(
    _In_ ACXPIN Pin,
    _Out_ PUNICODE_STRING Name)
{
    POPENA8DJ_ACX_PIN_CONTEXT pinContext = OpenA8DJGetAcxPinContext(Pin);

    if (Name == NULL ||
        Name->Buffer == NULL ||
        pinContext == NULL ||
        pinContext->Name == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (Name->MaximumLength < pinContext->Name->Length + sizeof(WCHAR)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(Name->Buffer, pinContext->Name->Buffer, pinContext->Name->Length);
    Name->Length = pinContext->Name->Length;
    Name->Buffer[Name->Length / sizeof(WCHAR)] = UNICODE_NULL;
    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_CreateAcxCircuit(
    _In_ WDFDEVICE Device,
    _In_ ACX_CIRCUIT_TYPE CircuitType,
    _In_ const GUID *ComponentId,
    _In_ PCUNICODE_STRING CircuitName,
    _In_ PCUNICODE_STRING EndpointName,
    _In_ ULONG PairIndex,
    _In_ BOOLEAN Render,
    _Out_ ACXCIRCUIT *Circuit)
{
    NTSTATUS status;
    PACXCIRCUIT_INIT circuitInit;
    ACXCIRCUIT circuit = NULL;
    ACXPIN pins[2] = { NULL, NULL };
    ACX_PIN_CONFIG pinConfig;
    ACX_PIN_CALLBACKS pinCallbacks;
    WDF_OBJECT_ATTRIBUTES attributes;
    POPENA8DJ_ACX_PIN_CONTEXT pinContext;

    *Circuit = NULL;
    OpenA8DJ_RecordAcxStage(Render ? 100 : 200, STATUS_SUCCESS);
    circuitInit = AcxCircuitInitAllocate(Device);
    if (circuitInit == NULL) {
        OpenA8DJ_RecordAcxStage(Render ? 101 : 201, STATUS_INSUFFICIENT_RESOURCES);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    AcxCircuitInitSetComponentId(circuitInit, ComponentId);
    AcxCircuitInitSetCircuitType(circuitInit, CircuitType);
    status = AcxCircuitInitAssignAcxCreateStreamCallback(circuitInit, OpenA8DJ_EvtCircuitCreateStream);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 102 : 202, status);
        AcxCircuitInitFree(circuitInit);
        return status;
    }

    status = AcxCircuitInitAssignName(circuitInit, CircuitName);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 103 : 203, status);
        AcxCircuitInitFree(circuitInit);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = AcxCircuitCreate(Device, &attributes, &circuitInit, &circuit);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 104 : 204, status);
        if (circuitInit != NULL) {
            AcxCircuitInitFree(circuitInit);
        }
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = circuit;
    ACX_PIN_CONFIG_INIT(&pinConfig);
    pinConfig.Type = Render ? AcxPinTypeSink : AcxPinTypeSource;
    pinConfig.Communication = AcxPinCommunicationSink;
    pinConfig.Category = &kOpenA8DJKsCategoryAudio;
    status = AcxPinCreate(circuit, &attributes, &pinConfig, &pins[0]);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 105 : 205, status);
        return status;
    }

    status = OpenA8DJ_AddDefaultFormatsToPin(Device, circuit, pins[0], PairIndex);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 106 : 206, status);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = circuit;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, OPENA8DJ_ACX_PIN_CONTEXT);
    attributes.ParentObject = circuit;
    ACX_PIN_CALLBACKS_INIT(&pinCallbacks);
    pinCallbacks.EvtAcxPinRetrieveName = OpenA8DJ_EvtAcxPinRetrieveName;
    ACX_PIN_CONFIG_INIT(&pinConfig);
    pinConfig.Type = Render ? AcxPinTypeSource : AcxPinTypeSink;
    pinConfig.Communication = AcxPinCommunicationNone;
    pinConfig.Category = Render ? &kOpenA8DJKsNodeTypeSpeaker : &kOpenA8DJKsNodeTypeMicrophone;
    pinConfig.PinCallbacks = &pinCallbacks;
    status = AcxPinCreate(circuit, &attributes, &pinConfig, &pins[1]);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 107 : 207, status);
        return status;
    }
    pinContext = OpenA8DJGetAcxPinContext(pins[1]);
    pinContext->Name = EndpointName;

    status = AcxCircuitAddPins(circuit, pins, RTL_NUMBER_OF(pins));
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 108 : 208, status);
        return status;
    }

    OpenA8DJ_RecordAcxStage(Render ? 109 : 209, STATUS_SUCCESS);
    *Circuit = circuit;
    return STATUS_SUCCESS;
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
    UCHAR state[OPENA8DJ_CONTROL_STATE_BYTES];
    NTSTATUS status;

    if (ControlState == NULL || ControlState->Size < sizeof(OPENA8DJ_CONTROL_STATE)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (ControlState->InputMode > OPENA8DJ_PROFILE_PHONO) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlCopyMemory(state, Context->RawControlState, sizeof(state));
    state[0] = ControlState->InputMode;
    state[1] = 2;
    state[2] = 3;
    state[4] = 2;

    OpenA8DJ_SetRawFlag(&state[3], (UCHAR)(1u << 0), ControlState->GndLiftTCVinyl != 0);
    OpenA8DJ_SetRawFlag(&state[3], (UCHAR)(1u << 1), ControlState->GndLiftTCCDLine != 0);
    OpenA8DJ_SetRawFlag(&state[3], (UCHAR)(1u << 2), ControlState->GndLiftPhono != 0);
    OpenA8DJ_SetRawFlag(&state[5], (UCHAR)(1u << 0), ControlState->SoftwareLock != 0);

    status = OpenA8DJ_WriteHardwareControlState(Context, state);
    if (!NT_SUCCESS(status)) {
        return status;
    }

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
    NTSTATUS status;

    OpenA8DJ_LoadControlState(Context, &state);

    switch (Profile) {
    case OPENA8DJ_PROFILE_TIMECODE_VINYL:
        state.InputMode = 0;
        state.SoftwareLock = 1;
        break;
    case OPENA8DJ_PROFILE_TIMECODE_CD_LINE:
        state.InputMode = 1;
        state.SoftwareLock = 1;
        break;
    case OPENA8DJ_PROFILE_PHONO:
        state.InputMode = 2;
        state.SoftwareLock = 1;
        break;
    case OPENA8DJ_PROFILE_UNLOCK:
        state.SoftwareLock = 0;
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    status = OpenA8DJ_StoreControlState(Context, &state);
    if (!NT_SUCCESS(status)) {
        return status;
    }
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
    Capabilities->WindowsAudioEndpointExposed = TRUE;
    Capabilities->UsbTransportReady = Context->BulkOutPipe != NULL &&
                                      Context->BulkInPipe != NULL &&
                                      Context->IsoInPipe != NULL &&
                                      Context->IsoOutPipe != NULL;
    Capabilities->MidiReady = FALSE;
    Capabilities->ControlsReady = Context->ControlsHardwareReady;

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
    Surface->AudioEndpointState = OPENA8DJ_COMPONENT_READY;
    Surface->UsbTransportState = (Context->BulkOutPipe != NULL &&
                                  Context->BulkInPipe != NULL &&
                                  Context->IsoInPipe != NULL &&
                                  Context->IsoOutPipe != NULL) ?
                                  OPENA8DJ_COMPONENT_READY : OPENA8DJ_COMPONENT_STUB;
    Surface->IsochronousEngineState = (Context->IsoInPipe != NULL &&
                                       Context->IsoOutPipe != NULL &&
                                       Context->StreamWorkItem != NULL) ?
                                      OPENA8DJ_COMPONENT_READY :
                                      OPENA8DJ_COMPONENT_PLANNED;
    Surface->MidiState = OPENA8DJ_COMPONENT_PLANNED;
    Surface->ControlState = Context->ControlsHardwareReady ?
                            OPENA8DJ_COMPONENT_READY :
                            OPENA8DJ_COMPONENT_STUB;
    Surface->AsioState = OPENA8DJ_COMPONENT_PLANNED;
    Surface->EndpointModel = OPENA8DJ_ENDPOINT_MODEL_PRIMARY_8CH_PLUS_STEREO;
    if (Surface->UsbTransportState == OPENA8DJ_COMPONENT_READY) {
        Surface->SurfaceFlags |= OPENA8DJ_SURFACE_FLAG_USB_TRANSPORT;
    }
    if (Surface->IsochronousEngineState == OPENA8DJ_COMPONENT_READY) {
        Surface->SurfaceFlags |= OPENA8DJ_SURFACE_FLAG_ISOCHRONOUS_ENGINE;
    }
    if (Surface->ControlState == OPENA8DJ_COMPONENT_READY) {
        Surface->SurfaceFlags |= OPENA8DJ_SURFACE_FLAG_CONTROLS;
    }
    OpenA8DJ_CopyFixedString(Surface->DriverModel,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "KMDF USB function driver surface v2");
    OpenA8DJ_CopyFixedString(Surface->AudioModel,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "ACX 1.0 RT primary 8ch plus stereo endpoints");
    OpenA8DJ_CopyFixedString(Surface->StreamingModel,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "capture-paced CAIAQ render worker with silence fallback");
    OpenA8DJ_CopyFixedString(Surface->SafetyPolicy,
                             OPENA8DJ_SURFACE_NAME_LENGTH,
                             "RT render falls back to silence if format or buffer is unsafe");
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
    Topology->EndpointModel = OPENA8DJ_ENDPOINT_MODEL_PRIMARY_8CH_PLUS_STEREO;
    Topology->RenderEndpointCount = 4;
    Topology->CaptureEndpointCount = 4;
    Topology->RenderChannelCount = OPENA8DJ_OUTPUT_CHANNELS;
    Topology->CaptureChannelCount = OPENA8DJ_INPUT_CHANNELS;
    OpenA8DJ_CopyFixedString(Topology->RenderEndpointName,
                             OPENA8DJ_ENDPOINT_NAME_LENGTH,
                             "Open Audio 8 DJ 8ch primary + B/C/D stereo");
    OpenA8DJ_CopyFixedString(Topology->CaptureEndpointName,
                             OPENA8DJ_ENDPOINT_NAME_LENGTH,
                             "Open Audio 8 DJ 8ch primary + B/C/D stereo");

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
    ULONG pairIndex;

    RtlZeroMemory(Diagnostics, sizeof(*Diagnostics));
    Diagnostics->Size = sizeof(*Diagnostics);
    Diagnostics->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
    Diagnostics->StartRequests = Context->StartRequests;
    Diagnostics->RejectedStartRequests = Context->RejectedStartRequests;
    Diagnostics->StopRequests = Context->StopRequests;
    Diagnostics->FormatChanges = Context->FormatChanges;
    Diagnostics->ControlWrites = Context->ControlWrites;
    Diagnostics->ProfileApplies = Context->ProfileApplies;
    Diagnostics->ControlsHardwareReady = Context->ControlsHardwareReady;
    Diagnostics->LastControlWriteMismatch = Context->LastControlWriteMismatch;
    RtlCopyMemory(Diagnostics->RawControlState, Context->RawControlState, sizeof(Diagnostics->RawControlState));
    RtlCopyMemory(Diagnostics->LastControlWriteRequest, Context->LastControlWriteRequest, sizeof(Diagnostics->LastControlWriteRequest));
    RtlCopyMemory(Diagnostics->LastControlWriteReadBack, Context->LastControlWriteReadBack, sizeof(Diagnostics->LastControlWriteReadBack));
    Diagnostics->LastControlReadNtStatus = (ULONG)Context->LastControlReadStatus;
    Diagnostics->LastControlWriteNtStatus = (ULONG)Context->LastControlWriteStatus;
    Diagnostics->LastControlReadbackNtStatus = (ULONG)Context->LastControlReadbackStatus;
    Diagnostics->StreamState = Context->StreamState;
    Diagnostics->AcxCreateStreamCallbacks = (ULONG64)Context->AcxCreateStreamCallbacks;
    Diagnostics->AcxPrepareCallbacks = (ULONG64)Context->AcxPrepareCallbacks;
    Diagnostics->AcxReleaseCallbacks = (ULONG64)Context->AcxReleaseCallbacks;
    Diagnostics->AcxRunCallbacks = (ULONG64)Context->AcxRunCallbacks;
    Diagnostics->AcxPauseCallbacks = (ULONG64)Context->AcxPauseCallbacks;
    Diagnostics->AcxLatencyCallbacks = (ULONG64)Context->AcxLatencyCallbacks;
    Diagnostics->AcxAllocatePacketCallbacks = (ULONG64)Context->AcxAllocatePacketCallbacks;
    Diagnostics->AcxFreePacketCallbacks = (ULONG64)Context->AcxFreePacketCallbacks;
    Diagnostics->AcxSetRenderPacketCallbacks = (ULONG64)Context->AcxSetRenderPacketCallbacks;
    Diagnostics->AcxGetCurrentPacketCallbacks = (ULONG64)Context->AcxGetCurrentPacketCallbacks;
    Diagnostics->AcxGetCapturePacketCallbacks = (ULONG64)Context->AcxGetCapturePacketCallbacks;
    Diagnostics->AcxGetPresentationPositionCallbacks = (ULONG64)Context->AcxGetPresentationPositionCallbacks;
    Diagnostics->AcxRtFramesRead = (ULONG64)Context->AcxRtFramesRead;
    Diagnostics->AcxRtNonZeroFrames = (ULONG64)Context->AcxRtNonZeroFrames;
    Diagnostics->AcxRtPeakAbsS24 = Context->AcxRtPeakAbsS24;
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        Diagnostics->AcxRtRenderPairNonZeroFrames[pairIndex] =
            (ULONG64)Context->AcxRtRenderPairNonZeroFrames[pairIndex];
        Diagnostics->AcxRtRenderPairPeakAbsS24[pairIndex] =
            Context->AcxRtRenderPairPeakAbsS24[pairIndex];
        Diagnostics->AcxRtCapturePairNonZeroFrames[pairIndex] =
            (ULONG64)Context->AcxRtCapturePairNonZeroFrames[pairIndex];
        Diagnostics->AcxRtCapturePairPeakAbsS16[pairIndex] =
            Context->AcxRtCapturePairPeakAbsS16[pairIndex];
    }
    Diagnostics->AcxRtChannels = Context->AcxRtChannels;
    Diagnostics->AcxRtBlockAlign = Context->AcxRtBlockAlign;
    Diagnostics->AcxRtBitsPerSample = Context->AcxRtBitsPerSample;
    Diagnostics->AcxRtIsFloat = Context->AcxRtIsFloat;
    Diagnostics->AcxRtPacketCount = Context->AcxRtPacketCount;
    Diagnostics->AcxRtPacketSize = Context->AcxRtPacketSize;
    Diagnostics->AcxRtFrameCount = Context->AcxRtFrameCount;
    Diagnostics->AcxLastSetRenderPacket = Context->AcxLastSetRenderPacket;
    Diagnostics->AcxLastSetRenderFlags = Context->AcxLastSetRenderFlags;
    Diagnostics->AcxLastSetRenderEosPacketLength = Context->AcxLastSetRenderEosPacketLength;
    Diagnostics->StreamWorkerIterations = (ULONG64)Context->StreamWorkerIterations;
    Diagnostics->StreamWorkerCaptureBytes = (ULONG64)Context->StreamWorkerCaptureBytes;
    Diagnostics->StreamWorkerPlaybackBytes = (ULONG64)Context->StreamWorkerPlaybackBytes;
    Diagnostics->StreamWorkerNoRenderIterations = (ULONG64)Context->StreamWorkerNoRenderIterations;
    Diagnostics->StreamWorkerLastCaptureBytes = Context->StreamWorkerLastCaptureBytes;
    Diagnostics->StreamWorkerLastPlaybackBytes = Context->StreamWorkerLastPlaybackBytes;
    Diagnostics->StreamWorkerLastRenderMask = Context->StreamWorkerLastRenderMask;
    Diagnostics->StreamWorkerLastCaptureMask = Context->StreamWorkerLastCaptureMask;
    Diagnostics->StreamWorkerMaxCaptureBytes = Context->StreamWorkerMaxCaptureBytes;
    Diagnostics->StreamWorkerMaxPlaybackBytes = Context->StreamWorkerMaxPlaybackBytes;
}

static VOID
OpenA8DJ_FillRenderTrace(_In_ POPENA8DJ_DEVICE_CONTEXT Context, _Out_ POPENA8DJ_RENDER_TRACE Trace)
{
    RtlZeroMemory(Trace, sizeof(*Trace));
    Trace->Size = sizeof(*Trace);
    Trace->FrameCount = OPENA8DJ_RENDER_TRACE_FRAME_COUNT;
    Trace->WriteIndex = (ULONG)Context->RenderTraceWriteIndex;
    Trace->WriteCount = (ULONG64)Context->RenderTraceWriteCount;
    Trace->RtChannels = Context->AcxRtChannels;
    Trace->RtBlockAlign = Context->AcxRtBlockAlign;
    Trace->RtBitsPerSample = Context->AcxRtBitsPerSample;
    Trace->RtFrameCount = Context->AcxRtFrameCount;
    Trace->ActiveRenderMask = OpenA8DJ_GetActiveRenderMask(Context);
    RtlCopyMemory(Trace->Frames, Context->RenderTraceFrames, sizeof(Trace->Frames));
}

static VOID
OpenA8DJ_RecordUsbPlaybackTrace(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_reads_bytes_(Length) const UCHAR *Buffer,
    _In_ ULONG Length,
    _In_ ULONG FixedPacketBytes,
    _In_ ULONG ActiveRenderMask)
{
    ULONG byteCount;

    if (Context == NULL || Buffer == NULL || Length == 0) {
        return;
    }

    byteCount = Length;
    if (byteCount > OPENA8DJ_USB_PLAYBACK_TRACE_BYTES) {
        byteCount = OPENA8DJ_USB_PLAYBACK_TRACE_BYTES;
    }

    RtlCopyMemory(Context->UsbPlaybackTraceBytes, Buffer, byteCount);
    Context->UsbPlaybackTraceByteCount = byteCount;
    Context->UsbPlaybackTraceFixedPacketBytes = FixedPacketBytes;
    Context->UsbPlaybackTraceActiveRenderMask = ActiveRenderMask;
    Context->UsbPlaybackTraceWorkerLastPlaybackBytes = Length;
    InterlockedIncrement64(&Context->UsbPlaybackTraceWriteCount);
}

static VOID
OpenA8DJ_FillUsbPlaybackTrace(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_USB_PLAYBACK_TRACE Trace)
{
    ULONG byteCount;

    RtlZeroMemory(Trace, sizeof(*Trace));
    Trace->Size = sizeof(*Trace);
    byteCount = Context->UsbPlaybackTraceByteCount;
    if (byteCount > OPENA8DJ_USB_PLAYBACK_TRACE_BYTES) {
        byteCount = OPENA8DJ_USB_PLAYBACK_TRACE_BYTES;
    }
    Trace->ByteCount = byteCount;
    Trace->FixedPacketBytes = Context->UsbPlaybackTraceFixedPacketBytes;
    Trace->ActiveRenderMask = Context->UsbPlaybackTraceActiveRenderMask;
    Trace->WorkerLastPlaybackBytes = Context->UsbPlaybackTraceWorkerLastPlaybackBytes;
    Trace->WorkerIteration = (ULONG64)Context->UsbPlaybackTraceWriteCount;
    RtlCopyMemory(Trace->Bytes, Context->UsbPlaybackTraceBytes, byteCount);
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
    Context->ConfiguredPipeCount = 0;
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

static NTSTATUS
OpenA8DJ_SelectBestAlternateSetting(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ WDFUSBINTERFACE UsbInterface)
{
    NTSTATUS status;
    UCHAR settingCount;
    UCHAR settingIndex;
    UCHAR bestSetting;
    UCHAR bestEndpointCount;
    UCHAR interfaceNumber;

    if (UsbInterface == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    settingCount = WdfUsbInterfaceGetNumSettings(UsbInterface);
    interfaceNumber = WdfUsbInterfaceGetInterfaceNumber(UsbInterface);
    bestSetting = 0;
    bestEndpointCount = 0;
    Context->AlternateSettingCount = settingCount;
    Context->SelectedAlternateSetting = 0;

    for (settingIndex = 0; settingIndex < settingCount; settingIndex++) {
        USB_INTERFACE_DESCRIPTOR descriptor;

        RtlZeroMemory(&descriptor, sizeof(descriptor));
        WdfUsbInterfaceGetDescriptor(UsbInterface, settingIndex, &descriptor);

        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   "OpenA8DJUsb: interface %u alt[%u] number=%u alternate=%u endpoints=%u class=0x%02x subclass=0x%02x protocol=0x%02x\n",
                   interfaceNumber,
                   settingIndex,
                   descriptor.bInterfaceNumber,
                   descriptor.bAlternateSetting,
                   descriptor.bNumEndpoints,
                   descriptor.bInterfaceClass,
                   descriptor.bInterfaceSubClass,
                   descriptor.bInterfaceProtocol));

        if (descriptor.bNumEndpoints > bestEndpointCount) {
            bestEndpointCount = descriptor.bNumEndpoints;
            bestSetting = settingIndex;
            Context->SelectedAlternateSetting = descriptor.bAlternateSetting;
        }
    }

    if (bestEndpointCount == 0) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_WARNING_LEVEL,
                   "OpenA8DJUsb: interface %u has no endpoints in %u alternate settings\n",
                   interfaceNumber,
                   settingCount));
        return STATUS_SUCCESS;
    }

    if (bestSetting != 0) {
        WDF_USB_INTERFACE_SELECT_SETTING_PARAMS settingParams;

        WDF_USB_INTERFACE_SELECT_SETTING_PARAMS_INIT_SETTING(
            &settingParams,
            bestSetting);

        status = WdfUsbInterfaceSelectSetting(
            UsbInterface,
            WDF_NO_OBJECT_ATTRIBUTES,
            &settingParams);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "OpenA8DJUsb: WdfUsbInterfaceSelectSetting interface %u setting %u failed 0x%08x\n",
                       interfaceNumber,
                       bestSetting,
                       status));
            return status;
        }
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: selected interface %u alternate setting index=%u endpoints=%u\n",
               interfaceNumber,
               bestSetting,
               bestEndpointCount));

    return STATUS_SUCCESS;
}

static VOID
OpenA8DJ_MapConfiguredPipesForInterface(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ WDFUSBINTERFACE UsbInterface)
{
    UCHAR pipeCount;
    UCHAR pipeIndex;
    UCHAR interfaceNumber;

    if (UsbInterface == NULL) {
        return;
    }

    interfaceNumber = WdfUsbInterfaceGetInterfaceNumber(UsbInterface);
    pipeCount = WdfUsbInterfaceGetNumConfiguredPipes(UsbInterface);
    Context->ConfiguredPipeCount = (UCHAR)(Context->ConfiguredPipeCount + pipeCount);

    for (pipeIndex = 0; pipeIndex < pipeCount; pipeIndex++) {
        WDF_USB_PIPE_INFORMATION pipeInfo;
        WDFUSBPIPE pipe;
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
        pipe = WdfUsbInterfaceGetConfiguredPipe(
            UsbInterface,
            pipeIndex,
            &pipeInfo);

        OpenA8DJ_MapPipe(Context, pipe, &pipeInfo);

        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   "OpenA8DJUsb: interface %u pipe %u endpoint 0x%02x type %d maxPacket %u\n",
                   interfaceNumber,
                   pipeIndex,
                   pipeInfo.EndpointAddress,
                   pipeInfo.PipeType,
                   pipeInfo.MaximumPacketSize));
    }
}

static VOID
OpenA8DJ_MapConfiguredPipes(_In_ WDFDEVICE Device)
{
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(Device);
    UCHAR interfaceIndex;

    OpenA8DJ_ResetPipeMap(context);

    for (interfaceIndex = 0;
         interfaceIndex < context->ConfiguredInterfaceCount &&
         interfaceIndex < OPENA8DJ_MAX_USB_INTERFACES;
         interfaceIndex++) {
        OpenA8DJ_MapConfiguredPipesForInterface(context, context->UsbInterfaces[interfaceIndex]);
    }
}

static NTSTATUS
OpenA8DJ_CaptureIsoSnapshotWithPayload(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_ISO_CAPTURE_SNAPSHOT Snapshot,
    _Out_writes_bytes_opt_(PayloadBufferLength) UCHAR *PayloadBuffer,
    _In_ ULONG PayloadBufferLength,
    _Out_opt_ ULONG *PayloadBytesCopied)
{
    NTSTATUS status;
    WDFMEMORY urbMemory;
    PURB urb;
    PVOID transferBuffer;
    ULONG packetIndex;
    ULONG maximumPacketSize;
    ULONG transferBufferLength;
    WDF_USB_PIPE_INFORMATION pipeInfo;
    WDF_OBJECT_ATTRIBUTES memoryAttributes;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    RtlZeroMemory(Snapshot, sizeof(*Snapshot));
    Snapshot->Size = sizeof(*Snapshot);
    Snapshot->PacketCount = OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    if (PayloadBytesCopied != NULL) {
        *PayloadBytesCopied = 0;
    }

    if (Context->IsoInPipe == NULL) {
        Snapshot->NtStatus = (ULONG)STATUS_DEVICE_NOT_READY;
        return STATUS_DEVICE_NOT_READY;
    }

    WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
    WdfUsbTargetPipeGetInformation(Context->IsoInPipe, &pipeInfo);
    maximumPacketSize = pipeInfo.MaximumPacketSize;
    if (maximumPacketSize == 0 || maximumPacketSize > 4096) {
        Snapshot->NtStatus = (ULONG)STATUS_INVALID_DEVICE_STATE;
        return STATUS_INVALID_DEVICE_STATE;
    }

    transferBufferLength = maximumPacketSize * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    Snapshot->MaximumPacketSize = maximumPacketSize;
    Snapshot->TransferBufferLength = transferBufferLength;

    transferBuffer = ExAllocatePoolZero(
        NonPagedPoolNx,
        transferBufferLength,
        OPENA8DJ_POOL_TAG);
    if (transferBuffer == NULL) {
        Snapshot->NtStatus = (ULONG)STATUS_INSUFFICIENT_RESOURCES;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = Context->UsbDevice;
    status = WdfUsbTargetDeviceCreateIsochUrb(
        Context->UsbDevice,
        &memoryAttributes,
        OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
        &urbMemory,
        &urb);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(transferBuffer, OPENA8DJ_POOL_TAG);
        Snapshot->NtStatus = (ULONG)status;
        return status;
    }

    urb->UrbIsochronousTransfer.Hdr.Function = URB_FUNCTION_ISOCH_TRANSFER;
    urb->UrbIsochronousTransfer.PipeHandle = WdfUsbTargetPipeWdmGetPipeHandle(Context->IsoInPipe);
    urb->UrbIsochronousTransfer.TransferFlags =
        USBD_TRANSFER_DIRECTION_IN |
        USBD_SHORT_TRANSFER_OK |
        USBD_START_ISO_TRANSFER_ASAP;
    urb->UrbIsochronousTransfer.TransferBufferLength = transferBufferLength;
    urb->UrbIsochronousTransfer.TransferBuffer = transferBuffer;
    urb->UrbIsochronousTransfer.TransferBufferMDL = NULL;
    urb->UrbIsochronousTransfer.StartFrame = 0;
    urb->UrbIsochronousTransfer.NumberOfPackets = OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    urb->UrbIsochronousTransfer.ErrorCount = 0;

    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset = packetIndex * maximumPacketSize;
        urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = maximumPacketSize;
        urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status = USBD_STATUS_SUCCESS;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(1));

    status = WdfUsbTargetPipeSendUrbSynchronously(
        Context->IsoInPipe,
        NULL,
        &sendOptions,
        urb);

    Snapshot->NtStatus = (ULONG)status;
    Snapshot->ErrorCount = urb->UrbIsochronousTransfer.ErrorCount;
    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        Snapshot->Packets[packetIndex].Offset = urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset;
        Snapshot->Packets[packetIndex].RequestedLength = maximumPacketSize;
        Snapshot->Packets[packetIndex].CompletedLength = urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length;
        Snapshot->Packets[packetIndex].UsbdStatus = urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status;
    }
    RtlCopyMemory(
        Snapshot->FirstBytes,
        transferBuffer,
        sizeof(Snapshot->FirstBytes) < transferBufferLength ? sizeof(Snapshot->FirstBytes) : transferBufferLength);
    if (PayloadBuffer != NULL && PayloadBufferLength != 0) {
        ULONG bytesToCopy = PayloadBufferLength < transferBufferLength ? PayloadBufferLength : transferBufferLength;

        RtlCopyMemory(PayloadBuffer, transferBuffer, bytesToCopy);
        if (PayloadBytesCopied != NULL) {
            *PayloadBytesCopied = bytesToCopy;
        }
    }

    ExFreePoolWithTag(transferBuffer, OPENA8DJ_POOL_TAG);
    WdfObjectDelete(urbMemory);
    return status;
}

static NTSTATUS
OpenA8DJ_CaptureIsoSnapshotPrepared(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_ISO_CAPTURE_SNAPSHOT Snapshot,
    _Inout_updates_bytes_(TransferBufferLength) UCHAR *TransferBuffer,
    _In_ ULONG TransferBufferLength,
    _In_ ULONG MaximumPacketSize,
    _Inout_ PURB Urb)
{
    NTSTATUS status;
    ULONG packetIndex;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    RtlZeroMemory(Snapshot, sizeof(*Snapshot));
    Snapshot->Size = sizeof(*Snapshot);
    Snapshot->PacketCount = OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    Snapshot->MaximumPacketSize = MaximumPacketSize;
    Snapshot->TransferBufferLength = TransferBufferLength;

    if (Context->IsoInPipe == NULL ||
        TransferBuffer == NULL ||
        Urb == NULL ||
        MaximumPacketSize == 0 ||
        TransferBufferLength != MaximumPacketSize * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT) {
        Snapshot->NtStatus = (ULONG)STATUS_INVALID_DEVICE_STATE;
        return STATUS_INVALID_DEVICE_STATE;
    }

    RtlZeroMemory(TransferBuffer, TransferBufferLength);
    RtlZeroMemory(Urb, GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT));
    Urb->UrbIsochronousTransfer.Hdr.Function = URB_FUNCTION_ISOCH_TRANSFER;
    Urb->UrbIsochronousTransfer.PipeHandle = WdfUsbTargetPipeWdmGetPipeHandle(Context->IsoInPipe);
    Urb->UrbIsochronousTransfer.TransferFlags =
        USBD_TRANSFER_DIRECTION_IN |
        USBD_SHORT_TRANSFER_OK |
        USBD_START_ISO_TRANSFER_ASAP;
    Urb->UrbIsochronousTransfer.TransferBufferLength = TransferBufferLength;
    Urb->UrbIsochronousTransfer.TransferBuffer = TransferBuffer;
    Urb->UrbIsochronousTransfer.TransferBufferMDL = NULL;
    Urb->UrbIsochronousTransfer.StartFrame = 0;
    Urb->UrbIsochronousTransfer.NumberOfPackets = OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    Urb->UrbIsochronousTransfer.ErrorCount = 0;

    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset = packetIndex * MaximumPacketSize;
        Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = MaximumPacketSize;
        Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status = USBD_STATUS_SUCCESS;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(1));
    status = WdfUsbTargetPipeSendUrbSynchronously(
        Context->IsoInPipe,
        NULL,
        &sendOptions,
        Urb);

    Snapshot->NtStatus = (ULONG)status;
    Snapshot->ErrorCount = Urb->UrbIsochronousTransfer.ErrorCount;
    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        Snapshot->Packets[packetIndex].Offset = Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset;
        Snapshot->Packets[packetIndex].RequestedLength = MaximumPacketSize;
        Snapshot->Packets[packetIndex].CompletedLength = Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length;
        Snapshot->Packets[packetIndex].UsbdStatus = Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status;
    }
    RtlCopyMemory(
        Snapshot->FirstBytes,
        TransferBuffer,
        sizeof(Snapshot->FirstBytes) < TransferBufferLength ? sizeof(Snapshot->FirstBytes) : TransferBufferLength);
    return status;
}

static NTSTATUS
OpenA8DJ_CaptureIsoSnapshot(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_ISO_CAPTURE_SNAPSHOT Snapshot)
{
    return OpenA8DJ_CaptureIsoSnapshotWithPayload(Context, Snapshot, NULL, 0, NULL);
}

static UCHAR
OpenA8DJ_Mode2CheckByte(_In_ ULONG Stream, _In_ ULONG ByteIndex)
{
    ULONG group = ByteIndex / 16u;
    return (UCHAR)((Stream << 1u) | ((~group) & 1u));
}

typedef struct _OPENA8DJ_MODE2_TONE_PACKER {
    UCHAR OutputFrameBytes[4][6];
    ULONG Phase;
    UCHAR OutputByteInFrame;
    BOOLEAN OutputFrameLoaded;
} OPENA8DJ_MODE2_TONE_PACKER, *POPENA8DJ_MODE2_TONE_PACKER;

static const LONG kOpenA8DJToneTable40[40] = {
    0, 5126, 10126, 14876, 19260, 23170, 26509, 29196,
    31163, 32364, 32767, 32364, 31163, 29196, 26509, 23170,
    19260, 14876, 10126, 5126, 0, -5126, -10126, -14876,
    -19260, -23170, -26509, -29196, -31163, -32364, -32767, -32364,
    -31163, -29196, -26509, -23170, -19260, -14876, -10126, -5126
};

static const LONG kOpenA8DJToneTable48[48] = {
    0, 4277, 8481, 12539, 16383, 19947, 23170, 25996,
    28377, 30273, 31650, 32487, 32767, 32487, 31650, 30273,
    28377, 25996, 23170, 19947, 16383, 12539, 8481, 4277,
    0, -4277, -8481, -12539, -16383, -19947, -23170, -25996,
    -28377, -30273, -31650, -32487, -32767, -32487, -31650, -30273,
    -28377, -25996, -23170, -19947, -16384, -12539, -8481, -4277
};

static const LONG kOpenA8DJToneTable56[56] = {
    0, 3669, 7291, 10822, 14217, 17433, 20430, 23170,
    25618, 27745, 29522, 30928, 31945, 32561, 32767, 32561,
    31945, 30928, 29522, 27745, 25618, 23170, 20430, 17433,
    14217, 10822, 7291, 3669, 0, -3669, -7291, -10822,
    -14217, -17433, -20430, -23170, -25618, -27745, -29522, -30928,
    -31945, -32561, -32767, -32561, -31945, -30928, -29522, -27745,
    -25618, -23170, -20430, -17433, -14217, -10822, -7291, -3669
};

static const LONG kOpenA8DJToneTable64[64] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787,
    23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30273, 28898, 27245, 25329,
    23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609,
    -32767, -32609, -32137, -31356, -30273, -28898, -27245, -25329,
    -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212
};

static const LONG *
OpenA8DJ_SelectToneTable(_In_ ULONG PeriodSamples, _Out_ ULONG *TableLength)
{
    if (PeriodSamples == 40u) {
        *TableLength = 40u;
        return kOpenA8DJToneTable40;
    }
    if (PeriodSamples == 48u) {
        *TableLength = 48u;
        return kOpenA8DJToneTable48;
    }
    if (PeriodSamples == 56u) {
        *TableLength = 56u;
        return kOpenA8DJToneTable56;
    }
    *TableLength = 64u;
    return kOpenA8DJToneTable64;
}

static VOID
OpenA8DJ_PackS24BE(_In_ LONG Sample, _Out_writes_bytes_(3) UCHAR *Out)
{
    ULONG value = (ULONG)Sample;
#if OPENA8DJ_OUTPUT_NATIVE_I24
    Out[0] = (UCHAR)value;
    Out[1] = (UCHAR)(value >> 8);
    Out[2] = (UCHAR)(value >> 16);
#else
    Out[0] = (UCHAR)(value >> 16);
    Out[1] = (UCHAR)(value >> 8);
    Out[2] = (UCHAR)value;
#endif
}

static VOID
OpenA8DJ_LoadToneFrame(
    _Inout_ POPENA8DJ_MODE2_TONE_PACKER Packer,
    _In_ ULONG PairIndex,
    _In_ ULONG AmplitudeQ15,
    _In_ ULONG PeriodSamples)
{
    LONGLONG sample;
    LONG sample24;
    ULONG tableLength;
    const LONG *table = OpenA8DJ_SelectToneTable(PeriodSamples, &tableLength);

    RtlZeroMemory(Packer->OutputFrameBytes, sizeof(Packer->OutputFrameBytes));
    sample = (LONGLONG)table[Packer->Phase] *
             (LONGLONG)AmplitudeQ15 *
             8388607ll;
    sample24 = (LONG)(sample / (32767ll * 32768ll));

    OpenA8DJ_PackS24BE(sample24, &Packer->OutputFrameBytes[PairIndex][0]);
    OpenA8DJ_PackS24BE(sample24, &Packer->OutputFrameBytes[PairIndex][3]);

    Packer->Phase++;
    if (Packer->Phase >= tableLength) {
        Packer->Phase = 0;
    }
    Packer->OutputFrameLoaded = TRUE;
}

static VOID
OpenA8DJ_FillMode2Tone(
    _Out_writes_bytes_(Length) UCHAR *Buffer,
    _In_ ULONG Length,
    _Inout_ POPENA8DJ_MODE2_TONE_PACKER Packer,
    _In_ ULONG PairIndex,
    _In_ ULONG AmplitudeQ15,
    _In_ ULONG PeriodSamples)
{
    ULONG index = 0;

    while (index < Length) {
        ULONG groupOffset = index % 16u;
        ULONG stream;

        if (groupOffset == 8u) {
            for (stream = 0; stream < 4u && index < Length; stream++) {
                Buffer[index] = OpenA8DJ_Mode2CheckByte(stream, index);
                index++;
            }
            continue;
        }

        if (!Packer->OutputFrameLoaded || Packer->OutputByteInFrame == 0) {
            OpenA8DJ_LoadToneFrame(Packer, PairIndex, AmplitudeQ15, PeriodSamples);
        }

        for (stream = 0; stream < 4u && index < Length; stream++) {
            Buffer[index] = Packer->OutputFrameBytes[stream][Packer->OutputByteInFrame];
            index++;
        }

        Packer->OutputByteInFrame++;
        if (Packer->OutputByteInFrame >= 6u) {
            Packer->OutputByteInFrame = 0;
        }
    }
}

static VOID
OpenA8DJ_FillMode2Silence(_Out_writes_bytes_(Length) UCHAR *Buffer, _In_ ULONG Length)
{
    ULONG index = 0;

    while (index < Length) {
        ULONG groupOffset = index % 16u;
        ULONG stream;

        if (groupOffset == 8u) {
            for (stream = 0; stream < 4u && index < Length; stream++) {
                Buffer[index] = OpenA8DJ_Mode2CheckByte(stream, index);
                index++;
            }
            continue;
        }

        for (stream = 0; stream < 4u && index < Length; stream++) {
            Buffer[index] = 0;
            index++;
        }
    }
}

static LONG
OpenA8DJ_Float32BitsToS24(_In_ ULONG Bits)
{
    ULONG exponent = (Bits >> 23) & 0xffu;
    ULONG mantissa = Bits & 0x7fffffu;
    BOOLEAN negative = (Bits & 0x80000000u) != 0;
    LONGLONG scaled;
    ULONG shift;

    if (exponent == 0) {
        return 0;
    }

    mantissa |= 0x800000u;
    if (exponent > 127u) {
        return negative ? -0x7fffff : 0x7fffff;
    }

    shift = 150u - exponent;
    if (shift >= 63u) {
        return 0;
    }

    scaled = ((LONGLONG)mantissa * 8388607ll) >> shift;
    if (scaled > 0x7fffffll) {
        scaled = 0x7fffffll;
    }
    return negative ? -(LONG)scaled : (LONG)scaled;
}

static LONG
OpenA8DJ_ReadRtSampleS24(
    _In_ const UCHAR *Frame,
    _In_ ULONG Channel,
    _In_ const OPENA8DJ_ACX_STREAM_CONTEXT *StreamContext)
{
    ULONG bytesPerSample;
    const UCHAR *sample;
    LONG value;

    if (Channel >= StreamContext->RtChannels ||
        StreamContext->RtBitsPerSample == 0 ||
        StreamContext->RtBitsPerSample > 32u) {
        return 0;
    }

    bytesPerSample = (StreamContext->RtBitsPerSample + 7u) / 8u;
    if (bytesPerSample == 0 ||
        (Channel + 1u) * bytesPerSample > StreamContext->RtBlockAlign) {
        return 0;
    }

    sample = Frame + Channel * bytesPerSample;
    if (StreamContext->RtIsFloat && StreamContext->RtBitsPerSample == 32u) {
        ULONG bits = ((ULONG)sample[0]) |
                     ((ULONG)sample[1] << 8) |
                     ((ULONG)sample[2] << 16) |
                     ((ULONG)sample[3] << 24);
        return OpenA8DJ_Float32BitsToS24(bits);
    }

    if (StreamContext->RtBitsPerSample <= 16u && bytesPerSample >= 2u) {
        SHORT sample16 = (SHORT)(((USHORT)sample[0]) | ((USHORT)sample[1] << 8));
        return ((LONG)sample16) << 8;
    }

    if (StreamContext->RtBitsPerSample <= 24u && bytesPerSample >= 3u) {
        value = ((LONG)sample[0]) | ((LONG)sample[1] << 8) | ((LONG)sample[2] << 16);
        if ((value & 0x00800000) != 0) {
            value |= (LONG)0xff000000;
        }
        return value;
    }

    if (bytesPerSample >= 4u) {
        value = ((LONG)sample[0]) |
                ((LONG)sample[1] << 8) |
                ((LONG)sample[2] << 16) |
                ((LONG)sample[3] << 24);
        return value >> 8;
    }

    return 0;
}

static SHORT
OpenA8DJ_UnpackS24BEToS16(_In_reads_bytes_(3) const UCHAR *Sample)
{
    LONG value = ((LONG)Sample[0] << 16) | ((LONG)Sample[1] << 8) | (LONG)Sample[2];

    if ((value & 0x00800000) != 0) {
        value |= (LONG)0xff000000;
    }
    return (SHORT)(value >> 8);
}

static VOID
OpenA8DJ_WriteRtCaptureSampleS16(
    _Inout_updates_bytes_(StreamContext->RtBlockAlign) UCHAR *Frame,
    _In_ ULONG Channel,
    _In_ SHORT Sample,
    _In_ const OPENA8DJ_ACX_STREAM_CONTEXT *StreamContext)
{
    ULONG bytesPerSample;
    UCHAR *sample;

    if (Channel >= StreamContext->RtChannels ||
        StreamContext->RtBitsPerSample == 0 ||
        StreamContext->RtBitsPerSample > 32u) {
        return;
    }

    bytesPerSample = (StreamContext->RtBitsPerSample + 7u) / 8u;
    if (bytesPerSample == 0 ||
        (Channel + 1u) * bytesPerSample > StreamContext->RtBlockAlign) {
        return;
    }

    sample = Frame + Channel * bytesPerSample;
    if (StreamContext->RtBitsPerSample <= 16u && bytesPerSample >= 2u) {
        sample[0] = (UCHAR)((USHORT)Sample & 0xffu);
        sample[1] = (UCHAR)(((USHORT)Sample >> 8) & 0xffu);
    } else if (StreamContext->RtBitsPerSample <= 24u && bytesPerSample >= 3u) {
        LONG sample24 = ((LONG)Sample) << 8;

        sample[0] = (UCHAR)(sample24 & 0xff);
        sample[1] = (UCHAR)((sample24 >> 8) & 0xff);
        sample[2] = (UCHAR)((sample24 >> 16) & 0xff);
    } else if (bytesPerSample >= 4u) {
        LONG sample32 = ((LONG)Sample) << 16;

        sample[0] = (UCHAR)(sample32 & 0xff);
        sample[1] = (UCHAR)((sample32 >> 8) & 0xff);
        sample[2] = (UCHAR)((sample32 >> 16) & 0xff);
        sample[3] = (UCHAR)((sample32 >> 24) & 0xff);
    }
}

static VOID
OpenA8DJ_CompleteCaptureFrame(_Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    UCHAR *frame;
    ULONG pairIndex;

    if (StreamContext->RtKernelAddress == NULL ||
        StreamContext->RtFrameCount == 0 ||
        StreamContext->RtBlockAlign == 0) {
        return;
    }

    frame = ((UCHAR *)StreamContext->RtKernelAddress) +
            (StreamContext->CaptureFrameCursor % StreamContext->RtFrameCount) *
            StreamContext->RtBlockAlign;
    RtlZeroMemory(frame, StreamContext->RtBlockAlign);
    if (StreamContext->RtChannels <= 2u) {
        pairIndex = StreamContext->PairIndex;
        if (pairIndex < OPENA8DJ_STEREO_PAIRS) {
            SHORT left = OpenA8DJ_UnpackS24BEToS16(&StreamContext->InputFrameBytes[pairIndex][0]);
            SHORT right = OpenA8DJ_UnpackS24BEToS16(&StreamContext->InputFrameBytes[pairIndex][3]);
            LONG left32 = left;
            LONG right32 = right;
            ULONG absLeft = left32 < 0 ? (ULONG)(-left32) : (ULONG)left32;
            ULONG absRight = right32 < 0 ? (ULONG)(-right32) : (ULONG)right32;
            ULONG pairPeak = absLeft > absRight ? absLeft : absRight;

            OpenA8DJ_WriteRtCaptureSampleS16(frame, 0, left, StreamContext);
            OpenA8DJ_WriteRtCaptureSampleS16(frame, 1, right, StreamContext);
            if (StreamContext->DeviceContext != NULL && pairPeak != 0) {
                InterlockedIncrement64(&StreamContext->DeviceContext->AcxRtCapturePairNonZeroFrames[pairIndex]);
                if (pairPeak > StreamContext->DeviceContext->AcxRtCapturePairPeakAbsS16[pairIndex]) {
                    StreamContext->DeviceContext->AcxRtCapturePairPeakAbsS16[pairIndex] = pairPeak;
                }
            }
        }
    } else {
        for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
            SHORT left = OpenA8DJ_UnpackS24BEToS16(&StreamContext->InputFrameBytes[pairIndex][0]);
            SHORT right = OpenA8DJ_UnpackS24BEToS16(&StreamContext->InputFrameBytes[pairIndex][3]);
            LONG left32 = left;
            LONG right32 = right;
            ULONG absLeft = left32 < 0 ? (ULONG)(-left32) : (ULONG)left32;
            ULONG absRight = right32 < 0 ? (ULONG)(-right32) : (ULONG)right32;
            ULONG pairPeak = absLeft > absRight ? absLeft : absRight;

            OpenA8DJ_WriteRtCaptureSampleS16(frame, pairIndex * 2u, left, StreamContext);
            OpenA8DJ_WriteRtCaptureSampleS16(frame, pairIndex * 2u + 1u, right, StreamContext);
            if (StreamContext->DeviceContext != NULL && pairPeak != 0) {
                InterlockedIncrement64(&StreamContext->DeviceContext->AcxRtCapturePairNonZeroFrames[pairIndex]);
                if (pairPeak > StreamContext->DeviceContext->AcxRtCapturePairPeakAbsS16[pairIndex]) {
                    StreamContext->DeviceContext->AcxRtCapturePairPeakAbsS16[pairIndex] = pairPeak;
                }
            }
        }
    }

    StreamContext->CaptureFrameCursor =
        (StreamContext->CaptureFrameCursor + 1u) % StreamContext->RtFrameCount;
    StreamContext->PositionBlocks++;
    if (StreamContext->RtPacketCount != 0 &&
        StreamContext->RtPacketSize != 0 &&
        StreamContext->RtBlockAlign != 0) {
        ULONG framesPerPacket = StreamContext->RtPacketSize / StreamContext->RtBlockAlign;

        if (framesPerPacket != 0 &&
            (StreamContext->PositionBlocks % framesPerPacket) == 0) {
            LARGE_INTEGER qpc;

            StreamContext->LastCompletedPacket = StreamContext->CurrentPacket;
            StreamContext->CurrentPacket =
                (StreamContext->CurrentPacket + 1u) % StreamContext->RtPacketCount;
            qpc = KeQueryPerformanceCounter(NULL);
            if (StreamContext->RtPacketCount > 1u && StreamContext->Stream != NULL) {
                (VOID)AcxRtStreamNotifyPacketComplete(
                    StreamContext->Stream,
                    StreamContext->LastCompletedPacket,
                    (ULONGLONG)qpc.QuadPart);
            }
        }
    }
}

static ULONG
OpenA8DJ_FillRtCaptureFromMode2(
    _In_reads_bytes_(Length) const UCHAR *Buffer,
    _In_ ULONG Length,
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    ULONG index = 0;
    ULONG framesWritten = 0;

    while (index + 4u <= Length) {
        ULONG groupOffset = index % 16u;
        ULONG stream;

        if (groupOffset == 0u) {
            index += 4u;
            continue;
        }

        for (stream = 0; stream < OPENA8DJ_STEREO_PAIRS; stream++) {
            StreamContext->InputFrameBytes[stream][StreamContext->InputByteInFrame] =
                Buffer[index + stream];
        }
        index += 4u;
        StreamContext->InputByteInFrame++;
        if (StreamContext->InputByteInFrame >= 6u) {
            StreamContext->InputByteInFrame = 0;
            OpenA8DJ_CompleteCaptureFrame(StreamContext);
            framesWritten++;
        }
    }

    return framesWritten;
}

static VOID
OpenA8DJ_RecordRenderTraceFrame(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ const OPENA8DJ_ACX_STREAM_CONTEXT *StreamContext,
    _In_ ULONG RtFrameCursor,
    _In_ LONG RawLeftS24,
    _In_ LONG RawRightS24,
    _In_ LONG OutputLeftS24,
    _In_ LONG OutputRightS24)
{
    LONG64 count;
    ULONG slot;
    OPENA8DJ_RENDER_TRACE_FRAME frame;

    if (Context == NULL || StreamContext == NULL) {
        return;
    }

    RtlZeroMemory(&frame, sizeof(frame));
    frame.PairIndex = StreamContext->PairIndex;
    frame.RtChannels = StreamContext->RtChannels;
    frame.RtBlockAlign = StreamContext->RtBlockAlign;
    frame.RtBitsPerSample = StreamContext->RtBitsPerSample;
    frame.RtFrameCursor = RtFrameCursor;
    frame.RtFrameCount = StreamContext->RtFrameCount;
    frame.OutputByteInFrame = StreamContext->OutputByteInFrame;
    frame.PositionBlocks = StreamContext->PositionBlocks;
    frame.RawLeftS24 = RawLeftS24;
    frame.RawRightS24 = RawRightS24;
    frame.OutputLeftS24 = OutputLeftS24;
    frame.OutputRightS24 = OutputRightS24;

    count = InterlockedIncrement64(&Context->RenderTraceWriteCount);
    slot = (ULONG)((count - 1) % OPENA8DJ_RENDER_TRACE_FRAME_COUNT);
    Context->RenderTraceFrames[slot] = frame;
    InterlockedExchange(&Context->RenderTraceWriteIndex, (LONG)slot);
}

static VOID
OpenA8DJ_LoadRtRenderFrame(_Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    UCHAR *frame;
    ULONG peak = 0;
    ULONG pairIndex;
    ULONG rtFrameCursor = 0;
    LONG samples[OPENA8DJ_STEREO_PAIRS][2];
    LONG rawSamples[OPENA8DJ_STEREO_PAIRS][2];

    RtlZeroMemory(StreamContext->OutputFrameBytes, sizeof(StreamContext->OutputFrameBytes));
    RtlZeroMemory(samples, sizeof(samples));
    RtlZeroMemory(rawSamples, sizeof(rawSamples));
    if (StreamContext->RenderPrefillFramesRemaining != 0) {
        StreamContext->RenderPrefillFramesRemaining--;
        StreamContext->PositionBlocks++;
        StreamContext->OutputFrameLoaded = TRUE;
        return;
    }
    if (StreamContext->RtKernelAddress != NULL &&
        StreamContext->RtFrameCount != 0 &&
        StreamContext->RtBlockAlign != 0) {
        rtFrameCursor = StreamContext->RenderFrameCursor % StreamContext->RtFrameCount;
        frame = ((UCHAR *)StreamContext->RtKernelAddress) +
                rtFrameCursor * StreamContext->RtBlockAlign;
        if (StreamContext->RtChannels <= 2u) {
            pairIndex = StreamContext->PairIndex;
            if (pairIndex < OPENA8DJ_STEREO_PAIRS) {
                LONG left = OpenA8DJ_ReadRtSampleS24(frame, 0, StreamContext);
                LONG right = OpenA8DJ_ReadRtSampleS24(frame, 1, StreamContext);
                ULONG absLeft = left < 0 ? (ULONG)(-left) : (ULONG)left;
                ULONG absRight = right < 0 ? (ULONG)(-right) : (ULONG)right;
                ULONG pairPeak = absLeft > absRight ? absLeft : absRight;

                samples[pairIndex][0] = left;
                samples[pairIndex][1] = right;
                rawSamples[pairIndex][0] = left;
                rawSamples[pairIndex][1] = right;
                if (pairPeak > peak) {
                    peak = pairPeak;
                }
                if (StreamContext->DeviceContext != NULL && pairPeak != 0) {
                    InterlockedIncrement64(&StreamContext->DeviceContext->AcxRtRenderPairNonZeroFrames[pairIndex]);
                    if (pairPeak > StreamContext->DeviceContext->AcxRtRenderPairPeakAbsS24[pairIndex]) {
                        StreamContext->DeviceContext->AcxRtRenderPairPeakAbsS24[pairIndex] = pairPeak;
                    }
                }
            }
        } else {
            for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
                ULONG leftChannel = pairIndex * 2u;
                ULONG rightChannel = leftChannel + 1u;
                LONG left = OpenA8DJ_ReadRtSampleS24(frame, leftChannel, StreamContext);
                LONG right = OpenA8DJ_ReadRtSampleS24(frame, rightChannel, StreamContext);
                ULONG absLeft = left < 0 ? (ULONG)(-left) : (ULONG)left;
                ULONG absRight = right < 0 ? (ULONG)(-right) : (ULONG)right;
                ULONG pairPeak = absLeft > absRight ? absLeft : absRight;

                samples[pairIndex][0] = left;
                samples[pairIndex][1] = right;
                rawSamples[pairIndex][0] = left;
                rawSamples[pairIndex][1] = right;
                if (pairPeak > peak) {
                    peak = pairPeak;
                }
                if (StreamContext->DeviceContext != NULL && pairPeak != 0) {
                    InterlockedIncrement64(&StreamContext->DeviceContext->AcxRtRenderPairNonZeroFrames[pairIndex]);
                    if (pairPeak > StreamContext->DeviceContext->AcxRtRenderPairPeakAbsS24[pairIndex]) {
                        StreamContext->DeviceContext->AcxRtRenderPairPeakAbsS24[pairIndex] = pairPeak;
                    }
                }
            }
        }
        for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
            OpenA8DJ_PackS24BE(samples[pairIndex][0], &StreamContext->OutputFrameBytes[pairIndex][0]);
            OpenA8DJ_PackS24BE(samples[pairIndex][1], &StreamContext->OutputFrameBytes[pairIndex][3]);
            StreamContext->LastOutputSamples[pairIndex][0] = samples[pairIndex][0];
            StreamContext->LastOutputSamples[pairIndex][1] = samples[pairIndex][1];
        }
        OpenA8DJ_RecordRenderTraceFrame(
            StreamContext->DeviceContext,
            StreamContext,
            rtFrameCursor,
            rawSamples[0][0],
            rawSamples[0][1],
            samples[0][0],
            samples[0][1]);
        StreamContext->HasLastOutputSamples = TRUE;
        StreamContext->RenderTransferFrameIndex++;
        StreamContext->RenderFrameCursor =
            (StreamContext->RenderFrameCursor + 1u) % StreamContext->RtFrameCount;
        StreamContext->PositionBlocks++;
        if (StreamContext->RtPacketCount != 0 &&
            StreamContext->RtPacketSize != 0 &&
            StreamContext->RtBlockAlign != 0) {
            ULONG framesPerPacket = StreamContext->RtPacketSize / StreamContext->RtBlockAlign;
            if (framesPerPacket != 0 &&
                (StreamContext->PositionBlocks % framesPerPacket) == 0) {
                ULONG completedPacket = StreamContext->CurrentPacket;
                LARGE_INTEGER qpc;

                StreamContext->CurrentPacket =
                    (StreamContext->CurrentPacket + 1u) % StreamContext->RtPacketCount;
                qpc = KeQueryPerformanceCounter(NULL);
                if (StreamContext->Stream != NULL) {
                    (VOID)AcxRtStreamNotifyPacketComplete(
                        StreamContext->Stream,
                        completedPacket,
                        (ULONGLONG)qpc.QuadPart);
                }
            }
        }
        if (StreamContext->DeviceContext != NULL) {
            InterlockedIncrement64(&StreamContext->DeviceContext->AcxRtFramesRead);
            if (peak != 0) {
                InterlockedIncrement64(&StreamContext->DeviceContext->AcxRtNonZeroFrames);
            }
            if (peak > StreamContext->DeviceContext->AcxRtPeakAbsS24) {
                StreamContext->DeviceContext->AcxRtPeakAbsS24 = peak;
            }
        }
    }
    StreamContext->OutputFrameLoaded = TRUE;
}

static VOID
OpenA8DJ_FillMode2FromRtRender(
    _Out_writes_bytes_(Length) UCHAR *Buffer,
    _In_ ULONG Length,
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    ULONG index = 0;

    StreamContext->RenderTransferFrameIndex = 0;
    while (index < Length) {
        ULONG groupOffset = index % 16u;
        ULONG stream;

        if (groupOffset == 8u) {
            for (stream = 0; stream < 4u && index < Length; stream++) {
                Buffer[index] = OpenA8DJ_Mode2CheckByte(stream, index);
                index++;
            }
            continue;
        }

        if (!StreamContext->OutputFrameLoaded || StreamContext->OutputByteInFrame == 0) {
            OpenA8DJ_LoadRtRenderFrame(StreamContext);
        }

        for (stream = 0; stream < 4u && index < Length; stream++) {
            Buffer[index] = StreamContext->OutputFrameBytes[stream][StreamContext->OutputByteInFrame];
            index++;
        }
        StreamContext->OutputByteInFrame++;
        if (StreamContext->OutputByteInFrame >= 6u) {
            StreamContext->OutputByteInFrame = 0;
        }
    }
    OpenA8DJ_UpdateStreamPositionQpc(StreamContext);
}

static VOID
OpenA8DJ_FillMode2FromActiveRtRenders(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_writes_bytes_(Length) UCHAR *Buffer,
    _In_ ULONG Length)
{
    ULONG index = 0;
    ULONG pairIndex;
    POPENA8DJ_ACX_STREAM_CONTEXT streams[OPENA8DJ_STEREO_PAIRS] = { NULL };

    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        streams[pairIndex] = OpenA8DJ_AcquireActiveStream(
            Context,
            &Context->ActiveRenderStreams[pairIndex]);
    }

    if (streams[0] != NULL &&
        streams[0]->RtKernelAddress != NULL &&
        streams[0]->RtFrameCount != 0 &&
        streams[0]->RtChannels > 2u) {
        OpenA8DJ_FillMode2FromRtRender(Buffer, Length, streams[0]);
        goto release_streams;
    }

    for (index = 0; index < OPENA8DJ_STEREO_PAIRS; index++) {
        POPENA8DJ_ACX_STREAM_CONTEXT streamContext = streams[index];

        if (streamContext != NULL) {
            streamContext->RenderTransferFrameIndex = 0;
        }
    }
    index = 0;
    while (index < Length) {
        ULONG groupOffset = index % 16u;

        if (groupOffset == 8u) {
            for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS && index < Length; pairIndex++) {
                Buffer[index] = OpenA8DJ_Mode2CheckByte(pairIndex, index);
                index++;
            }
            continue;
        }

        for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS && index < Length; pairIndex++) {
            POPENA8DJ_ACX_STREAM_CONTEXT streamContext = streams[pairIndex];

            if (streamContext != NULL &&
                streamContext->RtKernelAddress != NULL &&
                streamContext->RtFrameCount != 0) {
                ULONG byteInFrame;

                if (!streamContext->OutputFrameLoaded ||
                    streamContext->OutputByteInFrame == 0) {
                    OpenA8DJ_LoadRtRenderFrame(streamContext);
                }
                byteInFrame = streamContext->OutputByteInFrame;
                Buffer[index] = streamContext->OutputFrameBytes[pairIndex][byteInFrame];
            } else {
                Buffer[index] = 0;
            }
            index++;
        }

        for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
            POPENA8DJ_ACX_STREAM_CONTEXT streamContext = streams[pairIndex];

            if (streamContext != NULL &&
                streamContext->RtKernelAddress != NULL &&
                streamContext->RtFrameCount != 0) {
                streamContext->OutputByteInFrame++;
                if (streamContext->OutputByteInFrame >= 6u) {
                    streamContext->OutputByteInFrame = 0;
                }
            }
        }
    }
    for (index = 0; index < OPENA8DJ_STEREO_PAIRS; index++) {
        POPENA8DJ_ACX_STREAM_CONTEXT streamContext = streams[index];

        if (streamContext != NULL &&
            streamContext->RtKernelAddress != NULL &&
            streamContext->RtFrameCount != 0) {
            OpenA8DJ_UpdateStreamPositionQpc(streamContext);
        }
    }

release_streams:
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (streams[pairIndex] != NULL) {
            OpenA8DJ_ReleaseActiveStream(streams[pairIndex]);
        }
    }
}

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
static VOID
NTAPI
OpenA8DJ_EvtIsoOutputRequestComplete(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT CompletionContext)
{
    POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT slot =
        (POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT)CompletionContext;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    if (slot != NULL) {
        slot->CompletionStatus = Params->IoStatus.Status;
        slot->ErrorCount =
            slot->Urb != NULL ? slot->Urb->UrbIsochronousTransfer.ErrorCount : 1u;
        InterlockedExchange(&slot->InFlight, 0);
        KeSetEvent(&slot->CompleteEvent, IO_NO_INCREMENT, FALSE);
    }
}

static NTSTATUS
OpenA8DJ_InitializeAsyncIsoOutputSlots(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ WDFDEVICE Device,
    _Out_writes_(SlotCount) POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT Slots,
    _In_ ULONG SlotCount,
    _In_ ULONG BufferLength)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG slotIndex;

    RtlZeroMemory(Slots, sizeof(OPENA8DJ_ASYNC_ISO_OUTPUT_SLOT) * SlotCount);
    for (slotIndex = 0; slotIndex < SlotCount; slotIndex++) {
        WDF_OBJECT_ATTRIBUTES attributes;

        KeInitializeEvent(&Slots[slotIndex].CompleteEvent, NotificationEvent, TRUE);
        Slots[slotIndex].CompletionStatus = STATUS_SUCCESS;
        Slots[slotIndex].BufferLength = BufferLength;
        Slots[slotIndex].Buffer = (UCHAR *)ExAllocatePoolZero(
            NonPagedPoolNx,
            BufferLength,
            OPENA8DJ_POOL_TAG);
        if (Slots[slotIndex].Buffer == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Device;
        status = WdfRequestCreate(
            &attributes,
            WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe),
            &Slots[slotIndex].Request);
        if (!NT_SUCCESS(status)) {
            break;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Device;
        status = WdfUsbTargetDeviceCreateIsochUrb(
            Context->UsbDevice,
            &attributes,
            OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
            &Slots[slotIndex].UrbMemory,
            &Slots[slotIndex].Urb);
        if (!NT_SUCCESS(status)) {
            break;
        }
    }

    if (!NT_SUCCESS(status)) {
        for (slotIndex = 0; slotIndex < SlotCount; slotIndex++) {
            if (Slots[slotIndex].UrbMemory != NULL) {
                WdfObjectDelete(Slots[slotIndex].UrbMemory);
                Slots[slotIndex].UrbMemory = NULL;
                Slots[slotIndex].Urb = NULL;
            }
            if (Slots[slotIndex].Request != NULL) {
                WdfObjectDelete(Slots[slotIndex].Request);
                Slots[slotIndex].Request = NULL;
            }
            if (Slots[slotIndex].Buffer != NULL) {
                ExFreePoolWithTag(Slots[slotIndex].Buffer, OPENA8DJ_POOL_TAG);
                Slots[slotIndex].Buffer = NULL;
            }
        }
    }

    return status;
}

static VOID
OpenA8DJ_WaitForAsyncIsoOutputSlot(
    _Inout_ POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT Slot,
    _Out_ NTSTATUS *CompletionStatus,
    _Out_ ULONG *ErrorCount)
{
    if (InterlockedCompareExchange(&Slot->InFlight, 0, 0) != 0) {
        (VOID)KeWaitForSingleObject(
            &Slot->CompleteEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL);
    }
    *CompletionStatus = Slot->CompletionStatus;
    *ErrorCount = Slot->ErrorCount;
    Slot->CompletionStatus = STATUS_SUCCESS;
    Slot->ErrorCount = 0;
}

static VOID
OpenA8DJ_CleanupAsyncIsoOutputSlots(
    _Inout_updates_(SlotCount) POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT Slots,
    _In_ ULONG SlotCount)
{
    ULONG slotIndex;

    for (slotIndex = 0; slotIndex < SlotCount; slotIndex++) {
        NTSTATUS completionStatus;
        ULONG errorCount;

        OpenA8DJ_WaitForAsyncIsoOutputSlot(
            &Slots[slotIndex],
            &completionStatus,
            &errorCount);
        if (Slots[slotIndex].UrbMemory != NULL) {
            WdfObjectDelete(Slots[slotIndex].UrbMemory);
            Slots[slotIndex].UrbMemory = NULL;
            Slots[slotIndex].Urb = NULL;
        }
        if (Slots[slotIndex].Request != NULL) {
            WdfObjectDelete(Slots[slotIndex].Request);
            Slots[slotIndex].Request = NULL;
        }
        if (Slots[slotIndex].Buffer != NULL) {
            ExFreePoolWithTag(Slots[slotIndex].Buffer, OPENA8DJ_POOL_TAG);
            Slots[slotIndex].Buffer = NULL;
        }
    }
}
#endif

static ULONG
OpenA8DJ_GetIsoOutputPacketLength(
    _In_ const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *Capture,
    _In_ ULONG PacketIndex,
    _In_ ULONG FixedPacketBytes,
    _In_ ULONG RemainingTransferBytes)
{
    ULONG length;

    if (RemainingTransferBytes == 0) {
        return 0;
    }
    if (FixedPacketBytes == OPENA8DJ_ISO_OUTPUT_PACKET_BYTES_CAPTURE_SHAPE) {
        if (Capture->Packets[PacketIndex].UsbdStatus != USBD_STATUS_SUCCESS ||
            Capture->Packets[PacketIndex].CompletedLength == 0) {
            return 0;
        }
        length = Capture->Packets[PacketIndex].CompletedLength;
        return length < RemainingTransferBytes ? length : RemainingTransferBytes;
    }
    if (FixedPacketBytes != 0) {
        return FixedPacketBytes < RemainingTransferBytes ? FixedPacketBytes : RemainingTransferBytes;
    }

    length = Capture->Packets[PacketIndex].CompletedLength;
    return length < RemainingTransferBytes ? length : RemainingTransferBytes;
}

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
static NTSTATUS
OpenA8DJ_QueueAsyncIsoOutputBuffer(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Inout_ POPENA8DJ_ASYNC_ISO_OUTPUT_SLOT Slot,
    _In_ const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *Capture,
    _In_ ULONG TransferBufferLength,
    _In_ ULONG FixedPacketBytes,
    _Out_ ULONG *PreviousPlaybackErrorCount)
{
    NTSTATUS status;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    ULONG packetIndex;
    ULONG outputOffset;

    OpenA8DJ_WaitForAsyncIsoOutputSlot(
        Slot,
        &status,
        PreviousPlaybackErrorCount);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (Context->IsoOutPipe == NULL ||
        Slot->Request == NULL ||
        Slot->Urb == NULL ||
        Slot->Buffer == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    if (TransferBufferLength == 0 || TransferBufferLength > Slot->BufferLength) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        STATUS_SUCCESS);
    status = WdfRequestReuse(Slot->Request, &reuseParams);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Slot->Urb->UrbIsochronousTransfer.Hdr.Function = URB_FUNCTION_ISOCH_TRANSFER;
    Slot->Urb->UrbIsochronousTransfer.PipeHandle =
        WdfUsbTargetPipeWdmGetPipeHandle(Context->IsoOutPipe);
    Slot->Urb->UrbIsochronousTransfer.TransferFlags =
        USBD_TRANSFER_DIRECTION_OUT |
        USBD_START_ISO_TRANSFER_ASAP;
    Slot->Urb->UrbIsochronousTransfer.TransferBufferLength = TransferBufferLength;
    Slot->Urb->UrbIsochronousTransfer.TransferBuffer = Slot->Buffer;
    Slot->Urb->UrbIsochronousTransfer.TransferBufferMDL = NULL;
    Slot->Urb->UrbIsochronousTransfer.StartFrame = 0;
    Slot->Urb->UrbIsochronousTransfer.NumberOfPackets = OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    Slot->Urb->UrbIsochronousTransfer.ErrorCount = 0;

    outputOffset = 0;
    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        ULONG length = OpenA8DJ_GetIsoOutputPacketLength(
            Capture,
            packetIndex,
            FixedPacketBytes,
            TransferBufferLength - outputOffset);

        if (packetIndex + 1u == OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT &&
            outputOffset + length < TransferBufferLength) {
            length = TransferBufferLength - outputOffset;
        }
        Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset = outputOffset;
        Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = length;
        Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status = USBD_STATUS_SUCCESS;
        outputOffset += length;
    }

    status = WdfUsbTargetPipeFormatRequestForUrb(
        Context->IsoOutPipe,
        Slot->Request,
        Slot->UrbMemory,
        NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Slot->CompletionStatus = STATUS_PENDING;
    Slot->ErrorCount = 0;
    KeClearEvent(&Slot->CompleteEvent);
    InterlockedExchange(&Slot->InFlight, 1);
    WdfRequestSetCompletionRoutine(
        Slot->Request,
        OpenA8DJ_EvtIsoOutputRequestComplete,
        Slot);
    if (!WdfRequestSend(
            Slot->Request,
            WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe),
            NULL)) {
        status = WdfRequestGetStatus(Slot->Request);
        InterlockedExchange(&Slot->InFlight, 0);
        KeSetEvent(&Slot->CompleteEvent, IO_NO_INCREMENT, FALSE);
        return status;
    }

    return STATUS_SUCCESS;
}
#endif

static NTSTATUS
OpenA8DJ_SendIsoOutputBufferPrepared(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *Capture,
    _In_reads_bytes_(TransferBufferLength) PVOID TransferBuffer,
    _In_ ULONG TransferBufferLength,
    _In_ ULONG FixedPacketBytes,
    _Inout_ PURB Urb,
    _Out_ ULONG *PlaybackErrorCount)
{
    NTSTATUS status;
    ULONG packetIndex;
    ULONG outputOffset;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    if (PlaybackErrorCount != NULL) {
        *PlaybackErrorCount = 0;
    }

    if (Context->IsoOutPipe == NULL || Urb == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (TransferBufferLength == 0 || TransferBufferLength > 4096) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    Urb->UrbIsochronousTransfer.Hdr.Function = URB_FUNCTION_ISOCH_TRANSFER;
    Urb->UrbIsochronousTransfer.PipeHandle = WdfUsbTargetPipeWdmGetPipeHandle(Context->IsoOutPipe);
    Urb->UrbIsochronousTransfer.TransferFlags =
        USBD_TRANSFER_DIRECTION_OUT |
        USBD_START_ISO_TRANSFER_ASAP;
    Urb->UrbIsochronousTransfer.TransferBufferLength = TransferBufferLength;
    Urb->UrbIsochronousTransfer.TransferBuffer = TransferBuffer;
    Urb->UrbIsochronousTransfer.TransferBufferMDL = NULL;
    Urb->UrbIsochronousTransfer.StartFrame = 0;
    Urb->UrbIsochronousTransfer.NumberOfPackets = OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    Urb->UrbIsochronousTransfer.ErrorCount = 0;

    outputOffset = 0;
    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        ULONG length = OpenA8DJ_GetIsoOutputPacketLength(
            Capture,
            packetIndex,
            FixedPacketBytes,
            TransferBufferLength - outputOffset);

        if (packetIndex + 1u == OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT &&
            outputOffset + length < TransferBufferLength) {
            length = TransferBufferLength - outputOffset;
        }
        Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset = outputOffset;
        Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = length;
        Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status = USBD_STATUS_SUCCESS;
        outputOffset += length;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(1));

    status = WdfUsbTargetPipeSendUrbSynchronously(
        Context->IsoOutPipe,
        NULL,
        &sendOptions,
        Urb);

    if (PlaybackErrorCount != NULL) {
        *PlaybackErrorCount = Urb->UrbIsochronousTransfer.ErrorCount;
    }
    return status;
}

static NTSTATUS
OpenA8DJ_SendIsoOutputBuffer(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *Capture,
    _In_reads_bytes_(TransferBufferLength) PVOID TransferBuffer,
    _In_ ULONG TransferBufferLength,
    _In_ ULONG FixedPacketBytes,
    _Out_ ULONG *PlaybackErrorCount)
{
    NTSTATUS status;
    WDFMEMORY urbMemory;
    PURB urb;
    WDF_OBJECT_ATTRIBUTES memoryAttributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = Context->UsbDevice;
    status = WdfUsbTargetDeviceCreateIsochUrb(
        Context->UsbDevice,
        &memoryAttributes,
        OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
        &urbMemory,
        &urb);
    if (!NT_SUCCESS(status)) {
        if (PlaybackErrorCount != NULL) {
            *PlaybackErrorCount = 1;
        }
        return status;
    }

    status = OpenA8DJ_SendIsoOutputBufferPrepared(
        Context,
        Capture,
        TransferBuffer,
        TransferBufferLength,
        FixedPacketBytes,
        urb,
        PlaybackErrorCount);

    WdfObjectDelete(urbMemory);
    return status;
}

static NTSTATUS
OpenA8DJ_SendIsoSilencePulse(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_ISO_SILENCE_PULSE Pulse)
{
    NTSTATUS status;
    PVOID transferBuffer;
    ULONG packetIndex;
    ULONG outputOffset;

    RtlZeroMemory(Pulse, sizeof(*Pulse));
    Pulse->Size = sizeof(*Pulse);

    status = OpenA8DJ_CaptureIsoSnapshot(Context, &Pulse->Capture);
    Pulse->NtStatus = (ULONG)status;
    if (!NT_SUCCESS(status)) {
        return status;
    }

    outputOffset = 0;
    for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
        outputOffset += Pulse->Capture.Packets[packetIndex].CompletedLength;
    }
    if (outputOffset == 0 || outputOffset > 4096) {
        Pulse->PlaybackNtStatus = (ULONG)STATUS_INVALID_DEVICE_STATE;
        return STATUS_INVALID_DEVICE_STATE;
    }
    Pulse->PlaybackTransferBufferLength = outputOffset;

    transferBuffer = ExAllocatePoolZero(
        NonPagedPoolNx,
        Pulse->PlaybackTransferBufferLength,
        OPENA8DJ_POOL_TAG);
    if (transferBuffer == NULL) {
        Pulse->PlaybackNtStatus = (ULONG)STATUS_INSUFFICIENT_RESOURCES;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    OpenA8DJ_FillMode2Silence((UCHAR *)transferBuffer, Pulse->PlaybackTransferBufferLength);

    status = OpenA8DJ_SendIsoOutputBuffer(
        Context,
        &Pulse->Capture,
        transferBuffer,
        Pulse->PlaybackTransferBufferLength,
        0,
        &Pulse->PlaybackErrorCount);
    Pulse->PlaybackNtStatus = (ULONG)status;

    ExFreePoolWithTag(transferBuffer, OPENA8DJ_POOL_TAG);
    return status;
}

static NTSTATUS
OpenA8DJ_SendIsoToneBurst(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Inout_ POPENA8DJ_ISO_TONE_BURST Burst)
{
    NTSTATUS status = STATUS_SUCCESS;
    OPENA8DJ_MODE2_TONE_PACKER packer;
    PVOID transferBuffer;
    ULONG requestedTransfers;
    ULONG pairIndex;
    ULONG amplitudeQ15;
    ULONG packetBytes;
    ULONG periodSamples;
    ULONG transferIndex;

    if (Burst == NULL || Burst->Size < sizeof(OPENA8DJ_ISO_TONE_BURST)) {
        return STATUS_INVALID_PARAMETER;
    }

    requestedTransfers = Burst->RequestedTransfers;
    pairIndex = Burst->PairIndex;
    amplitudeQ15 = Burst->AmplitudeQ15;
    packetBytes = Burst->PacketBytes;
    periodSamples = Burst->PeriodSamples;
    if (periodSamples == 0) {
        periodSamples = 64u;
    }

    RtlZeroMemory(Burst, sizeof(*Burst));
    Burst->Size = sizeof(*Burst);
    Burst->RequestedTransfers = requestedTransfers;
    Burst->PairIndex = pairIndex;
    Burst->AmplitudeQ15 = amplitudeQ15;
    Burst->PacketBytes = packetBytes;
    Burst->PeriodSamples = periodSamples;

    if (requestedTransfers == 0 || requestedTransfers > 250u ||
        pairIndex >= OPENA8DJ_STEREO_PAIRS ||
        amplitudeQ15 == 0 || amplitudeQ15 > 8192u ||
        packetBytes > 512u ||
        (periodSamples != 40u && periodSamples != 48u && periodSamples != 56u && periodSamples != 64u)) {
        Burst->NtStatus = (ULONG)STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    transferBuffer = ExAllocatePoolZero(NonPagedPoolNx, 4096, OPENA8DJ_POOL_TAG);
    if (transferBuffer == NULL) {
        Burst->NtStatus = (ULONG)STATUS_INSUFFICIENT_RESOURCES;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(&packer, sizeof(packer));
    packer.OutputByteInFrame = 4u;

    for (transferIndex = 0; transferIndex < requestedTransfers; transferIndex++) {
        ULONG packetIndex;
        ULONG transferBytes = 0;
        ULONG playbackErrors = 0;

        status = OpenA8DJ_CaptureIsoSnapshot(Context, &Burst->LastCapture);
        Burst->LastCaptureNtStatus = (ULONG)status;
        if (transferIndex == 0) {
            Burst->FirstCaptureNtStatus = (ULONG)status;
        }
        if (!NT_SUCCESS(status)) {
            break;
        }
        Burst->CaptureErrorCount += Burst->LastCapture.ErrorCount;

        if (packetBytes != 0) {
            transferBytes = packetBytes * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
        } else {
            for (packetIndex = 0; packetIndex < OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT; packetIndex++) {
                transferBytes += Burst->LastCapture.Packets[packetIndex].CompletedLength;
            }
        }
        if (transferBytes == 0 || transferBytes > 4096) {
            status = STATUS_INVALID_DEVICE_STATE;
            break;
        }

        RtlZeroMemory(transferBuffer, 4096);
        OpenA8DJ_FillMode2Tone(
            (UCHAR *)transferBuffer,
            transferBytes,
            &packer,
            pairIndex,
            amplitudeQ15,
            periodSamples);

        status = OpenA8DJ_SendIsoOutputBuffer(
            Context,
            &Burst->LastCapture,
            transferBuffer,
            transferBytes,
            packetBytes,
            &playbackErrors);
        Burst->LastPlaybackNtStatus = (ULONG)status;
        if (transferIndex == 0) {
            Burst->FirstPlaybackNtStatus = (ULONG)status;
        }
        Burst->PlaybackErrorCount += playbackErrors;
        Burst->PlaybackBytes += transferBytes;
        if (!NT_SUCCESS(status)) {
            break;
        }

        Burst->CompletedTransfers++;
    }

    ExFreePoolWithTag(transferBuffer, OPENA8DJ_POOL_TAG);

    Burst->NtStatus = (ULONG)status;
    return status;
}

VOID
OpenA8DJ_EvtStreamWorkItem(_In_ WDFWORKITEM WorkItem)
{
    WDFDEVICE device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(device);
    UCHAR *playbackBuffer;
    UCHAR *captureTransferBuffer;
    ULONG captureTransferBufferLength;
    ULONG captureMaximumPacketSize;
    WDFMEMORY captureUrbMemory = NULL;
    PURB captureUrb = NULL;
    WDFMEMORY outputUrbMemory = NULL;
    PURB outputUrb = NULL;
    WDF_USB_PIPE_INFORMATION capturePipeInfo;
    WDF_OBJECT_ATTRIBUTES captureMemoryAttributes;
    WDF_OBJECT_ATTRIBUTES outputMemoryAttributes;
    ULONG playbackLength = OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
    OPENA8DJ_ASYNC_ISO_OUTPUT_SLOT outputSlots[OPENA8DJ_ASYNC_OUTPUT_SLOTS];
    BOOLEAN useAsyncOutput = FALSE;
    ULONG nextOutputSlot = 0;
#endif

    playbackBuffer = (UCHAR *)ExAllocatePoolZero(
        NonPagedPoolNx,
        playbackLength,
        OPENA8DJ_POOL_TAG);
    if (playbackBuffer == NULL) {
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
    if (context->IsoInPipe == NULL) {
        ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
    if (context->IsoOutPipe == NULL) {
        ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
    WDF_USB_PIPE_INFORMATION_INIT(&capturePipeInfo);
    WdfUsbTargetPipeGetInformation(context->IsoInPipe, &capturePipeInfo);
    captureMaximumPacketSize = capturePipeInfo.MaximumPacketSize;
    if (captureMaximumPacketSize == 0 || captureMaximumPacketSize > 4096) {
        ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
    captureTransferBufferLength = captureMaximumPacketSize * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    captureTransferBuffer = (UCHAR *)ExAllocatePoolZero(
        NonPagedPoolNx,
        captureTransferBufferLength,
        OPENA8DJ_POOL_TAG);
    if (captureTransferBuffer == NULL) {
        ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
    WDF_OBJECT_ATTRIBUTES_INIT(&captureMemoryAttributes);
    captureMemoryAttributes.ParentObject = context->UsbDevice;
    if (!NT_SUCCESS(WdfUsbTargetDeviceCreateIsochUrb(
            context->UsbDevice,
            &captureMemoryAttributes,
            OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
            &captureUrbMemory,
            &captureUrb))) {
        ExFreePoolWithTag(captureTransferBuffer, OPENA8DJ_POOL_TAG);
        ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
    WDF_OBJECT_ATTRIBUTES_INIT(&outputMemoryAttributes);
    outputMemoryAttributes.ParentObject = context->UsbDevice;
    if (!NT_SUCCESS(WdfUsbTargetDeviceCreateIsochUrb(
            context->UsbDevice,
            &outputMemoryAttributes,
            OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
            &outputUrbMemory,
            &outputUrb))) {
        WdfObjectDelete(captureUrbMemory);
        ExFreePoolWithTag(captureTransferBuffer, OPENA8DJ_POOL_TAG);
        ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&context->StreamWorkerActive, 0);
        return;
    }
#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
    if (NT_SUCCESS(OpenA8DJ_InitializeAsyncIsoOutputSlots(
            context,
            device,
            outputSlots,
            OPENA8DJ_ASYNC_OUTPUT_SLOTS,
            playbackLength))) {
        useAsyncOutput = TRUE;
    }
#endif

    context->StreamState.Streaming = TRUE;
    context->StreamState.StreamingEngineReady = TRUE;
    context->StreamState.SampleRate = context->CurrentFormat.SampleRate;
    context->StreamState.BufferFrames = context->CurrentFormat.BufferFrames;

    while (InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) == 0) {
        OPENA8DJ_ISO_CAPTURE_SNAPSHOT snapshot;
        NTSTATUS status;
        ULONG packetIndex;
        ULONG playbackErrors = 0;
#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        ULONG previousPlaybackErrors = 0;
#endif
        ULONG capturePayloadBytes = 0;
        BOOLEAN hasActiveRenderStream;
        ULONG activeRenderMask = 0;
        ULONG activeCaptureMask = 0;
        ULONG pairIndex;
        UCHAR *outputBuffer = playbackBuffer;

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        if (useAsyncOutput) {
            outputBuffer = outputSlots[nextOutputSlot].Buffer;
        }
#endif

        InterlockedIncrement64(&context->StreamWorkerIterations);
        status = OpenA8DJ_CaptureIsoSnapshotPrepared(
            context,
            &snapshot,
            captureTransferBuffer,
            captureTransferBufferLength,
            captureMaximumPacketSize,
            captureUrb);
        if (NT_SUCCESS(status)) {
            capturePayloadBytes = captureTransferBufferLength;
            for (packetIndex = 0; packetIndex < snapshot.PacketCount; packetIndex++) {
                if (snapshot.Packets[packetIndex].UsbdStatus == USBD_STATUS_SUCCESS &&
                    snapshot.Packets[packetIndex].CompletedLength > 0) {
                    context->StreamState.UsbInPacketsCompleted++;
                } else if (snapshot.Packets[packetIndex].UsbdStatus != USBD_STATUS_SUCCESS) {
                    context->StreamState.UsbPacketErrors++;
                }
            }
            context->StreamState.UsbPacketErrors += snapshot.ErrorCount;
        } else {
            LARGE_INTEGER delay;
            context->StreamState.UsbPacketErrors++;
            delay.QuadPart = WDF_REL_TIMEOUT_IN_MS(1);
            KeDelayExecutionThread(KernelMode, FALSE, &delay);
            continue;
        }

        if (capturePayloadBytes != 0) {
            for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
                POPENA8DJ_ACX_STREAM_CONTEXT activeCaptureStream =
                    OpenA8DJ_AcquireActiveStream(
                        context,
                        &context->ActiveCaptureStreams[pairIndex]);

                if (activeCaptureStream != NULL &&
                    activeCaptureStream->RtKernelAddress != NULL &&
                    activeCaptureStream->RtFrameCount != 0) {
                    ULONG framesWritten = 0;

                    activeCaptureMask |= (1u << pairIndex);
                    for (packetIndex = 0; packetIndex < snapshot.PacketCount; packetIndex++) {
                        ULONG offset = snapshot.Packets[packetIndex].Offset;
                        ULONG completedLength = snapshot.Packets[packetIndex].CompletedLength;

                        if (snapshot.Packets[packetIndex].UsbdStatus == USBD_STATUS_SUCCESS &&
                            completedLength != 0 &&
                            offset < capturePayloadBytes &&
                            completedLength <= capturePayloadBytes - offset) {
                            framesWritten += OpenA8DJ_FillRtCaptureFromMode2(
                                captureTransferBuffer + offset,
                                completedLength,
                                activeCaptureStream);
                        }
                    }
                    if (framesWritten != 0) {
                        context->StreamState.CaptureFramesDelivered += framesWritten;
                    }
                }
                if (activeCaptureStream != NULL) {
                    OpenA8DJ_ReleaseActiveStream(activeCaptureStream);
                }
            }
        }

        activeRenderMask = OpenA8DJ_GetActiveRenderMask(context);
        hasActiveRenderStream = activeRenderMask != 0;
        if (hasActiveRenderStream) {
#if OPENA8DJ_DEBUG_STREAM_TONE
            OPENA8DJ_MODE2_TONE_PACKER debugTonePacker;
            ULONG debugFramesRendered = playbackLength / 32u;

            RtlZeroMemory(&debugTonePacker, sizeof(debugTonePacker));
            debugTonePacker.OutputByteInFrame = 4u;
            OpenA8DJ_FillMode2Tone(
                outputBuffer,
                playbackLength,
                &debugTonePacker,
                0,
                2048,
                48);
            for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
                POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
                    OpenA8DJ_AcquireActiveStream(
                        context,
                        &context->ActiveRenderStreams[pairIndex]);

                if (streamContext != NULL &&
                    streamContext->RtKernelAddress != NULL &&
                    streamContext->RtFrameCount != 0) {
                    streamContext->PositionBlocks += debugFramesRendered;
                    OpenA8DJ_UpdateStreamPositionQpc(streamContext);
                }
                if (streamContext != NULL) {
                    OpenA8DJ_ReleaseActiveStream(streamContext);
                }
            }
#else
            OpenA8DJ_FillMode2FromActiveRtRenders(
                context,
                outputBuffer,
                playbackLength);
#endif
        } else {
            InterlockedIncrement64(&context->StreamWorkerNoRenderIterations);
            OpenA8DJ_FillMode2Silence(
                outputBuffer,
                playbackLength);
        }
        OpenA8DJ_RecordUsbPlaybackTrace(
            context,
            outputBuffer,
            playbackLength,
            OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES,
            activeRenderMask);

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        if (useAsyncOutput) {
            status = OpenA8DJ_QueueAsyncIsoOutputBuffer(
                context,
                &outputSlots[nextOutputSlot],
                &snapshot,
                playbackLength,
                OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES,
                &previousPlaybackErrors);
            playbackErrors += previousPlaybackErrors;
            nextOutputSlot = (nextOutputSlot + 1u) % OPENA8DJ_ASYNC_OUTPUT_SLOTS;
        } else {
#endif
            status = OpenA8DJ_SendIsoOutputBufferPrepared(
                context,
                &snapshot,
                outputBuffer,
                playbackLength,
                OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES,
                outputUrb,
                &playbackErrors);
#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        }
#endif
        if (NT_SUCCESS(status) && playbackErrors == 0) {
            ULONG renderFramesCompleted = playbackLength / 32u;

            context->StreamState.UsbOutPacketsCompleted += OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
            context->StreamState.RenderFramesSubmitted += renderFramesCompleted;
            InterlockedAdd64(&context->StreamWorkerCaptureBytes, capturePayloadBytes);
            InterlockedAdd64(&context->StreamWorkerPlaybackBytes, playbackLength);
            context->StreamWorkerLastCaptureBytes = capturePayloadBytes;
            context->StreamWorkerLastPlaybackBytes = playbackLength;
            context->StreamWorkerLastRenderMask = activeRenderMask;
            context->StreamWorkerLastCaptureMask = activeCaptureMask;
            if (capturePayloadBytes > context->StreamWorkerMaxCaptureBytes) {
                context->StreamWorkerMaxCaptureBytes = capturePayloadBytes;
            }
            if (playbackLength > context->StreamWorkerMaxPlaybackBytes) {
                context->StreamWorkerMaxPlaybackBytes = playbackLength;
            }
        } else {
            context->StreamState.UsbUnderruns++;
            context->StreamState.UsbPacketErrors += playbackErrors != 0 ? playbackErrors : 1;
        }

    }

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
    if (useAsyncOutput) {
        ULONG slotIndex;

        for (slotIndex = 0; slotIndex < OPENA8DJ_ASYNC_OUTPUT_SLOTS; slotIndex++) {
            NTSTATUS completionStatus;
            ULONG completionErrors;

            OpenA8DJ_WaitForAsyncIsoOutputSlot(
                &outputSlots[slotIndex],
                &completionStatus,
                &completionErrors);
            if (!NT_SUCCESS(completionStatus) || completionErrors != 0) {
                context->StreamState.UsbUnderruns++;
                context->StreamState.UsbPacketErrors += completionErrors != 0 ? completionErrors : 1;
            }
        }
        OpenA8DJ_CleanupAsyncIsoOutputSlots(outputSlots, OPENA8DJ_ASYNC_OUTPUT_SLOTS);
    }
#endif
    if (outputUrbMemory != NULL) {
        WdfObjectDelete(outputUrbMemory);
    }
    if (captureUrbMemory != NULL) {
        WdfObjectDelete(captureUrbMemory);
    }
    ExFreePoolWithTag(captureTransferBuffer, OPENA8DJ_POOL_TAG);
    ExFreePoolWithTag(playbackBuffer, OPENA8DJ_POOL_TAG);
    context->StreamState.Streaming = FALSE;
    OpenA8DJ_ClearAllActiveStreams(context);
    InterlockedExchange(&context->StreamWorkerActive, 0);
}

static NTSTATUS
OpenA8DJ_SendBulkPipeSynchronously(
    _In_ WDFUSBPIPE Pipe,
    _Inout_updates_bytes_(Length) UCHAR *Buffer,
    _In_ ULONG Length,
    _In_ ULONG TransferFlags,
    _Out_opt_ PULONG BytesTransferred)
{
    NTSTATUS status;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    if (BytesTransferred != NULL) {
        *BytesTransferred = 0;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, Buffer, Length);
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(1));

    if ((TransferFlags & USBD_TRANSFER_DIRECTION_IN) != 0) {
        status = WdfUsbTargetPipeReadSynchronously(
            Pipe,
            NULL,
            &sendOptions,
            &memoryDescriptor,
            BytesTransferred);
    } else {
        status = WdfUsbTargetPipeWriteSynchronously(
            Pipe,
            NULL,
            &sendOptions,
            &memoryDescriptor,
            BytesTransferred);
    }

    return status;
}

typedef struct _OPENA8DJ_BULK_READ_COMPLETION {
    PKEVENT Event;
    NTSTATUS Status;
    ULONG_PTR Information;
} OPENA8DJ_BULK_READ_COMPLETION, *POPENA8DJ_BULK_READ_COMPLETION;

static VOID
OpenA8DJ_EvtBulkReadComplete(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context)
{
    POPENA8DJ_BULK_READ_COMPLETION completion = (POPENA8DJ_BULK_READ_COMPLETION)Context;
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);

    completion->Status = WdfRequestGetStatus(Request);
    completion->Information = WdfRequestGetInformation(Request);
    KeSetEvent(completion->Event, IO_NO_INCREMENT, FALSE);
}

static NTSTATUS
OpenA8DJ_QueueBulkRead(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_writes_bytes_(ReplyCapacity) UCHAR *Reply,
    _In_ ULONG ReplyCapacity,
    _Out_ WDFREQUEST *ReadRequest,
    _Out_ WDFMEMORY *ReadMemory,
    _Inout_ POPENA8DJ_BULK_READ_COMPLETION Completion)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    *ReadRequest = NULL;
    *ReadMemory = NULL;
    Completion->Status = STATUS_PENDING;
    Completion->Information = 0;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Context->UsbDevice;

    status = WdfMemoryCreatePreallocated(
        &attributes,
        Reply,
        ReplyCapacity,
        ReadMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        WdfUsbTargetPipeGetIoTarget(Context->BulkInPipe),
        ReadRequest);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(*ReadMemory);
        *ReadMemory = NULL;
        return status;
    }

    status = WdfUsbTargetPipeFormatRequestForRead(
        Context->BulkInPipe,
        *ReadRequest,
        *ReadMemory,
        NULL);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(*ReadRequest);
        WdfObjectDelete(*ReadMemory);
        *ReadRequest = NULL;
        *ReadMemory = NULL;
        return status;
    }

    WdfRequestSetCompletionRoutine(
        *ReadRequest,
        OpenA8DJ_EvtBulkReadComplete,
        Completion);

    if (!WdfRequestSend(
            *ReadRequest,
            WdfUsbTargetPipeGetIoTarget(Context->BulkInPipe),
            WDF_NO_SEND_OPTIONS)) {
        status = WdfRequestGetStatus(*ReadRequest);
        WdfObjectDelete(*ReadRequest);
        WdfObjectDelete(*ReadMemory);
        *ReadRequest = NULL;
        *ReadMemory = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}

static VOID
OpenA8DJ_EvtEp1ReadComplete(
    _In_ WDFUSBPIPE Pipe,
    _In_ WDFMEMORY Buffer,
    _In_ size_t NumBytesTransferred,
    _In_ WDFCONTEXT Context)
{
    POPENA8DJ_DEVICE_CONTEXT deviceContext = (POPENA8DJ_DEVICE_CONTEXT)Context;
    UCHAR *bytes;
    size_t bufferLength;
    KIRQL oldIrql;
    BOOLEAN signalEvent = FALSE;
    ULONG copyLength;

    UNREFERENCED_PARAMETER(Pipe);

    if (NumBytesTransferred == 0) {
        InterlockedIncrement((volatile LONG *)&deviceContext->Ep1ReaderZeroReads);
        return;
    }

    bytes = (UCHAR *)WdfMemoryGetBuffer(Buffer, &bufferLength);
    if (bytes == NULL || bufferLength == 0) {
        return;
    }

    KeAcquireSpinLock(&deviceContext->Ep1Lock, &oldIrql);
    deviceContext->Ep1ReaderCompletions++;
    deviceContext->Ep1ReaderBytes += (ULONG)NumBytesTransferred;
    if (deviceContext->Ep1PendingActive &&
        bytes[0] == deviceContext->Ep1PendingCommand) {
        copyLength = (ULONG)NumBytesTransferred;
        if (copyLength > sizeof(deviceContext->Ep1Reply)) {
            copyLength = sizeof(deviceContext->Ep1Reply);
        }
        RtlCopyMemory(deviceContext->Ep1Reply, bytes, copyLength);
        deviceContext->Ep1ReplyLength = copyLength;
        deviceContext->Ep1ReplyReady = TRUE;
        deviceContext->Ep1PendingActive = FALSE;
        signalEvent = TRUE;
    }
    KeReleaseSpinLock(&deviceContext->Ep1Lock, oldIrql);

    if (signalEvent) {
        KeSetEvent(&deviceContext->Ep1Event, IO_NO_INCREMENT, FALSE);
    }
}

static BOOLEAN
OpenA8DJ_EvtEp1ReadersFailed(
    _In_ WDFUSBPIPE Pipe,
    _In_ NTSTATUS Status,
    _In_ USBD_STATUS UsbdStatus)
{
    UNREFERENCED_PARAMETER(Pipe);
    UNREFERENCED_PARAMETER(Status);
    UNREFERENCED_PARAMETER(UsbdStatus);
    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_WARNING_LEVEL,
               "OpenA8DJUsb: EP1 reader failed status=0x%08x usbd=0x%08x\n",
               Status,
               UsbdStatus));
    return TRUE;
}

static NTSTATUS
OpenA8DJ_StartEp1Reader(_In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status;
    WDF_USB_CONTINUOUS_READER_CONFIG readerConfig;

    if (Context->BulkInPipe == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(
        &readerConfig,
        OpenA8DJ_EvtEp1ReadComplete,
        Context,
        sizeof(Context->Ep1Reply));
    readerConfig.NumPendingReads = 2;
    readerConfig.EvtUsbTargetPipeReadersFailed = OpenA8DJ_EvtEp1ReadersFailed;

    status = WdfUsbTargetPipeConfigContinuousReader(
        Context->BulkInPipe,
        &readerConfig);
    Context->Ep1ReaderConfigStatus = status;
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(Context->BulkInPipe));
    Context->Ep1ReaderStartStatus = status;
    return status;
}

static NTSTATUS
OpenA8DJ_SendBulkCommand(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ UCHAR Command,
    _In_reads_bytes_(PayloadLength) const UCHAR *Payload,
    _In_ ULONG PayloadLength,
    _Out_writes_bytes_(ReplyCapacity) UCHAR *Reply,
    _In_ ULONG ReplyCapacity,
    _Out_ PULONG ReplyLength,
    _Out_opt_ PULONG WriteStatus,
    _Out_opt_ PULONG ReadStatus)
{
    NTSTATUS status;
    UCHAR commandBuffer[64];
    ULONG commandLength;
    ULONG bytesTransferred;
    ULONG attempt;
    KIRQL oldIrql;
    LARGE_INTEGER timeout;

    *ReplyLength = 0;
    if (WriteStatus != NULL) {
        *WriteStatus = STATUS_SUCCESS;
    }
    if (ReadStatus != NULL) {
        *ReadStatus = STATUS_SUCCESS;
    }
    if (Context->BulkOutPipe == NULL || Context->BulkInPipe == NULL) {
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)STATUS_DEVICE_NOT_READY;
        }
        return STATUS_DEVICE_NOT_READY;
    }
    if (PayloadLength + 1u > sizeof(commandBuffer)) {
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)STATUS_INVALID_BUFFER_SIZE;
        }
        return STATUS_INVALID_BUFFER_SIZE;
    }

    commandBuffer[0] = Command;
    if (PayloadLength > 0 && Payload != NULL) {
        RtlCopyMemory(commandBuffer + 1, Payload, PayloadLength);
    }
    commandLength = PayloadLength + 1u;

    if (Reply == NULL || ReplyCapacity == 0) {
        status = STATUS_UNSUCCESSFUL;
        for (attempt = 0; attempt < 2; attempt++) {
            bytesTransferred = 0;
            status = OpenA8DJ_SendBulkPipeSynchronously(
                Context->BulkOutPipe,
                commandBuffer,
                commandLength,
                USBD_TRANSFER_DIRECTION_OUT | USBD_SHORT_TRANSFER_OK,
                &bytesTransferred);
            if (WriteStatus != NULL) {
                *WriteStatus = (ULONG)status;
            }
            if (NT_SUCCESS(status)) {
                break;
            }
        }
        return status;
    }

    KeAcquireSpinLock(&Context->Ep1Lock, &oldIrql);
    Context->Ep1PendingCommand = Command;
    Context->Ep1PendingActive = TRUE;
    Context->Ep1ReplyReady = FALSE;
    Context->Ep1ReplyLength = 0;
    KeClearEvent(&Context->Ep1Event);
    KeReleaseSpinLock(&Context->Ep1Lock, oldIrql);

    status = STATUS_UNSUCCESSFUL;
    for (attempt = 0; attempt < 2; attempt++) {
        bytesTransferred = 0;
        status = OpenA8DJ_SendBulkPipeSynchronously(
            Context->BulkOutPipe,
            commandBuffer,
            commandLength,
            USBD_TRANSFER_DIRECTION_OUT | USBD_SHORT_TRANSFER_OK,
            &bytesTransferred);
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)status;
        }
        if (NT_SUCCESS(status)) {
            break;
        }
    }
    if (!NT_SUCCESS(status)) {
        KeAcquireSpinLock(&Context->Ep1Lock, &oldIrql);
        Context->Ep1PendingActive = FALSE;
        KeReleaseSpinLock(&Context->Ep1Lock, oldIrql);
        return status;
    }

    RtlZeroMemory(Reply, ReplyCapacity);
    timeout.QuadPart = WDF_REL_TIMEOUT_IN_SEC(2);
    status = KeWaitForSingleObject(
        &Context->Ep1Event,
        Executive,
        KernelMode,
        FALSE,
        &timeout);

    KeAcquireSpinLock(&Context->Ep1Lock, &oldIrql);
    if (status == STATUS_SUCCESS && Context->Ep1ReplyReady) {
        bytesTransferred = Context->Ep1ReplyLength;
        if (bytesTransferred > ReplyCapacity) {
            bytesTransferred = ReplyCapacity;
        }
        RtlCopyMemory(Reply, Context->Ep1Reply, bytesTransferred);
        *ReplyLength = bytesTransferred;
        Context->Ep1ReplyReady = FALSE;
        Context->Ep1ReplyLength = 0;
        status = STATUS_SUCCESS;
    } else {
        Context->Ep1PendingActive = FALSE;
        status = STATUS_IO_TIMEOUT;
    }
    KeReleaseSpinLock(&Context->Ep1Lock, oldIrql);

    if (ReadStatus != NULL) {
        *ReadStatus = (ULONG)status;
    }

    return status;
}

static VOID
OpenA8DJ_NormalizeRawControlState(
    _Inout_updates_(OPENA8DJ_CONTROL_STATE_BYTES) UCHAR *State)
{
    State[1] = 2;
    State[2] = 3;
    State[4] = 2;
}

static BOOLEAN
OpenA8DJ_ControlWriteMatches(
    _In_reads_bytes_(OPENA8DJ_CONTROL_STATE_BYTES) const UCHAR *ReadBack,
    _In_reads_bytes_(OPENA8DJ_CONTROL_STATE_BYTES) const UCHAR *Expected)
{
    if (ReadBack[0] != Expected[0]) {
        return FALSE;
    }
    if (((ReadBack[3] ^ Expected[3]) & OPENA8DJ_CONTROL_FLAGS_MASK) != 0) {
        return FALSE;
    }
    if (((ReadBack[5] ^ Expected[5]) & OPENA8DJ_CONTROL_LOCK_MASK) != 0) {
        return FALSE;
    }
    return TRUE;
}

static NTSTATUS
OpenA8DJ_ReadHardwareControlState(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_writes_bytes_(OPENA8DJ_CONTROL_STATE_BYTES) UCHAR *State)
{
    NTSTATUS status;
    ULONG replyLength = 0;
    ULONG writeStatus = 0;
    ULONG readStatus = 0;
    UCHAR autoMsgPayload[3] = {1, 0, 0};
    UCHAR reply[512];
    ULONG copyLength;
    ULONG attempt;

    if (State == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    for (attempt = 0; attempt < 3; attempt++) {
        (void)OpenA8DJ_SendBulkCommand(
            Context,
            OPENA8DJ_CAIAQ_COMMAND_AUTO_MSG,
            autoMsgPayload,
            sizeof(autoMsgPayload),
            NULL,
            0,
            &replyLength,
            &writeStatus,
            &readStatus);

        replyLength = 0;
        status = OpenA8DJ_SendBulkCommand(
            Context,
            OPENA8DJ_CAIAQ_COMMAND_READ_IO,
            NULL,
            0,
            reply,
            sizeof(reply),
            &replyLength,
            &writeStatus,
            &readStatus);
        if (NT_SUCCESS(status) &&
            replyLength >= 2 &&
            reply[0] == OPENA8DJ_CAIAQ_COMMAND_READ_IO) {
            break;
        }
    }
    if (!NT_SUCCESS(status)) {
        Context->ControlsHardwareReady = FALSE;
        Context->LastControlReadStatus = status;
        return status;
    }
    if (replyLength < 2 || reply[0] != OPENA8DJ_CAIAQ_COMMAND_READ_IO) {
        Context->ControlsHardwareReady = FALSE;
        Context->LastControlReadStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

    RtlZeroMemory(State, OPENA8DJ_CONTROL_STATE_BYTES);
    copyLength = replyLength - 1;
    if (copyLength > OPENA8DJ_CONTROL_STATE_BYTES) {
        copyLength = OPENA8DJ_CONTROL_STATE_BYTES;
    }
    RtlCopyMemory(State, reply + 1, copyLength);
    OpenA8DJ_NormalizeRawControlState(State);
    Context->ControlsHardwareReady = TRUE;
    Context->LastControlReadStatus = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_RefreshHardwareControlState(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status;
    UCHAR state[OPENA8DJ_CONTROL_STATE_BYTES];

    status = OpenA8DJ_ReadHardwareControlState(Context, state);
    if (NT_SUCCESS(status)) {
        RtlCopyMemory(Context->RawControlState, state, sizeof(state));
    }
    return status;
}

static NTSTATUS
OpenA8DJ_WriteHardwareControlState(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_reads_bytes_(OPENA8DJ_CONTROL_STATE_BYTES) const UCHAR *State)
{
    NTSTATUS status;
    ULONG replyLength = 0;
    ULONG writeStatus = 0;
    ULONG readStatus = 0;
    UCHAR readBack[OPENA8DJ_CONTROL_STATE_BYTES];

    if (State == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    RtlCopyMemory(Context->LastControlWriteRequest, State, sizeof(Context->LastControlWriteRequest));
    RtlZeroMemory(Context->LastControlWriteReadBack, sizeof(Context->LastControlWriteReadBack));
    Context->LastControlWriteMismatch = FALSE;
    Context->LastControlWriteStatus = STATUS_SUCCESS;
    Context->LastControlReadbackStatus = STATUS_NOT_SUPPORTED;

    status = OpenA8DJ_SendBulkCommand(
        Context,
        OPENA8DJ_CAIAQ_COMMAND_WRITE_IO,
        State,
        OPENA8DJ_CONTROL_STATE_BYTES,
        NULL,
        0,
        &replyLength,
        &writeStatus,
        &readStatus);
    Context->LastControlWriteStatus = status;
    if (!NT_SUCCESS(status)) {
        Context->ControlsHardwareReady = FALSE;
        return status;
    }

    status = OpenA8DJ_ReadHardwareControlState(Context, readBack);
    Context->LastControlReadbackStatus = status;
    if (!NT_SUCCESS(status)) {
        return status;
    }
    RtlCopyMemory(Context->LastControlWriteReadBack, readBack, sizeof(Context->LastControlWriteReadBack));
    if (!OpenA8DJ_ControlWriteMatches(readBack, State)) {
        Context->LastControlWriteMismatch = TRUE;
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    RtlCopyMemory(Context->RawControlState, readBack, sizeof(readBack));
    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_ApplyAudioParams(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_ POPENA8DJ_AUDIO_PARAMS_RESULT Result)
{
    NTSTATUS status;
    UCHAR resetPayload[5];
    UCHAR setPayload[5];
    ULONG replyLength;
    ULONG writeStatus;
    ULONG readStatus;

    RtlZeroMemory(Result, sizeof(*Result));
    Result->Size = sizeof(*Result);
    Result->SampleRate = OPENA8DJ_DEFAULT_SAMPLE_RATE;
    Result->RateCode = 1;
    Result->Depth = 2;
    Result->BytesPerPacket = OPENA8DJ_STREAM_OUTPUT_PACKET_BYTES;

    replyLength = 0;
    status = OpenA8DJ_SendBulkCommand(
        Context,
        0x01,
        NULL,
        0,
        Result->DeviceInfoReply,
        sizeof(Result->DeviceInfoReply),
        &replyLength,
        &writeStatus,
        &readStatus);
    Result->DeviceInfoWriteNtStatus = writeStatus;
    Result->DeviceInfoReadNtStatus = readStatus;
    Result->DeviceInfoReplyLength = replyLength;

    resetPayload[0] = 0xff;
    resetPayload[1] = 0;
    resetPayload[2] = (UCHAR)(Result->BytesPerPacket & 0xffu);
    resetPayload[3] = (UCHAR)(Result->BytesPerPacket >> 8);
    resetPayload[4] = 1;

    setPayload[0] = Result->RateCode;
    setPayload[1] = Result->Depth;
    setPayload[2] = (UCHAR)(Result->BytesPerPacket & 0xffu);
    setPayload[3] = (UCHAR)(Result->BytesPerPacket >> 8);
    setPayload[4] = 1;

    replyLength = 0;
    status = OpenA8DJ_SendBulkCommand(
        Context,
        0x09,
        resetPayload,
        sizeof(resetPayload),
        Result->ResetReply,
        sizeof(Result->ResetReply),
        &replyLength,
        &writeStatus,
        &readStatus);
    Result->ResetWriteNtStatus = writeStatus;
    Result->ResetReadNtStatus = readStatus;
    Result->ResetNtStatus = (ULONG)status;
    Result->ResetReplyLength = replyLength;

    replyLength = 0;
    status = OpenA8DJ_SendBulkCommand(
        Context,
        0x09,
        setPayload,
        sizeof(setPayload),
        Result->SetReply,
        sizeof(Result->SetReply),
        &replyLength,
        &writeStatus,
        &readStatus);
    Result->SetWriteNtStatus = writeStatus;
    Result->SetReadNtStatus = readStatus;
    Result->SetNtStatus = (ULONG)status;
    Result->SetReplyLength = replyLength;
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (replyLength < 2 ||
        Result->SetReply[0] != 0x09 ||
        Result->SetReply[1] != 1) {
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    ACX_DRIVER_CONFIG acxConfig;
    WDFDRIVER driver;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, OpenA8DJ_EvtDeviceAdd);

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: DriverEntry\n"));

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        &driver);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ACX_DRIVER_CONFIG_INIT(&acxConfig);
    return AcxDriverInitialize(driver, &acxConfig);
}

NTSTATUS
OpenA8DJ_EvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    POPENA8DJ_DEVICE_CONTEXT context;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_WORKITEM_CONFIG workItemConfig;
    WDF_OBJECT_ATTRIBUTES workItemAttributes;
    ACX_DEVICEINIT_CONFIG acxDeviceInitConfig;
    ACX_DEVICE_CONFIG acxDeviceConfig;
    ULONG interfaceIndex;

    UNREFERENCED_PARAMETER(Driver);

    ACX_DEVICEINIT_CONFIG_INIT(&acxDeviceInitConfig);
    status = AcxDeviceInitInitialize(DeviceInit, &acxDeviceInitConfig);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: AcxDeviceInitInitialize failed 0x%08x\n",
                   status));
        return status;
    }

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

    context = OpenA8DJGetDeviceContext(device);
    OpenA8DJ_InitializeDefaults(context);

    ACX_DEVICE_CONFIG_INIT(&acxDeviceConfig);
    status = AcxDeviceInitialize(device, &acxDeviceConfig);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: AcxDeviceInitialize failed 0x%08x\n",
                   status));
        return status;
    }

    for (interfaceIndex = 0; interfaceIndex < OPENA8DJ_STEREO_PAIRS; interfaceIndex++) {
        status = OpenA8DJ_CreateAcxCircuit(
            device,
            AcxCircuitTypeRender,
            gOpenA8DJRenderCircuitIds[interfaceIndex],
            gOpenA8DJRenderCircuitNames[interfaceIndex],
            gOpenA8DJRenderEndpointNames[interfaceIndex],
            interfaceIndex,
            TRUE,
            &context->RenderCircuits[interfaceIndex]);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "OpenA8DJUsb: render ACX circuit %lu create failed 0x%08x\n",
                       interfaceIndex,
                       status));
            return status;
        }

        status = OpenA8DJ_CreateAcxCircuit(
            device,
            AcxCircuitTypeCapture,
            gOpenA8DJCaptureCircuitIds[interfaceIndex],
            gOpenA8DJCaptureCircuitNames[interfaceIndex],
            gOpenA8DJCaptureEndpointNames[interfaceIndex],
            interfaceIndex,
            FALSE,
            &context->CaptureCircuits[interfaceIndex]);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "OpenA8DJUsb: capture ACX circuit %lu create failed 0x%08x\n",
                       interfaceIndex,
                       status));
            return status;
        }
    }

    WDF_WORKITEM_CONFIG_INIT(&workItemConfig, OpenA8DJ_EvtStreamWorkItem);
    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = device;
    status = WdfWorkItemCreate(
        &workItemConfig,
        &workItemAttributes,
        &context->StreamWorkItem);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: WdfWorkItemCreate failed 0x%08x\n",
                   status));
        return status;
    }

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
    UCHAR interfaceIndex;

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

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_MULTIPLE_INTERFACES(
        &configParams,
        0,
        NULL);

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

    context->InterfaceCount = WdfUsbTargetDeviceGetNumInterfaces(context->UsbDevice);
    context->ConfiguredInterfaceCount = configParams.Types.MultiInterface.NumberOfConfiguredInterfaces;
    if (context->ConfiguredInterfaceCount > OPENA8DJ_MAX_USB_INTERFACES) {
        context->ConfiguredInterfaceCount = OPENA8DJ_MAX_USB_INTERFACES;
    }
    for (interfaceIndex = 0; interfaceIndex < context->ConfiguredInterfaceCount; interfaceIndex++) {
        context->UsbInterfaces[interfaceIndex] =
            WdfUsbTargetDeviceGetInterface(context->UsbDevice, interfaceIndex);
        status = OpenA8DJ_SelectBestAlternateSetting(context, context->UsbInterfaces[interfaceIndex]);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    OpenA8DJ_MapConfiguredPipes(Device);
    if (!context->AcxCircuitsAdded) {
        for (interfaceIndex = 0; interfaceIndex < OPENA8DJ_STEREO_PAIRS; interfaceIndex++) {
            if (context->RenderCircuits[interfaceIndex] != NULL) {
                OpenA8DJ_RecordAcxStage(300, STATUS_SUCCESS);
                status = AcxDeviceAddCircuit(Device, context->RenderCircuits[interfaceIndex]);
                if (!NT_SUCCESS(status)) {
                    OpenA8DJ_RecordAcxStage(301, status);
                    KdPrintEx((DPFLTR_IHVDRIVER_ID,
                               DPFLTR_ERROR_LEVEL,
                               "OpenA8DJUsb: AcxDeviceAddCircuit(render %lu) failed 0x%08x\n",
                               interfaceIndex,
                               status));
                    return status;
                }
            }
            if (context->CaptureCircuits[interfaceIndex] != NULL) {
                OpenA8DJ_RecordAcxStage(302, STATUS_SUCCESS);
                status = AcxDeviceAddCircuit(Device, context->CaptureCircuits[interfaceIndex]);
                if (!NT_SUCCESS(status)) {
                    OpenA8DJ_RecordAcxStage(303, status);
                    KdPrintEx((DPFLTR_IHVDRIVER_ID,
                               DPFLTR_ERROR_LEVEL,
                               "OpenA8DJUsb: AcxDeviceAddCircuit(capture %lu) failed 0x%08x\n",
                               interfaceIndex,
                               status));
                    return status;
                }
            }
        }
        OpenA8DJ_RecordAcxStage(304, STATUS_SUCCESS);
        context->AcxCircuitsAdded = TRUE;
    }
    if (context->BulkInPipe != NULL) {
        status = OpenA8DJ_StartEp1Reader(context);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_WARNING_LEVEL,
                       "OpenA8DJUsb: EP1 reader config failed 0x%08x\n",
                       status));
        }
    }
    if (context->BulkOutPipe != NULL && context->BulkInPipe != NULL) {
        status = OpenA8DJ_RefreshHardwareControlState(context);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_WARNING_LEVEL,
                       "OpenA8DJUsb: hardware control READ_IO failed 0x%08x\n",
                       status));
        }
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: prepared Audio 8 DJ USB transport interfaces=%u configured=%u pipes=%u bulkOut=%p bulkIn=%p isoIn=%p isoOut=%p\n",
               context->InterfaceCount,
               context->ConfiguredInterfaceCount,
               context->ConfiguredPipeCount,
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
    POPENA8DJ_ISO_CAPTURE_SNAPSHOT isoSnapshot = NULL;
    POPENA8DJ_ISO_SILENCE_PULSE silencePulse = NULL;
    POPENA8DJ_ISO_TONE_BURST toneBurst = NULL;
    POPENA8DJ_AUDIO_PARAMS_RESULT audioParams = NULL;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    if (IoControlCode == IOCTL_OPENA8DJ_GET_USB_INFO) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_USB_INFO), (PVOID *)&info);
        if (NT_SUCCESS(status)) {
            RtlZeroMemory(info, sizeof(*info));
            info->Size = sizeof(*info);
            info->VendorId = OPENA8DJ_VENDOR_ID;
            info->ProductId = OPENA8DJ_PRODUCT_ID;
            info->TotalInterfaceCount = context->InterfaceCount;
            info->ConfiguredInterfaceCount = context->ConfiguredInterfaceCount;
            info->ConfiguredPipeCount = context->ConfiguredPipeCount;
            info->AlternateSettingCount = context->AlternateSettingCount;
            info->SelectedAlternateSetting = context->SelectedAlternateSetting;
            info->Ep1ReaderConfigNtStatus = (ULONG)context->Ep1ReaderConfigStatus;
            info->Ep1ReaderStartNtStatus = (ULONG)context->Ep1ReaderStartStatus;
            info->Ep1ReaderCompletions = context->Ep1ReaderCompletions;
            info->Ep1ReaderZeroReads = context->Ep1ReaderZeroReads;
            info->Ep1ReaderBytes = context->Ep1ReaderBytes;
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
    } else if (IoControlCode == IOCTL_OPENA8DJ_ISO_CAPTURE_SNAPSHOT) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_ISO_CAPTURE_SNAPSHOT), (PVOID *)&isoSnapshot);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_CaptureIsoSnapshot(context, isoSnapshot);
            bytesReturned = sizeof(*isoSnapshot);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_ISO_SILENCE_PULSE) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_ISO_SILENCE_PULSE), (PVOID *)&silencePulse);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_SendIsoSilencePulse(context, silencePulse);
            bytesReturned = sizeof(*silencePulse);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_ISO_TONE_BURST) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_ISO_TONE_BURST), (PVOID *)&toneBurst);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_SendIsoToneBurst(context, toneBurst);
            bytesReturned = sizeof(*toneBurst);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_APPLY_AUDIO_PARAMS) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_AUDIO_PARAMS_RESULT), (PVOID *)&audioParams);
        if (NT_SUCCESS(status)) {
            (void)OpenA8DJ_ApplyAudioParams(context, audioParams);
            status = STATUS_SUCCESS;
            bytesReturned = sizeof(*audioParams);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_CONTROL_STATE) {
        POPENA8DJ_CONTROL_STATE controlState = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_CONTROL_STATE), (PVOID *)&controlState);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_RefreshHardwareControlState(context);
        }
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
        OPENA8DJ_AUDIO_PARAMS_RESULT audioParamsResult;

        context->StartRequests++;
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_STREAM_STATE), (PVOID *)&streamState);
        if (NT_SUCCESS(status)) {
            if (context->StreamWorkItem == NULL ||
                context->IsoInPipe == NULL ||
                context->IsoOutPipe == NULL) {
                context->RejectedStartRequests++;
                context->StreamState.Streaming = FALSE;
                context->StreamState.StreamingEngineReady = FALSE;
                *streamState = context->StreamState;
                bytesReturned = sizeof(*streamState);
                status = STATUS_DEVICE_NOT_READY;
            } else if (InterlockedCompareExchange(&context->StreamWorkerActive, 1, 0) == 0) {
                RtlZeroMemory(&audioParamsResult, sizeof(audioParamsResult));
                (void)OpenA8DJ_ApplyAudioParams(context, &audioParamsResult);

                RtlZeroMemory(&context->StreamState, sizeof(context->StreamState));
                context->StreamState.Size = sizeof(context->StreamState);
                context->StreamState.Streaming = TRUE;
                context->StreamState.StreamingEngineReady = TRUE;
                context->StreamState.SampleRate = context->CurrentFormat.SampleRate;
                context->StreamState.BufferFrames = context->CurrentFormat.BufferFrames;
                InterlockedExchange(&context->StreamStopRequested, 0);
                WdfWorkItemEnqueue(context->StreamWorkItem);

                *streamState = context->StreamState;
                bytesReturned = sizeof(*streamState);
                status = STATUS_SUCCESS;
            } else {
                context->StreamState.Streaming = TRUE;
                context->StreamState.StreamingEngineReady = TRUE;
                *streamState = context->StreamState;
                bytesReturned = sizeof(*streamState);
                status = STATUS_SUCCESS;
            }
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_STOP_STREAMING) {
        POPENA8DJ_STREAM_STATE streamState = NULL;
        ULONG waitIndex;

        context->StopRequests++;
        InterlockedExchange(&context->StreamStopRequested, 1);
        for (waitIndex = 0;
             waitIndex < 100u && InterlockedCompareExchange(&context->StreamWorkerActive, 0, 0) != 0;
             waitIndex++) {
            LARGE_INTEGER delay;
            delay.QuadPart = WDF_REL_TIMEOUT_IN_MS(10);
            KeDelayExecutionThread(KernelMode, FALSE, &delay);
        }
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
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_RENDER_TRACE) {
        POPENA8DJ_RENDER_TRACE trace = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_RENDER_TRACE), (PVOID *)&trace);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_FillRenderTrace(context, trace);
            bytesReturned = sizeof(*trace);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_USB_PLAYBACK_TRACE) {
        POPENA8DJ_USB_PLAYBACK_TRACE trace = NULL;

        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_USB_PLAYBACK_TRACE), (PVOID *)&trace);
        if (NT_SUCCESS(status)) {
            OpenA8DJ_FillUsbPlaybackTrace(context, trace);
            bytesReturned = sizeof(*trace);
        }
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
