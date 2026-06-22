#pragma once

#include <ntddk.h>
#include <usbdi.h>
#include <usbdlib.h>
#include <wdf.h>
#include <wdfusb.h>

#include "..\include\OpenA8DJShared.h"

#define OPENA8DJ_EP_BULK_OUT 0x01
#define OPENA8DJ_EP_BULK_IN 0x81
#define OPENA8DJ_EP_ISO_IN 0x82
#define OPENA8DJ_EP_ISO_OUT 0x06

#define OPENA8DJ_POOL_TAG '8DAO'

typedef struct _OPENA8DJ_DEVICE_CONTEXT {
    WDFUSBDEVICE UsbDevice;
    WDFUSBINTERFACE UsbInterface;
    WDFUSBPIPE BulkOutPipe;
    WDFUSBPIPE BulkInPipe;
    WDFUSBPIPE IsoInPipe;
    WDFUSBPIPE IsoOutPipe;
    UCHAR InterfaceCount;
    UCHAR RawControlState[6];
    OPENA8DJ_AUDIO_FORMAT CurrentFormat;
    OPENA8DJ_STREAM_STATE StreamState;
    ULONG64 StartRequests;
    ULONG64 RejectedStartRequests;
    ULONG64 StopRequests;
    ULONG64 FormatChanges;
    ULONG64 ControlWrites;
    ULONG64 ProfileApplies;
} OPENA8DJ_DEVICE_CONTEXT, *POPENA8DJ_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(OPENA8DJ_DEVICE_CONTEXT, OpenA8DJGetDeviceContext)

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD OpenA8DJ_EvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE OpenA8DJ_EvtDevicePrepareHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OpenA8DJ_EvtIoDeviceControl;
