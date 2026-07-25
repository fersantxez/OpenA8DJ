#include "OpenA8DJMidiProtocol.h"

#include <stdio.h>
#include <string.h>

#define RANDOM_TEST_BYTES (1024u * 1024u)

#define EXPECT_TRUE(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "FAIL:%s:%d:%s\n", __FILE__, __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

static uint8_t gRandomInput[RANDOM_TEST_BYTES];
static uint8_t gRandomOutput[RANDOM_TEST_BYTES];

static int
RoundTripStream(const uint8_t *input, size_t length, size_t expectedFrames)
{
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES];
    uint8_t output[256];
    OPENA8DJ_MIDI_FRAME_VIEW view;
    size_t inputOffset = 0u;
    size_t outputOffset = 0u;
    size_t frameLength;
    size_t frameCount = 0u;

    EXPECT_TRUE(length <= sizeof(output));
    while (inputOffset < length) {
        EXPECT_TRUE(OpenA8DJMidiEncodeNextWriteFrame(
                        OPENA8DJ_MIDI_PORT,
                        input,
                        length,
                        &inputOffset,
                        frame,
                        sizeof(frame),
                        &frameLength) == OPENA8DJ_MIDI_OK);
        EXPECT_TRUE(frameLength >= OPENA8DJ_MIDI_HEADER_BYTES);
        EXPECT_TRUE(frameLength <= OPENA8DJ_MIDI_EP1_PACKET_BYTES);
        EXPECT_TRUE(frame[0] == OPENA8DJ_MIDI_COMMAND_WRITE);
        EXPECT_TRUE(frame[1] == OPENA8DJ_MIDI_PORT);
        EXPECT_TRUE(frame[2] <= OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES);

        frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
        EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, frameLength, &view) ==
                    OPENA8DJ_MIDI_OK);
        EXPECT_TRUE(view.consumed == frameLength);
        EXPECT_TRUE(outputOffset + view.length <= sizeof(output));
        memcpy(output + outputOffset, view.bytes, view.length);
        outputOffset += view.length;
        frameCount++;
    }

    EXPECT_TRUE(inputOffset == length);
    EXPECT_TRUE(outputOffset == length);
    EXPECT_TRUE(frameCount == expectedFrames);
    EXPECT_TRUE(memcmp(input, output, length) == 0);
    return 0;
}

static int
TestRequiredLengths(void)
{
    static const size_t lengths[] = { 1u, 3u, 61u, 62u, 122u };
    static const size_t frames[] = { 1u, 1u, 1u, 2u, 2u };
    uint8_t input[122];
    size_t index;
    size_t byteIndex;

    for (byteIndex = 0u; byteIndex < sizeof(input); byteIndex++) {
        input[byteIndex] = (uint8_t)((byteIndex * 73u + 19u) & 0xffu);
    }
    for (index = 0u; index < sizeof(lengths) / sizeof(lengths[0]); index++) {
        EXPECT_TRUE(RoundTripStream(input, lengths[index], frames[index]) == 0);
    }
    return 0;
}

static int
TestArbitraryMidiBytes(void)
{
    static const uint8_t stream[] = {
        0x90u, 0x3cu, 0x7fu, 0x3du, 0x00u,
        0xf0u, 0x00u, 0x7fu, 0xf8u, 0x01u, 0xfeu, 0xffu, 0xf7u,
        0x80u, 0x3cu, 0x00u
    };

    EXPECT_TRUE(RoundTripStream(stream, sizeof(stream), 1u) == 0);
    return 0;
}

static int
TestIndependentGoldenVectors(void)
{
    static const uint8_t payload[] = { 0x90u, 0x3cu, 0x7fu };
    static const uint8_t expectedWrite[] = {
        0x07u, 0x00u, 0x03u, 0x90u, 0x3cu, 0x7fu
    };
    static const uint8_t literalRead[] = {
        0x06u, 0x00u, 0x06u, 0xf0u, 0x00u, 0xf8u, 0x7fu, 0xfeu, 0xf7u
    };
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES];
    OPENA8DJ_MIDI_FRAME_VIEW view;
    size_t frameLength = 0u;

    memset(frame, 0xa5, sizeof(frame));
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    sizeof(payload),
                    frame,
                    sizeof(frame),
                    &frameLength) == OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(frameLength == sizeof(expectedWrite));
    EXPECT_TRUE(memcmp(frame, expectedWrite, sizeof(expectedWrite)) == 0);

    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(
                    literalRead,
                    sizeof(literalRead),
                    &view) == OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(view.port == 0u);
    EXPECT_TRUE(view.length == 6u);
    EXPECT_TRUE(view.consumed == sizeof(literalRead));
    EXPECT_TRUE(memcmp(view.bytes, literalRead + 3u, 6u) == 0);
    return 0;
}

static int
TestEncodeGuardsAndErrors(void)
{
    uint8_t guarded[OPENA8DJ_MIDI_EP1_PACKET_BYTES + 2u];
    uint8_t payload[OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES + 1u];
    uint8_t *frame = guarded + 1u;
    size_t frameLength = 99u;
    size_t offset;
    size_t index;

    for (index = 0u; index < sizeof(payload); index++) {
        payload[index] = (uint8_t)index;
    }
    memset(guarded, 0xa5, sizeof(guarded));
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(frameLength == OPENA8DJ_MIDI_EP1_PACKET_BYTES);
    EXPECT_TRUE(guarded[0] == 0xa5u);
    EXPECT_TRUE(guarded[sizeof(guarded) - 1u] == 0xa5u);
    EXPECT_TRUE(frame[0] == OPENA8DJ_MIDI_COMMAND_WRITE);
    EXPECT_TRUE(frame[1] == OPENA8DJ_MIDI_PORT);
    EXPECT_TRUE(frame[2] == OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES);
    EXPECT_TRUE(memcmp(frame + OPENA8DJ_MIDI_HEADER_BYTES,
                       payload,
                       OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES) == 0);

    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    0u,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(frameLength == 0u);
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    1u,
                    payload,
                    1u,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_UNSUPPORTED_PORT);
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES + 1u,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_PAYLOAD_TOO_LARGE);
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    1u,
                    frame,
                    OPENA8DJ_MIDI_HEADER_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_BUFFER_TOO_SMALL);
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    NULL,
                    1u,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    1u,
                    NULL,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(OpenA8DJMidiEncodeWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    1u,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    NULL) == OPENA8DJ_MIDI_INVALID_ARGUMENT);

    offset = 1u;
    EXPECT_TRUE(OpenA8DJMidiEncodeNextWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    1u,
                    &offset,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_EMPTY);
    EXPECT_TRUE(frameLength == 0u);
    offset = 2u;
    EXPECT_TRUE(OpenA8DJMidiEncodeNextWriteFrame(
                    OPENA8DJ_MIDI_PORT,
                    payload,
                    1u,
                    &offset,
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &frameLength) == OPENA8DJ_MIDI_INVALID_ARGUMENT);
    return 0;
}

static int
TestMalformedFrames(void)
{
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES + 1u];
    OPENA8DJ_MIDI_FRAME_VIEW view;
    size_t index;

    memset(frame, 0, sizeof(frame));
    frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
    frame[1] = OPENA8DJ_MIDI_PORT;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(NULL, 3u, &view) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 3u, NULL) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 0u, &view) ==
                OPENA8DJ_MIDI_INVALID_FRAME);
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 1u, &view) ==
                OPENA8DJ_MIDI_INVALID_FRAME);
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 2u, &view) ==
                OPENA8DJ_MIDI_INVALID_FRAME);
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES + 1u,
                    &view) == OPENA8DJ_MIDI_INVALID_FRAME);

    frame[0] = OPENA8DJ_MIDI_COMMAND_WRITE;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 3u, &view) ==
                OPENA8DJ_MIDI_INVALID_FRAME);
    frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
    frame[1] = 1u;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 3u, &view) ==
                OPENA8DJ_MIDI_UNSUPPORTED_PORT);
    frame[1] = OPENA8DJ_MIDI_PORT;
    frame[2] = OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES + 1u;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &view) == OPENA8DJ_MIDI_PAYLOAD_TOO_LARGE);
    frame[2] = 8u;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, 10u, &view) ==
                OPENA8DJ_MIDI_INVALID_FRAME);

    for (index = 0u; index < sizeof(frame); index++) {
        frame[index] = 0xccu;
    }
    frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
    frame[1] = OPENA8DJ_MIDI_PORT;
    frame[2] = 1u;
    frame[3] = 0xf8u;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(
                    frame,
                    OPENA8DJ_MIDI_EP1_PACKET_BYTES,
                    &view) == OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(view.port == OPENA8DJ_MIDI_PORT);
    EXPECT_TRUE(view.length == 1u);
    EXPECT_TRUE(view.bytes[0] == 0xf8u);
    EXPECT_TRUE(view.consumed == 4u);
    return 0;
}

static int
TestRingWrapAndFull(void)
{
    OPENA8DJ_MIDI_RING ring;
    uint8_t first[3000];
    uint8_t second[2000];
    uint8_t output[3000];
    uint8_t fill[OPENA8DJ_MIDI_RING_CAPACITY];
    uint8_t one = 0x7fu;
    size_t count;
    size_t index;

    for (index = 0u; index < sizeof(first); index++) {
        first[index] = (uint8_t)((index * 29u) & 0xffu);
    }
    for (index = 0u; index < sizeof(second); index++) {
        second[index] = (uint8_t)((index * 47u + 3u) & 0xffu);
    }

    OpenA8DJMidiRingInitialize(&ring);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&ring) == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingFree(&ring) == OPENA8DJ_MIDI_RING_CAPACITY);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, first, sizeof(first), &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == sizeof(first));
    EXPECT_TRUE(OpenA8DJMidiRingRead(&ring, output, 2500u, &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == 2500u);
    EXPECT_TRUE(memcmp(output, first, count) == 0);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, second, sizeof(second), &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == sizeof(second));
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&ring) == 2500u);
    EXPECT_TRUE(OpenA8DJMidiRingRead(&ring, output, sizeof(output), &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == 2500u);
    EXPECT_TRUE(memcmp(output, first + 2500u, 500u) == 0);
    EXPECT_TRUE(memcmp(output + 500u, second, sizeof(second)) == 0);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&ring) == 0u);

    memset(fill, 0x5a, sizeof(fill));
    OpenA8DJMidiRingInitialize(&ring);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, fill, sizeof(fill), &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == sizeof(fill));
    EXPECT_TRUE(OpenA8DJMidiRingFree(&ring) == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, &one, 1u, &count) ==
                OPENA8DJ_MIDI_RING_OVERFLOW);
    EXPECT_TRUE(count == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&ring) == sizeof(fill));
    EXPECT_TRUE(ring.overflowEvents == UINT64_C(1));
    EXPECT_TRUE(ring.overflowBytes == UINT64_C(1));

    OpenA8DJMidiRingInitialize(&ring);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(
                    &ring,
                    fill,
                    OPENA8DJ_MIDI_RING_CAPACITY - 2u,
                    &count) == OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, fill, 3u, &count) ==
                OPENA8DJ_MIDI_RING_OVERFLOW);
    EXPECT_TRUE(count == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&ring) ==
                OPENA8DJ_MIDI_RING_CAPACITY - 2u);
    EXPECT_TRUE(ring.overflowEvents == UINT64_C(1));
    EXPECT_TRUE(ring.overflowBytes == UINT64_C(3));

    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, NULL, 0u, &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingRead(&ring, NULL, 0u, &count) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(count == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(NULL, &one, 1u, &count) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(OpenA8DJMidiRingRead(NULL, &one, 1u, &count) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    return 0;
}

static int
TestCorruptRingStateIsRejected(void)
{
    OPENA8DJ_MIDI_RING ring;
    uint8_t byte = 0x90u;
    size_t count = 99u;

    OpenA8DJMidiRingInitialize(&ring);
    ring.count = OPENA8DJ_MIDI_RING_CAPACITY + 1u;
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&ring) == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingFree(&ring) == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, &byte, 1u, &count) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(count == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingRead(&ring, &byte, 1u, &count) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    EXPECT_TRUE(count == 0u);

    OpenA8DJMidiRingInitialize(&ring);
    ring.readOffset = OPENA8DJ_MIDI_RING_CAPACITY;
    EXPECT_TRUE(OpenA8DJMidiRingRead(&ring, &byte, 1u, &count) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    OpenA8DJMidiRingInitialize(&ring);
    ring.writeOffset = OPENA8DJ_MIDI_RING_CAPACITY;
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, &byte, 1u, &count) ==
                OPENA8DJ_MIDI_INVALID_ARGUMENT);
    return 0;
}

static int
TestOverflowCountersSaturate(void)
{
    OPENA8DJ_MIDI_RING ring;
    uint8_t bytes[2] = { 0x90u, 0x00u };
    size_t written = 99u;

    OpenA8DJMidiRingInitialize(&ring);
    ring.count = OPENA8DJ_MIDI_RING_CAPACITY;
    ring.overflowEvents = UINT64_MAX - UINT64_C(1);
    ring.overflowBytes = UINT64_MAX - UINT64_C(1);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, bytes, sizeof(bytes), &written) ==
                OPENA8DJ_MIDI_RING_OVERFLOW);
    EXPECT_TRUE(written == 0u);
    EXPECT_TRUE(ring.overflowEvents == UINT64_MAX);
    EXPECT_TRUE(ring.overflowBytes == UINT64_MAX);
    EXPECT_TRUE(OpenA8DJMidiRingWrite(&ring, bytes, 1u, &written) ==
                OPENA8DJ_MIDI_RING_OVERFLOW);
    EXPECT_TRUE(ring.overflowEvents == UINT64_MAX);
    EXPECT_TRUE(ring.overflowBytes == UINT64_MAX);
    return 0;
}

static uint32_t
NextRandom(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    *state = value;
    return value;
}

static int
TestOneMiBExact(void)
{
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES];
    OPENA8DJ_MIDI_FRAME_VIEW view;
    uint32_t randomState = UINT32_C(0x6d2b79f5);
    size_t inputOffset = 0u;
    size_t outputOffset = 0u;
    size_t frameLength;
    size_t frameCount = 0u;
    size_t index;

    for (index = 0u; index < RANDOM_TEST_BYTES; index++) {
        gRandomInput[index] = (uint8_t)(NextRandom(&randomState) & 0xffu);
    }
    memset(gRandomOutput, 0, sizeof(gRandomOutput));

    while (inputOffset < RANDOM_TEST_BYTES) {
        EXPECT_TRUE(OpenA8DJMidiEncodeNextWriteFrame(
                        OPENA8DJ_MIDI_PORT,
                        gRandomInput,
                        RANDOM_TEST_BYTES,
                        &inputOffset,
                        frame,
                        sizeof(frame),
                        &frameLength) == OPENA8DJ_MIDI_OK);
        EXPECT_TRUE(frame[0] == OPENA8DJ_MIDI_COMMAND_WRITE);
        frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
        EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, frameLength, &view) ==
                    OPENA8DJ_MIDI_OK);
        EXPECT_TRUE(outputOffset + view.length <= RANDOM_TEST_BYTES);
        memcpy(gRandomOutput + outputOffset, view.bytes, view.length);
        outputOffset += view.length;
        frameCount++;
    }

    EXPECT_TRUE(inputOffset == RANDOM_TEST_BYTES);
    EXPECT_TRUE(outputOffset == RANDOM_TEST_BYTES);
    EXPECT_TRUE(frameCount ==
                (RANDOM_TEST_BYTES + OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES - 1u) /
                    OPENA8DJ_MIDI_MAX_PAYLOAD_BYTES);
    EXPECT_TRUE(memcmp(gRandomInput, gRandomOutput, RANDOM_TEST_BYTES) == 0);
    return 0;
}

int
main(void)
{
    EXPECT_TRUE(TestRequiredLengths() == 0);
    EXPECT_TRUE(TestArbitraryMidiBytes() == 0);
    EXPECT_TRUE(TestIndependentGoldenVectors() == 0);
    EXPECT_TRUE(TestEncodeGuardsAndErrors() == 0);
    EXPECT_TRUE(TestMalformedFrames() == 0);
    EXPECT_TRUE(TestRingWrapAndFull() == 0);
    EXPECT_TRUE(TestCorruptRingStateIsRejected() == 0);
    EXPECT_TRUE(TestOverflowCountersSaturate() == 0);
    EXPECT_TRUE(TestOneMiBExact() == 0);

    printf("PASS: OpenA8DJ offline CAIAQ MIDI protocol contract\n");
    return 0;
}
