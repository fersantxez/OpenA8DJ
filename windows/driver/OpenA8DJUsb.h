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

#define OPENA8DJ_EP_BULK_OUT 0x01
#define OPENA8DJ_EP_BULK_IN 0x81
#define OPENA8DJ_EP_ISO_IN 0x82
#define OPENA8DJ_EP_ISO_OUT 0x06

#define OPENA8DJ_POOL_TAG '8DAO'
#define OPENA8DJ_MAX_USB_INTERFACES 8

typedef struct _OPENA8DJ_DEVICE_CONTEXT {
    WDFUSBDEVICE UsbDevice;
    WDFUSBINTERFACE UsbInterfaces[OPENA8DJ_MAX_USB_INTERFACES];
    WDFUSBPIPE BulkOutPipe;
    WDFUSBPIPE BulkInPipe;
    WDFUSBPIPE IsoInPipe;
    WDFUSBPIPE IsoOutPipe;
    WDFWORKITEM StreamWorkItem;
    ACXCIRCUIT RenderCircuits[OPENA8DJ_STEREO_PAIRS];
    ACXCIRCUIT CaptureCircuits[OPENA8DJ_STEREO_PAIRS];
    volatile PVOID ActiveRenderStreams[OPENA8DJ_STEREO_PAIRS];
    volatile PVOID ActiveCaptureStreams[OPENA8DJ_STEREO_PAIRS];
    volatile LONG StreamStopRequested;
    volatile LONG StreamWorkerActive;
    BOOLEAN AcxCircuitsAdded;
    KSPIN_LOCK Ep1Lock;
    KEVENT Ep1Event;
    BOOLEAN Ep1PendingActive;
    BOOLEAN Ep1ReplyReady;
    UCHAR Ep1PendingCommand;
    ULONG Ep1ReplyLength;
    UCHAR Ep1Reply[512];
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
} OPENA8DJ_DEVICE_CONTEXT, *POPENA8DJ_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(OPENA8DJ_DEVICE_CONTEXT, OpenA8DJGetDeviceContext)

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD OpenA8DJ_EvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE OpenA8DJ_EvtDevicePrepareHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OpenA8DJ_EvtIoDeviceControl;
EVT_WDF_WORKITEM OpenA8DJ_EvtStreamWorkItem;
