#ifndef OPENA8DJ_MIDI_BUS_INTERFACE_H
#define OPENA8DJ_MIDI_BUS_INTERFACE_H

#include <ntddk.h>

#include "OpenA8DJMidiBusContract.h"

/*
 * V1 is intentionally immutable. Incompatible evolution must use a new GUID,
 * as required by the WDM driver-defined interface contract.
 *
 * OpenStream, CloseStream and SetState run at PASSIVE_LEVEL.
 * ReadStream and WriteStream are nonblocking and callable at <= DISPATCH_LEVEL.
 * WriteStream is all-or-nothing: success means bytesWritten == byteCount;
 * failure means bytesWritten == 0. STATUS_DEVICE_BUSY is the only backpressure
 * result and must also report zero bytes. The child maps that result to the
 * PortCls-required STATUS_SUCCESS/zero-byte retry signal. A partial success is
 * not representable by V1 and is treated as a protocol error.
 *
 * The parent PDO provider must complete IRP_MN_QUERY_INTERFACE synchronously
 * from its PnP dispatch path and advertise SYNCHRONOUS_QUERY. It must persist
 * the OPENA8DJ_MIDI_PARENT_QUERY_CHECKPOINT_* sequence. This is required because
 * the official synchronous-IRP pattern must wait indefinitely after
 * STATUS_PENDING; a timeout cannot safely release stack-backed event/status
 * storage without first obtaining cancellation and completion ownership.
 *
 * A successful query transfers exactly one InterfaceReference to the child
 * factory. OpenA8DJMidiCreateMiniport consumes that one reference on success
 * and consumes none on failure. The miniport releases it exactly once from its
 * destructor. CloseStream must quiesce callbacks before returning. The parent
 * owns all USB and CAIAQ framing.
 */

DEFINE_GUID(
    GUID_OPENA8DJ_MIDI_BUS_INTERFACE_V1,
    0x8e45f4b4,
    0x608f,
    0x4b4b,
    0x9b,
    0x97,
    0x83,
    0xc4,
    0x7c,
    0x3f,
    0x52,
    0xf7);

#define OPENA8DJ_MIDI_BUS_DIRECTION_RENDER 0u
#define OPENA8DJ_MIDI_BUS_DIRECTION_CAPTURE 1u

typedef VOID
(NTAPI *OPENA8DJ_MIDI_BUS_NOTIFY_ROUTINE)(PVOID notificationContext);

typedef NTSTATUS
(NTAPI *OPENA8DJ_MIDI_BUS_OPEN_STREAM)(
    PVOID interfaceContext,
    ULONG direction,
    OPENA8DJ_MIDI_BUS_NOTIFY_ROUTINE notifyRoutine,
    PVOID notificationContext,
    PVOID *streamContext);

typedef VOID
(NTAPI *OPENA8DJ_MIDI_BUS_CLOSE_STREAM)(PVOID streamContext);

typedef NTSTATUS
(NTAPI *OPENA8DJ_MIDI_BUS_SET_STATE)(
    PVOID streamContext,
    ULONG state);

typedef NTSTATUS
(NTAPI *OPENA8DJ_MIDI_BUS_READ_STREAM)(
    PVOID streamContext,
    PVOID buffer,
    ULONG bufferLength,
    PULONG bytesRead);

typedef NTSTATUS
(NTAPI *OPENA8DJ_MIDI_BUS_WRITE_STREAM)(
    PVOID streamContext,
    const VOID *buffer,
    ULONG byteCount,
    PULONG bytesWritten);

typedef struct OPENA8DJ_MIDI_BUS_INTERFACE_V1 {
    INTERFACE Header;
    ULONG Magic;
    ULONG Capabilities;
    OPENA8DJ_MIDI_BUS_OPEN_STREAM OpenStream;
    OPENA8DJ_MIDI_BUS_CLOSE_STREAM CloseStream;
    OPENA8DJ_MIDI_BUS_SET_STATE SetState;
    OPENA8DJ_MIDI_BUS_READ_STREAM ReadStream;
    OPENA8DJ_MIDI_BUS_WRITE_STREAM WriteStream;
} OPENA8DJ_MIDI_BUS_INTERFACE_V1, *POPENA8DJ_MIDI_BUS_INTERFACE_V1;

C_ASSERT(FIELD_OFFSET(OPENA8DJ_MIDI_BUS_INTERFACE_V1, Header) == 0u);
C_ASSERT(sizeof(OPENA8DJ_MIDI_BUS_INTERFACE_V1) <= MAXUSHORT);
#if defined(_WIN64)
C_ASSERT(sizeof(OPENA8DJ_MIDI_BUS_INTERFACE_V1) ==
         OPENA8DJ_MIDI_BUS_INTERFACE_V1_X64_SIZE);
#endif

#endif
