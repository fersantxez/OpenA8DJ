#include "OpenA8DJMidiParentCore.h"

#include <stdio.h>
#include <string.h>

#define EXPECT_TRUE(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "FAIL:%s:%d:%s\n", __FILE__, __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

static void
NoopNotify(void *context)
{
    (void)context;
}

static int
Setup(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_RING *rxRing,
    OPENA8DJ_MIDI_RING *txRing)
{
    OPENA8DJ_MIDI_PARENT_EVENTS events;

    EXPECT_TRUE(OpenA8DJMidiParentInitialize(core, rxRing, txRing) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentPrepare(core, &events) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentOpen(
                    core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    NoopNotify,
                    NULL,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    core,
                    OPENA8DJ_MIDI_PARENT_ACCEPTING_IO,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    return 0;
}

static int
TestGoldenAndControlPreservation(void)
{
    static const uint8_t midi[] = { 0x06u, 0x00u, 0x03u, 0x90u, 0x40u, 0x7fu };
    static const uint8_t control[] = { 0x04u, 0xaau, 0xbbu, 0xccu };
    static const uint8_t writeCommandReply[] = { 0x07u, 0x00u, 0x01u, 0x55u };
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT result;
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT snapshot;
    OPENA8DJ_MIDI_PARENT_EVENTS events;
    uint8_t bytes[16];
    size_t bytesRead;

    EXPECT_TRUE(Setup(&core, &rxRing, &txRing) == 0);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    control,
                    sizeof(control),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_CONTROL_REPLY);
    EXPECT_TRUE(result.controlReply == control);
    EXPECT_TRUE(result.controlReplyLength == sizeof(control));
    EXPECT_TRUE(memcmp(result.controlReply, control, sizeof(control)) == 0);
    EXPECT_TRUE(snapshot.valid == 0u);

    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    writeCommandReply,
                    sizeof(writeCommandReply),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_CONTROL_REPLY);
    EXPECT_TRUE(result.controlReply == writeCommandReply);

    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    midi,
                    sizeof(midi),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_MIDI);
    EXPECT_TRUE(result.midiBytes == 3u && result.paddingBytes == 0u);
    EXPECT_TRUE(snapshot.valid != 0u);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_READY) != 0u);

    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    midi,
                    sizeof(midi),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(snapshot.valid == 0u);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_READY) == 0u);
    EXPECT_TRUE(OpenA8DJMidiParentRead(&core, bytes, sizeof(bytes), &bytesRead) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(bytesRead == 6u);
    EXPECT_TRUE(memcmp(bytes, midi + OPENA8DJ_MIDI_HEADER_BYTES, 3u) == 0);
    EXPECT_TRUE(memcmp(bytes + 3u, midi + OPENA8DJ_MIDI_HEADER_BYTES, 3u) == 0);
    return 0;
}

static int
TestPaddingAndMalformedMidi(void)
{
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT result;
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT snapshot;
    OPENA8DJ_MIDI_PARENT_EVENTS events;
    uint8_t packet[OPENA8DJ_MIDI_EP1_PACKET_BYTES];
    size_t before;

    EXPECT_TRUE(Setup(&core, &rxRing, &txRing) == 0);
    memset(packet, 0, sizeof(packet));
    packet[0] = OPENA8DJ_MIDI_COMMAND_READ;
    packet[1] = OPENA8DJ_MIDI_PORT;
    packet[2] = 3u;
    packet[3] = 0x80u;
    packet[4] = 0x40u;
    packet[5] = 0x00u;
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    packet,
                    sizeof(packet),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(result.paddingBytes == sizeof(packet) - 6u);
    EXPECT_TRUE(result.midiBytes == 3u);
    before = OpenA8DJMidiRingAvailable(&rxRing);

    packet[63] = 1u;
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    packet,
                    sizeof(packet),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_MALFORMED_MIDI);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_MALFORMED);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&rxRing) == before);

    packet[63] = 0u;
    packet[1] = 1u;
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    packet,
                    6u,
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_UNSUPPORTED_PORT);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_MALFORMED);
    EXPECT_TRUE(result.controlReply == NULL);

    packet[1] = OPENA8DJ_MIDI_PORT;
    packet[2] = 4u;
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    packet,
                    6u,
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_MALFORMED_MIDI);
    EXPECT_TRUE(result.controlReply == NULL);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    packet,
                    2u,
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_MALFORMED_MIDI);

    packet[2] = 0u;
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    packet,
                    3u,
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_MIDI);
    EXPECT_TRUE(result.midiBytes == 0u && snapshot.valid == 0u);
    return 0;
}

static int
TestOverflowAllOrNothing(void)
{
    static const uint8_t midi[] = { 0x06u, 0x00u, 0x03u, 0x90u, 0x41u, 0x7fu };
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT result;
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT snapshot;
    OPENA8DJ_MIDI_PARENT_EVENTS events;
    uint8_t fill[OPENA8DJ_MIDI_RING_CAPACITY - 1u];
    size_t bytesWritten;
    size_t before;

    EXPECT_TRUE(Setup(&core, &rxRing, &txRing) == 0);
    memset(fill, 0x55, sizeof(fill));
    EXPECT_TRUE(OpenA8DJMidiRingWrite(
                    &rxRing,
                    fill,
                    sizeof(fill),
                    &bytesWritten) == OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(bytesWritten == sizeof(fill));
    before = OpenA8DJMidiRingAvailable(&rxRing);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    midi,
                    sizeof(midi),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_RING_OVERFLOW);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_OVERFLOW);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_RX_OVERFLOW) != 0u);
    EXPECT_TRUE(snapshot.valid == 0u);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&rxRing) == before);
    EXPECT_TRUE(rxRing.overflowEvents == 1u);
    return 0;
}

static int
TestStateGateAndExclusiveClassification(void)
{
    static const uint8_t midi[] = { 0x06u, 0x00u, 0x01u, 0xf8u };
    static const uint8_t control[] = { 0x05u, 0x00u };
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT result;
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT snapshot;
    OPENA8DJ_MIDI_PARENT_EVENTS events;

    EXPECT_TRUE(Setup(&core, &rxRing, &txRing) == 0);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &core,
                    OPENA8DJ_MIDI_PARENT_STOPPING,
                    &events) == OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    midi,
                    sizeof(midi),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_INVALID_STATE);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_MIDI_MALFORMED);
    EXPECT_TRUE(result.controlReply == NULL);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    control,
                    sizeof(control),
                    &result,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(result.kind == OPENA8DJ_MIDI_PARENT_DEMUX_CONTROL_REPLY);
    EXPECT_TRUE(result.controlReply == control);
    return 0;
}

int
main(void)
{
    EXPECT_TRUE(TestGoldenAndControlPreservation() == 0);
    EXPECT_TRUE(TestPaddingAndMalformedMidi() == 0);
    EXPECT_TRUE(TestOverflowAllOrNothing() == 0);
    EXPECT_TRUE(TestStateGateAndExclusiveClassification() == 0);
    printf("PASS: OpenA8DJ MIDI EP1 demux\n");
    return 0;
}
