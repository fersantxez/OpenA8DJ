#ifndef OPENA8DJ_MIDI_PROTOCOL_H
#define OPENA8DJ_MIDI_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENA8DJ_MIDI_COMMAND_READ 0x06u
#define OPENA8DJ_MIDI_COMMAND_WRITE 0x07u
#define OPENA8DJ_MIDI_PORT 0u
#define OPENA8DJ_MIDI_EP1_PACKET_BYTES 64u
#define OPENA8DJ_MIDI_HEADER_BYTES 3u
#define OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES \
    (OPENA8DJ_MIDI_EP1_PACKET_BYTES - OPENA8DJ_MIDI_HEADER_BYTES)
#define OPENA8DJ_MIDI_RING_CAPACITY 4096u

typedef enum OPENA8DJ_MIDI_STATUS {
    OPENA8DJ_MIDI_OK = 0,
    OPENA8DJ_MIDI_INVALID_ARGUMENT,
    OPENA8DJ_MIDI_INVALID_FRAME,
    OPENA8DJ_MIDI_UNSUPPORTED_PORT,
    OPENA8DJ_MIDI_BUFFER_TOO_SMALL,
    OPENA8DJ_MIDI_PAYLOAD_TOO_LARGE,
    OPENA8DJ_MIDI_EMPTY,
    OPENA8DJ_MIDI_RING_OVERFLOW
} OPENA8DJ_MIDI_STATUS;

typedef struct OPENA8DJ_MIDI_FRAME_VIEW {
    uint8_t port;
    const uint8_t *bytes;
    size_t length;
    size_t consumed;
} OPENA8DJ_MIDI_FRAME_VIEW;

typedef struct OPENA8DJ_MIDI_RING {
    /*
     * This portable ring deliberately contains no platform lock or atomics.
     * It is NOT thread-safe: initialize/read/write/query operations on one
     * instance must all be serialized by the same caller-owned lock.  Kernel
     * integration must hold its MIDI queue spin lock for every operation.
     */
    uint8_t storage[OPENA8DJ_MIDI_RING_CAPACITY];
    size_t readOffset;
    size_t writeOffset;
    size_t count;
    uint64_t overflowEvents;
    uint64_t overflowBytes;
} OPENA8DJ_MIDI_RING;

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiEncodeWriteFrame(
    uint8_t port,
    const uint8_t *bytes,
    size_t length,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength);

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiDecodeReadFrame(
    const uint8_t *frame,
    size_t frameLength,
    OPENA8DJ_MIDI_FRAME_VIEW *view);

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiEncodeNextWriteFrame(
    uint8_t port,
    const uint8_t *stream,
    size_t streamLength,
    size_t *streamOffset,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength);

void
OpenA8DJMidiRingInitialize(OPENA8DJ_MIDI_RING *ring);

size_t
OpenA8DJMidiRingAvailable(const OPENA8DJ_MIDI_RING *ring);

size_t
OpenA8DJMidiRingFree(const OPENA8DJ_MIDI_RING *ring);

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiRingWrite(
    OPENA8DJ_MIDI_RING *ring,
    const uint8_t *bytes,
    size_t length,
    size_t *bytesWritten);

OPENA8DJ_MIDI_STATUS
OpenA8DJMidiRingRead(
    OPENA8DJ_MIDI_RING *ring,
    uint8_t *bytes,
    size_t capacity,
    size_t *bytesRead);

#ifdef __cplusplus
}
#endif

#endif
