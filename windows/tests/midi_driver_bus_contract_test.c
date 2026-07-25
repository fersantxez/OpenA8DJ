#include "OpenA8DJMidiBusContract.h"

#include <stdio.h>
#include <string.h>

#define EXPECT_TRUE(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "FAIL:%s:%d:%s\n", __FILE__, __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

static OPENA8DJ_MIDI_BUS_CONTRACT_VIEW
ValidContract(void)
{
    OPENA8DJ_MIDI_BUS_CONTRACT_VIEW view;

    memset(&view, 0, sizeof(view));
    view.size = OPENA8DJ_MIDI_BUS_INTERFACE_V1_X64_SIZE;
    view.requiredSize = OPENA8DJ_MIDI_BUS_INTERFACE_V1_X64_SIZE;
    view.version = OPENA8DJ_MIDI_BUS_INTERFACE_VERSION_V1;
    view.magic = OPENA8DJ_MIDI_BUS_INTERFACE_MAGIC;
    view.capabilities = OPENA8DJ_MIDI_BUS_REQUIRED_CAPABILITIES;
    view.functions = OPENA8DJ_MIDI_BUS_REQUIRED_FUNCTIONS;
    return view;
}

int
main(void)
{
    OPENA8DJ_MIDI_BUS_CONTRACT_VIEW view = ValidContract();
    uint32_t bit;

    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(NULL) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_NULL);
    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_OK);

    view.size = OPENA8DJ_MIDI_BUS_INTERFACE_V1_X64_SIZE - 1u;
    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_SIZE);
    view = ValidContract();
    view.requiredSize = 0u;
    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_SIZE);
    view = ValidContract();
    view.version++;
    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_VERSION);
    view = ValidContract();
    view.magic ^= UINT32_C(1);
    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_MAGIC);

    for (bit = UINT32_C(1); bit <= OPENA8DJ_MIDI_BUS_CAP_SYNCHRONOUS_QUERY;
         bit <<= 1u) {
        view = ValidContract();
        view.capabilities &= ~bit;
        EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                    OPENA8DJ_MIDI_BUS_CONTRACT_CAPABILITIES);
    }
    for (bit = UINT32_C(1); bit <= OPENA8DJ_MIDI_BUS_FUNCTION_WRITE;
         bit <<= 1u) {
        view = ValidContract();
        view.functions &= ~bit;
        EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                    OPENA8DJ_MIDI_BUS_CONTRACT_FUNCTIONS);
    }

    view = ValidContract();
    view.size++;
    view.capabilities |= UINT32_C(0x80000000);
    view.functions |= UINT32_C(0x40000000);
    EXPECT_TRUE(OpenA8DJMidiValidateBusContract(&view) ==
                OPENA8DJ_MIDI_BUS_CONTRACT_OK);

    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_SUCCESS, 32u, 32u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_COMPLETE);
    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_SUCCESS, 32u, 0u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR);
    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_SUCCESS, 32u, 16u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR);
    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_BUSY, 32u, 0u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_BACKPRESSURE);
    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_BUSY, 32u, 1u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR);
    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_FAILURE, 32u, 0u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_TRANSPORT_ERROR);
    EXPECT_TRUE(OpenA8DJMidiClassifyBusWriteResult(
                    OPENA8DJ_MIDI_BUS_WRITE_STATUS_FAILURE, 32u, 32u) ==
                OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR);

    printf("PASS: OpenA8DJ MIDI driver bus ABI contract\n");
    return 0;
}
