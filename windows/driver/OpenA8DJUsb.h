#pragma once

#include <ntddk.h>
#include <windef.h>
#include <usbdi.h>
#include <usbdlib.h>
#include <wdf.h>
#include <wdfusb.h>
#include <ks.h>
#include <mmsystem.h>
#include <ksmedia.h>
#include <acx.h>

#include "..\include\OpenA8DJShared.h"
#include "OpenA8DJBuildIdentity.h"

#define OPENA8DJ_EP_BULK_OUT 0x01
#define OPENA8DJ_EP_BULK_IN 0x81
#define OPENA8DJ_EP_ISO_IN 0x82
#define OPENA8DJ_EP_ISO_OUT 0x06

#define OPENA8DJ_POOL_TAG '8DAO'
#define OPENA8DJ_MAX_USB_INTERFACES 8
#define OPENA8DJ_ISO_ENGINE_SLOT_COUNT 8

typedef struct _OPENA8DJ_DEVICE_CONTEXT OPENA8DJ_DEVICE_CONTEXT, *POPENA8DJ_DEVICE_CONTEXT;

typedef enum _OPENA8DJ_ISO_ENGINE_DIRECTION {
    OpenA8DJIsoDirectionCapture = 0,
    OpenA8DJIsoDirectionOutput = 1
} OPENA8DJ_ISO_ENGINE_DIRECTION;

typedef enum _OPENA8DJ_ISO_ENGINE_SLOT_STATE {
    OpenA8DJIsoSlotUninitialized = 0,
    OpenA8DJIsoSlotIdle,
    OpenA8DJIsoSlotSubmitted,
    OpenA8DJIsoSlotCompleted,
    OpenA8DJIsoSlotProcessing
} OPENA8DJ_ISO_ENGINE_SLOT_STATE;

typedef enum _OPENA8DJ_ISO_ENGINE_STATE {
    OpenA8DJIsoEngineStopped = 0,
    OpenA8DJIsoEnginePriming,
    OpenA8DJIsoEngineRunning,
    OpenA8DJIsoEngineStopping
} OPENA8DJ_ISO_ENGINE_STATE;

typedef struct _OPENA8DJ_ISO_ENGINE_SLOT {
    POPENA8DJ_DEVICE_CONTEXT DeviceContext;
    OPENA8DJ_ISO_ENGINE_DIRECTION Direction;
    ULONG Index;
    WDFREQUEST Request;
    WDFMEMORY UrbMemory;
    PURB Urb;
    WDFMEMORY BufferMemory;
    UCHAR *Buffer;
    ULONG BufferCapacity;
    ULONG TransferLength;
    volatile LONG State;
    volatile LONG InFlight;
    volatile LONG NeedsReuse;
    LONG Generation;
    LONG ConsecutiveFailures;
    LONG64 SubmitSequence;
    LONG64 SubmitQpc;
    LONG64 CompletionQpc;
    LONG64 SourceCaptureCompletionQpc;
    ULONG RenderMask;
    ULONG RenderFrameCount[4];
    LONG RenderEpoch[4];
    PVOID RenderStreams[4];
    ULONG ScheduledStartFrame;
    USBD_STATUS UrbStatus;
    NTSTATUS CompletionStatus;
    ULONG ErrorCount;
} OPENA8DJ_ISO_ENGINE_SLOT, *POPENA8DJ_ISO_ENGINE_SLOT;

struct _OPENA8DJ_DEVICE_CONTEXT {
    WDFUSBDEVICE UsbDevice;
    WDFUSBINTERFACE UsbInterfaces[OPENA8DJ_MAX_USB_INTERFACES];
    WDFUSBPIPE BulkOutPipe;
    WDFUSBPIPE BulkInPipe;
    WDFUSBPIPE IsoInPipe;
    WDFUSBPIPE IsoOutPipe;
    WDFWORKITEM StreamWorkItem;
    WDFWORKITEM IsoProcessWorkItem;
    WDFWORKITEM IsoStopWorkItem;
    /*
     * Legacy KMDF transport resources.  These URBs are ordinary WDFMEMORY,
     * not FxUsbUrb/XRB objects, and the requests are reused across transfers.
     */
    WDFREQUEST IsoCaptureRequest;
    WDFMEMORY IsoCaptureUrbMemory;
    PURB IsoCaptureUrb;
    WDFREQUEST IsoOutputRequest;
    WDFMEMORY IsoOutputUrbMemory;
    PURB IsoOutputUrb;
    WDFMEMORY StreamCaptureBufferMemory;
    UCHAR *StreamCaptureBuffer;
    WDFMEMORY StreamPlaybackBufferMemory;
    UCHAR *StreamPlaybackBuffer;
    WDFWAITLOCK IsoTransportLock;
    WDFWAITLOCK IsoPurgeLock;
    WDFWAITLOCK SafetyCheckpointLock;
    volatile LONG IsoTransportDraining;
    KEVENT IsoPurgeCompleteEvent;
    OPENA8DJ_ISO_ENGINE_SLOT IsoCaptureSlots[OPENA8DJ_ISO_ENGINE_SLOT_COUNT];
    OPENA8DJ_ISO_ENGINE_SLOT IsoOutputSlots[OPENA8DJ_ISO_ENGINE_SLOT_COUNT];
    volatile LONG IsoEngineGeneration;
    volatile LONG IsoEngineRunning;
    volatile LONG IsoEngineState;
    volatile LONG IsoOneShotActive;
    volatile LONG IsoProcessWorkPending;
    volatile LONG IsoProcessWorkActive;
    volatile LONG IsoStopWorkPending;
    volatile LONG IsoStopWorkActive;
    volatile LONG IsoOutstandingCapture;
    volatile LONG IsoOutstandingOutput;
    volatile LONG64 IsoNextCaptureSubmitSequence;
    volatile LONG64 IsoNextOutputSubmitSequence;
    volatile LONG64 IsoNextCaptureProcessSequence;
    volatile LONG64 IsoNextOutputProcessSequence;
    volatile LONG64 IsoCaptureFrameQueries;
    volatile LONG64 IsoCaptureFrameQueryFailures;
    ULONG IsoCaptureFrameQueryCurrent;
    ULONG IsoCaptureSeedStartFrame;
    ULONG IsoNextCaptureStartFrame;
    volatile LONG64 IsoInputPipeResetRuns;
    volatile LONG64 IsoInputPipeResetFailures;
    volatile LONG IsoInputPipeResetLastNtStatus;
    volatile LONG64 IsoOutputPipeResetRuns;
    volatile LONG64 IsoOutputPipeResetFailures;
    volatile LONG IsoOutputPipeResetLastNtStatus;
    volatile LONG IsoTransportHealthy;
    volatile LONG IsoTransportHealthyGeneration;
    volatile LONG IsoInputTargetStartLastNtStatus;
    volatile LONG IsoOutputTargetStartLastNtStatus;
    volatile LONG IsoConsecutiveNoDataCaptures;
    volatile LONG64 IsoOutputQueueEmptyTransitions;
    volatile LONG64 IsoOutputLatePackets;
    volatile LONG64 IsoOutputBadStartFrames;
    volatile LONG64 IsoOutputOtherPacketErrors;
    volatile LONG64 IsoOutputPanicFlags;
    volatile LONG64 IsoCaptureLatePackets;
    volatile LONG64 IsoCaptureBadStartFrames;
    volatile LONG64 IsoCaptureOtherPacketErrors;
    volatile LONG IsoLastCaptureUrbStatus;
    volatile LONG IsoLastCapturePacketStatus;
    volatile LONG IsoLastCaptureErrorCount;
    volatile LONG IsoCaptureErrorSnapshotSequence;
    ULONG IsoLastCaptureErrorSlot;
    LONG IsoLastCaptureErrorGeneration;
    LONG64 IsoLastCaptureErrorSubmitSequence;
    ULONG IsoLastCaptureErrorScheduledStartFrame;
    ULONG IsoLastCaptureErrorFirstPacket;
    ULONG IsoLastCaptureErrorLastPacket;
    ULONG IsoLastCaptureErrorPacketCount;
    LONG64 IsoLastCaptureErrorSubmitQpc;
    LONG64 IsoLastCaptureErrorCompletionQpc;
    volatile LONG64 IsoCaptureToOutputSubmitMaxQpc;
    ULONG IsoLastCaptureStartFrame;
    ULONG IsoLastOutputStartFrame;
    volatile LONG64 AudioRateSettleRuns;
    volatile LONG64 AudioRateSettleSnapshots;
    volatile LONG64 AudioRateSettleMismatchedPackets;
    volatile LONG64 AudioRateSettleFailures;
    volatile LONG AudioRateSettleLastObservedBytes;
    volatile LONG AudioRateSettleActive;
    volatile LONG AudioRateSettleConsecutiveClean;
    volatile LONG AudioRateSettleAttemptsRemaining;
    volatile LONG64 AudioRateSettleStartQpc;
    volatile LONG64 AudioRateSettleBudgetQpc;
    KEVENT IsoEngineDrainedEvent;
    volatile LONG CanaryPhase;
    volatile LONG CanaryOperationsRemaining;
    ULONGLONG CanaryNonceHigh;
    ULONGLONG CanaryNonceLow;
    volatile LONG SafetyCheckpointSequence;
    volatile LONG SafetyTraceBudget;
    ULONG LastSafetyCheckpoint;
    NTSTATUS LastSafetyStatus;
    volatile LONG DeviceStopping;
    volatile LONG DevicePrepared;
    ACXCIRCUIT RenderCircuits[OPENA8DJ_STEREO_PAIRS];
    ACXCIRCUIT CaptureCircuits[OPENA8DJ_STEREO_PAIRS];
    BOOLEAN RenderCircuitAdded[OPENA8DJ_STEREO_PAIRS];
    BOOLEAN CaptureCircuitAdded[OPENA8DJ_STEREO_PAIRS];
    KSPIN_LOCK ActiveStreamLock;
    volatile PVOID ActiveRenderStreams[OPENA8DJ_STEREO_PAIRS];
    volatile PVOID ActiveCaptureStreams[OPENA8DJ_STEREO_PAIRS];
    volatile LONG StreamStopRequested;
    volatile LONG StreamWorkerActive;
    BOOLEAN AcxCircuitsAdded;
    WDFWAITLOCK BulkCommandLock;
    WDFWAITLOCK AudioConfigLock;
    volatile LONG AudioParamsConfigured;
    ULONG ConfiguredSampleRate;
    KSPIN_LOCK Ep1Lock;
    KEVENT Ep1Event;
    BOOLEAN Ep1PendingActive;
    BOOLEAN Ep1ReplyReady;
    UCHAR Ep1PendingCommand;
    ULONG Ep1ReplyLength;
    // CAIAQ EP1 commands and replies are one 64-byte USB bulk packet. A
    // larger continuous-reader transfer can remain pending after a full-size
    // 64-byte reply because no short packet terminates the transfer.
    UCHAR Ep1Reply[64];
    NTSTATUS Ep1ReaderConfigStatus;
    NTSTATUS Ep1ReaderStartStatus;
    ULONG Ep1ReaderCompletions;
    ULONG Ep1ReaderZeroReads;
    ULONG Ep1ReaderBytes;
    UCHAR InterfaceCount;
    UCHAR ConfiguredInterfaceCount;
    UCHAR ConfiguredPipeCount;
    UCHAR AlternateSettingCount;
    UCHAR SelectedAlternateSetting;
    UCHAR RawControlState[6];
    UCHAR LastControlWriteRequest[6];
    UCHAR LastControlWriteReadBack[6];
    BOOLEAN ControlsHardwareReady;
    BOOLEAN LastControlWriteMismatch;
    NTSTATUS LastControlReadStatus;
    NTSTATUS LastControlWriteStatus;
    NTSTATUS LastControlReadbackStatus;
    OPENA8DJ_AUDIO_FORMAT CurrentFormat;
    OPENA8DJ_STREAM_STATE StreamState;
    ULONG64 StartRequests;
    ULONG64 RejectedStartRequests;
    ULONG64 StopRequests;
    ULONG64 FormatChanges;
    ULONG64 ControlWrites;
    ULONG64 ProfileApplies;
    volatile LONG64 AcxCreateStreamCallbacks;
    volatile LONG64 AcxPrepareCallbacks;
    volatile LONG64 AcxReleaseCallbacks;
    volatile LONG64 AcxRunCallbacks;
    volatile LONG64 AcxPauseCallbacks;
    volatile LONG64 AcxLatencyCallbacks;
    volatile LONG64 AcxAllocatePacketCallbacks;
    volatile LONG64 AcxFreePacketCallbacks;
    volatile LONG64 AcxSetRenderPacketCallbacks;
    volatile LONG64 AcxGetCurrentPacketCallbacks;
    volatile LONG64 AcxGetCapturePacketCallbacks;
    volatile LONG64 AcxGetPresentationPositionCallbacks;
    volatile LONG64 AcxRtFramesRead;
    volatile LONG64 AcxRtNonZeroFrames;
    ULONG AcxRtPeakAbsS24;
    volatile LONG64 AcxRtRenderPairNonZeroFrames[OPENA8DJ_STEREO_PAIRS];
    ULONG AcxRtRenderPairPeakAbsS24[OPENA8DJ_STEREO_PAIRS];
    volatile LONG64 AcxRtCapturePairNonZeroFrames[OPENA8DJ_STEREO_PAIRS];
    ULONG AcxRtCapturePairPeakAbsS16[OPENA8DJ_STEREO_PAIRS];
    ULONG AcxRtChannels;
    ULONG AcxRtBlockAlign;
    ULONG AcxRtBitsPerSample;
    ULONG AcxRtIsFloat;
    ULONG AcxRtPacketCount;
    ULONG AcxRtPacketSize;
    ULONG AcxRtFrameCount;
    ULONG AcxLastSetRenderPacket;
    ULONG AcxLastSetRenderFlags;
    ULONG AcxLastSetRenderEosPacketLength;
    volatile LONG64 AcxRtRenderPacketCompletions;
    volatile LONG64 AcxRtRenderPacketNotifications;
    volatile LONG64 AcxRtRenderPacketNotificationFailures;
    ULONG AcxRtRenderCurrentPacket;
    ULONG AcxRtRenderLastNotificationNtStatus;
    volatile LONG64 StreamWorkerIterations;
    volatile LONG64 StreamWorkerCaptureBytes;
    volatile LONG64 StreamWorkerPlaybackBytes;
    volatile LONG64 StreamWorkerNoRenderIterations;
    ULONG StreamWorkerLastCaptureBytes;
    ULONG StreamWorkerLastPlaybackBytes;
    ULONG StreamWorkerLastRenderMask;
    ULONG StreamWorkerLastCaptureMask;
    ULONG StreamWorkerMaxCaptureBytes;
    ULONG StreamWorkerMaxPlaybackBytes;
    volatile LONG RenderTraceWriteIndex;
    volatile LONG64 RenderTraceWriteCount;
    OPENA8DJ_RENDER_TRACE_FRAME RenderTraceFrames[OPENA8DJ_RENDER_TRACE_FRAME_COUNT];
    volatile LONG64 UsbPlaybackTraceWriteCount;
    ULONG UsbPlaybackTraceByteCount;
    ULONG UsbPlaybackTraceFixedPacketBytes;
    ULONG UsbPlaybackTraceActiveRenderMask;
    ULONG UsbPlaybackTraceWorkerLastPlaybackBytes;
    UCHAR UsbPlaybackTraceBytes[OPENA8DJ_USB_PLAYBACK_TRACE_BYTES];
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(OPENA8DJ_DEVICE_CONTEXT, OpenA8DJGetDeviceContext)

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD OpenA8DJ_EvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE OpenA8DJ_EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE OpenA8DJ_EvtDeviceReleaseHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OpenA8DJ_EvtIoDeviceControl;
EVT_WDF_WORKITEM OpenA8DJ_EvtStreamWorkItem;
EVT_WDF_WORKITEM OpenA8DJ_EvtIsoProcessWorkItem;
EVT_WDF_WORKITEM OpenA8DJ_EvtIsoStopWorkItem;
