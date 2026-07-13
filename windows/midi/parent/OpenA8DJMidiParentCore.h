#ifndef OPENA8DJ_MIDI_PARENT_CORE_H
#define OPENA8DJ_MIDI_PARENT_CORE_H

#include "../OpenA8DJMidiProtocol.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Portable parent-side state machine.  Every function is caller-locked:
 * callers must serialize all operations on one core with the same lock.
 * The core allocates no memory and never invokes callbacks while locked.
 */
typedef enum OPENA8DJ_MIDI_PARENT_STATE {
    OPENA8DJ_MIDI_PARENT_OFFLINE = 0,
    OPENA8DJ_MIDI_PARENT_READY,
    OPENA8DJ_MIDI_PARENT_ACCEPTING_IO,
    OPENA8DJ_MIDI_PARENT_STOPPING,
    OPENA8DJ_MIDI_PARENT_REMOVED
} OPENA8DJ_MIDI_PARENT_STATE;

typedef enum OPENA8DJ_MIDI_PARENT_SLOT {
    OPENA8DJ_MIDI_PARENT_RX = 0,
    OPENA8DJ_MIDI_PARENT_TX
} OPENA8DJ_MIDI_PARENT_SLOT;

typedef enum OPENA8DJ_MIDI_PARENT_STATUS {
    OPENA8DJ_MIDI_PARENT_OK = 0,
    OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT,
    OPENA8DJ_MIDI_PARENT_INVALID_STATE,
    OPENA8DJ_MIDI_PARENT_ALREADY_OPEN,
    OPENA8DJ_MIDI_PARENT_NOT_OPEN,
    OPENA8DJ_MIDI_PARENT_NO_DATA,
    OPENA8DJ_MIDI_PARENT_RING_OVERFLOW,
    OPENA8DJ_MIDI_PARENT_TX_FAULTED,
    OPENA8DJ_MIDI_PARENT_BUFFER_TOO_SMALL,
    OPENA8DJ_MIDI_PARENT_STALE_NOTIFY,
    OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING,
    OPENA8DJ_MIDI_PARENT_MALFORMED_MIDI,
    OPENA8DJ_MIDI_PARENT_UNSUPPORTED_PORT
} OPENA8DJ_MIDI_PARENT_STATUS;

typedef uint32_t OPENA8DJ_MIDI_PARENT_EVENTS;

#define OPENA8DJ_MIDI_PARENT_EVENT_NONE UINT32_C(0)
#define OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED UINT32_C(0x00000001)
#define OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_READY UINT32_C(0x00000002)
#define OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_DRAINED UINT32_C(0x00000004)
#define OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_STARTED UINT32_C(0x00000008)
#define OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY UINT32_C(0x00000010)
#define OPENA8DJ_MIDI_PARENT_EVENT_TX_FAULT UINT32_C(0x00000020)
#define OPENA8DJ_MIDI_PARENT_EVENT_RX_OVERFLOW UINT32_C(0x00000040)
#define OPENA8DJ_MIDI_PARENT_EVENT_TX_OVERFLOW UINT32_C(0x00000080)

typedef void (*OPENA8DJ_MIDI_PARENT_NOTIFY)(void *context);

/* A token contains no callable pointer.  Validate/acquire it under the lock. */
typedef struct OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT {
    uint64_t epoch;
    uint8_t valid;
} OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT;

typedef struct OPENA8DJ_MIDI_PARENT_NOTIFY_DISPATCH {
    OPENA8DJ_MIDI_PARENT_NOTIFY callback;
    void *context;
} OPENA8DJ_MIDI_PARENT_NOTIFY_DISPATCH;

typedef enum OPENA8DJ_MIDI_PARENT_DEMUX_KIND {
    OPENA8DJ_MIDI_PARENT_DEMUX_NONE = 0,
    OPENA8DJ_MIDI_PARENT_DEMUX_CONTROL_REPLY,
    OPENA8DJ_MIDI_PARENT_DEMUX_MIDI,
    OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_MALFORMED,
    OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_OVERFLOW
} OPENA8DJ_MIDI_PARENT_DEMUX_KIND;

typedef struct OPENA8DJ_MIDI_PARENT_DEMUX_RESULT {
    OPENA8DJ_MIDI_PARENT_DEMUX_KIND kind;
    OPENA8DJ_MIDI_STATUS protocolStatus;
    const uint8_t *controlReply;
    size_t controlReplyLength;
    size_t midiBytes;
    size_t paddingBytes;
} OPENA8DJ_MIDI_PARENT_DEMUX_RESULT;

typedef struct OPENA8DJ_MIDI_PARENT_RUNDOWN {
    OPENA8DJ_MIDI_PARENT_STATE state;
    size_t notifyInFlight;
    uint8_t rxOpen;
    uint8_t txOpen;
    uint8_t teardownReady;
} OPENA8DJ_MIDI_PARENT_RUNDOWN;

typedef struct OPENA8DJ_MIDI_PARENT_CORE {
    OPENA8DJ_MIDI_RING *rxRing;
    OPENA8DJ_MIDI_RING *txRing;
    OPENA8DJ_MIDI_PARENT_STATE state;
    OPENA8DJ_MIDI_PARENT_NOTIFY rxNotify;
    void *rxNotifyContext;
    uint64_t notifyEpoch;
    size_t notifyInFlight;
    uint32_t txFaultCode;
    uint8_t rxOpen;
    uint8_t txOpen;
    uint8_t txFaulted;
    uint64_t prepareCount;
    uint64_t stopRunResetCount;
    uint64_t rxBytes;
    uint64_t txBytes;
    uint64_t controlReplies;
    uint64_t malformedMidiFrames;
    uint64_t notifyTransitions;
} OPENA8DJ_MIDI_PARENT_CORE;

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentInitialize(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_RING *rxRing,
    OPENA8DJ_MIDI_RING *txRing);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentPrepare(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentSetState(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_STATE state,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentOpen(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_SLOT slot,
    OPENA8DJ_MIDI_PARENT_NOTIFY notify,
    void *notifyContext,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentClose(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_SLOT slot,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

/*
 * Close invalidates unacquired RX snapshots immediately.  If a callback was
 * already acquired, close returns RUNDOWN_PENDING; the platform must defer
 * completion until CompleteNotify reports NOTIFY_DRAINED/TEARDOWN_READY.
 * STOP follows the same rule: RUNDOWN_PENDING is an instruction to close the
 * unique slots and wait for portable rundown, never to free parent storage.
 */

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentRead(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    uint8_t *bytes,
    size_t capacity,
    size_t *bytesRead);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentWrite(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    const uint8_t *bytes,
    size_t length,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentTakeTxFrame(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentFaultTx(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    uint32_t faultCode,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentDemuxEp1(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    const uint8_t *packet,
    size_t packetLength,
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT *result,
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT *notifySnapshot,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentAcquireNotify(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    const OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT *snapshot,
    OPENA8DJ_MIDI_PARENT_NOTIFY_DISPATCH *dispatch);

/*
 * Acquire under the caller lock, invoke dispatch.callback after unlocking,
 * then reacquire the lock and call CompleteNotify exactly once.  A stale
 * token exposes no callback pointer and therefore cannot run after close.
 */

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentCompleteNotify(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_EVENTS *events);

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentGetRundown(
    const OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_RUNDOWN *rundown);

#ifdef __cplusplus
}
#endif

#endif
