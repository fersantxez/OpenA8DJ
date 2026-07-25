#include <initguid.h>
#include "OpenA8DJUsb.h"
#include "Trace.h"
#include "OpenA8DJUsb.tmh"

#ifndef OPENA8DJ_VIRTUAL_MODE
#define OPENA8DJ_VIRTUAL_MODE 0
#endif

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
#define OPENA8DJ_BULK_WRITE_RETRY_LIMIT 3u
#define OPENA8DJ_BULK_WRITE_RETRY_DELAY_MS 100u
#define OPENA8DJ_STREAM_TRANSIENT_RETRY_LIMIT 3u
#define OPENA8DJ_STREAM_NO_DATA_COMPLETION_LIMIT 8192u
#define OPENA8DJ_ISO_TRANSFER_BUFFER_CAPACITY \
    (4096u * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT)
#define OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT 64u
#define OPENA8DJ_PERSISTENT_ISO_TRANSFER_BUFFER_CAPACITY \
    (4096u * OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT)
#define OPENA8DJ_HIGH_SPEED_ISO_PACKETS_PER_FRAME 8u
#define OPENA8DJ_PERSISTENT_ISO_TRANSFER_FRAMES \
    (OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT / OPENA8DJ_HIGH_SPEED_ISO_PACKETS_PER_FRAME)
#define OPENA8DJ_ISO_CAPTURE_START_LEAD_FRAMES 16u
#define OPENA8DJ_ISO_OUTPUT_LEAD_FRAMES 8u
#define OPENA8DJ_AUDIO_RATE_SETTLE_MAX_SNAPSHOTS 100u
#define OPENA8DJ_AUDIO_RATE_SETTLE_REQUIRED_CONSECUTIVE 2u
#define OPENA8DJ_AUDIO_RATE_SETTLE_BUDGET_MS 1000u
#ifndef OPENA8DJ_ENABLE_PERSISTENT_ASYNC_ISO
#define OPENA8DJ_ENABLE_PERSISTENT_ASYNC_ISO 1
#endif
static volatile LONG gOpenA8DJPersistentAsyncIsoEnabled =
    OPENA8DJ_ENABLE_PERSISTENT_ASYNC_ISO;

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

static BOOLEAN
OpenA8DJ_IsIdempotentBulkCommand(_In_ UCHAR Command);

static NTSTATUS
OpenA8DJ_RefreshHardwareControlState(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);

static NTSTATUS
OpenA8DJ_WriteHardwareControlState(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_reads_bytes_(OPENA8DJ_CONTROL_STATE_BYTES) const UCHAR *State);

static BOOLEAN
OpenA8DJ_GetAudioRateParameters(
    _In_ ULONG SampleRate,
    _Out_ PUCHAR RateCode,
    _Out_ PUSHORT BytesPerPacket);

static NTSTATUS
OpenA8DJ_ConfigureAudioParams(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG SampleRate,
    _Out_ POPENA8DJ_AUDIO_PARAMS_RESULT Result);

static NTSTATUS
OpenA8DJ_ConfigureAudioParamsIsolated(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG SampleRate,
    _Out_ POPENA8DJ_AUDIO_PARAMS_RESULT Result);

static NTSTATUS
OpenA8DJ_WaitForAudioRateSettle(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ USHORT ExpectedPacketBytes);

static BOOLEAN
OpenA8DJ_IsTransportReady(_In_ POPENA8DJ_DEVICE_CONTEXT Context);

static BOOLEAN
OpenA8DJ_HasActiveAcxStreams(_In_ POPENA8DJ_DEVICE_CONTEXT Context);

static BOOLEAN
OpenA8DJ_AbortIsoInputPipe(_In_ POPENA8DJ_DEVICE_CONTEXT Context);

static BOOLEAN
OpenA8DJ_AbortIsoOutputPipe(_In_ POPENA8DJ_DEVICE_CONTEXT Context);

static EVT_WDF_OBJECT_CONTEXT_CLEANUP OpenA8DJ_EvtDriverContextCleanup;

static NTSTATUS
OpenA8DJ_ArmNextBootFailSafe(_In_ PUNICODE_STRING RegistryPath);

static VOID
OpenA8DJ_RecordSafetyCheckpoint(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG Checkpoint,
    _In_ NTSTATUS Status);

static BOOLEAN
OpenA8DJ_ConsumeCanaryAuthorization(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG RequiredPhase,
    _In_ ULONG Checkpoint);

static NTSTATUS
OpenA8DJ_BeginIsoOneShot(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG RequiredPhase);

static NTSTATUS
OpenA8DJ_EndIsoOneShot(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);

static VOID
OpenA8DJ_PurgeIsoTargets(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ BOOLEAN RestartTargets);

static VOID
OpenA8DJ_PurgeIsoTargetsLocked(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ BOOLEAN RestartTargets);

static NTSTATUS
OpenA8DJ_EnsureIsoTransportResources(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);

static NTSTATUS
OpenA8DJ_DestroyIsoTransportResources(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);

static BOOLEAN
OpenA8DJ_TrySignalPersistentIsoDrained(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);

static BOOLEAN
OpenA8DJ_ArePersistentIsoSlotsIdle(_In_ POPENA8DJ_DEVICE_CONTEXT Context);

static NTSTATUS
OpenA8DJ_StartPersistentIsoEngine(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ USHORT OutputPacketBytes);

#if OPENA8DJ_VIRTUAL_MODE
static VOID
OpenA8DJ_RunVirtualStreamWorker(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context);
#endif

DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_A,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x21);
DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_B,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x22);
DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_C,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x23);
DEFINE_GUID(GUID_OPENA8DJ_RENDER_CIRCUIT_D,
    0x7d353482, 0x8838, 0x4f61, 0x9a, 0x1e, 0xa2, 0x88, 0x34, 0x15, 0x2a, 0x24);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_A,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x48);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_B,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x49);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_C,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x4a);
DEFINE_GUID(GUID_OPENA8DJ_CAPTURE_CIRCUIT_D,
    0xb0d0d99a, 0x2ef8, 0x4cf9, 0x9b, 0x81, 0x78, 0xb8, 0x63, 0x6d, 0x02, 0x4b);

static const GUID kOpenA8DJKsCategoryAudio = { STATIC_KSCATEGORY_AUDIO };
static const GUID kOpenA8DJKsDataFormatTypeAudio = { STATIC_KSDATAFORMAT_TYPE_AUDIO };
static const GUID kOpenA8DJKsDataFormatSubtypePcm = { STATIC_KSDATAFORMAT_SUBTYPE_PCM };
static const GUID kOpenA8DJKsDataFormatSubtypeIeeeFloat =
    { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID kOpenA8DJKsDataFormatSpecifierWaveFormatEx = { STATIC_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX };
static const GUID kOpenA8DJKsNodeTypeSpeaker = { STATIC_KSNODETYPE_SPEAKER };
static const GUID kOpenA8DJKsNodeTypeMicrophone = { STATIC_KSNODETYPE_MICROPHONE };
static const GUID kOpenA8DJKsNodeTypeLineConnector = { STATIC_KSNODETYPE_LINE_CONNECTOR };

DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameA, L"OpenA8DJ Render A v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameB, L"OpenA8DJ Render B v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameC, L"OpenA8DJ Render C v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJRenderCircuitNameD, L"OpenA8DJ Render D v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameA, L"OpenA8DJ Capture A v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameB, L"OpenA8DJ Capture B v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameC, L"OpenA8DJ Capture C v2");
DECLARE_CONST_UNICODE_STRING(gOpenA8DJCaptureCircuitNameD, L"OpenA8DJ Capture D v2");

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
#ifndef OPENA8DJ_ENABLE_ASYNC_OUTPUT
#define OPENA8DJ_ENABLE_ASYNC_OUTPUT 0
#endif
#define OPENA8DJ_ISO_OUTPUT_PACKET_BYTES_CAPTURE_SHAPE MAXULONG
#define OPENA8DJ_HW_LATENCY_MILLISECONDS OPENA8DJ_ISO_OUTPUT_LEAD_FRAMES
#define OPENA8DJ_RENDER_TRANSFER_BLEND_FRAMES 0u
#define OPENA8DJ_PHYSICAL_TONE_BURST_MAX_TRANSFERS 25u
#define OPENA8DJ_RT_RENDER_START_OFFSET_FRAMES 0u
#define OPENA8DJ_RT_RENDER_PREFILL_FRAMES 480u
#define OPENA8DJ_DEBUG_STREAM_TONE 0
#define OPENA8DJ_OUTPUT_NATIVE_I24 0
#define OPENA8DJ_USB_BYTES_PER_SAMPLE 4u
#define OPENA8DJ_AUDIO_STREAM_COUNT (OPENA8DJ_OUTPUT_CHANNELS / 2u)
#define OPENA8DJ_CLOCK_DRIFT_TOLERANCE 5u
#define OPENA8DJ_USB_OUTPUT_FRAME_BYTES (OPENA8DJ_OUTPUT_CHANNELS * OPENA8DJ_USB_BYTES_PER_SAMPLE)

#if OPENA8DJ_VIRTUAL_MODE
#define OPENA8DJ_VIRTUAL_TICK_FRAMES 48u
#define OPENA8DJ_VIRTUAL_TICK_BYTES \
    (OPENA8DJ_VIRTUAL_TICK_FRAMES * OPENA8DJ_USB_OUTPUT_FRAME_BYTES)
#endif

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
    ULONG RtSampleRate;
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
    KSPIN_LOCK RtPacketStateLock;
    ULONG CurrentPacket;
    ULONG LastCompletedPacket;
    BOOLEAN CapturePacketValid;
    BOOLEAN RenderEosReceived;
    volatile LONG RtPacketEpoch;
    volatile LONG RtPacketEpochActive;
    BOOLEAN HasRun;
    BOOLEAN RtClockRunning;
    ULONG RtPacketFrameProgress;
    volatile LONG RtPacketNotificationPending;
    ULONG PendingCompletedPacket;
    ULONGLONG PendingCompletionQpc;
    LONG PendingNotificationEpoch;
    volatile LONG64 CurrentPacketStartQpc;
    volatile LONG64 LastPacketStartQpc;
    ULONG RenderFrameCursor;
    ULONG CaptureFrameCursor;
    ULONG RenderTransferFrameIndex;
    ULONGLONG PositionBlocks;
    ULONGLONG PositionQpc;
    ULONGLONG LastPresentationBlocks;
    ULONG RenderPrefillFramesRemaining;
    EX_RUNDOWN_REF WorkerRundown;
    BOOLEAN WorkerRundownCompleted;
    volatile LONG WorkerReferences;
    KEVENT WorkerIdleEvent;
} OPENA8DJ_ACX_STREAM_CONTEXT, *POPENA8DJ_ACX_STREAM_CONTEXT;

C_ASSERT((FIELD_OFFSET(OPENA8DJ_ACX_STREAM_CONTEXT, CurrentPacketStartQpc) & 7) == 0);
C_ASSERT((FIELD_OFFSET(OPENA8DJ_ACX_STREAM_CONTEXT, LastPacketStartQpc) & 7) == 0);

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
    if (streamContext != NULL &&
        InterlockedIncrement(&streamContext->WorkerReferences) == 1) {
        KeClearEvent(&streamContext->WorkerIdleEvent);
    }
    KeReleaseSpinLock(&Context->ActiveStreamLock, oldIrql);
    return streamContext;
}

static VOID
OpenA8DJ_ReleaseActiveStream(_In_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    if (InterlockedDecrement(&StreamContext->WorkerReferences) == 0) {
        KeSetEvent(&StreamContext->WorkerIdleEvent, IO_NO_INCREMENT, FALSE);
    }
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
OpenA8DJ_RecordSafetyCheckpoint(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG Checkpoint,
    _In_ NTSTATUS Status)
{
    UNICODE_STRING path;
    ULONG sequence;
    ULONG statusValue = (ULONG)Status;
    ULONG phase;
    ULONG remaining;
    BOOLEAN sampledHotPath = FALSE;
    BOOLEAN persistenceLockHeld = FALSE;
    NTSTATUS persistenceLockStatus = STATUS_SUCCESS;
    BOOLEAN realtimeOnly =
        Checkpoint >= OPENA8DJ_CHECKPOINT_ISO_CAPTURE_SUBMIT &&
        Checkpoint <= OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RECLAIM_COMPLETE;

    if (!realtimeOnly && KeGetCurrentIrql() == PASSIVE_LEVEL &&
        Context->SafetyCheckpointLock != NULL) {
        persistenceLockStatus = WdfWaitLockAcquire(
            Context->SafetyCheckpointLock,
            NULL);
        persistenceLockHeld = NT_SUCCESS(persistenceLockStatus);
    }

    Context->LastSafetyStatus = Status;
    Context->LastSafetyCheckpoint = Checkpoint;
    sequence = (ULONG)InterlockedIncrement(&Context->SafetyCheckpointSequence);

    /*
     * Successful isochronous checkpoints are a hot path.  Preserve every
     * failure and the initial bounded trace, then sample the steady state.
     * LastSafetyCheckpoint/Status above remain current in crash-dump memory.
     */
    if (NT_SUCCESS(Status) && realtimeOnly) {
        LONG budget = InterlockedDecrement(&Context->SafetyTraceBudget);
        if (budget < 0) {
            if ((sequence & 0x3ffu) != 0) {
                return;
            }
            sampledHotPath = TRUE;
        }
    }

    /* IFR is the rolling, crash-dump-resident source of truth. */
    TraceEvents(
        NT_SUCCESS(Status) ? TRACE_LEVEL_INFORMATION : TRACE_LEVEL_ERROR,
        TRACE_SAFETY,
        "checkpoint=%lu sequence=%lu status=0x%08x phase=%ld remaining=%ld build=%s",
        Checkpoint,
        sequence,
        Status,
        InterlockedCompareExchange(&Context->CanaryPhase, 0, 0),
        InterlockedCompareExchange(&Context->CanaryOperationsRemaining, 0, 0),
        OPENA8DJ_BUILD_FINGERPRINT);

    /* Real-time ISO paths must never trigger KdPrint or synchronous registry I/O. */
    if (realtimeOnly || sampledHotPath) {
        return;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               NT_SUCCESS(Status) ? DPFLTR_INFO_LEVEL : DPFLTR_ERROR_LEVEL,
               "OpenA8DJUsb: safety checkpoint=%lu sequence=%lu status=0x%08x phase=%ld remaining=%ld build=%s\n",
               Checkpoint,
               sequence,
               Status,
               InterlockedCompareExchange(&Context->CanaryPhase, 0, 0),
               InterlockedCompareExchange(&Context->CanaryOperationsRemaining, 0, 0),
               OPENA8DJ_BUILD_FINGERPRINT));
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return;
    }
    if (Context->SafetyCheckpointLock == NULL || !persistenceLockHeld) {
        return;
    }

    phase = (ULONG)InterlockedCompareExchange(&Context->CanaryPhase, 0, 0);
    remaining = (ULONG)InterlockedCompareExchange(&Context->CanaryOperationsRemaining, 0, 0);
    RtlInitUnicodeString(&path, L"OpenA8DJUsbAcx\\Parameters");
    (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
        L"LastSafetySequence", REG_DWORD, &sequence, sizeof(sequence));
    (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
        L"LastSafetyStatus", REG_DWORD, &statusValue, sizeof(statusValue));
    (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
        L"CanaryPhase", REG_DWORD, &phase, sizeof(phase));
    (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
        L"CanaryOperationsRemaining", REG_DWORD, &remaining, sizeof(remaining));
    if (Checkpoint == OPENA8DJ_CHECKPOINT_STREAM_WORKER_EXIT) {
        ULONG generation = (ULONG)InterlockedCompareExchange(
            &Context->IsoEngineGeneration,
            0,
            0);

        (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
            L"LastIsoGeneration", REG_DWORD, &generation, sizeof(generation));
        (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
            L"LastIsoCurrentFrame", REG_DWORD,
            &Context->IsoCaptureFrameQueryCurrent,
            sizeof(Context->IsoCaptureFrameQueryCurrent));
        (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
            L"LastIsoSeedFrame", REG_DWORD,
            &Context->IsoCaptureSeedStartFrame,
            sizeof(Context->IsoCaptureSeedStartFrame));
        (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
            L"LastIsoNextFrame", REG_DWORD,
            &Context->IsoNextCaptureStartFrame,
            sizeof(Context->IsoNextCaptureStartFrame));
    }
    /* Write the checkpoint last so it is the commit marker after a crash. */
    (VOID)RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, path.Buffer,
        L"LastSafetyCheckpoint", REG_DWORD, &Checkpoint, sizeof(Checkpoint));
    WdfWaitLockRelease(Context->SafetyCheckpointLock);
}

static BOOLEAN
OpenA8DJ_ConsumeCanaryAuthorization(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG RequiredPhase,
    _In_ ULONG Checkpoint)
{
#if OPENA8DJ_VIRTUAL_MODE
    UNREFERENCED_PARAMETER(RequiredPhase);
    OpenA8DJ_RecordSafetyCheckpoint(Context, Checkpoint, STATUS_SUCCESS);
    return TRUE;
#else
    LONG remaining;

    if ((ULONG)InterlockedCompareExchange(&Context->CanaryPhase, 0, 0) != RequiredPhase) {
        OpenA8DJ_RecordSafetyCheckpoint(Context, Checkpoint, STATUS_ACCESS_DENIED);
        return FALSE;
    }

    for (;;) {
        remaining = InterlockedCompareExchange(&Context->CanaryOperationsRemaining, 0, 0);
        if (remaining <= 0) {
            OpenA8DJ_RecordSafetyCheckpoint(Context, Checkpoint, STATUS_ACCESS_DENIED);
            return FALSE;
        }
        if (InterlockedCompareExchange(
                &Context->CanaryOperationsRemaining,
                remaining - 1,
                remaining) == remaining) {
            break;
        }
    }

    if (remaining == 1) {
        InterlockedExchange(&Context->CanaryPhase, OPENA8DJ_CANARY_PHASE_NONE);
    }
    OpenA8DJ_RecordSafetyCheckpoint(Context, Checkpoint, STATUS_SUCCESS);
    return TRUE;
#endif
}

static NTSTATUS
OpenA8DJ_BeginIsoOneShot(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG RequiredPhase)
{
    NTSTATUS status;
    WDFIOTARGET inputTarget;
    WDFIOTARGET outputTarget;
    BOOLEAN purgeLockHeld = FALSE;

    if (Context == NULL || Context->IsoPurgeLock == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    status = WdfWaitLockAcquire(Context->IsoPurgeLock, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    purgeLockHeld = TRUE;
    if (InterlockedCompareExchange(&Context->IsoOneShotActive, 1, 0) != 0) {
        status = STATUS_DEVICE_BUSY;
        goto Fail;
    }
    if (Context->IsoTransportLock == NULL || Context->IsoInPipe == NULL ||
        Context->IsoOutPipe == NULL) {
        status = STATUS_DEVICE_NOT_READY;
        goto Fail;
    }

    status = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(status)) {
        goto Fail;
    }
    inputTarget = WdfUsbTargetPipeGetIoTarget(Context->IsoInPipe);
    outputTarget = WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe);
    if (InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0 ||
        InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoTransportDraining, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoTransportHealthy, 0, 0) == 0 ||
        InterlockedCompareExchange(&Context->IsoTransportHealthyGeneration, 0, 0) !=
            InterlockedCompareExchange(&Context->IsoEngineGeneration, 0, 0) ||
        InterlockedCompareExchange(&Context->IsoEngineState, 0, 0) !=
            OpenA8DJIsoEngineStopped ||
        InterlockedCompareExchange(&Context->IsoEngineRunning, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->StreamWorkerActive, 0, 0) != 0 ||
        OpenA8DJ_HasActiveAcxStreams(Context) ||
        InterlockedCompareExchange(&Context->IsoOutstandingCapture, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoOutstandingOutput, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoProcessWorkPending, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoProcessWorkActive, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoStopWorkPending, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoStopWorkActive, 0, 0) != 0 ||
        !OpenA8DJ_ArePersistentIsoSlotsIdle(Context) ||
        WdfIoTargetGetState(inputTarget) != WdfIoTargetStarted ||
        WdfIoTargetGetState(outputTarget) != WdfIoTargetStarted) {
        status = STATUS_DEVICE_BUSY;
    } else {
        status = STATUS_SUCCESS;
    }
    WdfWaitLockRelease(Context->IsoTransportLock);
    if (!NT_SUCCESS(status)) {
        goto Fail;
    }
    if (!OpenA8DJ_ConsumeCanaryAuthorization(
            Context,
            RequiredPhase,
            OPENA8DJ_CHECKPOINT_CANARY_CONSUMED)) {
        status = STATUS_ACCESS_DENIED;
        goto Fail;
    }
    return STATUS_SUCCESS;

Fail:
    InterlockedExchange(&Context->IsoOneShotActive, 0);
    if (purgeLockHeld) {
        WdfWaitLockRelease(Context->IsoPurgeLock);
    }
    return status;
}

static NTSTATUS
OpenA8DJ_EndIsoOneShot(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status = STATUS_DEVICE_NOT_READY;

    /*
     * Every bounded ASAP producer is followed by the same purge/reset/start
     * lifecycle as a drained persistent generation.  IsoOneShotActive remains
     * asserted until the health latch is republished, so an ACX start cannot
     * race the cleanup.
     */
    OpenA8DJ_PurgeIsoTargetsLocked(Context, TRUE);
    if (Context->IsoInPipe != NULL && Context->IsoOutPipe != NULL &&
        InterlockedCompareExchange(&Context->IsoTransportDraining, 0, 0) == 0 &&
        InterlockedCompareExchange(&Context->IsoTransportHealthy, 0, 0) != 0 &&
        InterlockedCompareExchange(&Context->IsoTransportHealthyGeneration, 0, 0) ==
            InterlockedCompareExchange(&Context->IsoEngineGeneration, 0, 0) &&
        InterlockedCompareExchange(&Context->IsoEngineState, 0, 0) ==
            OpenA8DJIsoEngineStopped &&
        WdfIoTargetGetState(WdfUsbTargetPipeGetIoTarget(Context->IsoInPipe)) ==
            WdfIoTargetStarted &&
        WdfIoTargetGetState(WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe)) ==
            WdfIoTargetStarted) {
        status = STATUS_SUCCESS;
    }
    InterlockedExchange(&Context->IsoOneShotActive, 0);
    WdfWaitLockRelease(Context->IsoPurgeLock);
    return status;
}

static NTSTATUS
OpenA8DJ_ConfigureAudioParamsIsolated(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG SampleRate,
    _Out_ POPENA8DJ_AUDIO_PARAMS_RESULT Result)
{
    NTSTATUS status;

    if (Context->IsoPurgeLock == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    status = WdfWaitLockAcquire(Context->IsoPurgeLock, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0 ||
        InterlockedCompareExchange(&Context->IsoOneShotActive, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoEngineState, 0, 0) !=
            OpenA8DJIsoEngineStopped ||
        InterlockedCompareExchange(&Context->StreamWorkerActive, 0, 0) != 0 ||
        OpenA8DJ_HasActiveAcxStreams(Context)) {
        status = STATUS_DEVICE_BUSY;
    } else {
        status = OpenA8DJ_ConfigureAudioParams(Context, SampleRate, Result);
    }
    WdfWaitLockRelease(Context->IsoPurgeLock);
    return status;
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
    Context->IsoProcessWorkItem = NULL;
    Context->IsoStopWorkItem = NULL;
    Context->IsoCaptureRequest = NULL;
    Context->IsoCaptureUrbMemory = NULL;
    Context->IsoCaptureUrb = NULL;
    Context->IsoOutputRequest = NULL;
    Context->IsoOutputUrbMemory = NULL;
    Context->IsoOutputUrb = NULL;
    Context->StreamCaptureBufferMemory = NULL;
    Context->StreamCaptureBuffer = NULL;
    Context->StreamPlaybackBufferMemory = NULL;
    Context->StreamPlaybackBuffer = NULL;
    Context->IsoTransportLock = NULL;
    Context->IsoPurgeLock = NULL;
    Context->SafetyCheckpointLock = NULL;
    Context->IsoTransportDraining = 0;
    KeInitializeEvent(&Context->IsoPurgeCompleteEvent, NotificationEvent, TRUE);
    RtlZeroMemory(Context->IsoCaptureSlots, sizeof(Context->IsoCaptureSlots));
    RtlZeroMemory(Context->IsoOutputSlots, sizeof(Context->IsoOutputSlots));
    Context->IsoEngineGeneration = 0;
    Context->IsoEngineRunning = 0;
    Context->IsoEngineState = OpenA8DJIsoEngineStopped;
    Context->IsoOneShotActive = 0;
    Context->IsoProcessWorkPending = 0;
    Context->IsoProcessWorkActive = 0;
    Context->IsoStopWorkPending = 0;
    Context->IsoStopWorkActive = 0;
    Context->IsoOutstandingCapture = 0;
    Context->IsoOutstandingOutput = 0;
    Context->IsoNextCaptureSubmitSequence = 0;
    Context->IsoNextOutputSubmitSequence = 0;
    Context->IsoNextCaptureProcessSequence = 1;
    Context->IsoNextOutputProcessSequence = 1;
    Context->IsoCaptureFrameQueries = 0;
    Context->IsoCaptureFrameQueryFailures = 0;
    Context->IsoCaptureFrameQueryCurrent = 0;
    Context->IsoCaptureSeedStartFrame = 0;
    Context->IsoNextCaptureStartFrame = 0;
    Context->IsoInputPipeResetRuns = 0;
    Context->IsoInputPipeResetFailures = 0;
    Context->IsoInputPipeResetLastNtStatus = (LONG)STATUS_SUCCESS;
    Context->IsoOutputPipeResetRuns = 0;
    Context->IsoOutputPipeResetFailures = 0;
    Context->IsoOutputPipeResetLastNtStatus = (LONG)STATUS_SUCCESS;
    Context->IsoTransportHealthy = 0;
    Context->IsoTransportHealthyGeneration = 0;
    Context->IsoInputTargetStartLastNtStatus = (LONG)STATUS_SUCCESS;
    Context->IsoOutputTargetStartLastNtStatus = (LONG)STATUS_SUCCESS;
    Context->IsoLastCaptureErrorSlot = MAXULONG;
    Context->IsoCaptureErrorSnapshotSequence = 0;
    Context->IsoLastCaptureErrorGeneration = 0;
    Context->IsoLastCaptureErrorSubmitSequence = 0;
    Context->IsoLastCaptureErrorScheduledStartFrame = 0;
    Context->IsoLastCaptureErrorFirstPacket = MAXULONG;
    Context->IsoLastCaptureErrorLastPacket = MAXULONG;
    Context->IsoLastCaptureErrorPacketCount = 0;
    Context->IsoLastCaptureErrorSubmitQpc = 0;
    Context->IsoLastCaptureErrorCompletionQpc = 0;
    KeInitializeEvent(&Context->IsoEngineDrainedEvent, NotificationEvent, TRUE);
    Context->CanaryPhase = OPENA8DJ_CANARY_PHASE_NONE;
    Context->CanaryOperationsRemaining = 0;
    Context->CanaryNonceHigh = 0;
    Context->CanaryNonceLow = 0;
    Context->SafetyCheckpointSequence = 0;
    Context->SafetyTraceBudget = 0;
    Context->LastSafetyCheckpoint = OPENA8DJ_CHECKPOINT_DRIVER_LOADED;
    Context->LastSafetyStatus = STATUS_SUCCESS;
    Context->DeviceStopping = 1;
    Context->DevicePrepared = 0;
    RtlZeroMemory(Context->RenderCircuits, sizeof(Context->RenderCircuits));
    RtlZeroMemory(Context->CaptureCircuits, sizeof(Context->CaptureCircuits));
    RtlZeroMemory(Context->RenderCircuitAdded, sizeof(Context->RenderCircuitAdded));
    RtlZeroMemory(Context->CaptureCircuitAdded, sizeof(Context->CaptureCircuitAdded));
    KeInitializeSpinLock(&Context->ActiveStreamLock);
    RtlZeroMemory((PVOID)Context->ActiveRenderStreams, sizeof(Context->ActiveRenderStreams));
    RtlZeroMemory((PVOID)Context->ActiveCaptureStreams, sizeof(Context->ActiveCaptureStreams));
    Context->StreamStopRequested = 1;
    Context->StreamWorkerActive = 0;
    Context->AcxCircuitsAdded = FALSE;
    Context->BulkCommandLock = NULL;
    Context->AudioConfigLock = NULL;
    Context->AudioParamsConfigured = 0;
    Context->ConfiguredSampleRate = 0;
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
    Context->AcxRtRenderPacketCompletions = 0;
    Context->AcxRtRenderPacketNotifications = 0;
    Context->AcxRtRenderPacketNotificationFailures = 0;
    Context->AcxRtRenderCurrentPacket = 0;
    Context->AcxRtRenderLastNotificationNtStatus = (ULONG)STATUS_SUCCESS;
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
OpenA8DJ_IsTransportReady(_In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    if (InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
        return FALSE;
    }

#if OPENA8DJ_VIRTUAL_MODE
    return TRUE;
#else
    return Context->BulkOutPipe != NULL &&
           Context->BulkInPipe != NULL &&
           Context->IsoInPipe != NULL &&
           Context->IsoOutPipe != NULL;
#endif
}

static NTSTATUS
OpenA8DJ_AllocatePersistentIsoSlot(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot,
    _In_ OPENA8DJ_ISO_ENGINE_DIRECTION Direction,
    _In_ ULONG Index,
    _In_ WDFUSBPIPE Pipe,
    _In_ ULONG BufferCapacity)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    if (Slot->Request != NULL) {
        return STATUS_SUCCESS;
    }
    if (Pipe == NULL || BufferCapacity == 0 ||
        BufferCapacity > OPENA8DJ_PERSISTENT_ISO_TRANSFER_BUFFER_CAPACITY) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Slot, sizeof(*Slot));
    Slot->DeviceContext = Context;
    Slot->Direction = Direction;
    Slot->Index = Index;
    Slot->BufferCapacity = BufferCapacity;
    Slot->State = OpenA8DJIsoSlotUninitialized;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Context->UsbDevice;
    status = WdfRequestCreate(
        &attributes,
        WdfUsbTargetPipeGetIoTarget(Pipe),
        &Slot->Request);
    if (!NT_SUCCESS(status)) {
        goto Failure;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Slot->Request;
    status = WdfMemoryCreate(
        &attributes,
        NonPagedPoolNx,
        OPENA8DJ_POOL_TAG,
        GET_ISO_URB_SIZE(OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT),
        &Slot->UrbMemory,
        (PVOID *)&Slot->Urb);
    if (!NT_SUCCESS(status)) {
        goto Failure;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Slot->Request;
    status = WdfMemoryCreate(
        &attributes,
        NonPagedPoolNx,
        OPENA8DJ_POOL_TAG,
        BufferCapacity,
        &Slot->BufferMemory,
        (PVOID *)&Slot->Buffer);
    if (!NT_SUCCESS(status)) {
        goto Failure;
    }

    RtlZeroMemory(Slot->Urb, GET_ISO_URB_SIZE(OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT));
    RtlZeroMemory(Slot->Buffer, BufferCapacity);
    InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
    return STATUS_SUCCESS;

Failure:
    if (Slot->Request != NULL) {
        WdfObjectDelete(Slot->Request);
    }
    RtlZeroMemory(Slot, sizeof(*Slot));
    Slot->DeviceContext = Context;
    Slot->Direction = Direction;
    Slot->Index = Index;
    Slot->State = OpenA8DJIsoSlotUninitialized;
    return status;
}

static NTSTATUS
OpenA8DJ_EnsureIsoTransportResources(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    const size_t bufferBytes = 4096u * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    WDF_USB_DEVICE_INFORMATION usbDeviceInfo;
    WDF_USB_PIPE_INFORMATION capturePipeInfo;
    WDF_USB_PIPE_INFORMATION outputPipeInfo;
    ULONG captureSlotBytes;
    ULONG outputSlotBytes;
    ULONG slotIndex;

    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0 ||
        Context->UsbDevice == NULL ||
        Context->IsoInPipe == NULL ||
        Context->IsoOutPipe == NULL ||
        Context->IsoTransportLock == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    WDF_USB_DEVICE_INFORMATION_INIT(&usbDeviceInfo);
    status = WdfUsbTargetDeviceRetrieveInformation(
        Context->UsbDevice,
        &usbDeviceInfo);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if ((usbDeviceInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) == 0) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    WDF_USB_PIPE_INFORMATION_INIT(&capturePipeInfo);
    WdfUsbTargetPipeGetInformation(Context->IsoInPipe, &capturePipeInfo);
    WDF_USB_PIPE_INFORMATION_INIT(&outputPipeInfo);
    WdfUsbTargetPipeGetInformation(Context->IsoOutPipe, &outputPipeInfo);
    if (capturePipeInfo.Interval != 1 || outputPipeInfo.Interval != 1 ||
        capturePipeInfo.MaximumPacketSize == 0 ||
        capturePipeInfo.MaximumPacketSize > 4096 ||
        outputPipeInfo.MaximumPacketSize == 0 ||
        outputPipeInfo.MaximumPacketSize > 4096) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    captureSlotBytes = capturePipeInfo.MaximumPacketSize * OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
    outputSlotBytes = outputPipeInfo.MaximumPacketSize * OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;

    status = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* ReleaseHardware sets these flags before it waits on this lock. */
    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
        status = STATUS_DEVICE_NOT_READY;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Context->UsbDevice;
    if (Context->StreamCaptureBufferMemory == NULL) {
        status = WdfMemoryCreate(
            &attributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            bufferBytes,
            &Context->StreamCaptureBufferMemory,
            (PVOID *)&Context->StreamCaptureBuffer);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }
    }
    if (Context->StreamPlaybackBufferMemory == NULL) {
        status = WdfMemoryCreate(
            &attributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            bufferBytes,
            &Context->StreamPlaybackBufferMemory,
            (PVOID *)&Context->StreamPlaybackBuffer);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }
    }
    if (Context->IsoCaptureRequest == NULL) {
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Context->UsbDevice;
        status = WdfRequestCreate(
            &attributes,
            WdfUsbTargetPipeGetIoTarget(Context->IsoInPipe),
            &Context->IsoCaptureRequest);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Context->IsoCaptureRequest;
        status = WdfMemoryCreate(
            &attributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT),
            &Context->IsoCaptureUrbMemory,
            (PVOID *)&Context->IsoCaptureUrb);
        if (!NT_SUCCESS(status)) {
            WdfObjectDelete(Context->IsoCaptureRequest);
            Context->IsoCaptureRequest = NULL;
            Context->IsoCaptureUrbMemory = NULL;
            Context->IsoCaptureUrb = NULL;
            goto Exit;
        }
        RtlZeroMemory(
            Context->IsoCaptureUrb,
            GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT));
    }
    if (Context->IsoOutputRequest == NULL) {
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Context->UsbDevice;
        status = WdfRequestCreate(
            &attributes,
            WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe),
            &Context->IsoOutputRequest);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Context->IsoOutputRequest;
        status = WdfMemoryCreate(
            &attributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT),
            &Context->IsoOutputUrbMemory,
            (PVOID *)&Context->IsoOutputUrb);
        if (!NT_SUCCESS(status)) {
            WdfObjectDelete(Context->IsoOutputRequest);
            Context->IsoOutputRequest = NULL;
            Context->IsoOutputUrbMemory = NULL;
            Context->IsoOutputUrb = NULL;
            goto Exit;
        }
        RtlZeroMemory(
            Context->IsoOutputUrb,
            GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT));
    }
    for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
        status = OpenA8DJ_AllocatePersistentIsoSlot(
            Context,
            &Context->IsoCaptureSlots[slotIndex],
            OpenA8DJIsoDirectionCapture,
            slotIndex,
            Context->IsoInPipe,
            captureSlotBytes);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }
        status = OpenA8DJ_AllocatePersistentIsoSlot(
            Context,
            &Context->IsoOutputSlots[slotIndex],
            OpenA8DJIsoDirectionOutput,
            slotIndex,
            Context->IsoOutPipe,
            outputSlotBytes);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }
    }
    status = STATUS_SUCCESS;

Exit:
    WdfWaitLockRelease(Context->IsoTransportLock);
    return status;
}

static NTSTATUS
OpenA8DJ_DestroyIsoTransportResources(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS lockStatus;
    ULONG slotIndex;

    if (Context->IsoTransportLock == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    lockStatus = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(lockStatus)) {
        return lockStatus;
    }

    if (!OpenA8DJ_TrySignalPersistentIsoDrained(Context)) {
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_DRAIN_COMPLETE,
            STATUS_DEVICE_BUSY);
        WdfWaitLockRelease(Context->IsoTransportLock);
        return STATUS_DEVICE_BUSY;
    }

    if (Context->IsoCaptureRequest != NULL) {
        WdfObjectDelete(Context->IsoCaptureRequest);
        Context->IsoCaptureRequest = NULL;
        Context->IsoCaptureUrbMemory = NULL;
        Context->IsoCaptureUrb = NULL;
    }
    if (Context->IsoOutputRequest != NULL) {
        WdfObjectDelete(Context->IsoOutputRequest);
        Context->IsoOutputRequest = NULL;
        Context->IsoOutputUrbMemory = NULL;
        Context->IsoOutputUrb = NULL;
    }
    for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
        POPENA8DJ_ISO_ENGINE_SLOT captureSlot = &Context->IsoCaptureSlots[slotIndex];
        POPENA8DJ_ISO_ENGINE_SLOT outputSlot = &Context->IsoOutputSlots[slotIndex];

        NT_ASSERT(InterlockedCompareExchange(&captureSlot->InFlight, 0, 0) == 0);
        NT_ASSERT(InterlockedCompareExchange(&outputSlot->InFlight, 0, 0) == 0);
        NT_ASSERT(InterlockedCompareExchange(&captureSlot->State, 0, 0) == OpenA8DJIsoSlotIdle);
        NT_ASSERT(InterlockedCompareExchange(&outputSlot->State, 0, 0) == OpenA8DJIsoSlotIdle);
        if (captureSlot->Request != NULL) {
            WdfObjectDelete(captureSlot->Request);
        }
        if (outputSlot->Request != NULL) {
            WdfObjectDelete(outputSlot->Request);
        }
        RtlZeroMemory(captureSlot, sizeof(*captureSlot));
        RtlZeroMemory(outputSlot, sizeof(*outputSlot));
    }
    if (Context->StreamCaptureBufferMemory != NULL) {
        WdfObjectDelete(Context->StreamCaptureBufferMemory);
        Context->StreamCaptureBufferMemory = NULL;
        Context->StreamCaptureBuffer = NULL;
    }
    if (Context->StreamPlaybackBufferMemory != NULL) {
        WdfObjectDelete(Context->StreamPlaybackBufferMemory);
        Context->StreamPlaybackBufferMemory = NULL;
        Context->StreamPlaybackBuffer = NULL;
    }

    WdfWaitLockRelease(Context->IsoTransportLock);
    return STATUS_SUCCESS;
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
OpenA8DJ_InvalidateAudioParamsConfiguration(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status;

    if (Context->AudioConfigLock == NULL) {
        InterlockedExchange(&Context->AudioParamsConfigured, 0);
        InterlockedExchange(&Context->AudioRateSettleActive, 0);
        InterlockedExchange(&Context->AudioRateSettleConsecutiveClean, 0);
        InterlockedExchange(&Context->AudioRateSettleAttemptsRemaining, 0);
        Context->ConfiguredSampleRate = 0;
        return;
    }

    status = WdfWaitLockAcquire(Context->AudioConfigLock, NULL);
    if (NT_SUCCESS(status)) {
        InterlockedExchange(&Context->AudioParamsConfigured, 0);
        InterlockedExchange(&Context->AudioRateSettleActive, 0);
        InterlockedExchange(&Context->AudioRateSettleConsecutiveClean, 0);
        InterlockedExchange(&Context->AudioRateSettleAttemptsRemaining, 0);
        Context->ConfiguredSampleRate = 0;
        WdfWaitLockRelease(Context->AudioConfigLock);
    }
}

static VOID
OpenA8DJ_MarkStreamWorkerStopped(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    Context->StreamState.Streaming = FALSE;
    Context->StreamState.StreamingEngineReady = FALSE;
    OpenA8DJ_InvalidateAudioParamsConfiguration(Context);
    InterlockedExchange(&Context->StreamWorkerActive, 0);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_STREAM_WORKER_EXIT,
        STATUS_SUCCESS);
}

static VOID
OpenA8DJ_InitPcmFormat(
    _Out_ KSDATAFORMAT_WAVEFORMATEXTENSIBLE *Format,
    _In_ USHORT Channels,
    _In_ ULONG ChannelMask,
    _In_ ULONG SampleRate)
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
    Format->WaveFormatExt.Format.nSamplesPerSec = SampleRate;
    Format->WaveFormatExt.Format.nAvgBytesPerSec = SampleRate * blockAlign;
    Format->WaveFormatExt.Format.nBlockAlign = blockAlign;
    Format->WaveFormatExt.Format.wBitsPerSample = 16;
    Format->WaveFormatExt.Format.cbSize =
        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    Format->WaveFormatExt.Samples.wValidBitsPerSample = 16;
    Format->WaveFormatExt.dwChannelMask = ChannelMask;
    Format->WaveFormatExt.SubFormat = kOpenA8DJKsDataFormatSubtypePcm;
}

static NTSTATUS
OpenA8DJ_AddPcmFormatToPin(
    _In_ WDFDEVICE Device,
    _In_ ACXCIRCUIT Circuit,
    _In_ ACXPIN Pin,
    _In_ USHORT Channels,
    _In_ ULONG ChannelMask,
    _In_ ULONG SampleRate,
    _In_ BOOLEAN DefaultFormat)
{
    NTSTATUS status;
    ACXDATAFORMAT acxFormat = NULL;
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE format;
    ACX_DATAFORMAT_CONFIG formatConfig;
    WDF_OBJECT_ATTRIBUTES attributes;
    ACXDATAFORMATLIST formatList;

    OpenA8DJ_InitPcmFormat(&format, Channels, ChannelMask, SampleRate);
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

    if (DefaultFormat) {
        return AcxDataFormatListAssignDefaultDataFormat(formatList, acxFormat);
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
        status = OpenA8DJ_AddPcmFormatToPin(
            Device, Circuit, Pin, 8, KSAUDIO_SPEAKER_7POINT1_SURROUND, 48000, TRUE);
    } else {
        status = OpenA8DJ_AddPcmFormatToPin(
            Device, Circuit, Pin, 2, KSAUDIO_SPEAKER_STEREO, 48000, TRUE);
    }
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (PairIndex == 0) {
        status = OpenA8DJ_AddPcmFormatToPin(
            Device, Circuit, Pin, 8, KSAUDIO_SPEAKER_7POINT1_SURROUND, 44100, FALSE);
    } else {
        status = OpenA8DJ_AddPcmFormatToPin(
            Device, Circuit, Pin, 2, KSAUDIO_SPEAKER_STEREO, 44100, FALSE);
    }
    return status;
}

static VOID
OpenA8DJ_ResetRtPacketEpoch(
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext,
    _In_ ULONGLONG CurrentPacketStartQpc,
    _In_ BOOLEAN EpochActive)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&StreamContext->RtPacketStateLock, &oldIrql);
    StreamContext->CurrentPacket = 0;
    StreamContext->LastCompletedPacket = 0;
    StreamContext->CapturePacketValid = FALSE;
    StreamContext->RenderEosReceived = FALSE;
    StreamContext->HasRun = FALSE;
    StreamContext->RtClockRunning = FALSE;
    StreamContext->RtPacketFrameProgress = 0;
    StreamContext->RtPacketNotificationPending = 0;
    StreamContext->PendingCompletedPacket = 0;
    StreamContext->PendingCompletionQpc = 0;
    StreamContext->PendingNotificationEpoch = 0;
    StreamContext->CurrentPacketStartQpc = (LONG64)CurrentPacketStartQpc;
    StreamContext->LastPacketStartQpc = 0;
    StreamContext->PositionBlocks = 0;
    StreamContext->PositionQpc = CurrentPacketStartQpc;
    StreamContext->LastPresentationBlocks = 0;
    InterlockedIncrement(&StreamContext->RtPacketEpoch);
    InterlockedExchange(
        &StreamContext->RtPacketEpochActive,
        EpochActive ? 1 : 0);
    KeReleaseSpinLock(&StreamContext->RtPacketStateLock, oldIrql);
}

static BOOLEAN
OpenA8DJ_StartRtPacketClockIfNeeded(
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext,
    _In_ ULONGLONG CurrentPacketStartQpc)
{
    KIRQL oldIrql;
    BOOLEAN firstRun = FALSE;

    KeAcquireSpinLock(&StreamContext->RtPacketStateLock, &oldIrql);
    if (!StreamContext->HasRun) {
        StreamContext->HasRun = TRUE;
        StreamContext->CurrentPacketStartQpc = (LONG64)CurrentPacketStartQpc;
        firstRun = TRUE;
    }
    /*
     * A pause does not reset packet or frame position, but the elapsed paused
     * interval must never be included in presentation-position interpolation.
     */
    StreamContext->PositionQpc = CurrentPacketStartQpc;
    StreamContext->RtClockRunning = TRUE;
    KeReleaseSpinLock(&StreamContext->RtPacketStateLock, oldIrql);
    return firstRun;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamPrepareHardware(
    _In_ ACXSTREAM Stream)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);
    LARGE_INTEGER qpc;

    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxPrepareCallbacks);
    }
    if (streamContext->RtKernelAddress != NULL && streamContext->RtBufferBytes != 0) {
        RtlZeroMemory(streamContext->RtKernelAddress, streamContext->RtBufferBytes);
    }
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
    qpc = KeQueryPerformanceCounter(NULL);
    OpenA8DJ_ResetRtPacketEpoch(streamContext, (ULONGLONG)qpc.QuadPart, TRUE);
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
    OpenA8DJ_ResetRtPacketEpoch(streamContext, 0, FALSE);
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
    NTSTATUS status = STATUS_DEVICE_NOT_READY;
    NTSTATUS purgeLockStatus = STATUS_DEVICE_NOT_READY;
    OPENA8DJ_AUDIO_PARAMS_RESULT audioParamsResult;
    BOOLEAN purgeLockHeld = FALSE;
    BOOLEAN firstRun;
    LARGE_INTEGER qpc;

    if (deviceContext != NULL) {
        InterlockedIncrement64(&deviceContext->AcxRunCallbacks);
        if (deviceContext->IsoPurgeLock != NULL) {
            purgeLockStatus = WdfWaitLockAcquire(deviceContext->IsoPurgeLock, NULL);
            purgeLockHeld = NT_SUCCESS(purgeLockStatus);
            if (!purgeLockHeld) {
                status = purgeLockStatus;
            }
        }
        if (purgeLockHeld &&
            InterlockedCompareExchange(&deviceContext->IsoOneShotActive, 0, 0) == 0 &&
            InterlockedCompareExchange(&deviceContext->DeviceStopping, 0, 0) == 0 &&
            OpenA8DJ_IsTransportReady(deviceContext) &&
            streamContext->RtKernelAddress != NULL &&
            deviceContext->StreamWorkItem != NULL) {
            RtlZeroMemory(&audioParamsResult, sizeof(audioParamsResult));
            status = OpenA8DJ_ConfigureAudioParams(
                deviceContext,
                streamContext->RtSampleRate,
                &audioParamsResult);
        }
        if (NT_SUCCESS(status)) {
            if (InterlockedCompareExchange(&deviceContext->DeviceStopping, 0, 0) != 0 ||
                InterlockedCompareExchange(&deviceContext->DevicePrepared, 0, 0) == 0 ||
                InterlockedCompareExchange(&deviceContext->IsoOneShotActive, 0, 0) != 0 ||
                !OpenA8DJ_IsTransportReady(deviceContext)) {
                status = STATUS_DEVICE_NOT_READY;
            }
        }
        if (NT_SUCCESS(status)) {
            qpc = KeQueryPerformanceCounter(NULL);
            firstRun = OpenA8DJ_StartRtPacketClockIfNeeded(
                streamContext,
                (ULONGLONG)qpc.QuadPart);
            if (firstRun) {
                streamContext->RenderPrefillFramesRemaining =
                    streamContext->IsRender ? OPENA8DJ_RT_RENDER_PREFILL_FRAMES : 0u;
            }
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
            status = STATUS_SUCCESS;
        }
        if (purgeLockHeld) {
            WdfWaitLockRelease(deviceContext->IsoPurgeLock);
        }
    } else {
        status = STATUS_INVALID_DEVICE_STATE;
    }
    OpenA8DJ_RecordAcxStage(412, status);
    return status;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamPause(
    _In_ ACXSTREAM Stream)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext = OpenA8DJGetAcxStreamContext(Stream);
    LARGE_INTEGER timeout;
    NTSTATUS status = STATUS_SUCCESS;
    KIRQL oldIrql;

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
        KeAcquireSpinLock(&streamContext->RtPacketStateLock, &oldIrql);
        streamContext->RtClockRunning = FALSE;
        KeReleaseSpinLock(&streamContext->RtPacketStateLock, oldIrql);
        if (!OpenA8DJ_HasActiveAcxStreams(streamContext->DeviceContext)) {
            InterlockedExchange(&streamContext->DeviceContext->StreamStopRequested, 1);
        }
        timeout.QuadPart = -2LL * 10LL * 1000LL * 1000LL;
        status = KeWaitForSingleObject(
            &streamContext->WorkerIdleEvent,
            Executive,
            KernelMode,
            FALSE,
            &timeout);
        if (status == STATUS_TIMEOUT) {
            status = STATUS_IO_TIMEOUT;
        }
    } else {
        status = STATUS_INVALID_DEVICE_STATE;
    }
    OpenA8DJ_RecordAcxStage(413, status);
    return status;
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
    /*
     * The explicit USB StartFrame lead is presentation delay, not bytes in a
     * hardware FIFO.  Reporting it as FIFO storage can exceed a 512-frame RT
     * packet and causes WASAPI exclusive initialization to reject the pin.
     */
    *FifoSize = 0;
    *Delay = streamContext->IsRender
        ? OPENA8DJ_HW_LATENCY_MILLISECONDS * 10000u
        : 0u;
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
    LARGE_INTEGER qpc;

    *Packets = NULL;
    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxAllocatePacketCallbacks);
    }
    if (PacketCount == 0 || PacketCount > 2 || PacketSize == 0 ||
        streamContext->RtBlockAlign == 0 ||
        (PacketSize % streamContext->RtBlockAlign) != 0) {
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
    if (rawBytes > MAXULONG - (PAGE_SIZE - 1u)) {
        return STATUS_INTEGER_OVERFLOW;
    }
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
    qpc = KeQueryPerformanceCounter(NULL);
    OpenA8DJ_ResetRtPacketEpoch(streamContext, (ULONGLONG)qpc.QuadPart, TRUE);
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
            OpenA8DJ_PurgeIsoTargets(streamContext->DeviceContext, TRUE);
            if (streamContext->DeviceContext->StreamWorkItem != NULL) {
                WdfWorkItemFlush(streamContext->DeviceContext->StreamWorkItem);
            }
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
    KIRQL oldIrql;
    ULONG currentPacket;
    NTSTATUS status = STATUS_SUCCESS;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxSetRenderPacketCallbacks);
        streamContext->DeviceContext->AcxLastSetRenderPacket = Packet;
        streamContext->DeviceContext->AcxLastSetRenderFlags = Flags;
        streamContext->DeviceContext->AcxLastSetRenderEosPacketLength = EosPacketLength;
    }

    if (!streamContext->IsRender ||
        (Flags & ~KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM) != 0 ||
        ((Flags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM) != 0 &&
         (EosPacketLength > streamContext->RtPacketSize ||
          streamContext->RtBlockAlign == 0 ||
          (EosPacketLength % streamContext->RtBlockAlign) != 0))) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&streamContext->RtPacketStateLock, &oldIrql);
    currentPacket = streamContext->CurrentPacket;
    if (streamContext->RenderEosReceived) {
        status = STATUS_INVALID_DEVICE_STATE;
    } else if (Packet <= currentPacket) {
        status = STATUS_DATA_LATE_ERROR;
    } else if ((Packet - currentPacket) > 1u) {
        status = STATUS_DATA_OVERRUN;
    } else if ((Flags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM) != 0) {
        streamContext->RenderEosReceived = TRUE;
    }
    KeReleaseSpinLock(&streamContext->RtPacketStateLock, oldIrql);
    return status;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamGetCurrentPacket(
    _In_ ACXSTREAM Stream,
    _Out_ PULONG CurrentPacket)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;
    KIRQL oldIrql;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxGetCurrentPacketCallbacks);
    }
    KeAcquireSpinLock(&streamContext->RtPacketStateLock, &oldIrql);
    *CurrentPacket = streamContext->CurrentPacket;
    KeReleaseSpinLock(&streamContext->RtPacketStateLock, oldIrql);
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
    KIRQL oldIrql;
    BOOLEAN packetValid;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxGetCapturePacketCallbacks);
    }
    KeAcquireSpinLock(&streamContext->RtPacketStateLock, &oldIrql);
    packetValid = streamContext->CapturePacketValid;
    *LastCapturePacket = streamContext->LastCompletedPacket;
    *QPCPacketStart = (ULONGLONG)streamContext->LastPacketStartQpc;
    KeReleaseSpinLock(&streamContext->RtPacketStateLock, oldIrql);
    *MoreData = FALSE;
    return packetValid ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}

static NTSTATUS
NTAPI
OpenA8DJ_EvtAcxStreamGetPresentationPosition(
    _In_ ACXSTREAM Stream,
    _Out_ PULONGLONG PositionInBlocks,
    _Out_ PULONGLONG QPCPosition)
{
    POPENA8DJ_ACX_STREAM_CONTEXT streamContext;
    KIRQL oldIrql;
    LARGE_INTEGER frequency;
    LARGE_INTEGER currentQpc;
    ULONGLONG positionBlocks;
    ULONGLONG snapshotQpc;
    ULONGLONG deltaQpc;
    ULONGLONG deltaFrames;
    ULONGLONG qpcFrequency;
    ULONG sampleRate;
    BOOLEAN clockRunning;

    streamContext = OpenA8DJGetAcxStreamContext(Stream);
    if (streamContext->DeviceContext != NULL) {
        InterlockedIncrement64(&streamContext->DeviceContext->AcxGetPresentationPositionCallbacks);
    }
    currentQpc = KeQueryPerformanceCounter(&frequency);
    KeAcquireSpinLock(&streamContext->RtPacketStateLock, &oldIrql);
    positionBlocks = streamContext->PositionBlocks;
    snapshotQpc = streamContext->PositionQpc;
    clockRunning = streamContext->RtClockRunning;
    sampleRate = streamContext->RtSampleRate;
    KeReleaseSpinLock(&streamContext->RtPacketStateLock, oldIrql);

    qpcFrequency = (ULONGLONG)frequency.QuadPart;
    if (clockRunning && sampleRate != 0 && qpcFrequency != 0 &&
        (ULONGLONG)currentQpc.QuadPart > snapshotQpc) {
        deltaQpc = (ULONGLONG)currentQpc.QuadPart - snapshotQpc;
        /* Do not extrapolate indefinitely if the USB completion clock stalls. */
        if (deltaQpc > qpcFrequency / 10u) {
            deltaQpc = qpcFrequency / 10u;
        }
        deltaFrames = (deltaQpc / qpcFrequency) * sampleRate;
        deltaFrames += ((deltaQpc % qpcFrequency) * sampleRate) / qpcFrequency;
        positionBlocks += deltaFrames;
    }
    KeAcquireSpinLock(&streamContext->RtPacketStateLock, &oldIrql);
    if (positionBlocks < streamContext->LastPresentationBlocks) {
        positionBlocks = streamContext->LastPresentationBlocks;
    } else {
        streamContext->LastPresentationBlocks = positionBlocks;
    }
    KeReleaseSpinLock(&streamContext->RtPacketStateLock, oldIrql);
    *PositionInBlocks = positionBlocks;
    *QPCPosition = (ULONGLONG)currentQpc.QuadPart;
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
    KeInitializeSpinLock(&streamContext->RtPacketStateLock);
    streamContext->DeviceContext = deviceContext;
    streamContext->Stream = stream;
    streamContext->IsRender = isRender;
    streamContext->PairIndex = pairIndex;
    ExInitializeRundownProtection(&streamContext->WorkerRundown);
    streamContext->WorkerRundownCompleted = FALSE;
    streamContext->WorkerReferences = 0;
    KeInitializeEvent(&streamContext->WorkerIdleEvent, NotificationEvent, TRUE);
    streamContext->RtChannels = AcxDataFormatGetChannelsCount(StreamFormat);
    streamContext->RtBlockAlign = AcxDataFormatGetBlockAlign(StreamFormat);
    streamContext->RtBitsPerSample = AcxDataFormatGetBitsPerSample(StreamFormat);
    streamContext->RtSampleRate = OPENA8DJ_DEFAULT_SAMPLE_RATE;
    streamContext->RtIsFloat = FALSE;
    {
        WAVEFORMATEX *waveFormat = (WAVEFORMATEX *)AcxDataFormatGetWaveFormatEx(StreamFormat);
        if (waveFormat != NULL) {
            streamContext->RtChannels = waveFormat->nChannels;
            streamContext->RtBlockAlign = waveFormat->nBlockAlign;
            streamContext->RtBitsPerSample = waveFormat->wBitsPerSample;
            streamContext->RtSampleRate = waveFormat->nSamplesPerSec;
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
    if (streamContext->RtSampleRate == 0) {
        streamContext->RtSampleRate = OPENA8DJ_DEFAULT_SAMPLE_RATE;
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

    #pragma warning(suppress: 6001) /* ACX supplies a writable UNICODE_STRING descriptor. */
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
    pinConfig.Communication = Render ?
                              AcxPinCommunicationSink :
                              AcxPinCommunicationSource;
    pinConfig.Category = &kOpenA8DJKsCategoryAudio;
    status = AcxPinCreate(circuit, &attributes, &pinConfig, &pins[0]);
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RecordAcxStage(Render ? 105 : 205, status);
        return status;
    }
    if (pins[0] == NULL) {
        return STATUS_INVALID_DEVICE_STATE;
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

static NTSTATUS
OpenA8DJ_RemoveAcxCircuits(
    _In_ WDFDEVICE Device,
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS firstStatus = STATUS_SUCCESS;
    NTSTATUS status;
    ULONG pairIndex;
    BOOLEAN circuitsRemain = FALSE;

    if (!Context->AcxCircuitsAdded) {
        return STATUS_SUCCESS;
    }

    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (Context->RenderCircuitAdded[pairIndex] &&
            Context->RenderCircuits[pairIndex] != NULL) {
            OpenA8DJ_RecordAcxStage(305, STATUS_SUCCESS);
            status = AcxDeviceRemoveCircuit(Device, Context->RenderCircuits[pairIndex]);
            if (!NT_SUCCESS(status)) {
                OpenA8DJ_RecordAcxStage(306, status);
                circuitsRemain = TRUE;
                if (NT_SUCCESS(firstStatus)) {
                    firstStatus = status;
                }
            } else {
                Context->RenderCircuitAdded[pairIndex] = FALSE;
            }
        }
        if (Context->CaptureCircuitAdded[pairIndex] &&
            Context->CaptureCircuits[pairIndex] != NULL) {
            OpenA8DJ_RecordAcxStage(307, STATUS_SUCCESS);
            status = AcxDeviceRemoveCircuit(Device, Context->CaptureCircuits[pairIndex]);
            if (!NT_SUCCESS(status)) {
                OpenA8DJ_RecordAcxStage(308, status);
                circuitsRemain = TRUE;
                if (NT_SUCCESS(firstStatus)) {
                    firstStatus = status;
                }
            } else {
                Context->CaptureCircuitAdded[pairIndex] = FALSE;
            }
        }
    }

    Context->AcxCircuitsAdded = circuitsRemain;
    OpenA8DJ_RecordAcxStage(309, firstStatus);
    return firstStatus;
}

static NTSTATUS
OpenA8DJ_AddAcxCircuits(
    _In_ WDFDEVICE Device,
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN renderAdded[OPENA8DJ_STEREO_PAIRS];
    BOOLEAN captureAdded[OPENA8DJ_STEREO_PAIRS];
    ULONG pairIndex;

    if (Context->AcxCircuitsAdded) {
        return STATUS_SUCCESS;
    }

    RtlZeroMemory(renderAdded, sizeof(renderAdded));
    RtlZeroMemory(captureAdded, sizeof(captureAdded));
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (Context->RenderCircuits[pairIndex] != NULL) {
            OpenA8DJ_RecordAcxStage(300, STATUS_SUCCESS);
            status = AcxDeviceAddCircuit(Device, Context->RenderCircuits[pairIndex]);
            if (!NT_SUCCESS(status)) {
                OpenA8DJ_RecordAcxStage(301, status);
                goto rollback;
            }
            renderAdded[pairIndex] = TRUE;
            Context->RenderCircuitAdded[pairIndex] = TRUE;
        }
        if (Context->CaptureCircuits[pairIndex] != NULL) {
            OpenA8DJ_RecordAcxStage(302, STATUS_SUCCESS);
            status = AcxDeviceAddCircuit(Device, Context->CaptureCircuits[pairIndex]);
            if (!NT_SUCCESS(status)) {
                OpenA8DJ_RecordAcxStage(303, status);
                goto rollback;
            }
            captureAdded[pairIndex] = TRUE;
            Context->CaptureCircuitAdded[pairIndex] = TRUE;
        }
    }

    Context->AcxCircuitsAdded = TRUE;
    OpenA8DJ_RecordAcxStage(304, STATUS_SUCCESS);
    return STATUS_SUCCESS;

rollback:
    {
        NTSTATUS failureStatus = status;
        BOOLEAN circuitsRemain = FALSE;

    for (pairIndex = OPENA8DJ_STEREO_PAIRS; pairIndex > 0; pairIndex--) {
        ULONG index = pairIndex - 1u;

        if (captureAdded[index]) {
            status = AcxDeviceRemoveCircuit(Device, Context->CaptureCircuits[index]);
            if (NT_SUCCESS(status)) {
                Context->CaptureCircuitAdded[index] = FALSE;
            } else {
                circuitsRemain = TRUE;
            }
        }
        if (renderAdded[index]) {
            status = AcxDeviceRemoveCircuit(Device, Context->RenderCircuits[index]);
            if (NT_SUCCESS(status)) {
                Context->RenderCircuitAdded[index] = FALSE;
            } else {
                circuitsRemain = TRUE;
            }
        }
    }
    Context->AcxCircuitsAdded = circuitsRemain;
    status = failureStatus;
    }
    return status;
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
    if (InterlockedCompareExchange(&Context->StreamWorkerActive, 0, 0) != 0 ||
        Context->StreamState.Streaming) {
        return STATUS_DEVICE_BUSY;
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

    if (((state[3] ^ Context->RawControlState[3]) & OPENA8DJ_CONTROL_FLAGS_MASK) != 0) {
        RtlCopyMemory(Context->LastControlWriteRequest, state, sizeof(Context->LastControlWriteRequest));
        RtlCopyMemory(Context->LastControlWriteReadBack, Context->RawControlState, sizeof(Context->LastControlWriteReadBack));
        Context->LastControlWriteMismatch = TRUE;
        Context->LastControlWriteStatus = STATUS_NOT_SUPPORTED;
        Context->LastControlReadbackStatus = STATUS_SUCCESS;
        return STATUS_NOT_SUPPORTED;
    }

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

    status = OpenA8DJ_RefreshHardwareControlState(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }
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
    BOOLEAN transportReady;
    BOOLEAN endpointReady;
    ULONG index;

    transportReady = OpenA8DJ_IsTransportReady(Context);
    endpointReady = transportReady && Context->AcxCircuitsAdded;

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
    Capabilities->WindowsAudioEndpointExposed = endpointReady;
#if OPENA8DJ_VIRTUAL_MODE
    Capabilities->UsbTransportReady = FALSE;
#else
    Capabilities->UsbTransportReady = transportReady;
#endif
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
    BOOLEAN transportReady;
    BOOLEAN endpointReady;

    transportReady = OpenA8DJ_IsTransportReady(Context);
    endpointReady = transportReady && Context->AcxCircuitsAdded;

    RtlZeroMemory(Surface, sizeof(*Surface));
    Surface->Size = sizeof(*Surface);
    Surface->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
    Surface->SurfaceFlags = OPENA8DJ_SURFACE_FLAG_EXPERIMENTAL;
    Surface->StableSampleRateFlags = OPENA8DJ_RATE_FLAG_44100 | OPENA8DJ_RATE_FLAG_48000;
    Surface->PlannedSampleRateFlags = OPENA8DJ_RATE_FLAG_88200 | OPENA8DJ_RATE_FLAG_96000;
    Surface->AudioEndpointState = endpointReady ?
                                  OPENA8DJ_COMPONENT_READY :
                                  OPENA8DJ_COMPONENT_PLANNED;
#if OPENA8DJ_VIRTUAL_MODE
    Surface->UsbTransportState = OPENA8DJ_COMPONENT_STUB;
#else
    Surface->UsbTransportState = transportReady ?
                                 OPENA8DJ_COMPONENT_READY : OPENA8DJ_COMPONENT_STUB;
#endif
    Surface->IsochronousEngineState = (transportReady &&
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
    LARGE_INTEGER qpcFrequency;
    LONG errorSequenceAfter;
    LONG errorSequenceBefore;
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
    Diagnostics->AcxRtRenderPacketCompletions =
        (ULONG64)Context->AcxRtRenderPacketCompletions;
    Diagnostics->AcxRtRenderPacketNotifications =
        (ULONG64)Context->AcxRtRenderPacketNotifications;
    Diagnostics->AcxRtRenderPacketNotificationFailures =
        (ULONG64)Context->AcxRtRenderPacketNotificationFailures;
    Diagnostics->AcxRtRenderCurrentPacket = Context->AcxRtRenderCurrentPacket;
    Diagnostics->AcxRtRenderLastNotificationNtStatus =
        Context->AcxRtRenderLastNotificationNtStatus;
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
    (VOID)KeQueryPerformanceCounter(&qpcFrequency);
    Diagnostics->PersistentIsoPacketCount = OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
    Diagnostics->PersistentIsoSlotCount = OPENA8DJ_ISO_ENGINE_SLOT_COUNT;
    Diagnostics->PersistentIsoOutputLeadFrames = OPENA8DJ_ISO_OUTPUT_LEAD_FRAMES;
    Diagnostics->IsoQpcFrequency = (ULONG64)qpcFrequency.QuadPart;
    Diagnostics->IsoOutputQueueEmptyTransitions =
        (ULONG64)Context->IsoOutputQueueEmptyTransitions;
    Diagnostics->IsoOutputLatePackets = (ULONG64)Context->IsoOutputLatePackets;
    Diagnostics->IsoOutputBadStartFrames = (ULONG64)Context->IsoOutputBadStartFrames;
    Diagnostics->IsoOutputOtherPacketErrors =
        (ULONG64)Context->IsoOutputOtherPacketErrors;
    Diagnostics->IsoOutputPanicFlags = (ULONG64)Context->IsoOutputPanicFlags;
    Diagnostics->IsoCaptureLatePackets = (ULONG64)Context->IsoCaptureLatePackets;
    Diagnostics->IsoCaptureBadStartFrames =
        (ULONG64)Context->IsoCaptureBadStartFrames;
    Diagnostics->IsoCaptureOtherPacketErrors =
        (ULONG64)Context->IsoCaptureOtherPacketErrors;
    Diagnostics->IsoCaptureToOutputSubmitMaxQpc =
        (ULONG64)Context->IsoCaptureToOutputSubmitMaxQpc;
    Diagnostics->IsoLastCaptureStartFrame = Context->IsoLastCaptureStartFrame;
    Diagnostics->IsoLastOutputStartFrame = Context->IsoLastOutputStartFrame;
    Diagnostics->AudioRateSettleRuns = (ULONG64)Context->AudioRateSettleRuns;
    Diagnostics->AudioRateSettleSnapshots = (ULONG64)Context->AudioRateSettleSnapshots;
    Diagnostics->AudioRateSettleMismatchedPackets =
        (ULONG64)Context->AudioRateSettleMismatchedPackets;
    Diagnostics->AudioRateSettleFailures = (ULONG64)Context->AudioRateSettleFailures;
    Diagnostics->AudioRateSettleLastObservedBytes =
        (ULONG)InterlockedCompareExchange(
            &Context->AudioRateSettleLastObservedBytes,
            0,
            0);
    Diagnostics->IsoCaptureFrameQueries =
        (ULONG64)Context->IsoCaptureFrameQueries;
    Diagnostics->IsoCaptureFrameQueryFailures =
        (ULONG64)Context->IsoCaptureFrameQueryFailures;
    Diagnostics->IsoCaptureFrameQueryCurrent = Context->IsoCaptureFrameQueryCurrent;
    Diagnostics->IsoCaptureSeedStartFrame = Context->IsoCaptureSeedStartFrame;
    Diagnostics->IsoCaptureNextStartFrame = Context->IsoNextCaptureStartFrame;
    Diagnostics->IsoInputPipeResetRuns =
        (ULONG64)Context->IsoInputPipeResetRuns;
    Diagnostics->IsoInputPipeResetFailures =
        (ULONG64)Context->IsoInputPipeResetFailures;
    Diagnostics->IsoInputPipeResetLastNtStatus =
        (ULONG)InterlockedCompareExchange(
            &Context->IsoInputPipeResetLastNtStatus,
            0,
            0);
    Diagnostics->IsoOutputPipeResetRuns =
        (ULONG64)Context->IsoOutputPipeResetRuns;
    Diagnostics->IsoOutputPipeResetFailures =
        (ULONG64)Context->IsoOutputPipeResetFailures;
    Diagnostics->IsoOutputPipeResetLastNtStatus =
        (ULONG)InterlockedCompareExchange(
            &Context->IsoOutputPipeResetLastNtStatus,
            0,
            0);
    Diagnostics->IsoOneShotActive =
        (ULONG)InterlockedCompareExchange(&Context->IsoOneShotActive, 0, 0);
    Diagnostics->IsoTransportHealthy =
        (ULONG)InterlockedCompareExchange(&Context->IsoTransportHealthy, 0, 0);
    Diagnostics->IsoTransportHealthyGeneration =
        InterlockedCompareExchange(&Context->IsoTransportHealthyGeneration, 0, 0);
    Diagnostics->IsoInputTargetStartLastNtStatus =
        (ULONG)InterlockedCompareExchange(
            &Context->IsoInputTargetStartLastNtStatus,
            0,
            0);
    Diagnostics->IsoOutputTargetStartLastNtStatus =
        (ULONG)InterlockedCompareExchange(
            &Context->IsoOutputTargetStartLastNtStatus,
            0,
            0);
    for (;;) {
        errorSequenceBefore = InterlockedCompareExchange(
            &Context->IsoCaptureErrorSnapshotSequence,
            0,
            0);
        if ((errorSequenceBefore & 1) != 0) {
            continue;
        }
        KeMemoryBarrier();
        Diagnostics->IsoLastCaptureUrbStatus =
            (ULONG)Context->IsoLastCaptureUrbStatus;
        Diagnostics->IsoLastCapturePacketStatus =
            (ULONG)Context->IsoLastCapturePacketStatus;
        Diagnostics->IsoLastCaptureErrorCount =
            (ULONG)Context->IsoLastCaptureErrorCount;
        Diagnostics->IsoLastCaptureErrorSlot = Context->IsoLastCaptureErrorSlot;
        Diagnostics->IsoLastCaptureErrorGeneration =
            Context->IsoLastCaptureErrorGeneration;
        Diagnostics->IsoLastCaptureErrorSubmitSequence =
            (ULONG64)Context->IsoLastCaptureErrorSubmitSequence;
        Diagnostics->IsoLastCaptureErrorScheduledStartFrame =
            Context->IsoLastCaptureErrorScheduledStartFrame;
        Diagnostics->IsoLastCaptureErrorFirstPacket =
            Context->IsoLastCaptureErrorFirstPacket;
        Diagnostics->IsoLastCaptureErrorLastPacket =
            Context->IsoLastCaptureErrorLastPacket;
        Diagnostics->IsoLastCaptureErrorPacketCount =
            Context->IsoLastCaptureErrorPacketCount;
        Diagnostics->IsoLastCaptureErrorSubmitQpc =
            (ULONG64)Context->IsoLastCaptureErrorSubmitQpc;
        Diagnostics->IsoLastCaptureErrorCompletionQpc =
            (ULONG64)Context->IsoLastCaptureErrorCompletionQpc;
        KeMemoryBarrier();
        errorSequenceAfter = InterlockedCompareExchange(
            &Context->IsoCaptureErrorSnapshotSequence,
            0,
            0);
        if (errorSequenceBefore == errorSequenceAfter &&
            (errorSequenceAfter & 1) == 0) {
            break;
        }
    }
    Diagnostics->IsoCaptureErrorSnapshotSequence = (ULONG)errorSequenceAfter;
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

typedef struct _OPENA8DJ_ISO_REQUEST_COMPLETION {
    KEVENT Event;
    PURB Urb;
    NTSTATUS Status;
    ULONG ErrorCount;
} OPENA8DJ_ISO_REQUEST_COMPLETION, *POPENA8DJ_ISO_REQUEST_COMPLETION;

static VOID
NTAPI
OpenA8DJ_EvtIsoRequestComplete(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT CompletionContext)
{
    POPENA8DJ_ISO_REQUEST_COMPLETION completion =
        (POPENA8DJ_ISO_REQUEST_COMPLETION)CompletionContext;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    if (completion != NULL) {
        completion->Status = Params->IoStatus.Status;
        completion->ErrorCount =
            completion->Urb != NULL ?
            completion->Urb->UrbIsochronousTransfer.ErrorCount : 1u;
        KeSetEvent(&completion->Event, IO_NO_INCREMENT, FALSE);
    }
}

static NTSTATUS
OpenA8DJ_SendIsoUrbWithRequest(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ WDFUSBPIPE Pipe,
    _In_ WDFREQUEST Request,
    _In_ WDFMEMORY UrbMemory,
    _In_ PURB Urb,
    _Out_opt_ PULONG ErrorCount)
{
    NTSTATUS status;
    NTSTATUS waitStatus;
    LARGE_INTEGER timeout;
    OPENA8DJ_ISO_REQUEST_COMPLETION completion;

    if (ErrorCount != NULL) {
        *ErrorCount = 0;
    }
    if (Pipe == NULL || Request == NULL || UrbMemory == NULL || Urb == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&completion, sizeof(completion));
    KeInitializeEvent(&completion.Event, NotificationEvent, FALSE);
    completion.Urb = Urb;
    completion.Status = STATUS_PENDING;

    status = WdfUsbTargetPipeFormatRequestForUrb(
        Pipe,
        Request,
        UrbMemory,
        NULL);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        Pipe == Context->IsoInPipe ?
            OPENA8DJ_CHECKPOINT_ISO_CAPTURE_FORMATTED :
            OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SUBMIT,
        status);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    WdfRequestSetCompletionRoutine(
        Request,
        OpenA8DJ_EvtIsoRequestComplete,
        &completion);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        Pipe == Context->IsoInPipe ?
            OPENA8DJ_CHECKPOINT_ISO_CAPTURE_SUBMIT :
            OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SUBMIT,
        STATUS_PENDING);
    if (!WdfRequestSend(
            Request,
            WdfUsbTargetPipeGetIoTarget(Pipe),
            WDF_NO_SEND_OPTIONS)) {
        status = WdfRequestGetStatus(Request);
        return status;
    }

    timeout.QuadPart = WDF_REL_TIMEOUT_IN_SEC(1);
    waitStatus = KeWaitForSingleObject(
        &completion.Event,
        Executive,
        KernelMode,
        FALSE,
        &timeout);
    if (waitStatus != STATUS_SUCCESS) {
        /*
         * The explicit request is now cancellable.  Irrespective of whether
         * cancellation raced with completion, wait for the completion event
         * before deleting the request or returning to a caller that may
         * release a temporary transfer buffer.
         */
        (VOID)WdfRequestCancelSentRequest(Request);
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_TIMEOUT,
            STATUS_IO_TIMEOUT);
        /*
         * PurgeIoAndWait is the ownership barrier: it does not return until
         * every delivered request has completed or been cancelled.  The URB
         * and its buffer remain parent-owned and are never destroyed here.
         */
        WdfIoTargetPurge(
            WdfUsbTargetPipeGetIoTarget(Pipe),
            WdfIoTargetPurgeIoAndWait);
        (VOID)KeWaitForSingleObject(
            &completion.Event,
            Executive,
            KernelMode,
            FALSE,
            NULL);
    }

    status = completion.Status;
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        Pipe == Context->IsoInPipe ?
            OPENA8DJ_CHECKPOINT_ISO_CAPTURE_COMPLETE :
            OPENA8DJ_CHECKPOINT_ISO_OUTPUT_COMPLETE,
        status);
    if (ErrorCount != NULL) {
        *ErrorCount = completion.ErrorCount;
    }
    return status;
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
    WDFREQUEST request = NULL;
    WDFMEMORY urbMemory;
    PURB urb;
    PVOID transferBuffer;
    ULONG packetIndex;
    ULONG maximumPacketSize;
    ULONG transferBufferLength;
    WDF_USB_PIPE_INFORMATION pipeInfo;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    NTSTATUS reuseStatus;

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

    status = OpenA8DJ_EnsureIsoTransportResources(Context);
    if (!NT_SUCCESS(status)) {
        Snapshot->NtStatus = (ULONG)status;
        return status;
    }
    status = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(status)) {
        Snapshot->NtStatus = (ULONG)status;
        return status;
    }

    /* Do not use or resurrect snapshot resources once PnP teardown has begun. */
    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
        status = STATUS_DEVICE_NOT_READY;
        WdfWaitLockRelease(Context->IsoTransportLock);
        Snapshot->NtStatus = (ULONG)status;
        return status;
    }

    request = Context->IsoCaptureRequest;
    urbMemory = Context->IsoCaptureUrbMemory;
    urb = Context->IsoCaptureUrb;
    transferBuffer = Context->StreamCaptureBuffer;
    status = request != NULL && urbMemory != NULL && urb != NULL && transferBuffer != NULL ?
        STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_CAPTURE_REQUEST_CREATED,
        status);
    if (!NT_SUCCESS(status)) {
        WdfWaitLockRelease(Context->IsoTransportLock);
        Snapshot->NtStatus = (ULONG)status;
        return status;
    }
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_CAPTURE_BUFFER_CREATED,
        STATUS_SUCCESS);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_CAPTURE_URB_CREATED,
        STATUS_SUCCESS);

    /* Plain legacy URB memory has no hidden XRB/FxUsbUrb state. */
    RtlZeroMemory(urb, GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT));
    RtlZeroMemory(transferBuffer, transferBufferLength);
    urb->UrbIsochronousTransfer.Hdr.Length =
        (USHORT)GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT);
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
        /* Length is an OUT field for isochronous IN and must start at zero. */
        urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = 0;
        urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status = USBD_STATUS_SUCCESS;
    }

    status = OpenA8DJ_SendIsoUrbWithRequest(
        Context,
        Context->IsoInPipe,
        request,
        urbMemory,
        urb,
        &Snapshot->ErrorCount);

    Snapshot->NtStatus = (ULONG)status;
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

    if (!NT_SUCCESS(status)) {
        (VOID)OpenA8DJ_AbortIsoInputPipe(Context);
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: cancelled failed isochronous input request\n"));
    } else {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_TRACE_LEVEL,
                   "OpenA8DJUsb: completed isochronous input request\n"));
    }
    /*
     * Formatting takes an additional target-side reference on UrbMemory.
     * WdfRequestReuse drops that reference and leaves the persistent slot idle.
     */
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_CAPTURE_RECLAIM_START,
        status);
    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        status);
    reuseStatus = WdfRequestReuse(request, &reuseParams);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_CAPTURE_RECLAIM_COMPLETE,
        reuseStatus);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_CAPTURE_SLOT_IDLE,
        reuseStatus);
    if (!NT_SUCCESS(reuseStatus)) {
        Snapshot->NtStatus = (ULONG)reuseStatus;
        WdfWaitLockRelease(Context->IsoTransportLock);
        return reuseStatus;
    }
    WdfWaitLockRelease(Context->IsoTransportLock);
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
OpenA8DJ_AdvanceRtFrames(
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext,
    _In_ ULONG FrameCount,
    _In_ ULONGLONG CompletionQpc,
    _In_ LONG ExpectedEpoch)
{
    ULONG framesPerPacket;
    ULONG completedPacket = 0;
    ULONG completedCount = 0;
    ULONGLONG totalProgress;
    KIRQL oldIrql;

    if (FrameCount == 0 ||
        StreamContext->RtPacketCount == 0 ||
        StreamContext->RtPacketSize == 0 ||
        StreamContext->RtBlockAlign == 0) {
        return;
    }

    framesPerPacket = StreamContext->RtPacketSize / StreamContext->RtBlockAlign;
    if (framesPerPacket == 0) {
        return;
    }

    KeAcquireSpinLock(&StreamContext->RtPacketStateLock, &oldIrql);
    if (InterlockedCompareExchange(&StreamContext->RtPacketEpochActive, 0, 0) == 0 ||
        InterlockedCompareExchange(&StreamContext->RtPacketEpoch, 0, 0) != ExpectedEpoch) {
        KeReleaseSpinLock(&StreamContext->RtPacketStateLock, oldIrql);
        return;
    }

    StreamContext->PositionBlocks += FrameCount;
    StreamContext->PositionQpc = CompletionQpc;
    totalProgress = (ULONGLONG)StreamContext->RtPacketFrameProgress + FrameCount;
    while (totalProgress >= framesPerPacket) {
        totalProgress -= framesPerPacket;
        completedPacket = StreamContext->CurrentPacket;
        StreamContext->LastCompletedPacket = completedPacket;
        StreamContext->LastPacketStartQpc = StreamContext->CurrentPacketStartQpc;
        StreamContext->CapturePacketValid = TRUE;
        StreamContext->CurrentPacket++;
        StreamContext->CurrentPacketStartQpc = (LONG64)CompletionQpc;
        completedCount++;
    }
    StreamContext->RtPacketFrameProgress = (ULONG)totalProgress;

    if (completedCount != 0 && StreamContext->Stream != NULL) {
        StreamContext->PendingCompletedPacket = completedPacket;
        StreamContext->PendingCompletionQpc = CompletionQpc;
        StreamContext->PendingNotificationEpoch = ExpectedEpoch;
        StreamContext->RtPacketNotificationPending = 1;
    }
    KeReleaseSpinLock(&StreamContext->RtPacketStateLock, oldIrql);

    if (completedCount != 0 &&
        StreamContext->IsRender &&
        StreamContext->DeviceContext != NULL) {
        InterlockedAdd64(
            &StreamContext->DeviceContext->AcxRtRenderPacketCompletions,
            completedCount);
        StreamContext->DeviceContext->AcxRtRenderCurrentPacket =
            completedPacket + 1u;
    }
}

static VOID
OpenA8DJ_ReleaseOutputSlotRenderStreams(
    _Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot,
    _In_ BOOLEAN TransferSucceeded)
{
    ULONG pairIndex;

    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
            (POPENA8DJ_ACX_STREAM_CONTEXT)Slot->RenderStreams[pairIndex];

        if (streamContext != NULL) {
            if (TransferSucceeded &&
                (Slot->RenderMask & (1u << pairIndex)) != 0 &&
                Slot->RenderFrameCount[pairIndex] != 0) {
                OpenA8DJ_AdvanceRtFrames(
                    streamContext,
                    Slot->RenderFrameCount[pairIndex],
                    (ULONGLONG)Slot->CompletionQpc,
                    Slot->RenderEpoch[pairIndex]);
            }
            OpenA8DJ_ReleaseActiveStream(streamContext);
            Slot->RenderStreams[pairIndex] = NULL;
        }
        Slot->RenderFrameCount[pairIndex] = 0;
        Slot->RenderEpoch[pairIndex] = 0;
    }
    Slot->RenderMask = 0;
}

static VOID
OpenA8DJ_FlushRtPacketNotification(
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext)
{
    NTSTATUS status;
    ULONG completedPacket = 0;
    ULONGLONG completionQpc = 0;
    KIRQL oldIrql;
    BOOLEAN notify = FALSE;

    KeAcquireSpinLock(&StreamContext->RtPacketStateLock, &oldIrql);
    if (StreamContext->RtPacketNotificationPending != 0 &&
        StreamContext->Stream != NULL &&
        StreamContext->RtPacketEpochActive != 0 &&
        StreamContext->PendingNotificationEpoch == StreamContext->RtPacketEpoch) {
        completedPacket = StreamContext->PendingCompletedPacket;
        completionQpc = StreamContext->PendingCompletionQpc;
        notify = TRUE;
    }
    StreamContext->RtPacketNotificationPending = 0;
    KeReleaseSpinLock(&StreamContext->RtPacketStateLock, oldIrql);

    if (!notify) {
        return;
    }
    status = AcxRtStreamNotifyPacketComplete(
        StreamContext->Stream,
        completedPacket,
        completionQpc);

    if (StreamContext->IsRender && StreamContext->DeviceContext != NULL) {
        InterlockedIncrement64(
            &StreamContext->DeviceContext->AcxRtRenderPacketNotifications);
        StreamContext->DeviceContext->AcxRtRenderLastNotificationNtStatus =
            (ULONG)status;
        if (!NT_SUCCESS(status)) {
            InterlockedIncrement64(
                &StreamContext->DeviceContext->AcxRtRenderPacketNotificationFailures);
        }
    }
}

static VOID
OpenA8DJ_FlushActiveRtPacketNotifications(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    ULONG pairIndex;

    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        POPENA8DJ_ACX_STREAM_CONTEXT renderStream =
            OpenA8DJ_AcquireActiveStream(
                Context,
                &Context->ActiveRenderStreams[pairIndex]);
        POPENA8DJ_ACX_STREAM_CONTEXT captureStream =
            OpenA8DJ_AcquireActiveStream(
                Context,
                &Context->ActiveCaptureStreams[pairIndex]);

        if (renderStream != NULL) {
            OpenA8DJ_FlushRtPacketNotification(renderStream);
            OpenA8DJ_ReleaseActiveStream(renderStream);
        }
        if (captureStream != NULL) {
            OpenA8DJ_FlushRtPacketNotification(captureStream);
            OpenA8DJ_ReleaseActiveStream(captureStream);
        }
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
}

static ULONG
OpenA8DJ_FillRtCaptureFromMode2(
    _In_reads_bytes_(Length) const UCHAR *Buffer,
    _In_ ULONG Length,
    _Inout_ POPENA8DJ_ACX_STREAM_CONTEXT StreamContext,
    _In_ ULONGLONG CompletionQpc)
{
    ULONG index = 0;
    ULONG framesWritten = 0;
    LONG packetEpoch = InterlockedCompareExchange(&StreamContext->RtPacketEpoch, 0, 0);

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

    OpenA8DJ_AdvanceRtFrames(
        StreamContext,
        framesWritten,
        CompletionQpc,
        packetEpoch);
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
        /* The zero prefill is transport latency, not decoded stream data. */
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
        if (StreamContext->DeviceContext != NULL) {
            OpenA8DJ_RecordRenderTraceFrame(
                StreamContext->DeviceContext,
                StreamContext,
                rtFrameCursor,
                rawSamples[0][0],
                rawSamples[0][1],
                samples[0][0],
                samples[0][1]);
        }
        StreamContext->HasLastOutputSamples = TRUE;
        StreamContext->RenderTransferFrameIndex++;
        StreamContext->RenderFrameCursor =
            (StreamContext->RenderFrameCursor + 1u) % StreamContext->RtFrameCount;
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
}

static VOID
OpenA8DJ_FillMode2FromActiveRtRenders(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _Out_writes_bytes_(Length) UCHAR *Buffer,
    _In_ ULONG Length,
    _Out_ PULONG RenderMask,
    _Out_writes_(OPENA8DJ_STEREO_PAIRS) PULONG RenderFrameCounts,
    _Out_writes_(OPENA8DJ_STEREO_PAIRS) PLONG RenderEpochs,
    _Out_writes_(OPENA8DJ_STEREO_PAIRS) PVOID *RenderStreams)
{
    ULONG index = 0;
    ULONG pairIndex;
    POPENA8DJ_ACX_STREAM_CONTEXT streams[OPENA8DJ_STEREO_PAIRS] = { NULL };

    *RenderMask = 0;
    RtlZeroMemory(RenderFrameCounts, sizeof(ULONG) * OPENA8DJ_STEREO_PAIRS);
    RtlZeroMemory(RenderEpochs, sizeof(LONG) * OPENA8DJ_STEREO_PAIRS);
    RtlZeroMemory(RenderStreams, sizeof(PVOID) * OPENA8DJ_STEREO_PAIRS);

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
release_streams:
    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        if (streams[pairIndex] != NULL) {
            RenderFrameCounts[pairIndex] = streams[pairIndex]->RenderTransferFrameIndex;
            RenderEpochs[pairIndex] =
                InterlockedCompareExchange(&streams[pairIndex]->RtPacketEpoch, 0, 0);
            if (RenderFrameCounts[pairIndex] != 0) {
                *RenderMask |= (1u << pairIndex);
            }
            RenderStreams[pairIndex] = streams[pairIndex];
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

    UNREFERENCED_PARAMETER(Device);
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
        attributes.ParentObject = Context->UsbDevice;
        status = WdfRequestCreate(
            &attributes,
            WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe),
            &Slots[slotIndex].Request);
        if (!NT_SUCCESS(status)) {
            break;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = Slots[slotIndex].Request;
        status = WdfMemoryCreate(
            &attributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT),
            &Slots[slotIndex].UrbMemory,
            (PVOID *)&Slots[slotIndex].Urb);
        if (!NT_SUCCESS(status)) {
            break;
        }
    }

    if (!NT_SUCCESS(status)) {
        for (slotIndex = 0; slotIndex < SlotCount; slotIndex++) {
            if (Slots[slotIndex].Request != NULL) {
                WdfObjectDelete(Slots[slotIndex].Request);
                Slots[slotIndex].Request = NULL;
            }
            Slots[slotIndex].UrbMemory = NULL;
            Slots[slotIndex].Urb = NULL;
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
        if (Slots[slotIndex].Request != NULL) {
            WDF_REQUEST_REUSE_PARAMS reuseParams;

            WDF_REQUEST_REUSE_PARAMS_INIT(
                &reuseParams,
                WDF_REQUEST_REUSE_NO_FLAGS,
                completionStatus);
            (VOID)WdfRequestReuse(Slots[slotIndex].Request, &reuseParams);
            WdfObjectDelete(Slots[slotIndex].Request);
            Slots[slotIndex].Request = NULL;
        }
        Slots[slotIndex].UrbMemory = NULL;
        Slots[slotIndex].Urb = NULL;
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

    RtlZeroMemory(
        Slot->Urb,
        GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT));
    Slot->Urb->UrbIsochronousTransfer.Hdr.Length =
        (USHORT)GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT);
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

static BOOLEAN
OpenA8DJ_AbortIsoInputPipe(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    if (Context == NULL || Context->IsoInPipe == NULL) {
        return FALSE;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(
        &sendOptions,
        WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions,
        WDF_REL_TIMEOUT_IN_SEC(1));
    status = WdfUsbTargetPipeAbortSynchronously(
        Context->IsoInPipe,
        NULL,
        &sendOptions);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_WARNING_LEVEL,
                   "OpenA8DJUsb: isochronous input abort failed 0x%08x\n",
                   status));
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN
OpenA8DJ_AbortIsoOutputPipe(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    NTSTATUS status;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    if (Context == NULL || Context->IsoOutPipe == NULL) {
        return FALSE;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(
        &sendOptions,
        WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions,
        WDF_REL_TIMEOUT_IN_SEC(1));
    status = WdfUsbTargetPipeAbortSynchronously(
        Context->IsoOutPipe,
        NULL,
        &sendOptions);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: isochronous output abort failed 0x%08x\n",
                   status));
        return FALSE;
    }
    return TRUE;
}

static NTSTATUS
OpenA8DJ_SendIsoOutputBufferPrepared(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *Capture,
    _In_reads_bytes_(TransferBufferLength) PVOID TransferBuffer,
    _In_ ULONG TransferBufferLength,
    _In_ ULONG FixedPacketBytes,
    _In_ WDFREQUEST Request,
    _In_ WDFMEMORY UrbMemory,
    _Inout_ PURB Urb,
    _Out_ ULONG *PlaybackErrorCount)
{
    NTSTATUS status;
    ULONG packetIndex;
    ULONG outputOffset;

    if (PlaybackErrorCount != NULL) {
        *PlaybackErrorCount = 0;
    }

    if (Context->IsoOutPipe == NULL || Request == NULL || UrbMemory == NULL || Urb == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (TransferBufferLength == 0 ||
        TransferBufferLength > OPENA8DJ_ISO_TRANSFER_BUFFER_CAPACITY) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    Urb->UrbIsochronousTransfer.Hdr.Length =
        (USHORT)GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT);
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

    status = OpenA8DJ_SendIsoUrbWithRequest(
        Context,
        Context->IsoOutPipe,
        Request,
        UrbMemory,
        Urb,
        PlaybackErrorCount);
    return status;
}

static NTSTATUS
OpenA8DJ_SendIsoOutputBufferLocked(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ const OPENA8DJ_ISO_CAPTURE_SNAPSHOT *Capture,
    _In_reads_bytes_(TransferBufferLength) PVOID TransferBuffer,
    _In_ ULONG TransferBufferLength,
    _In_ ULONG FixedPacketBytes,
    _Out_ ULONG *PlaybackErrorCount)
{
    NTSTATUS status;
    WDFREQUEST request;
    WDFMEMORY urbMemory;
    PURB urb;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    NTSTATUS reuseStatus;

    /* Caller owns IsoTransportLock for validation, buffer fill, submit and reuse. */
    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
        if (PlaybackErrorCount != NULL) {
            *PlaybackErrorCount = 1;
        }
        return STATUS_DEVICE_NOT_READY;
    }

    request = Context->IsoOutputRequest;
    urbMemory = Context->IsoOutputUrbMemory;
    urb = Context->IsoOutputUrb;
    if (request == NULL || urbMemory == NULL || urb == NULL) {
        if (PlaybackErrorCount != NULL) {
            *PlaybackErrorCount = 1;
        }
        return STATUS_DEVICE_NOT_READY;
    }

    RtlZeroMemory(urb, GET_ISO_URB_SIZE(OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT));
    status = OpenA8DJ_SendIsoOutputBufferPrepared(
        Context,
        Capture,
        TransferBuffer,
        TransferBufferLength,
        FixedPacketBytes,
        request,
        urbMemory,
        urb,
        PlaybackErrorCount);

    if (!NT_SUCCESS(status)) {
        (VOID)OpenA8DJ_AbortIsoOutputPipe(Context);
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: cancelled failed isochronous output request\n"));
    } else {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_TRACE_LEVEL,
                   "OpenA8DJUsb: completed isochronous output request\n"));
    }
    /* Release the formatted-memory reference and retain the persistent slot. */
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RECLAIM_START,
        status);
    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        status);
    reuseStatus = WdfRequestReuse(request, &reuseParams);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RECLAIM_COMPLETE,
        reuseStatus);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SLOT_IDLE,
        reuseStatus);
    if (!NT_SUCCESS(reuseStatus)) {
        return reuseStatus;
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

    if (Context->UsbDevice == NULL) {
        if (PlaybackErrorCount != NULL) {
            *PlaybackErrorCount = 1;
        }
        return STATUS_DEVICE_NOT_READY;
    }
    status = OpenA8DJ_EnsureIsoTransportResources(Context);
    if (!NT_SUCCESS(status)) {
        if (PlaybackErrorCount != NULL) {
            *PlaybackErrorCount = 1;
        }
        return status;
    }
    status = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(status)) {
        if (PlaybackErrorCount != NULL) {
            *PlaybackErrorCount = 1;
        }
        return status;
    }
    status = OpenA8DJ_SendIsoOutputBufferLocked(
        Context,
        Capture,
        TransferBuffer,
        TransferBufferLength,
        FixedPacketBytes,
        PlaybackErrorCount);
    WdfWaitLockRelease(Context->IsoTransportLock);
    return status;
}

static BOOLEAN
OpenA8DJ_PersistentIsoMaySubmit(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ POPENA8DJ_ISO_ENGINE_SLOT Slot)
{
    /* Caller owns IsoTransportLock for the complete gate-and-submit sequence. */
    return InterlockedCompareExchange(&Context->IsoEngineRunning, 0, 0) != 0 &&
           InterlockedCompareExchange(&Context->IsoEngineState, 0, 0) ==
               OpenA8DJIsoEngineRunning &&
           InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) != 0 &&
           InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) == 0 &&
           InterlockedCompareExchange(&Context->StreamStopRequested, 0, 0) == 0 &&
           Slot->Generation ==
               InterlockedCompareExchange(&Context->IsoEngineGeneration, 0, 0);
}

static BOOLEAN
OpenA8DJ_ArePersistentIsoSlotsIdle(_In_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    ULONG slotIndex;

    for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
        if (InterlockedCompareExchange(
                &Context->IsoCaptureSlots[slotIndex].State,
                0,
                0) != OpenA8DJIsoSlotIdle ||
            InterlockedCompareExchange(
                &Context->IsoOutputSlots[slotIndex].State,
                0,
                0) != OpenA8DJIsoSlotIdle ||
            InterlockedCompareExchange(
                &Context->IsoCaptureSlots[slotIndex].InFlight,
                0,
                0) != 0 ||
            InterlockedCompareExchange(
                &Context->IsoOutputSlots[slotIndex].InFlight,
                0,
                0) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN
OpenA8DJ_TrySignalPersistentIsoDrained(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    if (InterlockedCompareExchange(&Context->IsoOutstandingCapture, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoOutstandingOutput, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoProcessWorkPending, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoProcessWorkActive, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoStopWorkPending, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoStopWorkActive, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoTransportDraining, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoEngineState, 0, 0) !=
            OpenA8DJIsoEngineStopped ||
        !OpenA8DJ_ArePersistentIsoSlotsIdle(Context)) {
        KeClearEvent(&Context->IsoEngineDrainedEvent);
        return FALSE;
    }

    KeSetEvent(&Context->IsoEngineDrainedEvent, IO_NO_INCREMENT, FALSE);
    if (InterlockedCompareExchange(&Context->IsoEngineRunning, 0, 0) == 0 ||
        InterlockedCompareExchange(&Context->StreamStopRequested, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
        Context->StreamState.Streaming = FALSE;
        Context->StreamState.StreamingEngineReady = FALSE;
        InterlockedExchange(&Context->AudioParamsConfigured, 0);
        if (InterlockedExchange(&Context->StreamWorkerActive, 0) != 0) {
            OpenA8DJ_RecordSafetyCheckpoint(
                Context,
                OPENA8DJ_CHECKPOINT_STREAM_WORKER_EXIT,
                STATUS_SUCCESS);
        }
    }
    return TRUE;
}

static VOID
OpenA8DJ_QueuePersistentIsoProcess(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    InterlockedExchange(&Context->IsoProcessWorkPending, 1);
    KeClearEvent(&Context->IsoEngineDrainedEvent);
    if (Context->IsoProcessWorkItem != NULL &&
        InterlockedCompareExchange(&Context->IsoProcessWorkActive, 0, 0) == 0) {
        WdfWorkItemEnqueue(Context->IsoProcessWorkItem);
    }
}

static VOID
OpenA8DJ_QueuePersistentIsoStop(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    if (Context->IsoStopWorkItem != NULL &&
        InterlockedCompareExchange(&Context->IsoStopWorkPending, 1, 0) == 0) {
        WdfWorkItemEnqueue(Context->IsoStopWorkItem);
    }
}

static LONG
OpenA8DJ_PersistentIsoDecrementOutstanding(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ OPENA8DJ_ISO_ENGINE_DIRECTION Direction)
{
    LONG remaining;

    if (Direction == OpenA8DJIsoDirectionCapture) {
        remaining = InterlockedDecrement(&Context->IsoOutstandingCapture);
    } else {
        remaining = InterlockedDecrement(&Context->IsoOutstandingOutput);
    }
    NT_ASSERT(remaining >= 0);
    if (remaining < 0) {
        if (Direction == OpenA8DJIsoDirectionCapture) {
            InterlockedExchange(&Context->IsoOutstandingCapture, 0);
        } else {
            InterlockedExchange(&Context->IsoOutstandingOutput, 0);
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            Direction == OpenA8DJIsoDirectionCapture ?
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_COMPLETE :
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_COMPLETE,
            STATUS_INTERNAL_ERROR);
        remaining = 0;
    }
    return remaining;
}

static BOOLEAN
OpenA8DJ_IsPersistentIsoLateStatus(_In_ USBD_STATUS Status)
{
    return Status == USBD_STATUS_ISO_NA_LATE_USBPORT ||
           Status == USBD_STATUS_ISO_NOT_ACCESSED_BY_HW ||
           Status == USBD_STATUS_ISO_NOT_ACCESSED_LATE;
}

static VOID
OpenA8DJ_RecordPersistentOutputUsbdStatus(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ USBD_STATUS Status)
{
    if (Status == USBD_STATUS_SUCCESS) {
        return;
    }
    if (Status == USBD_STATUS_BAD_START_FRAME) {
        InterlockedIncrement64(&Context->IsoOutputBadStartFrames);
    } else if (OpenA8DJ_IsPersistentIsoLateStatus(Status)) {
        InterlockedIncrement64(&Context->IsoOutputLatePackets);
    } else {
        InterlockedIncrement64(&Context->IsoOutputOtherPacketErrors);
    }
}

static VOID
OpenA8DJ_RecordPersistentCaptureUsbdStatus(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ USBD_STATUS Status)
{
    if (Status == USBD_STATUS_SUCCESS) {
        return;
    }
    if (Status == USBD_STATUS_BAD_START_FRAME) {
        InterlockedIncrement64(&Context->IsoCaptureBadStartFrames);
    } else if (OpenA8DJ_IsPersistentIsoLateStatus(Status)) {
        InterlockedIncrement64(&Context->IsoCaptureLatePackets);
    } else {
        InterlockedIncrement64(&Context->IsoCaptureOtherPacketErrors);
    }
}

static NTSTATUS
OpenA8DJ_PreparePersistentCaptureSlot(
    _Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot)
{
    POPENA8DJ_DEVICE_CONTEXT context = Slot->DeviceContext;
    ULONG packetBytes;
    ULONG packetIndex;

    if (context == NULL || context->IsoInPipe == NULL ||
        Slot->Urb == NULL || Slot->Buffer == NULL ||
        Slot->BufferCapacity == 0 ||
        (Slot->BufferCapacity % OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT) != 0) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    packetBytes = Slot->BufferCapacity / OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
    Slot->TransferLength = Slot->BufferCapacity;
    RtlZeroMemory(Slot->Buffer, Slot->TransferLength);
    RtlZeroMemory(Slot->Urb, GET_ISO_URB_SIZE(OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT));
    Slot->Urb->UrbIsochronousTransfer.Hdr.Length =
        (USHORT)GET_ISO_URB_SIZE(OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT);
    Slot->Urb->UrbIsochronousTransfer.Hdr.Function = URB_FUNCTION_ISOCH_TRANSFER;
    Slot->Urb->UrbIsochronousTransfer.PipeHandle =
        WdfUsbTargetPipeWdmGetPipeHandle(context->IsoInPipe);
    Slot->Urb->UrbIsochronousTransfer.TransferFlags =
        USBD_TRANSFER_DIRECTION_IN |
        USBD_SHORT_TRANSFER_OK |
        USBD_START_ISO_TRANSFER_ASAP;
    Slot->Urb->UrbIsochronousTransfer.TransferBufferLength = Slot->TransferLength;
    Slot->Urb->UrbIsochronousTransfer.TransferBuffer = Slot->Buffer;
    Slot->Urb->UrbIsochronousTransfer.TransferBufferMDL = NULL;
    Slot->Urb->UrbIsochronousTransfer.NumberOfPackets = OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
    /*
     * ASAP lets the USB stack preserve the endpoint's native continuous
     * cadence and writes the actual start frame back into the URB.  The pipe
     * is reset after every drained generation below, so ASAP cannot inherit
     * stale next-frame tracking across a rapid stop/start.
     */
    Slot->ScheduledStartFrame = 0;
    Slot->Urb->UrbIsochronousTransfer.StartFrame = 0;
    for (packetIndex = 0; packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT; packetIndex++) {
        Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset = packetIndex * packetBytes;
        /* Length is written by the host controller for isochronous IN. */
        Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = 0;
        Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status = USBD_STATUS_SUCCESS;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_NormalizePersistentCaptureSlot(
    _Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot,
    _Out_ PULONG PayloadBytes)
{
    POPENA8DJ_DEVICE_CONTEXT context;
    WDF_USB_PIPE_INFORMATION capturePipeInfo;
    UCHAR configuredRateCode;
    USHORT configuredPacketBytes;
    ULONG packetStride;
    ULONG packetIndex;
    ULONG payloadBytes = 0;
    ULONG observedBytes = 0;
    BOOLEAN settleActive;
    BOOLEAN settleClean = TRUE;
    BOOLEAN observedExpectedPacket = FALSE;

    if (Slot == NULL || PayloadBytes == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *PayloadBytes = 0;
    context = Slot->DeviceContext;
    if (context == NULL || context->IsoInPipe == NULL ||
        Slot->Direction != OpenA8DJIsoDirectionCapture ||
        Slot->Urb == NULL || Slot->Buffer == NULL ||
        Slot->TransferLength == 0 ||
        Slot->TransferLength > Slot->BufferCapacity ||
        (Slot->BufferCapacity % OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT) != 0 ||
        !OpenA8DJ_GetAudioRateParameters(
            context->ConfiguredSampleRate,
            &configuredRateCode,
            &configuredPacketBytes)) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    UNREFERENCED_PARAMETER(configuredRateCode);
    packetStride = Slot->BufferCapacity / OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
    WDF_USB_PIPE_INFORMATION_INIT(&capturePipeInfo);
    WdfUsbTargetPipeGetInformation(context->IsoInPipe, &capturePipeInfo);
    if (packetStride == 0 || configuredPacketBytes == 0 ||
        configuredPacketBytes > packetStride ||
        capturePipeInfo.MaximumPacketSize != packetStride) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    settleActive =
        InterlockedCompareExchange(&context->AudioRateSettleActive, 0, 0) != 0;

    for (packetIndex = 0; packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT; packetIndex++) {
        PUSBD_ISO_PACKET_DESCRIPTOR descriptor =
            &Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex];
        ULONG expectedOffset = packetIndex * packetStride;
        ULONG offset = descriptor->Offset;
        ULONG length = descriptor->Length;

        if (length > observedBytes) {
            observedBytes = length;
        }

        if (descriptor->Status != USBD_STATUS_SUCCESS ||
            offset != expectedOffset ||
            offset > Slot->TransferLength ||
            length > Slot->TransferLength - offset ||
            offset > Slot->BufferCapacity ||
            packetStride > Slot->BufferCapacity - offset) {
            return STATUS_DEVICE_DATA_ERROR;
        }

        if (length != 0 && length != configuredPacketBytes) {
            /* Match the Mac transport: stale geometry is never decoded or echoed. */
            InterlockedIncrement64(&context->AudioRateSettleMismatchedPackets);
            if (settleActive) {
                settleClean = FALSE;
            }
            descriptor->Length = 0;
            length = 0;
        }
        if (length == configuredPacketBytes) {
            observedExpectedPacket = TRUE;
        }

        if ((length % OPENA8DJ_USB_OUTPUT_FRAME_BYTES) != 0 ||
            length > MAXULONG - payloadBytes) {
            return STATUS_DEVICE_DATA_ERROR;
        }
        payloadBytes += length;
    }

    if (settleActive) {
        LARGE_INTEGER currentCounter;
        LONGLONG startQpc =
            InterlockedCompareExchange64(&context->AudioRateSettleStartQpc, 0, 0);
        LONGLONG budgetQpc =
            InterlockedCompareExchange64(&context->AudioRateSettleBudgetQpc, 0, 0);
        LONG attemptsRemaining =
            InterlockedDecrement(&context->AudioRateSettleAttemptsRemaining);

        InterlockedIncrement64(&context->AudioRateSettleSnapshots);
        InterlockedExchange(
            &context->AudioRateSettleLastObservedBytes,
            (LONG)observedBytes);

        if (!settleClean) {
            /* Never decode a transfer containing mixed old/new geometry. */
            for (packetIndex = 0;
                 packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
                 packetIndex++) {
                Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = 0;
            }
            payloadBytes = 0;
        }

        currentCounter = KeQueryPerformanceCounter(NULL);
        if ((attemptsRemaining <= 0 ||
             budgetQpc <= 0 ||
             currentCounter.QuadPart - startQpc >= budgetQpc) &&
            InterlockedCompareExchange(
                &context->AudioRateSettleActive,
                0,
                1) == 1) {
            InterlockedIncrement64(&context->AudioRateSettleFailures);
            OpenA8DJ_RecordSafetyCheckpoint(
                context,
                OPENA8DJ_CHECKPOINT_AUDIO_RATE_SETTLE_COMPLETE,
                STATUS_IO_TIMEOUT);
            return STATUS_IO_TIMEOUT;
        }

        if (settleClean && observedExpectedPacket) {
            if ((ULONG)InterlockedIncrement(
                    &context->AudioRateSettleConsecutiveClean) >=
                OPENA8DJ_AUDIO_RATE_SETTLE_REQUIRED_CONSECUTIVE) {
                if (InterlockedCompareExchange(
                        &context->AudioRateSettleActive,
                        0,
                        1) == 1) {
                    OpenA8DJ_RecordSafetyCheckpoint(
                        context,
                        OPENA8DJ_CHECKPOINT_AUDIO_RATE_SETTLE_COMPLETE,
                        STATUS_SUCCESS);
                }
            }
        } else {
            InterlockedExchange(&context->AudioRateSettleConsecutiveClean, 0);
        }
    }

    *PayloadBytes = payloadBytes;
    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_PreparePersistentOutputSlotFromCapture(
    _Inout_ POPENA8DJ_ISO_ENGINE_SLOT OutputSlot,
    _In_ const OPENA8DJ_ISO_ENGINE_SLOT *CaptureSlot)
{
    POPENA8DJ_DEVICE_CONTEXT context = OutputSlot->DeviceContext;
    WDF_USB_PIPE_INFORMATION outputPipeInfo;
    UCHAR configuredRateCode;
    USHORT configuredPacketBytes;
    ULONG packetIndex;
    ULONG renderPairIndex;
    ULONG transferLength = 0;
    ULONG activeRenderMask;

    if (context == NULL || CaptureSlot == NULL ||
        CaptureSlot->DeviceContext != context ||
        CaptureSlot->Direction != OpenA8DJIsoDirectionCapture ||
        OutputSlot->Direction != OpenA8DJIsoDirectionOutput ||
        CaptureSlot->Index != OutputSlot->Index ||
        context->IsoOutPipe == NULL ||
        CaptureSlot->Urb == NULL || OutputSlot->Urb == NULL ||
        OutputSlot->Buffer == NULL || OutputSlot->BufferCapacity == 0) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    WDF_USB_PIPE_INFORMATION_INIT(&outputPipeInfo);
    WdfUsbTargetPipeGetInformation(context->IsoOutPipe, &outputPipeInfo);
    if (outputPipeInfo.MaximumPacketSize == 0 ||
        outputPipeInfo.MaximumPacketSize > OutputSlot->BufferCapacity) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    if (!OpenA8DJ_GetAudioRateParameters(
            context->ConfiguredSampleRate,
            &configuredRateCode,
            &configuredPacketBytes)) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    UNREFERENCED_PARAMETER(configuredRateCode);

    for (packetIndex = 0; packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT; packetIndex++) {
        ULONG offset =
            CaptureSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset;
        ULONG length =
            CaptureSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length;
        USBD_STATUS packetStatus =
            CaptureSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status;

        if (packetStatus != USBD_STATUS_SUCCESS ||
            offset > CaptureSlot->TransferLength ||
            length > CaptureSlot->TransferLength - offset ||
            length > outputPipeInfo.MaximumPacketSize ||
            length > configuredPacketBytes ||
            (length % OPENA8DJ_USB_OUTPUT_FRAME_BYTES) != 0 ||
            length > MAXULONG - transferLength ||
            transferLength + length > OutputSlot->BufferCapacity) {
            return STATUS_DEVICE_DATA_ERROR;
        }
        transferLength += length;
    }
    if (transferLength == 0) {
        return STATUS_DEVICE_DATA_ERROR;
    }

    OutputSlot->TransferLength = transferLength;
    OutputSlot->SourceCaptureCompletionQpc = CaptureSlot->CompletionQpc;
    for (renderPairIndex = 0;
         renderPairIndex < OPENA8DJ_STEREO_PAIRS;
         renderPairIndex++) {
        if (OutputSlot->RenderStreams[renderPairIndex] != NULL) {
            return STATUS_DEVICE_BUSY;
        }
    }
    OutputSlot->RenderMask = 0;
    RtlZeroMemory(OutputSlot->RenderFrameCount, sizeof(OutputSlot->RenderFrameCount));
    RtlZeroMemory(OutputSlot->RenderEpoch, sizeof(OutputSlot->RenderEpoch));
    RtlZeroMemory(OutputSlot->RenderStreams, sizeof(OutputSlot->RenderStreams));
    activeRenderMask = OpenA8DJ_GetActiveRenderMask(context);
    if (activeRenderMask != 0) {
        OpenA8DJ_FillMode2FromActiveRtRenders(
            context,
            OutputSlot->Buffer,
            OutputSlot->TransferLength,
            &OutputSlot->RenderMask,
            OutputSlot->RenderFrameCount,
            OutputSlot->RenderEpoch,
            OutputSlot->RenderStreams);
    } else {
        InterlockedIncrement64(&context->StreamWorkerNoRenderIterations);
        OpenA8DJ_FillMode2Silence(OutputSlot->Buffer, OutputSlot->TransferLength);
    }
    OpenA8DJ_RecordUsbPlaybackTrace(
        context,
        OutputSlot->Buffer,
        OutputSlot->TransferLength,
        OPENA8DJ_ISO_OUTPUT_PACKET_BYTES_CAPTURE_SHAPE,
        activeRenderMask);
    context->StreamWorkerLastRenderMask = activeRenderMask;

    RtlZeroMemory(
        OutputSlot->Urb,
        GET_ISO_URB_SIZE(OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT));
    OutputSlot->Urb->UrbIsochronousTransfer.Hdr.Length =
        (USHORT)GET_ISO_URB_SIZE(OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT);
    OutputSlot->Urb->UrbIsochronousTransfer.Hdr.Function =
        URB_FUNCTION_ISOCH_TRANSFER;
    OutputSlot->Urb->UrbIsochronousTransfer.PipeHandle =
        WdfUsbTargetPipeWdmGetPipeHandle(context->IsoOutPipe);
    OutputSlot->Urb->UrbIsochronousTransfer.TransferFlags =
        USBD_TRANSFER_DIRECTION_OUT;
    OutputSlot->Urb->UrbIsochronousTransfer.TransferBufferLength = transferLength;
    OutputSlot->Urb->UrbIsochronousTransfer.TransferBuffer = OutputSlot->Buffer;
    OutputSlot->Urb->UrbIsochronousTransfer.TransferBufferMDL = NULL;
    OutputSlot->Urb->UrbIsochronousTransfer.NumberOfPackets =
        OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
    OutputSlot->ScheduledStartFrame =
        CaptureSlot->Urb->UrbIsochronousTransfer.StartFrame +
        OPENA8DJ_PERSISTENT_ISO_TRANSFER_FRAMES +
        OPENA8DJ_ISO_OUTPUT_LEAD_FRAMES;
    OutputSlot->Urb->UrbIsochronousTransfer.StartFrame =
        OutputSlot->ScheduledStartFrame;
    context->IsoLastCaptureStartFrame =
        CaptureSlot->Urb->UrbIsochronousTransfer.StartFrame;
    context->IsoLastOutputStartFrame = OutputSlot->ScheduledStartFrame;

    transferLength = 0;
    for (packetIndex = 0; packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT; packetIndex++) {
        ULONG length =
            CaptureSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length;

        OutputSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset =
            transferLength;
        OutputSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length = length;
        OutputSlot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status =
            USBD_STATUS_SUCCESS;
        transferLength += length;
    }
    return STATUS_SUCCESS;
}

static VOID
OpenA8DJ_ProcessPersistentIsoDirection(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ OPENA8DJ_ISO_ENGINE_DIRECTION Direction);

static VOID
NTAPI
OpenA8DJ_EvtPersistentIsoComplete(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT CompletionContext)
{
    POPENA8DJ_ISO_ENGINE_SLOT slot =
        (POPENA8DJ_ISO_ENGINE_SLOT)CompletionContext;
    POPENA8DJ_DEVICE_CONTEXT context;
    LONG previousState;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    if (slot == NULL || slot->DeviceContext == NULL) {
        return;
    }
    context = slot->DeviceContext;
    slot->CompletionQpc = KeQueryPerformanceCounter(NULL).QuadPart;
    slot->CompletionStatus = Params->IoStatus.Status;
    slot->UrbStatus = slot->Urb != NULL ?
        slot->Urb->UrbIsochronousTransfer.Hdr.Status : USBD_STATUS_INVALID_PARAMETER;
    slot->ErrorCount = slot->Urb != NULL ?
        slot->Urb->UrbIsochronousTransfer.ErrorCount : 1u;
    previousState = InterlockedCompareExchange(
        &slot->State,
        OpenA8DJIsoSlotCompleted,
        OpenA8DJIsoSlotSubmitted);
    if (previousState != OpenA8DJIsoSlotSubmitted) {
        slot->CompletionStatus = STATUS_INTERNAL_ERROR;
        slot->ErrorCount++;
    }
    InterlockedExchange(&slot->NeedsReuse, 1);
    if (InterlockedExchange(&slot->InFlight, 0) != 0) {
        LONG remaining = OpenA8DJ_PersistentIsoDecrementOutstanding(
            context,
            slot->Direction);

        if (slot->Direction == OpenA8DJIsoDirectionOutput &&
            remaining == 0 &&
            InterlockedCompareExchange(&context->IsoEngineRunning, 0, 0) != 0 &&
            InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) == 0 &&
            InterlockedCompareExchange(&context->DeviceStopping, 0, 0) == 0) {
            InterlockedIncrement64(&context->IsoOutputQueueEmptyTransitions);
        }
    }
    OpenA8DJ_QueuePersistentIsoProcess(context);
}

static NTSTATUS
OpenA8DJ_SubmitPersistentIsoSlot(_Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot)
{
    POPENA8DJ_DEVICE_CONTEXT context = Slot->DeviceContext;
    WDFUSBPIPE pipe;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    NTSTATUS status;

    if (context == NULL || !OpenA8DJ_PersistentIsoMaySubmit(context, Slot)) {
        InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
        return STATUS_CANCELLED;
    }
    pipe = Slot->Direction == OpenA8DJIsoDirectionCapture ?
        context->IsoInPipe : context->IsoOutPipe;
    if (pipe == NULL || Slot->Request == NULL ||
        Slot->UrbMemory == NULL || Slot->Urb == NULL) {
        InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
        return STATUS_DEVICE_NOT_READY;
    }

    if (InterlockedExchange(&Slot->NeedsReuse, 0) != 0) {
        WDF_REQUEST_REUSE_PARAMS_INIT(
            &reuseParams,
            WDF_REQUEST_REUSE_NO_FLAGS,
            Slot->CompletionStatus);
        status = WdfRequestReuse(Slot->Request, &reuseParams);
        if (!NT_SUCCESS(status)) {
            InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
            OpenA8DJ_RecordSafetyCheckpoint(
                context,
                Slot->Direction == OpenA8DJIsoDirectionCapture ?
                    OPENA8DJ_CHECKPOINT_ISO_CAPTURE_RECLAIM_COMPLETE :
                    OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RECLAIM_COMPLETE,
                status);
            return status;
        }
    }

    status = WdfUsbTargetPipeFormatRequestForUrb(
        pipe,
        Slot->Request,
        Slot->UrbMemory,
        NULL);
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&Slot->NeedsReuse, 1);
        InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
        OpenA8DJ_RecordSafetyCheckpoint(
            context,
            Slot->Direction == OpenA8DJIsoDirectionCapture ?
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_FORMATTED :
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SUBMIT,
            status);
        return status;
    }

    Slot->CompletionStatus = STATUS_PENDING;
    Slot->ErrorCount = 0;
    Slot->SubmitQpc = KeQueryPerformanceCounter(NULL).QuadPart;
    if (Slot->Direction == OpenA8DJIsoDirectionOutput &&
        Slot->SourceCaptureCompletionQpc > 0 &&
        Slot->SubmitQpc > Slot->SourceCaptureCompletionQpc) {
        LONG64 captureToSubmit = Slot->SubmitQpc - Slot->SourceCaptureCompletionQpc;

        if (captureToSubmit > context->IsoCaptureToOutputSubmitMaxQpc) {
            context->IsoCaptureToOutputSubmitMaxQpc = captureToSubmit;
        }
    }
    if (Slot->Direction == OpenA8DJIsoDirectionCapture) {
        Slot->SubmitSequence = InterlockedIncrement64(&context->IsoNextCaptureSubmitSequence);
        InterlockedIncrement(&context->IsoOutstandingCapture);
    } else {
        Slot->SubmitSequence = InterlockedIncrement64(&context->IsoNextOutputSubmitSequence);
        InterlockedIncrement(&context->IsoOutstandingOutput);
    }
    KeClearEvent(&context->IsoEngineDrainedEvent);
    InterlockedExchange(&Slot->InFlight, 1);
    InterlockedExchange(&Slot->State, OpenA8DJIsoSlotSubmitted);
    WdfRequestSetCompletionRoutine(
        Slot->Request,
        OpenA8DJ_EvtPersistentIsoComplete,
        Slot);
    if (!WdfRequestSend(
            Slot->Request,
            WdfUsbTargetPipeGetIoTarget(pipe),
            WDF_NO_SEND_OPTIONS)) {
        status = WdfRequestGetStatus(Slot->Request);
        Slot->CompletionStatus = status;
        Slot->ErrorCount = 1;
        InterlockedExchange(&Slot->NeedsReuse, 1);
        if (InterlockedCompareExchange(
                &Slot->State,
                OpenA8DJIsoSlotCompleted,
                OpenA8DJIsoSlotSubmitted) == OpenA8DJIsoSlotSubmitted &&
            InterlockedExchange(&Slot->InFlight, 0) != 0) {
            OpenA8DJ_PersistentIsoDecrementOutstanding(context, Slot->Direction);
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            context,
            Slot->Direction == OpenA8DJIsoDirectionCapture ?
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_SUBMIT :
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SUBMIT,
            status);
        OpenA8DJ_QueuePersistentIsoProcess(context);
        return status;
    }
    return STATUS_SUCCESS;
}

static VOID
OpenA8DJ_ProcessPersistentCaptureSlot(_Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot)
{
    POPENA8DJ_DEVICE_CONTEXT context = Slot->DeviceContext;
    ULONG packetIndex;
    ULONG pairIndex;
    ULONG payloadBytes = 0;
    ULONG outputPanicFlags = 0;
    ULONG activeCaptureMask = 0;

    for (packetIndex = 0; packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT; packetIndex++) {
        ULONG offset = Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset;
        ULONG length = Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length;
        USBD_STATUS packetStatus =
            Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status;

        if (packetStatus == USBD_STATUS_SUCCESS &&
            offset <= Slot->TransferLength && length <= Slot->TransferLength - offset) {
            ULONG checkOffset;

            payloadBytes += length;
            context->StreamState.UsbInPacketsCompleted++;
            for (checkOffset = 0u; checkOffset + 3u < length; checkOffset += 16u) {
                ULONG stream;

                for (stream = 0; stream < 4u; stream++) {
                    if ((Slot->Buffer[offset + checkOffset + stream] & 0x80u) != 0) {
                        outputPanicFlags++;
                    }
                }
            }
        } else if (packetStatus != USBD_STATUS_SUCCESS) {
            context->StreamState.UsbPacketErrors++;
        }
    }
    context->StreamState.UsbPacketErrors += Slot->ErrorCount;
    if (outputPanicFlags != 0) {
        InterlockedAdd64(&context->IsoOutputPanicFlags, outputPanicFlags);
    }

    for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
        POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
            OpenA8DJ_AcquireActiveStream(
                context,
                &context->ActiveCaptureStreams[pairIndex]);
        ULONG framesWritten = 0;

        if (streamContext != NULL && streamContext->RtKernelAddress != NULL &&
            streamContext->RtFrameCount != 0) {
            activeCaptureMask |= (1u << pairIndex);
            for (packetIndex = 0; packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT; packetIndex++) {
                ULONG offset = Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Offset;
                ULONG length = Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Length;

                if (Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status ==
                        USBD_STATUS_SUCCESS &&
                    offset <= Slot->TransferLength && length <= Slot->TransferLength - offset) {
                    framesWritten += OpenA8DJ_FillRtCaptureFromMode2(
                        Slot->Buffer + offset,
                        length,
                        streamContext,
                        (ULONGLONG)Slot->CompletionQpc);
                }
            }
            context->StreamState.CaptureFramesDelivered += framesWritten;
        }
        if (streamContext != NULL) {
            OpenA8DJ_ReleaseActiveStream(streamContext);
        }
    }

    InterlockedIncrement64(&context->StreamWorkerIterations);
    InterlockedAdd64(&context->StreamWorkerCaptureBytes, payloadBytes);
    context->StreamWorkerLastCaptureBytes = payloadBytes;
    context->StreamWorkerLastCaptureMask = activeCaptureMask;
    if (payloadBytes > context->StreamWorkerMaxCaptureBytes) {
        context->StreamWorkerMaxCaptureBytes = payloadBytes;
    }
}

static VOID
OpenA8DJ_RequestPersistentIsoFatalStopLocked(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ NTSTATUS Status)
{
    /* Caller owns IsoTransportLock.  Purge itself is deferred to PASSIVE work. */
    InterlockedExchange(&Context->StreamStopRequested, 1);
    InterlockedExchange(&Context->IsoEngineState, OpenA8DJIsoEngineStopping);
    if (InterlockedExchange(&Context->IsoEngineRunning, 0) != 0) {
        InterlockedIncrement(&Context->IsoEngineGeneration);
    }
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_DRAIN_ENTER,
        Status);
    OpenA8DJ_QueuePersistentIsoStop(Context);
}

static VOID
OpenA8DJ_ProcessPersistentIsoSlot(_Inout_ POPENA8DJ_ISO_ENGINE_SLOT Slot)
{
    POPENA8DJ_DEVICE_CONTEXT context = Slot->DeviceContext;
    NTSTATUS status = Slot->CompletionStatus;
    BOOLEAN transferSucceeded = NT_SUCCESS(status) && Slot->ErrorCount == 0;
    BOOLEAN captureHasPayload = TRUE;
    BOOLEAN expectedTeardown =
        InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0 ||
        InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&context->DevicePrepared, 0, 0) == 0 ||
        InterlockedCompareExchange(&context->IsoEngineState, 0, 0) ==
            OpenA8DJIsoEngineStopping;

    if (Slot->Direction == OpenA8DJIsoDirectionOutput) {
        ULONG packetIndex;
        ULONG packetErrorCount = 0;

        if (Slot->UrbStatus != USBD_STATUS_SUCCESS) {
            transferSucceeded = FALSE;
        }
        for (packetIndex = 0;
             packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
             packetIndex++) {
            USBD_STATUS packetStatus =
                Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status;

            if (packetStatus != USBD_STATUS_SUCCESS) {
                transferSucceeded = FALSE;
                if (!expectedTeardown) {
                    packetErrorCount++;
                    OpenA8DJ_RecordPersistentOutputUsbdStatus(context, packetStatus);
                    status = STATUS_DEVICE_DATA_ERROR;
                }
            }
        }
        if (!expectedTeardown &&
            Slot->UrbStatus != USBD_STATUS_SUCCESS &&
            packetErrorCount == 0) {
            OpenA8DJ_RecordPersistentOutputUsbdStatus(context, Slot->UrbStatus);
            status = STATUS_DEVICE_DATA_ERROR;
        }
    }
    if (Slot->Direction == OpenA8DJIsoDirectionCapture) {
        ULONG packetIndex;
        ULONG packetErrorCount = 0;
        ULONG firstLatePacket = MAXULONG;
        ULONG lastLatePacket = MAXULONG;
        ULONG latePacketCount = 0;
        USBD_STATUS lastPacketErrorStatus = USBD_STATUS_SUCCESS;
        if (Slot->UrbStatus != USBD_STATUS_SUCCESS) {
            transferSucceeded = FALSE;
        }
        for (packetIndex = 0;
             packetIndex < OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
             packetIndex++) {
            USBD_STATUS packetStatus =
                Slot->Urb->UrbIsochronousTransfer.IsoPacket[packetIndex].Status;

            if (packetStatus != USBD_STATUS_SUCCESS) {
                transferSucceeded = FALSE;
                if (!expectedTeardown) {
                    packetErrorCount++;
                    lastPacketErrorStatus = packetStatus;
                    if (OpenA8DJ_IsPersistentIsoLateStatus(packetStatus)) {
                        if (firstLatePacket == MAXULONG) {
                            firstLatePacket = packetIndex;
                        }
                        lastLatePacket = packetIndex;
                        latePacketCount++;
                    }
                    OpenA8DJ_RecordPersistentCaptureUsbdStatus(context, packetStatus);
                    status = STATUS_DEVICE_DATA_ERROR;
                }
            }
        }
        if (!expectedTeardown &&
            Slot->UrbStatus != USBD_STATUS_SUCCESS &&
            packetErrorCount == 0) {
            OpenA8DJ_RecordPersistentCaptureUsbdStatus(context, Slot->UrbStatus);
            lastPacketErrorStatus = Slot->UrbStatus;
            status = STATUS_DEVICE_DATA_ERROR;
        }
        if (!expectedTeardown &&
            (packetErrorCount != 0 || Slot->UrbStatus != USBD_STATUS_SUCCESS)) {
            LONG snapshotSequence = InterlockedIncrement(
                &context->IsoCaptureErrorSnapshotSequence);

            NT_ASSERT((snapshotSequence & 1) != 0);
            KeMemoryBarrier();
            context->IsoLastCaptureUrbStatus = (LONG)Slot->UrbStatus;
            context->IsoLastCapturePacketStatus = (LONG)lastPacketErrorStatus;
            context->IsoLastCaptureErrorCount = (LONG)Slot->ErrorCount;
            context->IsoLastCaptureErrorSlot = Slot->Index;
            context->IsoLastCaptureErrorGeneration = Slot->Generation;
            context->IsoLastCaptureErrorSubmitSequence = Slot->SubmitSequence;
            context->IsoLastCaptureErrorScheduledStartFrame = Slot->ScheduledStartFrame;
            context->IsoLastCaptureErrorFirstPacket = firstLatePacket;
            context->IsoLastCaptureErrorLastPacket = lastLatePacket;
            context->IsoLastCaptureErrorPacketCount = latePacketCount;
            context->IsoLastCaptureErrorSubmitQpc = Slot->SubmitQpc;
            context->IsoLastCaptureErrorCompletionQpc = Slot->CompletionQpc;
            KeMemoryBarrier();
            snapshotSequence = InterlockedIncrement(
                &context->IsoCaptureErrorSnapshotSequence);
            NT_ASSERT((snapshotSequence & 1) == 0);
        }
    }

    if (transferSucceeded && Slot->Direction == OpenA8DJIsoDirectionCapture) {
        ULONG capturePayloadBytes = 0;

        status = OpenA8DJ_NormalizePersistentCaptureSlot(
            Slot,
            &capturePayloadBytes);
        if (!NT_SUCCESS(status)) {
            InterlockedExchange(
                &context->IsoOutputSlots[Slot->Index].State,
                OpenA8DJIsoSlotIdle);
            InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
            if (InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0 ||
                InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0) {
                return;
            }
            context->StreamState.UsbPacketErrors++;
            OpenA8DJ_RecordSafetyCheckpoint(
                context,
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_COMPLETE,
                status);
            OpenA8DJ_RequestPersistentIsoFatalStopLocked(context, status);
            return;
        }
        captureHasPayload = capturePayloadBytes != 0;
        if (captureHasPayload) {
            InterlockedExchange(&context->IsoConsecutiveNoDataCaptures, 0);
        } else if ((ULONG)InterlockedIncrement(
                       &context->IsoConsecutiveNoDataCaptures) >
                   OPENA8DJ_STREAM_NO_DATA_COMPLETION_LIMIT) {
            InterlockedExchange(
                &context->IsoOutputSlots[Slot->Index].State,
                OpenA8DJIsoSlotIdle);
            InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
            OpenA8DJ_RecordSafetyCheckpoint(
                context,
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_COMPLETE,
                STATUS_IO_TIMEOUT);
            OpenA8DJ_RequestPersistentIsoFatalStopLocked(
                context,
                STATUS_IO_TIMEOUT);
            return;
        }
    }

    if (transferSucceeded) {
        Slot->ConsecutiveFailures = 0;
        if (Slot->Direction == OpenA8DJIsoDirectionCapture && captureHasPayload) {
            OpenA8DJ_ProcessPersistentCaptureSlot(Slot);
        } else if (Slot->Direction == OpenA8DJIsoDirectionOutput) {
            ULONG renderFrames = Slot->TransferLength / OPENA8DJ_USB_OUTPUT_FRAME_BYTES;

            context->StreamState.UsbOutPacketsCompleted += OPENA8DJ_PERSISTENT_ISO_PACKET_COUNT;
            context->StreamState.RenderFramesSubmitted += renderFrames;
            InterlockedAdd64(&context->StreamWorkerPlaybackBytes, Slot->TransferLength);
            context->StreamWorkerLastPlaybackBytes = Slot->TransferLength;
            if (Slot->TransferLength > context->StreamWorkerMaxPlaybackBytes) {
                context->StreamWorkerMaxPlaybackBytes = Slot->TransferLength;
            }
        }
    } else if (!expectedTeardown) {
        Slot->ConsecutiveFailures++;
        context->StreamState.UsbPacketErrors += Slot->ErrorCount != 0 ? Slot->ErrorCount : 1;
        if (Slot->Direction == OpenA8DJIsoDirectionOutput) {
            context->StreamState.UsbUnderruns++;
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            context,
            Slot->Direction == OpenA8DJIsoDirectionCapture ?
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_COMPLETE :
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_COMPLETE,
            NT_SUCCESS(status) ? STATUS_DEVICE_DATA_ERROR : status);
        if ((ULONG)Slot->ConsecutiveFailures > OPENA8DJ_STREAM_TRANSIENT_RETRY_LIMIT) {
            OpenA8DJ_RequestPersistentIsoFatalStopLocked(
                context,
                NT_SUCCESS(status) ? STATUS_DEVICE_DATA_ERROR : status);
        }
    }

    if (Slot->Direction == OpenA8DJIsoDirectionOutput) {
        OpenA8DJ_ReleaseOutputSlotRenderStreams(Slot, transferSucceeded);
    }

    if (!OpenA8DJ_PersistentIsoMaySubmit(context, Slot)) {
        if (Slot->Direction == OpenA8DJIsoDirectionCapture) {
            (VOID)InterlockedCompareExchange(
                &context->IsoOutputSlots[Slot->Index].State,
                OpenA8DJIsoSlotIdle,
                OpenA8DJIsoSlotProcessing);
        }
        InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
        return;
    }
    if (Slot->Direction == OpenA8DJIsoDirectionOutput) {
        InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
        return;
    }

    if (transferSucceeded && captureHasPayload) {
        POPENA8DJ_ISO_ENGINE_SLOT outputSlot = &context->IsoOutputSlots[Slot->Index];

        if (InterlockedCompareExchange(
                &outputSlot->State,
                OpenA8DJIsoSlotProcessing,
                OpenA8DJIsoSlotProcessing) != OpenA8DJIsoSlotProcessing) {
            status = STATUS_DEVICE_BUSY;
        } else {
            status = OpenA8DJ_PreparePersistentOutputSlotFromCapture(
                outputSlot,
                Slot);
        }
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_SubmitPersistentIsoSlot(outputSlot);
        }
        if (!NT_SUCCESS(status)) {
            OpenA8DJ_ReleaseOutputSlotRenderStreams(outputSlot, FALSE);
            InterlockedExchange(&outputSlot->State, OpenA8DJIsoSlotIdle);
            InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
            OpenA8DJ_RecordSafetyCheckpoint(
                context,
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SUBMIT,
                status);
            OpenA8DJ_RequestPersistentIsoFatalStopLocked(context, status);
            return;
        }
    } else {
        (VOID)InterlockedCompareExchange(
            &context->IsoOutputSlots[Slot->Index].State,
            OpenA8DJIsoSlotIdle,
            OpenA8DJIsoSlotProcessing);
    }

    status = OpenA8DJ_PreparePersistentCaptureSlot(Slot);
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&Slot->State, OpenA8DJIsoSlotIdle);
        OpenA8DJ_RecordSafetyCheckpoint(
            context,
            Slot->Direction == OpenA8DJIsoDirectionCapture ?
                OPENA8DJ_CHECKPOINT_ISO_CAPTURE_FORMATTED :
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_SUBMIT,
            status);
        OpenA8DJ_RequestPersistentIsoFatalStopLocked(context, status);
        return;
    }
    status = OpenA8DJ_SubmitPersistentIsoSlot(Slot);
    if (!NT_SUCCESS(status) && status != STATUS_CANCELLED) {
        Slot->ConsecutiveFailures++;
        if ((ULONG)Slot->ConsecutiveFailures > OPENA8DJ_STREAM_TRANSIENT_RETRY_LIMIT) {
            OpenA8DJ_RequestPersistentIsoFatalStopLocked(context, status);
        }
    }
}

static POPENA8DJ_ISO_ENGINE_SLOT
OpenA8DJ_FindCompletedPersistentIsoSlot(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ OPENA8DJ_ISO_ENGINE_DIRECTION Direction,
    _In_ LONG64 ExpectedSequence)
{
    POPENA8DJ_ISO_ENGINE_SLOT slots =
        Direction == OpenA8DJIsoDirectionCapture ?
            Context->IsoCaptureSlots : Context->IsoOutputSlots;
    ULONG slotIndex;

    for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
        POPENA8DJ_ISO_ENGINE_SLOT slot = &slots[slotIndex];

        if (slot->SubmitSequence != ExpectedSequence) {
            continue;
        }
        if (Direction == OpenA8DJIsoDirectionCapture) {
            POPENA8DJ_ISO_ENGINE_SLOT outputSlot =
                &Context->IsoOutputSlots[slot->Index];

            if (InterlockedCompareExchange(
                    &outputSlot->State,
                    OpenA8DJIsoSlotProcessing,
                    OpenA8DJIsoSlotIdle) != OpenA8DJIsoSlotIdle) {
                return NULL;
            }
            if (InterlockedCompareExchange(
                    &slot->State,
                    OpenA8DJIsoSlotProcessing,
                    OpenA8DJIsoSlotCompleted) == OpenA8DJIsoSlotCompleted) {
                return slot;
            }
            InterlockedExchange(&outputSlot->State, OpenA8DJIsoSlotIdle);
            return NULL;
        }
        if (InterlockedCompareExchange(
                &slot->State,
                OpenA8DJIsoSlotProcessing,
                OpenA8DJIsoSlotCompleted) == OpenA8DJIsoSlotCompleted) {
            return slot;
        }
    }
    return NULL;
}

static VOID
OpenA8DJ_ProcessPersistentIsoDirection(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ OPENA8DJ_ISO_ENGINE_DIRECTION Direction)
{
    volatile LONG64 *nextProcessSequence =
        Direction == OpenA8DJIsoDirectionCapture ?
            &Context->IsoNextCaptureProcessSequence : &Context->IsoNextOutputProcessSequence;

    for (;;) {
        LONG64 expected = InterlockedCompareExchange64(nextProcessSequence, 0, 0);
        POPENA8DJ_ISO_ENGINE_SLOT slot =
            OpenA8DJ_FindCompletedPersistentIsoSlot(Context, Direction, expected);

        if (slot == NULL) {
            break;
        }
        InterlockedIncrement64(nextProcessSequence);
        OpenA8DJ_ProcessPersistentIsoSlot(slot);
    }
}

VOID
OpenA8DJ_EvtIsoProcessWorkItem(_In_ WDFWORKITEM WorkItem)
{
    WDFDEVICE device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(device);

    if (InterlockedCompareExchange(&context->IsoProcessWorkActive, 1, 0) != 0) {
        return;
    }
    do {
        NTSTATUS lockStatus;

        InterlockedExchange(&context->IsoProcessWorkPending, 0);
        lockStatus = WdfWaitLockAcquire(context->IsoTransportLock, NULL);
        if (!NT_SUCCESS(lockStatus)) {
            InterlockedExchange(&context->IsoProcessWorkPending, 1);
            break;
        }
        OpenA8DJ_ProcessPersistentIsoDirection(
            context,
            OpenA8DJIsoDirectionOutput);
        OpenA8DJ_ProcessPersistentIsoDirection(
            context,
            OpenA8DJIsoDirectionCapture);
        WdfWaitLockRelease(context->IsoTransportLock);
        OpenA8DJ_FlushActiveRtPacketNotifications(context);
    } while (InterlockedCompareExchange(&context->IsoProcessWorkPending, 0, 0) != 0);

    {
        NTSTATUS lockStatus = WdfWaitLockAcquire(context->IsoTransportLock, NULL);
        InterlockedExchange(&context->IsoProcessWorkActive, 0);
        if (NT_SUCCESS(lockStatus)) {
            (VOID)OpenA8DJ_TrySignalPersistentIsoDrained(context);
            WdfWaitLockRelease(context->IsoTransportLock);
        }
    }
    if (InterlockedCompareExchange(&context->IsoProcessWorkPending, 0, 0) != 0) {
        WdfWorkItemEnqueue(context->IsoProcessWorkItem);
    }
}

VOID
OpenA8DJ_EvtIsoStopWorkItem(_In_ WDFWORKITEM WorkItem)
{
    WDFDEVICE device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(device);

    InterlockedExchange(&context->IsoStopWorkActive, 1);
    OpenA8DJ_PurgeIsoTargets(context, TRUE);
    {
        NTSTATUS lockStatus = WdfWaitLockAcquire(context->IsoTransportLock, NULL);
        InterlockedExchange(&context->IsoStopWorkPending, 0);
        InterlockedExchange(&context->IsoStopWorkActive, 0);
        if (NT_SUCCESS(lockStatus)) {
            (VOID)OpenA8DJ_TrySignalPersistentIsoDrained(context);
            WdfWaitLockRelease(context->IsoTransportLock);
        }
    }
}

static NTSTATUS
OpenA8DJ_StartPersistentIsoEngine(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ USHORT OutputPacketBytes)
{
    NTSTATUS status = STATUS_SUCCESS;
    NTSTATUS lockStatus;
    LONG generation;
    ULONG currentFrameNumber = 0;
    ULONG slotIndex;

    lockStatus = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(lockStatus)) {
        return lockStatus;
    }
    if (Context->UsbDevice == NULL ||
        OutputPacketBytes == 0 ||
        Context->IsoInPipe == NULL ||
        Context->IsoOutPipe == NULL ||
        InterlockedCompareExchange(&Context->IsoTransportHealthy, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &Context->IsoTransportHealthyGeneration,
            0,
            0) != InterlockedCompareExchange(&Context->IsoEngineGeneration, 0, 0) ||
        WdfIoTargetGetState(WdfUsbTargetPipeGetIoTarget(Context->IsoInPipe)) !=
            WdfIoTargetStarted ||
        WdfIoTargetGetState(WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe)) !=
            WdfIoTargetStarted ||
        InterlockedCompareExchange(&Context->IsoEngineState, 0, 0) !=
            OpenA8DJIsoEngineStopped ||
        InterlockedCompareExchange(&Context->IsoOneShotActive, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoOutstandingCapture, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoOutstandingOutput, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoProcessWorkPending, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoProcessWorkActive, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoStopWorkPending, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->IsoStopWorkActive, 0, 0) != 0) {
        status = STATUS_DEVICE_BUSY;
        goto Exit;
    }
    for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
        if (Context->IsoCaptureSlots[slotIndex].Request == NULL ||
            Context->IsoOutputSlots[slotIndex].Request == NULL ||
            InterlockedCompareExchange(
                &Context->IsoCaptureSlots[slotIndex].State,
                0,
                0) != OpenA8DJIsoSlotIdle ||
            InterlockedCompareExchange(
                &Context->IsoOutputSlots[slotIndex].State,
                0,
                0) != OpenA8DJIsoSlotIdle) {
            status = STATUS_INVALID_DEVICE_STATE;
            goto Exit;
        }
    }

    generation = InterlockedIncrement(&Context->IsoEngineGeneration);
    InterlockedExchange64(&Context->IsoNextCaptureSubmitSequence, 0);
    InterlockedExchange64(&Context->IsoNextOutputSubmitSequence, 0);
    InterlockedExchange64(&Context->IsoNextCaptureProcessSequence, 1);
    InterlockedExchange64(&Context->IsoNextOutputProcessSequence, 1);
    InterlockedExchange(&Context->IsoConsecutiveNoDataCaptures, 0);
    InterlockedExchange(&Context->IsoProcessWorkPending, 0);
    InterlockedExchange(&Context->IsoProcessWorkActive, 0);
    KeClearEvent(&Context->IsoEngineDrainedEvent);
    InterlockedExchange(&Context->IsoEngineRunning, 0);
    InterlockedExchange(&Context->IsoEngineState, OpenA8DJIsoEnginePriming);

    /*
     * Do not reuse USBD_START_ISO_TRANSFER_ASAP across stream generations.
     * Windows tracks the next ASAP frame for up to 1024 frames after the
     * previous transfer.  A quick stop/start therefore makes a new first URB
     * look late.  Seed an explicit, contiguous capture schedule instead.
     */
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_FRAME_QUERY_START,
        STATUS_PENDING);
    InterlockedIncrement64(&Context->IsoCaptureFrameQueries);
    Context->IsoCaptureFrameQueryCurrent = 0;
    Context->IsoCaptureSeedStartFrame = 0;
    Context->IsoNextCaptureStartFrame = 0;
    status = WdfUsbTargetDeviceRetrieveCurrentFrameNumber(
        Context->UsbDevice,
        &currentFrameNumber);
    if (NT_SUCCESS(status)) {
        Context->IsoCaptureFrameQueryCurrent = currentFrameNumber;
        Context->IsoCaptureSeedStartFrame =
            currentFrameNumber + OPENA8DJ_ISO_CAPTURE_START_LEAD_FRAMES;
        Context->IsoNextCaptureStartFrame = Context->IsoCaptureSeedStartFrame;
    } else {
        InterlockedIncrement64(&Context->IsoCaptureFrameQueryFailures);
    }
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_ISO_FRAME_QUERY_COMPLETE,
        status);
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&Context->IsoEngineState, OpenA8DJIsoEngineStopped);
        goto Exit;
    }
    /* Capture owns cadence; output slots remain idle until an IN layout arrives. */
    for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
        POPENA8DJ_ISO_ENGINE_SLOT captureSlot = &Context->IsoCaptureSlots[slotIndex];
        POPENA8DJ_ISO_ENGINE_SLOT outputSlot = &Context->IsoOutputSlots[slotIndex];

        captureSlot->Generation = generation;
        outputSlot->Generation = generation;
        captureSlot->ConsecutiveFailures = 0;
        outputSlot->ConsecutiveFailures = 0;
        status = OpenA8DJ_PreparePersistentCaptureSlot(captureSlot);
        if (!NT_SUCCESS(status)) {
            break;
        }
        UNREFERENCED_PARAMETER(outputSlot);
    }
    if (NT_SUCCESS(status)) {
        InterlockedExchange(&Context->IsoEngineRunning, 1);
        InterlockedExchange(&Context->IsoEngineState, OpenA8DJIsoEngineRunning);
        for (slotIndex = 0; slotIndex < OPENA8DJ_ISO_ENGINE_SLOT_COUNT; slotIndex++) {
            status = OpenA8DJ_SubmitPersistentIsoSlot(
                &Context->IsoCaptureSlots[slotIndex]);
            if (!NT_SUCCESS(status)) {
                break;
            }
        }
    }
    if (!NT_SUCCESS(status)) {
        OpenA8DJ_RequestPersistentIsoFatalStopLocked(Context, status);
    }

Exit:
    WdfWaitLockRelease(Context->IsoTransportLock);
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
    if (outputOffset == 0 || outputOffset > OPENA8DJ_ISO_TRANSFER_BUFFER_CAPACITY) {
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

    transferBuffer = ExAllocatePoolZero(
        NonPagedPoolNx,
        OPENA8DJ_ISO_TRANSFER_BUFFER_CAPACITY,
        OPENA8DJ_POOL_TAG);
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
        if (transferBytes == 0 ||
            transferBytes > OPENA8DJ_ISO_TRANSFER_BUFFER_CAPACITY) {
            status = STATUS_INVALID_DEVICE_STATE;
            break;
        }

        RtlZeroMemory(transferBuffer, OPENA8DJ_ISO_TRANSFER_BUFFER_CAPACITY);
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

#if OPENA8DJ_VIRTUAL_MODE
static VOID
OpenA8DJ_RunVirtualStreamWorker(_Inout_ POPENA8DJ_DEVICE_CONTEXT Context)
{
    UCHAR playbackBuffer[OPENA8DJ_VIRTUAL_TICK_BYTES];
    LARGE_INTEGER delay;
    ULONG completedRenderMask;
    ULONG renderFrameCounts[OPENA8DJ_STEREO_PAIRS];
    LONG renderEpochs[OPENA8DJ_STEREO_PAIRS];
    PVOID renderStreams[OPENA8DJ_STEREO_PAIRS];

    delay.QuadPart = WDF_REL_TIMEOUT_IN_MS(1);
    Context->StreamState.Streaming = TRUE;
    Context->StreamState.StreamingEngineReady = TRUE;
    Context->StreamState.SampleRate = Context->ConfiguredSampleRate;
    Context->StreamState.BufferFrames = Context->CurrentFormat.BufferFrames;

    while (InterlockedCompareExchange(&Context->StreamStopRequested, 0, 0) == 0 &&
           InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) == 0 &&
           InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) != 0) {
        ULONG activeRenderMask = OpenA8DJ_GetActiveRenderMask(Context);
        ULONG activeCaptureMask = 0;
        ULONG captureBytes = 0;
        ULONG pairIndex;
        LARGE_INTEGER completionQpc;

        RtlZeroMemory(playbackBuffer, sizeof(playbackBuffer));
        completedRenderMask = 0;
        RtlZeroMemory(renderFrameCounts, sizeof(renderFrameCounts));
        RtlZeroMemory(renderEpochs, sizeof(renderEpochs));
        RtlZeroMemory(renderStreams, sizeof(renderStreams));
        if (activeRenderMask != 0) {
            OpenA8DJ_FillMode2FromActiveRtRenders(
                Context,
                playbackBuffer,
                sizeof(playbackBuffer),
                &completedRenderMask,
                renderFrameCounts,
                renderEpochs,
                renderStreams);
        } else {
            InterlockedIncrement64(&Context->StreamWorkerNoRenderIterations);
            OpenA8DJ_FillMode2Silence(playbackBuffer, sizeof(playbackBuffer));
        }

        for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
            POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
                OpenA8DJ_AcquireActiveStream(
                    Context,
                    &Context->ActiveCaptureStreams[pairIndex]);

            if (streamContext != NULL &&
                streamContext->RtKernelAddress != NULL &&
                streamContext->RtFrameCount != 0 &&
                streamContext->RtBlockAlign != 0) {
                ULONG frameIndex;

                activeCaptureMask |= (1u << pairIndex);
                RtlZeroMemory(
                    streamContext->InputFrameBytes,
                    sizeof(streamContext->InputFrameBytes));
                for (frameIndex = 0; frameIndex < OPENA8DJ_VIRTUAL_TICK_FRAMES; frameIndex++) {
                    OpenA8DJ_CompleteCaptureFrame(streamContext);
                }
                completionQpc = KeQueryPerformanceCounter(NULL);
                OpenA8DJ_AdvanceRtFrames(
                    streamContext,
                    OPENA8DJ_VIRTUAL_TICK_FRAMES,
                    (ULONGLONG)completionQpc.QuadPart,
                    InterlockedCompareExchange(&streamContext->RtPacketEpoch, 0, 0));
                captureBytes += OPENA8DJ_VIRTUAL_TICK_FRAMES * streamContext->RtBlockAlign;
                Context->StreamState.CaptureFramesDelivered += OPENA8DJ_VIRTUAL_TICK_FRAMES;
            }
            if (streamContext != NULL) {
                OpenA8DJ_ReleaseActiveStream(streamContext);
            }
        }

        completionQpc = KeQueryPerformanceCounter(NULL);
        for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
            POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
                (POPENA8DJ_ACX_STREAM_CONTEXT)renderStreams[pairIndex];

            if (streamContext != NULL) {
                if ((completedRenderMask & (1u << pairIndex)) != 0 &&
                    renderFrameCounts[pairIndex] != 0) {
                    OpenA8DJ_AdvanceRtFrames(
                        streamContext,
                        renderFrameCounts[pairIndex],
                        (ULONGLONG)completionQpc.QuadPart,
                        renderEpochs[pairIndex]);
                }
                OpenA8DJ_ReleaseActiveStream(streamContext);
                renderStreams[pairIndex] = NULL;
            }
        }
        OpenA8DJ_FlushActiveRtPacketNotifications(Context);

        InterlockedIncrement64(&Context->StreamWorkerIterations);
        InterlockedAdd64(&Context->StreamWorkerCaptureBytes, captureBytes);
        InterlockedAdd64(&Context->StreamWorkerPlaybackBytes, sizeof(playbackBuffer));
        Context->StreamWorkerLastCaptureBytes = captureBytes;
        Context->StreamWorkerLastPlaybackBytes = sizeof(playbackBuffer);
        Context->StreamWorkerLastRenderMask = activeRenderMask;
        Context->StreamWorkerLastCaptureMask = activeCaptureMask;
        if (captureBytes > Context->StreamWorkerMaxCaptureBytes) {
            Context->StreamWorkerMaxCaptureBytes = captureBytes;
        }
        if (sizeof(playbackBuffer) > Context->StreamWorkerMaxPlaybackBytes) {
            Context->StreamWorkerMaxPlaybackBytes = sizeof(playbackBuffer);
        }
        if (activeRenderMask != 0) {
            Context->StreamState.RenderFramesSubmitted += OPENA8DJ_VIRTUAL_TICK_FRAMES;
        }
        OpenA8DJ_RecordUsbPlaybackTrace(
            Context,
            playbackBuffer,
            sizeof(playbackBuffer),
            OPENA8DJ_VIRTUAL_TICK_BYTES,
            activeRenderMask);
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    Context->StreamState.Streaming = FALSE;
    Context->StreamState.StreamingEngineReady = FALSE;
    OpenA8DJ_ClearAllActiveStreams(Context);
    OpenA8DJ_InvalidateAudioParamsConfiguration(Context);
    InterlockedExchange(&Context->StreamWorkerActive, 0);
}
#endif

VOID
OpenA8DJ_EvtStreamWorkItem(_In_ WDFWORKITEM WorkItem)
{
    WDFDEVICE device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(device);
    NTSTATUS workerStatus = STATUS_SUCCESS;
    UCHAR configuredRateCode;
    USHORT configuredPacketBytes;
#if !OPENA8DJ_VIRTUAL_MODE
    UCHAR *playbackBuffer;
    UCHAR *captureTransferBuffer;
    ULONG captureTransferBufferLength;
    ULONG captureMaximumPacketSize;
    WDF_USB_PIPE_INFORMATION capturePipeInfo;
    WDF_OBJECT_ATTRIBUTES bufferAttributes;
    NTSTATUS bufferStatus;
    ULONG playbackPacketBytes;
    ULONG playbackLength;
    ULONG consecutiveCaptureFailures = 0;
    ULONG consecutiveOutputFailures = 0;
#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
    OPENA8DJ_ASYNC_ISO_OUTPUT_SLOT outputSlots[OPENA8DJ_ASYNC_OUTPUT_SLOTS];
    BOOLEAN useAsyncOutput = FALSE;
    ULONG nextOutputSlot = 0;
#endif
#endif

    OpenA8DJ_RecordSafetyCheckpoint(
        context,
        OPENA8DJ_CHECKPOINT_STREAM_WORKER_ENTER,
        STATUS_SUCCESS);
    if (InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&context->DevicePrepared, 0, 0) == 0) {
        OpenA8DJ_MarkStreamWorkerStopped(context);
        return;
    }

    if (InterlockedCompareExchange(&context->AudioParamsConfigured, 0, 0) == 0 ||
        !OpenA8DJ_GetAudioRateParameters(
            context->ConfiguredSampleRate,
            &configuredRateCode,
            &configuredPacketBytes)) {
        OpenA8DJ_MarkStreamWorkerStopped(context);
        return;
    }
#if OPENA8DJ_VIRTUAL_MODE
    OpenA8DJ_RunVirtualStreamWorker(context);
    return;
#else
    UNREFERENCED_PARAMETER(configuredRateCode);
    if (InterlockedCompareExchange(
            &gOpenA8DJPersistentAsyncIsoEnabled,
            0,
            0) != 0) {
        context->StreamState.Streaming = TRUE;
        context->StreamState.StreamingEngineReady = TRUE;
        context->StreamState.SampleRate = context->ConfiguredSampleRate;
        context->StreamState.BufferFrames = context->CurrentFormat.BufferFrames;
        workerStatus = OpenA8DJ_StartPersistentIsoEngine(
            context,
            configuredPacketBytes);
        if (!NT_SUCCESS(workerStatus)) {
            InterlockedExchange(&context->StreamStopRequested, 1);
            OpenA8DJ_PurgeIsoTargets(context, TRUE);
            OpenA8DJ_MarkStreamWorkerStopped(context);
        }
        return;
    }
    playbackPacketBytes = configuredPacketBytes;
    playbackLength = playbackPacketBytes * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;

    WDF_OBJECT_ATTRIBUTES_INIT(&bufferAttributes);
    bufferAttributes.ParentObject = context->UsbDevice;
    if (context->StreamPlaybackBufferMemory == NULL ||
        context->StreamPlaybackBuffer == NULL) {
        bufferStatus = WdfMemoryCreate(
            &bufferAttributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            4096u * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
            &context->StreamPlaybackBufferMemory,
            (PVOID *)&context->StreamPlaybackBuffer);
        if (!NT_SUCCESS(bufferStatus)) {
            OpenA8DJ_MarkStreamWorkerStopped(context);
            return;
        }
    }
    playbackBuffer = context->StreamPlaybackBuffer;
    if (context->IsoInPipe == NULL) {
        OpenA8DJ_MarkStreamWorkerStopped(context);
        return;
    }
    if (context->IsoOutPipe == NULL) {
        OpenA8DJ_MarkStreamWorkerStopped(context);
        return;
    }
    WDF_USB_PIPE_INFORMATION_INIT(&capturePipeInfo);
    WdfUsbTargetPipeGetInformation(context->IsoInPipe, &capturePipeInfo);
    captureMaximumPacketSize = capturePipeInfo.MaximumPacketSize;
    if (captureMaximumPacketSize == 0 || captureMaximumPacketSize > 4096) {
        OpenA8DJ_MarkStreamWorkerStopped(context);
        return;
    }
    captureTransferBufferLength = captureMaximumPacketSize * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT;
    if (context->StreamCaptureBufferMemory == NULL ||
        context->StreamCaptureBuffer == NULL) {
        bufferStatus = WdfMemoryCreate(
            &bufferAttributes,
            NonPagedPoolNx,
            OPENA8DJ_POOL_TAG,
            4096u * OPENA8DJ_ISO_SNAPSHOT_PACKET_COUNT,
            &context->StreamCaptureBufferMemory,
            (PVOID *)&context->StreamCaptureBuffer);
        if (!NT_SUCCESS(bufferStatus)) {
            OpenA8DJ_MarkStreamWorkerStopped(context);
            return;
        }
    }
    captureTransferBuffer = context->StreamCaptureBuffer;
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
    context->StreamState.SampleRate = context->ConfiguredSampleRate;
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
        ULONG completedRenderMask = 0;
        ULONG renderFrameCounts[OPENA8DJ_STEREO_PAIRS] = { 0 };
        LONG renderEpochs[OPENA8DJ_STEREO_PAIRS] = { 0 };
        PVOID renderStreams[OPENA8DJ_STEREO_PAIRS] = { NULL };
        ULONG pairIndex;
        UCHAR *outputBuffer = playbackBuffer;

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        if (useAsyncOutput) {
            outputBuffer = outputSlots[nextOutputSlot].Buffer;
        }
#endif

        InterlockedIncrement64(&context->StreamWorkerIterations);
        status = OpenA8DJ_CaptureIsoSnapshotWithPayload(
            context,
            &snapshot,
            captureTransferBuffer,
            captureTransferBufferLength,
            &capturePayloadBytes);
        if (NT_SUCCESS(status)) {
            consecutiveCaptureFailures = 0;
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
            workerStatus = status;
            if (InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
                InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0) {
                break;
            }
            context->StreamState.UsbPacketErrors++;
            consecutiveCaptureFailures++;
            delay.QuadPart = WDF_REL_TIMEOUT_IN_MS(1);
            KeDelayExecutionThread(KernelMode, FALSE, &delay);
            if (consecutiveCaptureFailures <= OPENA8DJ_STREAM_TRANSIENT_RETRY_LIMIT) {
                continue;
            }
            /* DeviceStopping remains reserved for PnP teardown. */
            InterlockedExchange(&context->StreamStopRequested, 1);
            (VOID)OpenA8DJ_AbortIsoInputPipe(context);
            break;
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
                                activeCaptureStream,
                                (ULONGLONG)KeQueryPerformanceCounter(NULL).QuadPart);
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

        /*
         * PnP release, stream pause, or a prior transfer failure may have
         * stopped the transport while capture was in flight.  Never submit
         * the paired output URB after that transition.
         */
        if (InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
            InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0) {
            break;
        }

        activeRenderMask = OpenA8DJ_GetActiveRenderMask(context);
        hasActiveRenderStream = activeRenderMask != 0;
        if (hasActiveRenderStream) {
#if OPENA8DJ_DEBUG_STREAM_TONE
            OPENA8DJ_MODE2_TONE_PACKER debugTonePacker;
            ULONG debugFramesRendered = playbackLength / OPENA8DJ_USB_OUTPUT_FRAME_BYTES;

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
                    completedRenderMask |= (1u << pairIndex);
                    renderFrameCounts[pairIndex] = debugFramesRendered;
                    renderEpochs[pairIndex] =
                        InterlockedCompareExchange(&streamContext->RtPacketEpoch, 0, 0);
                    renderStreams[pairIndex] = streamContext;
                }
                if (streamContext != NULL && renderStreams[pairIndex] == NULL) {
                    OpenA8DJ_ReleaseActiveStream(streamContext);
                }
            }
#else
            OpenA8DJ_FillMode2FromActiveRtRenders(
                context,
                outputBuffer,
                playbackLength,
                &completedRenderMask,
                renderFrameCounts,
                renderEpochs,
                renderStreams);
#endif
        } else {
            InterlockedIncrement64(&context->StreamWorkerNoRenderIterations);
            OpenA8DJ_FillMode2Silence(
                outputBuffer,
                playbackLength);
        }
        OpenA8DJ_FlushActiveRtPacketNotifications(context);
        OpenA8DJ_RecordUsbPlaybackTrace(
            context,
            outputBuffer,
            playbackLength,
            playbackPacketBytes,
            activeRenderMask);

#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        if (useAsyncOutput) {
            status = OpenA8DJ_QueueAsyncIsoOutputBuffer(
                context,
                &outputSlots[nextOutputSlot],
                &snapshot,
                playbackLength,
                playbackPacketBytes,
                &previousPlaybackErrors);
            playbackErrors += previousPlaybackErrors;
            nextOutputSlot = (nextOutputSlot + 1u) % OPENA8DJ_ASYNC_OUTPUT_SLOTS;
        } else {
#endif
            status = OpenA8DJ_SendIsoOutputBuffer(
                context,
                &snapshot,
                outputBuffer,
                playbackLength,
                playbackPacketBytes,
                &playbackErrors);
#if OPENA8DJ_ENABLE_ASYNC_OUTPUT
        }
#endif
        if (NT_SUCCESS(status) && playbackErrors == 0) {
            ULONG renderFramesCompleted = playbackLength / OPENA8DJ_USB_OUTPUT_FRAME_BYTES;
            LARGE_INTEGER completionQpc = KeQueryPerformanceCounter(NULL);

            for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
                POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
                    (POPENA8DJ_ACX_STREAM_CONTEXT)renderStreams[pairIndex];

                if (streamContext != NULL) {
                    if ((completedRenderMask & (1u << pairIndex)) != 0 &&
                        renderFrameCounts[pairIndex] != 0) {
                        OpenA8DJ_AdvanceRtFrames(
                            streamContext,
                            renderFrameCounts[pairIndex],
                            (ULONGLONG)completionQpc.QuadPart,
                            renderEpochs[pairIndex]);
                    }
                    OpenA8DJ_ReleaseActiveStream(streamContext);
                    renderStreams[pairIndex] = NULL;
                }
            }
            OpenA8DJ_FlushActiveRtPacketNotifications(context);

            consecutiveOutputFailures = 0;
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
            for (pairIndex = 0; pairIndex < OPENA8DJ_STEREO_PAIRS; pairIndex++) {
                POPENA8DJ_ACX_STREAM_CONTEXT streamContext =
                    (POPENA8DJ_ACX_STREAM_CONTEXT)renderStreams[pairIndex];

                if (streamContext != NULL) {
                    OpenA8DJ_ReleaseActiveStream(streamContext);
                    renderStreams[pairIndex] = NULL;
                }
            }
            context->StreamState.UsbUnderruns++;
            context->StreamState.UsbPacketErrors += playbackErrors != 0 ? playbackErrors : 1;
            if (!NT_SUCCESS(status)) {
                LARGE_INTEGER delay;

                workerStatus = status;
                if (InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
                    InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0) {
                    break;
                }
                consecutiveOutputFailures++;
                delay.QuadPart = WDF_REL_TIMEOUT_IN_MS(1);
                KeDelayExecutionThread(KernelMode, FALSE, &delay);
                if (consecutiveOutputFailures <= OPENA8DJ_STREAM_TRANSIENT_RETRY_LIMIT) {
                    continue;
                }
                InterlockedExchange(&context->StreamStopRequested, 1);
                (VOID)OpenA8DJ_AbortIsoOutputPipe(context);
                break;
            }
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
    /* Persistent legacy requests, plain URBs, and buffers remain idle here. */
    context->StreamState.Streaming = FALSE;
    OpenA8DJ_ClearAllActiveStreams(context);
    OpenA8DJ_InvalidateAudioParamsConfiguration(context);
    InterlockedExchange(&context->StreamWorkerActive, 0);
    OpenA8DJ_RecordSafetyCheckpoint(
        context,
        OPENA8DJ_CHECKPOINT_STREAM_WORKER_EXIT,
        workerStatus);
#endif
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
OpenA8DJ_StartEp1Reader(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ WDFUSBPIPE BulkInPipe)
{
    NTSTATUS status;
    WDF_USB_CONTINUOUS_READER_CONFIG readerConfig;
    WDFIOTARGET pipeTarget;

    if (BulkInPipe == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }
    pipeTarget = WdfUsbTargetPipeGetIoTarget(BulkInPipe);

    if (!NT_SUCCESS(Context->Ep1ReaderConfigStatus)) {
        // USB pipe targets are already started after configuration selection.
        // KMDF requires the continuous reader to be configured before the
        // WdfIoTargetStart that publishes its read requests.
        if (WdfIoTargetGetState(pipeTarget) == WdfIoTargetStarted) {
            WdfIoTargetStop(pipeTarget, WdfIoTargetCancelSentIo);
        }
        // Audio 8 DJ CAIAQ replies are 64-byte EP1 messages even though the
        // high-speed bulk endpoint advertises a larger maximum packet size.
        WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(BulkInPipe);
        WDF_USB_CONTINUOUS_READER_CONFIG_INIT(
            &readerConfig,
            OpenA8DJ_EvtEp1ReadComplete,
            Context,
            sizeof(Context->Ep1Reply));
        readerConfig.NumPendingReads = 2;
        readerConfig.EvtUsbTargetPipeReadersFailed = OpenA8DJ_EvtEp1ReadersFailed;

        status = WdfUsbTargetPipeConfigContinuousReader(
            BulkInPipe,
            &readerConfig);
        Context->Ep1ReaderConfigStatus = status;
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    if (WdfIoTargetGetState(pipeTarget) == WdfIoTargetStarted) {
        Context->Ep1ReaderStartStatus = STATUS_SUCCESS;
        return STATUS_SUCCESS;
    }
    status = WdfIoTargetStart(pipeTarget);
    Context->Ep1ReaderStartStatus = status;
    return status;
}

static VOID
OpenA8DJ_StopPipeTarget(_In_opt_ WDFUSBPIPE Pipe)
{
    if (Pipe != NULL) {
        WdfIoTargetStop(
            WdfUsbTargetPipeGetIoTarget(Pipe),
            WdfIoTargetCancelSentIo);
    }
}

static VOID
OpenA8DJ_PurgeIsoTargets(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ BOOLEAN RestartTargets)
{
    NTSTATUS purgeLockStatus;

    if (Context->IsoPurgeLock == NULL) {
        return;
    }
    purgeLockStatus = WdfWaitLockAcquire(Context->IsoPurgeLock, NULL);
    if (!NT_SUCCESS(purgeLockStatus)) {
        return;
    }
    if (RestartTargets &&
        (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
         InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0)) {
        InterlockedExchange(&Context->IsoTransportHealthy, 0);
        WdfWaitLockRelease(Context->IsoPurgeLock);
        return;
    }
    OpenA8DJ_PurgeIsoTargetsLocked(Context, RestartTargets);
    WdfWaitLockRelease(Context->IsoPurgeLock);
}

static VOID
OpenA8DJ_PurgeIsoTargetsLocked(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ BOOLEAN RestartTargets)
{
    WDFIOTARGET inputTarget = NULL;
    WDFIOTARGET outputTarget = NULL;
    NTSTATUS lockStatus;
    BOOLEAN drained;
    BOOLEAN engineDrained;
    BOOLEAN quiescent;
    BOOLEAN restartAllowed;
    NTSTATUS inputResetStatus = STATUS_SUCCESS;
    NTSTATUS outputResetStatus = STATUS_SUCCESS;
    NTSTATUS inputStartStatus = STATUS_SUCCESS;
    NTSTATUS outputStartStatus = STATUS_SUCCESS;
    NTSTATUS restartStatus = STATUS_SUCCESS;
    WDF_REQUEST_SEND_OPTIONS resetOptions;

    restartAllowed = RestartTargets &&
        InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) == 0 &&
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) != 0;
    lockStatus = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(lockStatus)) {
        return;
    }
    InterlockedExchange(&Context->IsoTransportDraining, 1);
    InterlockedExchange(&Context->IsoTransportHealthy, 0);
    InterlockedExchange(&Context->IsoEngineState, OpenA8DJIsoEngineStopping);
    KeClearEvent(&Context->IsoPurgeCompleteEvent);
    if (InterlockedExchange(&Context->IsoEngineRunning, 0) != 0) {
        InterlockedIncrement(&Context->IsoEngineGeneration);
    }
    KeClearEvent(&Context->IsoEngineDrainedEvent);

    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_DRAIN_ENTER,
        STATUS_SUCCESS);
    if (Context->IsoInPipe != NULL) {
        inputTarget = WdfUsbTargetPipeGetIoTarget(Context->IsoInPipe);
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_PURGE_INPUT_ENTER,
            STATUS_PENDING);
        WdfIoTargetPurge(inputTarget, WdfIoTargetPurgeIoAndWait);
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_PURGE_INPUT_COMPLETE,
            STATUS_SUCCESS);
    }
    if (Context->IsoOutPipe != NULL) {
        outputTarget = WdfUsbTargetPipeGetIoTarget(Context->IsoOutPipe);
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_PURGE_OUTPUT_ENTER,
            STATUS_PENDING);
        WdfIoTargetPurge(outputTarget, WdfIoTargetPurgeIoAndWait);
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_PURGE_OUTPUT_COMPLETE,
            STATUS_SUCCESS);
    }
    WdfWaitLockRelease(Context->IsoTransportLock);

    /* Completion never blocks on the lifecycle lock; flush its passive owner. */
    if (Context->IsoProcessWorkItem != NULL) {
        WdfWorkItemFlush(Context->IsoProcessWorkItem);
    }

    /*
     * Microsoft documents that USBD_START_ISO_TRANSFER_ASAP tracks the next
     * frame per pipe until the pipe is reset (or remains idle for 1024
     * frames).  Purge alone does not reset that tracking.  Reset the stopped,
     * fully drained input target before restarting it so rapid stream
     * generations retain ASAP quality without stale-frame late packets.
     */
    if (restartAllowed && Context->IsoInPipe != NULL && inputTarget != NULL) {
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_INPUT_RESET_START,
            STATUS_PENDING);
        InterlockedIncrement64(&Context->IsoInputPipeResetRuns);
        WDF_REQUEST_SEND_OPTIONS_INIT(
            &resetOptions,
            WDF_REQUEST_SEND_OPTION_TIMEOUT |
            WDF_REQUEST_SEND_OPTION_IGNORE_TARGET_STATE);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
            &resetOptions,
            WDF_REL_TIMEOUT_IN_SEC(1));
        inputResetStatus = WdfUsbTargetPipeResetSynchronously(
            Context->IsoInPipe,
            NULL,
            &resetOptions);
        InterlockedExchange(
            &Context->IsoInputPipeResetLastNtStatus,
            (LONG)inputResetStatus);
        if (!NT_SUCCESS(inputResetStatus)) {
            InterlockedIncrement64(&Context->IsoInputPipeResetFailures);
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_INPUT_RESET_COMPLETE,
            inputResetStatus);
    }
    if (restartAllowed && Context->IsoOutPipe != NULL && outputTarget != NULL) {
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RESET_START,
            STATUS_PENDING);
        InterlockedIncrement64(&Context->IsoOutputPipeResetRuns);
        WDF_REQUEST_SEND_OPTIONS_INIT(
            &resetOptions,
            WDF_REQUEST_SEND_OPTION_TIMEOUT |
            WDF_REQUEST_SEND_OPTION_IGNORE_TARGET_STATE);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
            &resetOptions,
            WDF_REL_TIMEOUT_IN_SEC(1));
        outputResetStatus = WdfUsbTargetPipeResetSynchronously(
            Context->IsoOutPipe,
            NULL,
            &resetOptions);
        InterlockedExchange(
            &Context->IsoOutputPipeResetLastNtStatus,
            (LONG)outputResetStatus);
        if (!NT_SUCCESS(outputResetStatus)) {
            InterlockedIncrement64(&Context->IsoOutputPipeResetFailures);
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_OUTPUT_RESET_COMPLETE,
            outputResetStatus);
    }

    lockStatus = WdfWaitLockAcquire(Context->IsoTransportLock, NULL);
    if (!NT_SUCCESS(lockStatus)) {
        return;
    }
    quiescent =
        InterlockedCompareExchange(&Context->IsoOutstandingCapture, 0, 0) == 0 &&
        InterlockedCompareExchange(&Context->IsoOutstandingOutput, 0, 0) == 0 &&
        InterlockedCompareExchange(&Context->IsoProcessWorkPending, 0, 0) == 0 &&
        InterlockedCompareExchange(&Context->IsoProcessWorkActive, 0, 0) == 0 &&
        OpenA8DJ_ArePersistentIsoSlotsIdle(Context);
    if (quiescent) {
        InterlockedExchange(&Context->IsoEngineState, OpenA8DJIsoEngineStopped);
    }
    if (!NT_SUCCESS(inputResetStatus)) {
        restartStatus = inputResetStatus;
    } else if (!NT_SUCCESS(outputResetStatus)) {
        restartStatus = outputResetStatus;
    }
    if (quiescent && restartAllowed && NT_SUCCESS(inputResetStatus) &&
        NT_SUCCESS(outputResetStatus) &&
        InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) == 0 &&
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) != 0) {
        if (inputTarget != NULL) {
            OpenA8DJ_RecordSafetyCheckpoint(
                Context,
                OPENA8DJ_CHECKPOINT_ISO_INPUT_START_ENTER,
                STATUS_PENDING);
            inputStartStatus = WdfIoTargetStart(inputTarget);
        } else {
            inputStartStatus = STATUS_DEVICE_NOT_READY;
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_INPUT_START_COMPLETE,
            inputStartStatus);
        if (NT_SUCCESS(inputStartStatus) && outputTarget != NULL) {
            OpenA8DJ_RecordSafetyCheckpoint(
                Context,
                OPENA8DJ_CHECKPOINT_ISO_OUTPUT_START_ENTER,
                STATUS_PENDING);
            outputStartStatus = WdfIoTargetStart(outputTarget);
        } else if (outputTarget == NULL) {
            outputStartStatus = STATUS_DEVICE_NOT_READY;
        } else {
            outputStartStatus = STATUS_CANCELLED;
        }
        OpenA8DJ_RecordSafetyCheckpoint(
            Context,
            OPENA8DJ_CHECKPOINT_ISO_OUTPUT_START_COMPLETE,
            outputStartStatus);
        InterlockedExchange(
            &Context->IsoInputTargetStartLastNtStatus,
            (LONG)inputStartStatus);
        InterlockedExchange(
            &Context->IsoOutputTargetStartLastNtStatus,
            (LONG)outputStartStatus);
        restartStatus = !NT_SUCCESS(inputStartStatus) ?
            inputStartStatus : outputStartStatus;
        if (NT_SUCCESS(restartStatus)) {
            InterlockedExchange(
                &Context->IsoTransportHealthyGeneration,
                InterlockedCompareExchange(&Context->IsoEngineGeneration, 0, 0));
            InterlockedExchange(&Context->IsoTransportHealthy, 1);
        } else {
            /* A partial restart is never exposed to the next stream. */
            if (inputTarget != NULL) {
                OpenA8DJ_RecordSafetyCheckpoint(
                    Context,
                    OPENA8DJ_CHECKPOINT_ISO_REPURGE_INPUT_ENTER,
                    STATUS_PENDING);
                WdfIoTargetPurge(inputTarget, WdfIoTargetPurgeIoAndWait);
                OpenA8DJ_RecordSafetyCheckpoint(
                    Context,
                    OPENA8DJ_CHECKPOINT_ISO_REPURGE_INPUT_COMPLETE,
                    STATUS_SUCCESS);
            }
            if (outputTarget != NULL) {
                OpenA8DJ_RecordSafetyCheckpoint(
                    Context,
                    OPENA8DJ_CHECKPOINT_ISO_REPURGE_OUTPUT_ENTER,
                    STATUS_PENDING);
                WdfIoTargetPurge(outputTarget, WdfIoTargetPurgeIoAndWait);
                OpenA8DJ_RecordSafetyCheckpoint(
                    Context,
                    OPENA8DJ_CHECKPOINT_ISO_REPURGE_OUTPUT_COMPLETE,
                    STATUS_SUCCESS);
            }
        }
    }
    InterlockedExchange(&Context->IsoTransportDraining, 0);
    engineDrained = quiescent && OpenA8DJ_TrySignalPersistentIsoDrained(Context);
    drained = engineDrained && NT_SUCCESS(inputResetStatus) &&
        NT_SUCCESS(outputResetStatus) &&
        NT_SUCCESS(restartStatus) &&
        (!restartAllowed ||
         InterlockedCompareExchange(&Context->IsoTransportHealthy, 0, 0) != 0);
    KeSetEvent(&Context->IsoPurgeCompleteEvent, IO_NO_INCREMENT, FALSE);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_DRAIN_COMPLETE,
        drained ? STATUS_SUCCESS :
            (!NT_SUCCESS(inputResetStatus) ? inputResetStatus :
             (!NT_SUCCESS(outputResetStatus) ? outputResetStatus :
              (!NT_SUCCESS(restartStatus) ? restartStatus : STATUS_DEVICE_BUSY))));
    WdfWaitLockRelease(Context->IsoTransportLock);
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
    LARGE_INTEGER retryDelay;
    BOOLEAN commandLockHeld = FALSE;
    BOOLEAN allowRetry;
    ULONG attemptLimit;
    WDFUSBPIPE bulkOutPipe;
    WDFUSBPIPE bulkInPipe;

    *ReplyLength = 0;
    if (WriteStatus != NULL) {
        *WriteStatus = STATUS_SUCCESS;
    }
    if (ReadStatus != NULL) {
        *ReadStatus = STATUS_SUCCESS;
    }
    if (PayloadLength + 1u > sizeof(commandBuffer)) {
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)STATUS_INVALID_BUFFER_SIZE;
        }
        return STATUS_INVALID_BUFFER_SIZE;
    }

    if (Context->BulkCommandLock == NULL) {
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)STATUS_DEVICE_NOT_READY;
        }
        return STATUS_DEVICE_NOT_READY;
    }

    status = WdfWaitLockAcquire(Context->BulkCommandLock, NULL);
    if (!NT_SUCCESS(status)) {
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)status;
        }
        return status;
    }
    commandLockHeld = TRUE;

    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0 ||
        Context->BulkOutPipe == NULL || Context->BulkInPipe == NULL) {
        status = STATUS_DEVICE_NOT_READY;
        if (WriteStatus != NULL) {
            *WriteStatus = (ULONG)status;
        }
        WdfWaitLockRelease(Context->BulkCommandLock);
        return status;
    }
    bulkOutPipe = Context->BulkOutPipe;
    bulkInPipe = Context->BulkInPipe;

    status = OpenA8DJ_StartEp1Reader(Context, bulkInPipe);
    if (!NT_SUCCESS(status)) {
        if (ReadStatus != NULL) {
            *ReadStatus = (ULONG)status;
        }
        WdfWaitLockRelease(Context->BulkCommandLock);
        return status;
    }

    commandBuffer[0] = Command;
    if (PayloadLength > 0 && Payload != NULL) {
        RtlCopyMemory(commandBuffer + 1, Payload, PayloadLength);
    }
    commandLength = PayloadLength + 1u;
    retryDelay.QuadPart = WDF_REL_TIMEOUT_IN_MS(OPENA8DJ_BULK_WRITE_RETRY_DELAY_MS);
    allowRetry = OpenA8DJ_IsIdempotentBulkCommand(Command);
    attemptLimit = allowRetry ? OPENA8DJ_BULK_WRITE_RETRY_LIMIT : 1u;

    if (Reply == NULL || ReplyCapacity == 0) {
        status = STATUS_UNSUCCESSFUL;
        for (attempt = 0; attempt < attemptLimit; attempt++) {
            if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
                InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }
            if (attempt != 0) {
                (VOID)KeDelayExecutionThread(KernelMode, FALSE, &retryDelay);
                if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
                    InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
                    status = STATUS_DEVICE_NOT_READY;
                    break;
                }
            }
            bytesTransferred = 0;
            status = OpenA8DJ_SendBulkPipeSynchronously(
                bulkOutPipe,
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
        WdfWaitLockRelease(Context->BulkCommandLock);
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
    for (attempt = 0; attempt < attemptLimit; attempt++) {
        if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
            InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
            status = STATUS_DEVICE_NOT_READY;
            break;
        }
        if (attempt != 0) {
            (VOID)KeDelayExecutionThread(KernelMode, FALSE, &retryDelay);
            if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
                InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }
        }
        bytesTransferred = 0;
        status = OpenA8DJ_SendBulkPipeSynchronously(
            bulkOutPipe,
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
        if (commandLockHeld) {
            WdfWaitLockRelease(Context->BulkCommandLock);
        }
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

    if (commandLockHeld) {
        WdfWaitLockRelease(Context->BulkCommandLock);
    }

    return status;
}

static BOOLEAN
OpenA8DJ_IsIdempotentBulkCommand(_In_ UCHAR Command)
{
    switch (Command) {
    case 0x01: /* device information query */
    case OPENA8DJ_CAIAQ_COMMAND_READ_IO:
    case OPENA8DJ_CAIAQ_COMMAND_WRITE_IO: /* complete absolute control state */
    case OPENA8DJ_CAIAQ_COMMAND_AUTO_MSG: /* complete absolute reader state */
        return TRUE;
    default:
        return FALSE;
    }
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
        status = OpenA8DJ_SendBulkCommand(
            Context,
            OPENA8DJ_CAIAQ_COMMAND_AUTO_MSG,
            autoMsgPayload,
            sizeof(autoMsgPayload),
            NULL,
            0,
            &replyLength,
            &writeStatus,
            &readStatus);
        if (!NT_SUCCESS(status)) {
            continue;
        }

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

static BOOLEAN
OpenA8DJ_GetAudioRateParameters(
    _In_ ULONG SampleRate,
    _Out_ PUCHAR RateCode,
    _Out_ PUSHORT BytesPerPacket)
{
    ULONG bytesPerPacket;

    if (RateCode == NULL || BytesPerPacket == NULL) {
        return FALSE;
    }
    *RateCode = 0;
    *BytesPerPacket = 0;

    switch (SampleRate) {
    case 44100:
        *RateCode = 0;
        break;
    case 48000:
        *RateCode = 1;
        break;
    default:
        return FALSE;
    }

    /*
     * CAIAQ mode-2 packet sizing: rate frames plus the bounded five-frame
     * drift allowance, four USB bytes per sample, and four stereo streams.
     * This mirrors the repository's OpenA8DJUSB.m transport oracle.
     */
    bytesPerPacket = ((SampleRate / 8000u) + OPENA8DJ_CLOCK_DRIFT_TOLERANCE) *
                     OPENA8DJ_USB_BYTES_PER_SAMPLE *
                     2u *
                     OPENA8DJ_AUDIO_STREAM_COUNT;
    if (bytesPerPacket == 0 || bytesPerPacket > 512u) {
        return FALSE;
    }

    *BytesPerPacket = (USHORT)bytesPerPacket;
    return TRUE;
}

static NTSTATUS
OpenA8DJ_ApplyAudioParams(
    _In_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG SampleRate,
    _Out_ POPENA8DJ_AUDIO_PARAMS_RESULT Result)
{
    NTSTATUS status;
    UCHAR rateCode;
    USHORT bytesPerPacket;
    UCHAR resetPayload[5];
    UCHAR setPayload[5];
    ULONG replyLength;
    ULONG writeStatus;
    ULONG readStatus;

    if (!OpenA8DJ_GetAudioRateParameters(SampleRate, &rateCode, &bytesPerPacket)) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Result, sizeof(*Result));
    Result->Size = sizeof(*Result);
    Result->SampleRate = SampleRate;
    Result->RateCode = rateCode;
    Result->Depth = 2;
    Result->BytesPerPacket = bytesPerPacket;

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
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (replyLength < 1 || Result->DeviceInfoReply[0] != 0x01) {
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

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
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (replyLength < 2 ||
        Result->ResetReply[0] != 0x09 ||
        Result->ResetReply[1] != 0) {
        Result->ResetNtStatus = (ULONG)STATUS_DEVICE_PROTOCOL_ERROR;
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

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
        Result->SetNtStatus = (ULONG)STATUS_DEVICE_PROTOCOL_ERROR;
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS
OpenA8DJ_WaitForAudioRateSettle(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ USHORT ExpectedPacketBytes)
{
    LARGE_INTEGER startCounter;
    LARGE_INTEGER frequency;
    LONGLONG budgetTicks;
    NTSTATUS status = STATUS_SUCCESS;

    if (ExpectedPacketBytes == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    InterlockedIncrement64(&Context->AudioRateSettleRuns);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_AUDIO_RATE_SETTLE_START,
        STATUS_PENDING);
    startCounter = KeQueryPerformanceCounter(&frequency);
    if (frequency.QuadPart <= 0) {
        status = STATUS_INVALID_DEVICE_STATE;
        goto Failure;
    }
    budgetTicks = (frequency.QuadPart / 1000) * OPENA8DJ_AUDIO_RATE_SETTLE_BUDGET_MS;
    if (budgetTicks <= 0) {
        budgetTicks = 1;
    }
    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0) {
        status = STATUS_DEVICE_NOT_READY;
        goto Failure;
    }
    InterlockedExchange(&Context->AudioRateSettleConsecutiveClean, 0);
    InterlockedExchange(
        &Context->AudioRateSettleAttemptsRemaining,
        OPENA8DJ_AUDIO_RATE_SETTLE_MAX_SNAPSHOTS);
    InterlockedExchange(&Context->AudioRateSettleLastObservedBytes, 0);
    InterlockedExchange64(&Context->AudioRateSettleStartQpc, startCounter.QuadPart);
    InterlockedExchange64(&Context->AudioRateSettleBudgetQpc, budgetTicks);
    /* Publish active last; persistent capture completions perform the proof. */
    InterlockedExchange(&Context->AudioRateSettleActive, 1);
    return STATUS_SUCCESS;

Failure:
    InterlockedIncrement64(&Context->AudioRateSettleFailures);
    OpenA8DJ_RecordSafetyCheckpoint(
        Context,
        OPENA8DJ_CHECKPOINT_AUDIO_RATE_SETTLE_COMPLETE,
        status);
    return status;
}

static NTSTATUS
OpenA8DJ_ConfigureAudioParams(
    _Inout_ POPENA8DJ_DEVICE_CONTEXT Context,
    _In_ ULONG SampleRate,
    _Out_ POPENA8DJ_AUDIO_PARAMS_RESULT Result)
{
    NTSTATUS status;
    UCHAR rateCode;
    USHORT bytesPerPacket;

    if (!OpenA8DJ_GetAudioRateParameters(SampleRate, &rateCode, &bytesPerPacket)) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Result, sizeof(*Result));
    Result->Size = sizeof(*Result);
    Result->SampleRate = SampleRate;
    Result->RateCode = rateCode;
    Result->Depth = 2;
    Result->BytesPerPacket = bytesPerPacket;

    if (Context->AudioConfigLock == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    status = WdfWaitLockAcquire(Context->AudioConfigLock, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

#if OPENA8DJ_VIRTUAL_MODE
    if (!OpenA8DJ_IsTransportReady(Context)) {
        WdfWaitLockRelease(Context->AudioConfigLock);
        return STATUS_DEVICE_NOT_READY;
    }
    if (InterlockedCompareExchange(&Context->StreamWorkerActive, 0, 0) != 0 &&
        InterlockedCompareExchange(&Context->AudioParamsConfigured, 0, 0) != 0 &&
        Context->ConfiguredSampleRate != SampleRate) {
        WdfWaitLockRelease(Context->AudioConfigLock);
        return STATUS_DEVICE_BUSY;
    }
    Context->ConfiguredSampleRate = SampleRate;
    InterlockedExchange(&Context->AudioParamsConfigured, 1);
    Context->CurrentFormat.SampleRate = SampleRate;
    Context->StreamState.SampleRate = SampleRate;
    WdfWaitLockRelease(Context->AudioConfigLock);
    return STATUS_SUCCESS;
#else
    if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
        InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0 ||
        !OpenA8DJ_IsTransportReady(Context)) {
        WdfWaitLockRelease(Context->AudioConfigLock);
        return STATUS_DEVICE_NOT_READY;
    }
    if (InterlockedCompareExchange(&Context->AudioParamsConfigured, 0, 0) != 0 &&
        Context->ConfiguredSampleRate == SampleRate) {
        Context->CurrentFormat.SampleRate = SampleRate;
        Context->StreamState.SampleRate = SampleRate;
        WdfWaitLockRelease(Context->AudioConfigLock);
        return STATUS_SUCCESS;
    }
    if (InterlockedCompareExchange(&Context->StreamWorkerActive, 0, 0) != 0 &&
        InterlockedCompareExchange(&Context->AudioParamsConfigured, 0, 0) != 0) {
        WdfWaitLockRelease(Context->AudioConfigLock);
        return STATUS_DEVICE_BUSY;
    }

    status = OpenA8DJ_ApplyAudioParams(Context, SampleRate, Result);
    if (NT_SUCCESS(status)) {
        status = OpenA8DJ_WaitForAudioRateSettle(Context, bytesPerPacket);
        if (NT_SUCCESS(status)) {
            if (InterlockedCompareExchange(&Context->DeviceStopping, 0, 0) != 0 ||
                InterlockedCompareExchange(&Context->DevicePrepared, 0, 0) == 0 ||
                !OpenA8DJ_IsTransportReady(Context)) {
                status = STATUS_DEVICE_NOT_READY;
            } else {
                Context->ConfiguredSampleRate = SampleRate;
                InterlockedExchange(&Context->AudioParamsConfigured, 1);
                Context->CurrentFormat.SampleRate = SampleRate;
                Context->StreamState.SampleRate = SampleRate;
            }
        }
    }
    WdfWaitLockRelease(Context->AudioConfigLock);
    return status;
#endif
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    ACX_DRIVER_CONFIG acxConfig;
    WDFDRIVER driver;
    WDF_OBJECT_ATTRIBUTES attributes;
    NTSTATUS status;

#if !OPENA8DJ_VIRTUAL_MODE
    /*
     * A physical candidate is load-once.  Persist and flush Start=disabled
     * before WDF/ACX or USB initialization.  The canary explicitly changes
     * Start to demand immediately before this load; a crash therefore cannot
     * make the candidate load again during the next boot.
     */
    status = OpenA8DJ_ArmNextBootFailSafe(RegistryPath);
    if (!NT_SUCCESS(status)) {
        return status;
    }
#endif
    WPP_INIT_TRACING(DriverObject, RegistryPath);
    WDF_DRIVER_CONFIG_INIT(&config, OpenA8DJ_EvtDeviceAdd);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = OpenA8DJ_EvtDriverContextCleanup;

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: DriverEntry\n"));

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        &driver);
    if (!NT_SUCCESS(status)) {
        WPP_CLEANUP(DriverObject);
        return status;
    }

    ACX_DRIVER_CONFIG_INIT(&acxConfig);
    return AcxDriverInitialize(driver, &acxConfig);
}

static NTSTATUS
OpenA8DJ_ArmNextBootFailSafe(_In_ PUNICODE_STRING RegistryPath)
{
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING startName;
    HANDLE serviceKey = NULL;
    ULONG disabled = 4u;
    NTSTATUS status;

    InitializeObjectAttributes(
        &attributes,
        RegistryPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);
    status = ZwOpenKey(&serviceKey, KEY_SET_VALUE, &attributes);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlInitUnicodeString(&startName, L"Start");
    status = ZwSetValueKey(
        serviceKey,
        &startName,
        0,
        REG_DWORD,
        &disabled,
        sizeof(disabled));
    if (NT_SUCCESS(status)) {
        status = ZwFlushKey(serviceKey);
    }
    ZwClose(serviceKey);
    return status;
}

static VOID
OpenA8DJ_EvtDriverContextCleanup(_In_ WDFOBJECT DriverObject)
{
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
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
    pnpCallbacks.EvtDeviceReleaseHardware = OpenA8DJ_EvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, OPENA8DJ_DEVICE_CONTEXT);
    attributes.ExecutionLevel = WdfExecutionLevelPassive;

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

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    status = WdfWaitLockCreate(&attributes, &context->BulkCommandLock);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: WdfWaitLockCreate failed 0x%08x\n",
                   status));
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    status = WdfWaitLockCreate(&attributes, &context->AudioConfigLock);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: audio config WdfWaitLockCreate failed 0x%08x\n",
                   status));
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    status = WdfWaitLockCreate(&attributes, &context->IsoTransportLock);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: ISO transport WdfWaitLockCreate failed 0x%08x\n",
                   status));
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    status = WdfWaitLockCreate(&attributes, &context->IsoPurgeLock);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: ISO purge WdfWaitLockCreate failed 0x%08x\n",
                   status));
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    status = WdfWaitLockCreate(&attributes, &context->SafetyCheckpointLock);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: safety checkpoint WdfWaitLockCreate failed 0x%08x\n",
                   status));
        return status;
    }

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

    WDF_WORKITEM_CONFIG_INIT(&workItemConfig, OpenA8DJ_EvtIsoProcessWorkItem);
    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = device;
    status = WdfWorkItemCreate(
        &workItemConfig,
        &workItemAttributes,
        &context->IsoProcessWorkItem);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: ISO process WdfWorkItemCreate failed 0x%08x\n",
                   status));
        return status;
    }

    WDF_WORKITEM_CONFIG_INIT(&workItemConfig, OpenA8DJ_EvtIsoStopWorkItem);
    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = device;
    status = WdfWorkItemCreate(
        &workItemConfig,
        &workItemAttributes,
        &context->IsoStopWorkItem);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: ISO stop WdfWorkItemCreate failed 0x%08x\n",
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
#if !OPENA8DJ_VIRTUAL_MODE
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    UCHAR interfaceIndex;
#endif

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    InterlockedExchange(&context->DeviceStopping, 1);
    InterlockedExchange(&context->DevicePrepared, 0);
    InterlockedExchange(&context->CanaryPhase, OPENA8DJ_CANARY_PHASE_NONE);
    InterlockedExchange(&context->CanaryOperationsRemaining, 0);
    context->CanaryNonceHigh = 0;
    context->CanaryNonceLow = 0;
    OpenA8DJ_RecordSafetyCheckpoint(
        context,
        OPENA8DJ_CHECKPOINT_PREPARE_ENTER,
        STATUS_SUCCESS);

#if OPENA8DJ_VIRTUAL_MODE
    status = OpenA8DJ_AddAcxCircuits(Device, context);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJVirtual: ACX circuit registration failed 0x%08x\n",
                   status));
        return status;
    }

    InterlockedExchange(&context->DeviceStopping, 0);
    InterlockedExchange(&context->DevicePrepared, 1);
    return STATUS_SUCCESS;
#else
    if (context->UsbDevice == NULL) {
        status = WdfUsbTargetDeviceCreate(
            Device,
            WDF_NO_OBJECT_ATTRIBUTES,
            &context->UsbDevice);
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "OpenA8DJUsb: WdfUsbTargetDeviceCreate failed 0x%08x\n",
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
    /*
     * The mapped USB transport is now prepared enough to allocate its
     * parent-owned requests.  Publish this state before Ensure so its
     * teardown guard does not reject the legitimate PrepareHardware path.
     * KMDF does not expose the device to client I/O until this callback
     * returns; ReleaseHardware can still revoke both flags at any point.
     */
    InterlockedExchange(&context->DeviceStopping, 0);
    InterlockedExchange(&context->DevicePrepared, 1);
    status = OpenA8DJ_EnsureIsoTransportResources(context);
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&context->DeviceStopping, 1);
        InterlockedExchange(&context->DevicePrepared, 0);
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: persistent ISO transport allocation failed 0x%08x\n",
                   status));
        return status;
    }
    if (context->IsoInPipe == NULL || context->IsoOutPipe == NULL ||
        WdfIoTargetGetState(WdfUsbTargetPipeGetIoTarget(context->IsoInPipe)) !=
            WdfIoTargetStarted ||
        WdfIoTargetGetState(WdfUsbTargetPipeGetIoTarget(context->IsoOutPipe)) !=
            WdfIoTargetStarted) {
        InterlockedExchange(&context->DeviceStopping, 1);
        InterlockedExchange(&context->DevicePrepared, 0);
        return STATUS_DEVICE_NOT_READY;
    }
    InterlockedExchange(
        &context->IsoTransportHealthyGeneration,
        InterlockedCompareExchange(&context->IsoEngineGeneration, 0, 0));
    InterlockedExchange(&context->IsoTransportHealthy, 1);
    status = OpenA8DJ_AddAcxCircuits(Device, context);
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&context->DeviceStopping, 1);
        InterlockedExchange(&context->DevicePrepared, 0);
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: ACX circuit registration failed 0x%08x\n",
                   status));
        return status;
    }
    /*
     * Physical load is deliberately inert.  Do not start a continuous USB
     * reader, configure audio, or submit isochronous traffic during PnP
     * prepare.  A one-shot canary IOCTL must authorize the first operation.
     */
    context->ControlsHardwareReady = FALSE;
    context->Ep1ReaderConfigStatus = STATUS_DEVICE_NOT_READY;
    context->Ep1ReaderStartStatus = STATUS_DEVICE_NOT_READY;

    InterlockedExchange(&context->DeviceStopping, 0);
    InterlockedExchange(&context->DevicePrepared, 1);
    OpenA8DJ_RecordSafetyCheckpoint(
        context,
        OPENA8DJ_CHECKPOINT_PREPARE_SAFE,
        STATUS_SUCCESS);

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
#endif
}

NTSTATUS
OpenA8DJ_EvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    POPENA8DJ_DEVICE_CONTEXT context = OpenA8DJGetDeviceContext(Device);
    NTSTATUS circuitStatus;
    NTSTATUS destroyStatus;
    NTSTATUS bulkLockStatus;
    NTSTATUS purgeLockStatus;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    purgeLockStatus = WdfWaitLockAcquire(context->IsoPurgeLock, NULL);
    if (!NT_SUCCESS(purgeLockStatus)) {
        return purgeLockStatus;
    }
    InterlockedExchange(&context->DeviceStopping, 1);
    InterlockedExchange(&context->DevicePrepared, 0);
    InterlockedExchange(&context->StreamStopRequested, 1);
    InterlockedExchange(&context->CanaryPhase, OPENA8DJ_CANARY_PHASE_NONE);
    InterlockedExchange(&context->CanaryOperationsRemaining, 0);

    bulkLockStatus = WdfWaitLockAcquire(context->BulkCommandLock, NULL);
    if (!NT_SUCCESS(bulkLockStatus)) {
        WdfWaitLockRelease(context->IsoPurgeLock);
        return bulkLockStatus;
    }
    OpenA8DJ_StopPipeTarget(context->BulkInPipe);
    OpenA8DJ_StopPipeTarget(context->BulkOutPipe);
    WdfWaitLockRelease(context->BulkCommandLock);
    OpenA8DJ_PurgeIsoTargetsLocked(context, FALSE);
    WdfWaitLockRelease(context->IsoPurgeLock);

    if (context->StreamWorkItem != NULL) {
        WdfWorkItemFlush(context->StreamWorkItem);
    }
    if (context->IsoProcessWorkItem != NULL) {
        WdfWorkItemFlush(context->IsoProcessWorkItem);
    }
    if (context->IsoStopWorkItem != NULL) {
        WdfWorkItemFlush(context->IsoStopWorkItem);
    }
    destroyStatus = OpenA8DJ_DestroyIsoTransportResources(context);
    if (!NT_SUCCESS(destroyStatus)) {
        OpenA8DJ_RecordSafetyCheckpoint(
            context,
            OPENA8DJ_CHECKPOINT_DRAIN_COMPLETE,
            destroyStatus);
        return destroyStatus;
    }

    OpenA8DJ_ClearAllActiveStreams(context);
    OpenA8DJ_InvalidateAudioParamsConfiguration(context);
    context->StreamState.Streaming = FALSE;
    context->StreamState.StreamingEngineReady = FALSE;
    circuitStatus = OpenA8DJ_RemoveAcxCircuits(Device, context);
    OpenA8DJ_ResetPipeMap(context);
    RtlZeroMemory(context->UsbInterfaces, sizeof(context->UsbInterfaces));

    if (!NT_SUCCESS(circuitStatus)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "OpenA8DJUsb: ACX circuit removal failed 0x%08x\n",
                   circuitStatus));
        return circuitStatus;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID,
               DPFLTR_INFO_LEVEL,
               "OpenA8DJUsb: release hardware completed after stopping USB targets and stream worker\n"));
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

    if (IoControlCode == IOCTL_OPENA8DJ_ARM_PHYSICAL_CANARY) {
        POPENA8DJ_CANARY_ARM_REQUEST arm = NULL;

        status = OpenA8DJ_RetrieveInput(
            Request,
            sizeof(OPENA8DJ_CANARY_ARM_REQUEST),
            (PVOID *)&arm);
        if (NT_SUCCESS(status)) {
            if (arm->Size != sizeof(*arm) ||
                arm->ApiVersion != OPENA8DJ_DRIVER_API_VERSION ||
                arm->Phase == OPENA8DJ_CANARY_PHASE_NONE ||
                arm->Phase > OPENA8DJ_CANARY_PHASE_MAX ||
                arm->MaxOperations != 1u ||
                 (arm->NonceHigh == 0 && arm->NonceLow == 0) ||
                 InterlockedCompareExchange(&context->DevicePrepared, 0, 0) == 0 ||
                 InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
                 InterlockedCompareExchange(&context->StreamWorkerActive, 0, 0) != 0 ||
                 InterlockedCompareExchange(&context->IsoOneShotActive, 0, 0) != 0 ||
                 InterlockedCompareExchange(&context->IsoEngineState, 0, 0) !=
                     OpenA8DJIsoEngineStopped) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                context->CanaryNonceHigh = arm->NonceHigh;
                context->CanaryNonceLow = arm->NonceLow;
                InterlockedExchange(&context->SafetyTraceBudget, 64);
                InterlockedExchange(&context->CanaryOperationsRemaining, 1);
                InterlockedExchange(&context->CanaryPhase, (LONG)arm->Phase);
                OpenA8DJ_RecordSafetyCheckpoint(
                    context,
                    OPENA8DJ_CHECKPOINT_CANARY_ARMED,
                    STATUS_SUCCESS);
                status = STATUS_SUCCESS;
            }
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_SAFETY_STATE) {
        POPENA8DJ_SAFETY_STATE safety = NULL;

        status = OpenA8DJ_RetrieveOutput(
            Request,
            sizeof(OPENA8DJ_SAFETY_STATE),
            (PVOID *)&safety);
        if (NT_SUCCESS(status)) {
            RtlZeroMemory(safety, sizeof(*safety));
            safety->Size = sizeof(*safety);
            safety->ApiVersion = OPENA8DJ_DRIVER_API_VERSION;
            safety->ArmedPhase = (ULONG)InterlockedCompareExchange(&context->CanaryPhase, 0, 0);
            safety->OperationsRemaining = (ULONG)InterlockedCompareExchange(
                &context->CanaryOperationsRemaining, 0, 0);
            safety->NonceHigh = context->CanaryNonceHigh;
            safety->NonceLow = context->CanaryNonceLow;
            safety->LastCheckpoint = context->LastSafetyCheckpoint;
            safety->LastCheckpointSequence = (ULONG)InterlockedCompareExchange(
                &context->SafetyCheckpointSequence, 0, 0);
            safety->LastNtStatus = (ULONG)context->LastSafetyStatus;
            safety->DevicePrepared = (ULONG)InterlockedCompareExchange(&context->DevicePrepared, 0, 0);
            safety->DeviceStopping = (ULONG)InterlockedCompareExchange(&context->DeviceStopping, 0, 0);
            safety->StreamWorkerActive = (ULONG)InterlockedCompareExchange(&context->StreamWorkerActive, 0, 0);
            OpenA8DJ_CopyFixedString(
                safety->BuildFingerprint,
                sizeof(safety->BuildFingerprint),
                OPENA8DJ_BUILD_FINGERPRINT);
            bytesReturned = sizeof(*safety);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_DISARM_PHYSICAL_CANARY) {
        NTSTATUS disarmLockStatus = context->IsoPurgeLock != NULL ?
            WdfWaitLockAcquire(context->IsoPurgeLock, NULL) :
            STATUS_DEVICE_NOT_READY;

        status = disarmLockStatus;
        if (NT_SUCCESS(status)) {
            InterlockedExchange(&context->CanaryPhase, OPENA8DJ_CANARY_PHASE_NONE);
            InterlockedExchange(&context->CanaryOperationsRemaining, 0);
            InterlockedExchange(&context->StreamStopRequested, 1);
            OpenA8DJ_PurgeIsoTargetsLocked(context, TRUE);
            WdfWaitLockRelease(context->IsoPurgeLock);
            if (context->StreamWorkItem != NULL) {
                WdfWorkItemFlush(context->StreamWorkItem);
            }
            context->CanaryNonceHigh = 0;
            context->CanaryNonceLow = 0;
            status = STATUS_SUCCESS;
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_GET_USB_INFO) {
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
            status = OpenA8DJ_BeginIsoOneShot(
                context,
                OPENA8DJ_CANARY_PHASE_ISO_CAPTURE);
        }
        if (NT_SUCCESS(status)) {
            NTSTATUS operationStatus = OpenA8DJ_CaptureIsoSnapshot(context, isoSnapshot);
            NTSTATUS cleanupStatus = OpenA8DJ_EndIsoOneShot(context);

            bytesReturned = sizeof(*isoSnapshot);
            status = !NT_SUCCESS(cleanupStatus) ? cleanupStatus : operationStatus;
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_ISO_SILENCE_PULSE) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_ISO_SILENCE_PULSE), (PVOID *)&silencePulse);
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_BeginIsoOneShot(
                context,
                OPENA8DJ_CANARY_PHASE_ISO_OUTPUT);
        }
        if (NT_SUCCESS(status)) {
            NTSTATUS operationStatus = OpenA8DJ_SendIsoSilencePulse(context, silencePulse);
            NTSTATUS cleanupStatus = OpenA8DJ_EndIsoOneShot(context);

            bytesReturned = sizeof(*silencePulse);
            status = !NT_SUCCESS(cleanupStatus) ? cleanupStatus : operationStatus;
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_ISO_TONE_BURST) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_ISO_TONE_BURST), (PVOID *)&toneBurst);
        if (NT_SUCCESS(status)) {
#if !OPENA8DJ_VIRTUAL_MODE
            if (toneBurst->RequestedTransfers > OPENA8DJ_PHYSICAL_TONE_BURST_MAX_TRANSFERS) {
                toneBurst->RequestedTransfers = OPENA8DJ_PHYSICAL_TONE_BURST_MAX_TRANSFERS;
            }
#endif
            status = OpenA8DJ_BeginIsoOneShot(
                context,
                OPENA8DJ_CANARY_PHASE_ISO_OUTPUT);
        }
        if (NT_SUCCESS(status)) {
            NTSTATUS operationStatus = OpenA8DJ_SendIsoToneBurst(context, toneBurst);
            NTSTATUS cleanupStatus = OpenA8DJ_EndIsoOneShot(context);

            status = !NT_SUCCESS(cleanupStatus) ? cleanupStatus : operationStatus;
            bytesReturned = sizeof(*toneBurst);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_APPLY_AUDIO_PARAMS) {
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_AUDIO_PARAMS_RESULT), (PVOID *)&audioParams);
        if (NT_SUCCESS(status) && !OpenA8DJ_ConsumeCanaryAuthorization(
                context,
                OPENA8DJ_CANARY_PHASE_CONTROL_READ,
                OPENA8DJ_CHECKPOINT_CANARY_CONSUMED)) {
            status = STATUS_ACCESS_DENIED;
        }
        if (NT_SUCCESS(status)) {
            status = OpenA8DJ_ConfigureAudioParamsIsolated(
                context,
                context->CurrentFormat.SampleRate,
                audioParams);
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
            if (InterlockedCompareExchange(&context->StreamWorkerActive, 0, 0) != 0 ||
                context->StreamState.Streaming) {
                status = STATUS_DEVICE_BUSY;
            } else {
                context->CurrentFormat = *inputFormat;
                context->CurrentFormat.Size = sizeof(context->CurrentFormat);
                context->StreamState.SampleRate = context->CurrentFormat.SampleRate;
                context->StreamState.BufferFrames = context->CurrentFormat.BufferFrames;
                OpenA8DJ_InvalidateAudioParamsConfiguration(context);
                context->FormatChanges++;
                status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_AUDIO_FORMAT), (PVOID *)&outputFormat);
            }
        }
        if (NT_SUCCESS(status)) {
            *outputFormat = context->CurrentFormat;
            bytesReturned = sizeof(*outputFormat);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_START_STREAMING) {
        POPENA8DJ_STREAM_STATE streamState = NULL;
        OPENA8DJ_AUDIO_PARAMS_RESULT audioParamsResult;
        NTSTATUS audioParamsStatus;
        NTSTATUS lifecycleStatus;
        BOOLEAN lifecycleLockHeld = FALSE;

        context->StartRequests++;
        status = OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_STREAM_STATE), (PVOID *)&streamState);
        if (NT_SUCCESS(status) && !OpenA8DJ_ConsumeCanaryAuthorization(
                context,
                OPENA8DJ_CANARY_PHASE_STREAMING,
                OPENA8DJ_CHECKPOINT_CANARY_CONSUMED)) {
            status = STATUS_ACCESS_DENIED;
        }
        if (NT_SUCCESS(status)) {
            if (context->IsoPurgeLock == NULL) {
                status = STATUS_DEVICE_NOT_READY;
            } else {
                lifecycleStatus = WdfWaitLockAcquire(context->IsoPurgeLock, NULL);
                if (NT_SUCCESS(lifecycleStatus)) {
                    lifecycleLockHeld = TRUE;
                } else {
                    status = lifecycleStatus;
                }
            }
        }
        if (NT_SUCCESS(status)) {
            if (InterlockedCompareExchange(&context->IsoOneShotActive, 0, 0) != 0 ||
                InterlockedCompareExchange(&context->DeviceStopping, 0, 0) != 0 ||
                !OpenA8DJ_IsTransportReady(context) ||
                context->StreamWorkItem == NULL) {
                context->RejectedStartRequests++;
                context->StreamState.Streaming = FALSE;
                context->StreamState.StreamingEngineReady = FALSE;
                *streamState = context->StreamState;
                bytesReturned = sizeof(*streamState);
                status = STATUS_DEVICE_NOT_READY;
            } else if (InterlockedCompareExchange(&context->StreamWorkerActive, 1, 0) == 0) {
                // A newly prepared physical device is deliberately born in
                // the stopped state. START owns the stopped-to-running
                // transition after atomically claiming the inactive worker.
                InterlockedExchange(&context->StreamStopRequested, 0);
                RtlZeroMemory(&audioParamsResult, sizeof(audioParamsResult));
                audioParamsStatus = OpenA8DJ_ConfigureAudioParams(
                    context,
                    context->CurrentFormat.SampleRate,
                    &audioParamsResult);
                if (!NT_SUCCESS(audioParamsStatus)) {
                    context->RejectedStartRequests++;
                    context->StreamState.Streaming = FALSE;
                    context->StreamState.StreamingEngineReady = FALSE;
                    InterlockedExchange(&context->StreamStopRequested, 1);
                    InterlockedExchange(&context->StreamWorkerActive, 0);
                    *streamState = context->StreamState;
                    bytesReturned = sizeof(*streamState);
                    status = audioParamsStatus;
                } else {
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
                }
            } else if (InterlockedCompareExchange(&context->StreamStopRequested, 0, 0) != 0) {
                context->RejectedStartRequests++;
                context->StreamState.Streaming = FALSE;
                context->StreamState.StreamingEngineReady = FALSE;
                *streamState = context->StreamState;
                bytesReturned = sizeof(*streamState);
                status = STATUS_DEVICE_BUSY;
            } else {
                context->StreamState.Streaming = TRUE;
                context->StreamState.StreamingEngineReady = TRUE;
                *streamState = context->StreamState;
                bytesReturned = sizeof(*streamState);
                status = STATUS_SUCCESS;
            }
        }
        if (lifecycleLockHeld) {
            WdfWaitLockRelease(context->IsoPurgeLock);
        }
    } else if (IoControlCode == IOCTL_OPENA8DJ_STOP_STREAMING) {
        POPENA8DJ_STREAM_STATE streamState = NULL;
        BOOLEAN workerStopped;
        NTSTATUS stopLockStatus;

        context->StopRequests++;
        stopLockStatus = context->IsoPurgeLock != NULL ?
            WdfWaitLockAcquire(context->IsoPurgeLock, NULL) :
            STATUS_DEVICE_NOT_READY;
        if (NT_SUCCESS(stopLockStatus)) {
            InterlockedExchange(&context->StreamStopRequested, 1);
            OpenA8DJ_PurgeIsoTargetsLocked(context, TRUE);
            WdfWaitLockRelease(context->IsoPurgeLock);
            if (context->StreamWorkItem != NULL) {
                WdfWorkItemFlush(context->StreamWorkItem);
            }
        }
        workerStopped = InterlockedCompareExchange(&context->StreamWorkerActive, 0, 0) == 0;
        if (workerStopped) {
            InterlockedExchange(&context->StreamStopRequested, 0);
        }
        context->StreamState.Streaming = FALSE;
        context->StreamState.StreamingEngineReady = FALSE;
        status = NT_SUCCESS(stopLockStatus) ?
            OpenA8DJ_RetrieveOutput(Request, sizeof(OPENA8DJ_STREAM_STATE), (PVOID *)&streamState) :
            stopLockStatus;
        if (NT_SUCCESS(status)) {
            *streamState = context->StreamState;
            bytesReturned = sizeof(*streamState);
            if (!workerStopped) {
                status = STATUS_IO_TIMEOUT;
            }
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
