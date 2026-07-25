#include "OpenA8DJMidiProtocol.h"

#include <string.h>

static size_t
OpenA8DJMidiMinimumSize(size_t left, size_t right)
{
    return left < right ? left : right;
}

static int
OpenA8DJMidiRingIsValid(const OPENA8DJ_MIDI_RING *ring)
{
    return ring != NULL &&
           ring->readOffset < OPENA8DJ_MIDI_RING_CAPACITY &&
           ring->writeOffset < OPENA8DJ_MIDI_RING_CAPACITY &&
           ring->count <= OPENA8DJ_MIDI_RING_CAPACITY;
}

static void
OpenA8DJMidiRecordOverflow(OPENA8DJ_MIDI_RING *ring, size_t length)
{
    uint64_t amount = (uint64_t)length;

    if (ring->overflowEvents != UINT64_MAX) {
        ring->overflowEvents++;
    }
    if (UINT64_MAX - ring->overflowBytes < amount) {
        ring->overflowBytes = UINT64_MAX;
    } else {
        ring->overflowBytes += amount;
    }
}

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiEncodeWriteFrame(
    uint8_t port,
    const uint8_t *bytes,
    size_t length,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength)
{
    size_t required;

    if (frameLength == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    *frameLength = 0u;

    if (bytes == NULL || frame == NULL || length == 0u) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    if (port != OPENA8DJ_MIDI_PORT) {
        return OPENA8DJ_MIDI_UNSUPPORTED_PORT;
    }
    if (length > OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES) {
        return OPENA8DJ_MIDI_PAYLOAD_TOO_LARGE;
    }

    required = OPENA8DJ_MIDI_HEADER_BYTES + length;
    if (frameCapacity < required) {
        return OPENA8DJ_MIDI_BUFFER_TOO_SMALL;
    }

    frame[0] = OPENA8DJ_MIDI_COMMAND_WRITE;
    frame[1] = port;
    frame[2] = (uint8_t)length;
    memcpy(frame + OPENA8DJ_MIDI_HEADER_BYTES, bytes, length);
    *frameLength = required;
    return OPENA8DJ_MIDI_OK;
}

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiDecodeReadFrame(
    const uint8_t *frame,
    size_t frameLength,
    OPENA8DJ_MIDI_FRAME_VIEW *view)
{
    size_t payloadLength;

    if (view == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    view->port = 0u;
    view->bytes = NULL;
    view->length = 0u;
    view->consumed = 0u;

    if (frame == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    if (frameLength < OPENA8DJ_MIDI_HEADER_BYTES ||
        frameLength > OPENA8DJ_MIDI_EP1_PACKET_BYTES) {
        return OPENA8DJ_MIDI_INVALID_FRAME;
    }
    if (frame[0] != OPENA8DJ_MIDI_COMMAND_READ) {
        return OPENA8DJ_MIDI_INVALID_FRAME;
    }
    if (frame[1] != OPENA8DJ_MIDI_PORT) {
        return OPENA8DJ_MIDI_UNSUPPORTED_PORT;
    }

    payloadLength = (size_t)frame[2];
    if (payloadLength > OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES) {
        return OPENA8DJ_MIDI_PAYLOAD_TOO_LARGE;
    }
    if (payloadLength > frameLength - OPENA8DJ_MIDI_HEADER_BYTES) {
        return OPENA8DJ_MIDI_INVALID_FRAME;
    }

    view->port = frame[1];
    view->bytes = frame + OPENA8DJ_MIDI_HEADER_BYTES;
    view->length = payloadLength;
    view->consumed = OPENA8DJ_MIDI_HEADER_BYTES + payloadLength;
    return OPENA8DJ_MIDI_OK;
}

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiEncodeNextWriteFrame(
    uint8_t port,
    const uint8_t *stream,
    size_t streamLength,
    size_t *streamOffset,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength)
{
    OPENA8DJ_MIDI_STATUS status;
    size_t remaining;
    size_t chunkLength;

    if (frameLength == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    *frameLength = 0u;

    if (streamOffset == NULL || frame == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    if (*streamOffset > streamLength) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    if (*streamOffset == streamLength) {
        return OPENA8DJ_MIDI_EMPTY;
    }
    if (stream == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }

    remaining = streamLength - *streamOffset;
    chunkLength = OpenA8DJMidiMinimumSize(
        remaining,
        OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES);
    status = OpenA8DJMidiEncodeWriteFrame(
        port,
        stream + *streamOffset,
        chunkLength,
        frame,
        frameCapacity,
        frameLength);
    if (status == OPENA8DJ_MIDI_OK) {
        *streamOffset += chunkLength;
    }
    return status;
}

void
OpenA8DJMidiRingInitialize(OPENA8DJ_MIDI_RING *ring)
{
    if (ring != NULL) {
        memset(ring, 0, sizeof(*ring));
    }
}

size_t
OpenA8DJMidiRingAvailable(const OPENA8DJ_MIDI_RING *ring)
{
    return OpenA8DJMidiRingIsValid(ring) ? ring->count : 0u;
}

size_t
OpenA8DJMidiRingFree(const OPENA8DJ_MIDI_RING *ring)
{
    return OpenA8DJMidiRingIsValid(ring) ?
        OPENA8DJ_MIDI_RING_CAPACITY - ring->count : 0u;
}

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiRingWrite(
    OPENA8DJ_MIDI_RING *ring,
    const uint8_t *bytes,
    size_t length,
    size_t *bytesWritten)
{
    size_t firstPart;
    size_t secondPart;

    if (bytesWritten == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    *bytesWritten = 0u;

    if (!OpenA8DJMidiRingIsValid(ring) ||
        (bytes == NULL && length != 0u)) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    if (length > OPENA8DJ_MIDI_RING_CAPACITY - ring->count) {
        OpenA8DJMidiRecordOverflow(ring, length);
        return OPENA8DJ_MIDI_RING_OVERFLOW;
    }
    if (length == 0u) {
        return OPENA8DJ_MIDI_OK;
    }

    firstPart = OpenA8DJMidiMinimumSize(
        length,
        OPENA8DJ_MIDI_RING_CAPACITY - ring->writeOffset);
    secondPart = length - firstPart;
    memcpy(ring->storage + ring->writeOffset, bytes, firstPart);
    if (secondPart != 0u) {
        memcpy(ring->storage, bytes + firstPart, secondPart);
    }

    ring->writeOffset =
        (ring->writeOffset + length) % OPENA8DJ_MIDI_RING_CAPACITY;
    ring->count += length;
    *bytesWritten = length;
    return OPENA8DJ_MIDI_OK;
}

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiRingRead(
    OPENA8DJ_MIDI_RING *ring,
    uint8_t *bytes,
    size_t capacity,
    size_t *bytesRead)
{
    size_t length;
    size_t firstPart;
    size_t secondPart;

    if (bytesRead == NULL) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }
    *bytesRead = 0u;

    if (!OpenA8DJMidiRingIsValid(ring) ||
        (bytes == NULL && capacity != 0u)) {
        return OPENA8DJ_MIDI_INVALID_ARGUMENT;
    }

    length = OpenA8DJMidiMinimumSize(capacity, ring->count);
    if (length == 0u) {
        return OPENA8DJ_MIDI_OK;
    }

    firstPart = OpenA8DJMidiMinimumSize(
        length,
        OPENA8DJ_MIDI_RING_CAPACITY - ring->readOffset);
    secondPart = length - firstPart;
    memcpy(bytes, ring->storage + ring->readOffset, firstPart);
    if (secondPart != 0u) {
        memcpy(bytes + firstPart, ring->storage, secondPart);
    }

    ring->readOffset =
        (ring->readOffset + length) % OPENA8DJ_MIDI_RING_CAPACITY;
    ring->count -= length;
    *bytesRead = length;
    return OPENA8DJ_MIDI_OK;
}
