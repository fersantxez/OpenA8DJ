#include "OpenA8DJMidiParentCore.h"

#include <string.h>

static void
OpenA8DJMidiParentSetEvents(
    OPENA8DJ_MIDI_PARENT_EVENTS *events,
    OPENA8DJ_MIDI_PARENT_EVENTS value)
{
    if (events != NULL) {
        *events = value;
    }
}

static void
OpenA8DJMidiParentAddEvent(
    OPENA8DJ_MIDI_PARENT_EVENTS *events,
    OPENA8DJ_MIDI_PARENT_EVENTS value)
{
    if (events != NULL) {
        *events |= value;
    }
}

static int
OpenA8DJMidiParentCoreIsValid(const OPENA8DJ_MIDI_PARENT_CORE *core)
{
    return core != NULL &&
           core->rxRing != NULL &&
           core->txRing != NULL &&
           core->rxRing != core->txRing &&
           core->state <= OPENA8DJ_MIDI_PARENT_REMOVED;
}

static int
OpenA8DJMidiParentTeardownReady(const OPENA8DJ_MIDI_PARENT_CORE *core)
{
    return core->rxOpen == 0u &&
           core->txOpen == 0u &&
           core->notifyInFlight == 0u;
}

static void
OpenA8DJMidiParentIncrement(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        (*value)++;
    }
}

static void
OpenA8DJMidiParentAdd(uint64_t *value, size_t amount)
{
    uint64_t increment = (uint64_t)amount;

    if (UINT64_MAX - *value < increment) {
        *value = UINT64_MAX;
    } else {
        *value += increment;
    }
}

static void
OpenA8DJMidiParentResetTransport(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    int stopRunReset)
{
    OpenA8DJMidiRingInitialize(core->rxRing);
    OpenA8DJMidiRingInitialize(core->txRing);
    core->txFaulted = 0u;
    core->txFaultCode = 0u;
    core->notifyEpoch++;
    if (core->notifyEpoch == 0u) {
        core->notifyEpoch = 1u;
    }
    if (stopRunReset != 0) {
        OpenA8DJMidiParentIncrement(&core->stopRunResetCount);
    } else {
        OpenA8DJMidiParentIncrement(&core->prepareCount);
    }
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentInitialize(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_RING *rxRing,
    OPENA8DJ_MIDI_RING *txRing)
{
    if (core == NULL || rxRing == NULL || txRing == NULL || rxRing == txRing) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }

    memset(core, 0, sizeof(*core));
    core->rxRing = rxRing;
    core->txRing = txRing;
    core->state = OPENA8DJ_MIDI_PARENT_OFFLINE;
    OpenA8DJMidiRingInitialize(rxRing);
    OpenA8DJMidiRingInitialize(txRing);
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentPrepare(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core)) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->state != OPENA8DJ_MIDI_PARENT_OFFLINE ||
        !OpenA8DJMidiParentTeardownReady(core)) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }

    OpenA8DJMidiParentResetTransport(core, 0);
    core->state = OPENA8DJ_MIDI_PARENT_READY;
    OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED);
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentSetState(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_STATE state,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    int teardownReady;

    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core) ||
        state > OPENA8DJ_MIDI_PARENT_REMOVED) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (state == core->state) {
        return OPENA8DJ_MIDI_PARENT_OK;
    }

    if (state == OPENA8DJ_MIDI_PARENT_ACCEPTING_IO) {
        if (core->state == OPENA8DJ_MIDI_PARENT_READY) {
            core->state = state;
        } else if (core->state == OPENA8DJ_MIDI_PARENT_STOPPING &&
                   core->notifyInFlight == 0u) {
            OpenA8DJMidiParentResetTransport(core, 1);
            core->state = state;
        } else {
            return core->state == OPENA8DJ_MIDI_PARENT_STOPPING ?
                OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING :
                OPENA8DJ_MIDI_PARENT_INVALID_STATE;
        }
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED);
        return OPENA8DJ_MIDI_PARENT_OK;
    }

    if (state == OPENA8DJ_MIDI_PARENT_STOPPING &&
        (core->state == OPENA8DJ_MIDI_PARENT_READY ||
         core->state == OPENA8DJ_MIDI_PARENT_ACCEPTING_IO)) {
        core->state = state;
        core->notifyEpoch++;
        if (core->notifyEpoch == 0u) {
            core->notifyEpoch = 1u;
        }
        OpenA8DJMidiParentAddEvent(
            events,
            OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED |
            OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_STARTED);
        teardownReady = OpenA8DJMidiParentTeardownReady(core);
        if (teardownReady != 0) {
            OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY);
            return OPENA8DJ_MIDI_PARENT_OK;
        }
        return OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING;
    }

    if (state == OPENA8DJ_MIDI_PARENT_OFFLINE &&
        core->state == OPENA8DJ_MIDI_PARENT_STOPPING) {
        if (!OpenA8DJMidiParentTeardownReady(core)) {
            return OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING;
        }
        core->state = state;
        OpenA8DJMidiParentAddEvent(
            events,
            OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED |
            OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY);
        return OPENA8DJ_MIDI_PARENT_OK;
    }

    if (state == OPENA8DJ_MIDI_PARENT_REMOVED &&
        core->state == OPENA8DJ_MIDI_PARENT_OFFLINE &&
        OpenA8DJMidiParentTeardownReady(core)) {
        core->state = state;
        OpenA8DJMidiParentAddEvent(
            events,
            OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED |
            OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY);
        return OPENA8DJ_MIDI_PARENT_OK;
    }

    return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentOpen(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_SLOT slot,
    OPENA8DJ_MIDI_PARENT_NOTIFY notify,
    void *notifyContext,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core) ||
        slot > OPENA8DJ_MIDI_PARENT_TX) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->state != OPENA8DJ_MIDI_PARENT_READY &&
        core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }

    if (slot == OPENA8DJ_MIDI_PARENT_RX) {
        if (notify == NULL) {
            return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
        }
        if (core->rxOpen != 0u) {
            return OPENA8DJ_MIDI_PARENT_ALREADY_OPEN;
        }
        core->rxOpen = 1u;
        core->rxNotify = notify;
        core->rxNotifyContext = notifyContext;
        core->notifyEpoch++;
        if (core->notifyEpoch == 0u) {
            core->notifyEpoch = 1u;
        }
    } else {
        if (core->txOpen != 0u) {
            return OPENA8DJ_MIDI_PARENT_ALREADY_OPEN;
        }
        core->txOpen = 1u;
    }
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentClose(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_SLOT slot,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core) ||
        slot > OPENA8DJ_MIDI_PARENT_TX) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }

    if (slot == OPENA8DJ_MIDI_PARENT_RX) {
        if (core->rxOpen == 0u) {
            return OPENA8DJ_MIDI_PARENT_NOT_OPEN;
        }
        core->rxOpen = 0u;
        core->rxNotify = NULL;
        core->rxNotifyContext = NULL;
        core->notifyEpoch++;
        if (core->notifyEpoch == 0u) {
            core->notifyEpoch = 1u;
        }
        if (core->notifyInFlight != 0u) {
            return OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING;
        }
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_DRAINED);
    } else {
        if (core->txOpen == 0u) {
            return OPENA8DJ_MIDI_PARENT_NOT_OPEN;
        }
        core->txOpen = 0u;
    }

    if (core->state == OPENA8DJ_MIDI_PARENT_STOPPING &&
        OpenA8DJMidiParentTeardownReady(core)) {
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY);
    }
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentRead(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    uint8_t *bytes,
    size_t capacity,
    size_t *bytesRead)
{
    OPENA8DJ_MIDI_STATUS status;

    if (bytesRead == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    *bytesRead = 0u;
    if (!OpenA8DJMidiParentCoreIsValid(core) ||
        (bytes == NULL && capacity != 0u)) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }
    if (core->rxOpen == 0u) {
        return OPENA8DJ_MIDI_PARENT_NOT_OPEN;
    }
    if (OpenA8DJMidiRingAvailable(core->rxRing) == 0u) {
        return OPENA8DJ_MIDI_PARENT_NO_DATA;
    }

    status = OpenA8DJMidiRingRead(core->rxRing, bytes, capacity, bytesRead);
    return status == OPENA8DJ_MIDI_OK ?
        OPENA8DJ_MIDI_PARENT_OK : OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentWrite(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    const uint8_t *bytes,
    size_t length,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OPENA8DJ_MIDI_STATUS status;
    size_t bytesWritten = 0u;

    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core) ||
        (bytes == NULL && length != 0u)) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }
    if (core->txOpen == 0u) {
        return OPENA8DJ_MIDI_PARENT_NOT_OPEN;
    }
    if (core->txFaulted != 0u) {
        return OPENA8DJ_MIDI_PARENT_TX_FAULTED;
    }

    status = OpenA8DJMidiRingWrite(core->txRing, bytes, length, &bytesWritten);
    if (status == OPENA8DJ_MIDI_RING_OVERFLOW) {
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_TX_OVERFLOW);
        return OPENA8DJ_MIDI_PARENT_RING_OVERFLOW;
    }
    if (status != OPENA8DJ_MIDI_OK || bytesWritten != length) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    OpenA8DJMidiParentAdd(&core->txBytes, length);
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentTakeTxFrame(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength)
{
    uint8_t payload[OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES];
    size_t available;
    size_t payloadLength;
    size_t bytesRead = 0u;
    OPENA8DJ_MIDI_STATUS status;

    if (frameLength == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    *frameLength = 0u;
    if (!OpenA8DJMidiParentCoreIsValid(core) || frame == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }
    if (core->txOpen == 0u) {
        return OPENA8DJ_MIDI_PARENT_NOT_OPEN;
    }
    if (core->txFaulted != 0u) {
        return OPENA8DJ_MIDI_PARENT_TX_FAULTED;
    }

    available = OpenA8DJMidiRingAvailable(core->txRing);
    if (available == 0u) {
        return OPENA8DJ_MIDI_PARENT_NO_DATA;
    }
    payloadLength = available < OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES ?
        available : OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES;
    if (frameCapacity < OPENA8DJ_MIDI_HEADER_BYTES + payloadLength) {
        return OPENA8DJ_MIDI_PARENT_BUFFER_TOO_SMALL;
    }

    status = OpenA8DJMidiRingRead(
        core->txRing,
        payload,
        payloadLength,
        &bytesRead);
    if (status != OPENA8DJ_MIDI_OK || bytesRead != payloadLength) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    status = OpenA8DJMidiEncodeWriteFrame(
        OPENA8DJ_MIDI_PORT,
        payload,
        payloadLength,
        frame,
        frameCapacity,
        frameLength);
    return status == OPENA8DJ_MIDI_OK ?
        OPENA8DJ_MIDI_PARENT_OK : OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentFaultTx(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    uint32_t faultCode,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core)) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->state != OPENA8DJ_MIDI_PARENT_READY &&
        core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO &&
        core->state != OPENA8DJ_MIDI_PARENT_STOPPING) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }
    core->txFaulted = 1u;
    core->txFaultCode = faultCode;
    OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_TX_FAULT);
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentDemuxEp1(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    const uint8_t *packet,
    size_t packetLength,
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT *result,
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT *notifySnapshot,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OPENA8DJ_MIDI_FRAME_VIEW view;
    OPENA8DJ_MIDI_STATUS protocolStatus;
    size_t index;
    size_t bytesWritten = 0u;
    int wasEmpty;

    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (result == NULL || notifySnapshot == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    memset(notifySnapshot, 0, sizeof(*notifySnapshot));
    result->protocolStatus = OPENA8DJ_MIDI_OK;

    if (!OpenA8DJMidiParentCoreIsValid(core) || packet == NULL ||
        packetLength == 0u || packetLength > OPENA8DJ_MIDI_EP1_PACKET_BYTES) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }

    if (packet[0] != OPENA8DJ_MIDI_COMMAND_READ) {
        result->kind = OPENA8DJ_MIDI_PARENT_DEMUX_CONTROL_REPLY;
        result->controlReply = packet;
        result->controlReplyLength = packetLength;
        OpenA8DJMidiParentIncrement(&core->controlReplies);
        return OPENA8DJ_MIDI_PARENT_OK;
    }

    result->kind = OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_MALFORMED;
    protocolStatus = OpenA8DJMidiDecodeReadFrame(packet, packetLength, &view);
    result->protocolStatus = protocolStatus;
    if (protocolStatus != OPENA8DJ_MIDI_OK) {
        OpenA8DJMidiParentIncrement(&core->malformedMidiFrames);
        return protocolStatus == OPENA8DJ_MIDI_UNSUPPORTED_PORT ?
            OPENA8DJ_MIDI_PARENT_UNSUPPORTED_PORT :
            OPENA8DJ_MIDI_PARENT_MALFORMED_MIDI;
    }
    for (index = view.consumed; index < packetLength; index++) {
        if (packet[index] != 0u) {
            result->protocolStatus = OPENA8DJ_MIDI_INVALID_FRAME;
            OpenA8DJMidiParentIncrement(&core->malformedMidiFrames);
            return OPENA8DJ_MIDI_PARENT_MALFORMED_MIDI;
        }
    }
    result->paddingBytes = packetLength - view.consumed;
    if (core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }

    wasEmpty = OpenA8DJMidiRingAvailable(core->rxRing) == 0u;
    protocolStatus = OpenA8DJMidiRingWrite(
        core->rxRing,
        view.bytes,
        view.length,
        &bytesWritten);
    if (protocolStatus == OPENA8DJ_MIDI_RING_OVERFLOW) {
        result->kind = OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_OVERFLOW;
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_RX_OVERFLOW);
        return OPENA8DJ_MIDI_PARENT_RING_OVERFLOW;
    }
    if (protocolStatus != OPENA8DJ_MIDI_OK || bytesWritten != view.length) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }

    result->kind = OPENA8DJ_MIDI_PARENT_DEMUX_MIDI;
    result->midiBytes = bytesWritten;
    OpenA8DJMidiParentAdd(&core->rxBytes, bytesWritten);
    if (wasEmpty != 0 && bytesWritten != 0u &&
        core->rxOpen != 0u && core->rxNotify != NULL) {
        notifySnapshot->epoch = core->notifyEpoch;
        notifySnapshot->valid = 1u;
        OpenA8DJMidiParentIncrement(&core->notifyTransitions);
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_READY);
    }
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentAcquireNotify(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    const OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT *snapshot,
    OPENA8DJ_MIDI_PARENT_NOTIFY_DISPATCH *dispatch)
{
    if (dispatch == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    dispatch->callback = NULL;
    dispatch->context = NULL;
    if (!OpenA8DJMidiParentCoreIsValid(core) || snapshot == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (snapshot->valid == 0u || core->state != OPENA8DJ_MIDI_PARENT_ACCEPTING_IO ||
        core->rxOpen == 0u || core->rxNotify == NULL ||
        snapshot->epoch != core->notifyEpoch) {
        return OPENA8DJ_MIDI_PARENT_STALE_NOTIFY;
    }

    core->notifyInFlight++;
    dispatch->callback = core->rxNotify;
    dispatch->context = core->rxNotifyContext;
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentCompleteNotify(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_EVENTS *events)
{
    OpenA8DJMidiParentSetEvents(events, OPENA8DJ_MIDI_PARENT_EVENT_NONE);
    if (!OpenA8DJMidiParentCoreIsValid(core)) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }
    if (core->notifyInFlight == 0u) {
        return OPENA8DJ_MIDI_PARENT_INVALID_STATE;
    }

    core->notifyInFlight--;
    if (core->notifyInFlight == 0u && core->rxOpen == 0u) {
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_DRAINED);
    }
    if (core->state == OPENA8DJ_MIDI_PARENT_STOPPING &&
        OpenA8DJMidiParentTeardownReady(core)) {
        OpenA8DJMidiParentAddEvent(events, OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY);
    }
    return OPENA8DJ_MIDI_PARENT_OK;
}

OPENA8DJ_MIDI_PARENT_STATUS
OpenA8DJMidiParentGetRundown(
    const OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_PARENT_RUNDOWN *rundown)
{
    if (!OpenA8DJMidiParentCoreIsValid(core) || rundown == NULL) {
        return OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT;
    }

    rundown->state = core->state;
    rundown->notifyInFlight = core->notifyInFlight;
    rundown->rxOpen = core->rxOpen;
    rundown->txOpen = core->txOpen;
    rundown->teardownReady = (uint8_t)OpenA8DJMidiParentTeardownReady(core);
    return OPENA8DJ_MIDI_PARENT_OK;
}
