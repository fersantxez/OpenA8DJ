#include "OpenA8DJMidiBusContract.h"

OPENA8DJ_MIDI_BUS_CONTRACT_RESULT
OpenA8DJMidiValidateBusContract(
    const OPENA8DJ_MIDI_BUS_CONTRACT_VIEW *contractView)
{
    if (contractView == NULL) {
        return OPENA8DJ_MIDI_BUS_CONTRACT_NULL;
    }
    if (contractView->requiredSize == 0u ||
        contractView->size < contractView->requiredSize) {
        return OPENA8DJ_MIDI_BUS_CONTRACT_SIZE;
    }
    if (contractView->version != OPENA8DJ_MIDI_BUS_INTERFACE_VERSION_V1) {
        return OPENA8DJ_MIDI_BUS_CONTRACT_VERSION;
    }
    if (contractView->magic != OPENA8DJ_MIDI_BUS_INTERFACE_MAGIC) {
        return OPENA8DJ_MIDI_BUS_CONTRACT_MAGIC;
    }
    if ((contractView->capabilities & OPENA8DJ_MIDI_BUS_REQUIRED_CAPABILITIES) !=
        OPENA8DJ_MIDI_BUS_REQUIRED_CAPABILITIES) {
        return OPENA8DJ_MIDI_BUS_CONTRACT_CAPABILITIES;
    }
    if ((contractView->functions & OPENA8DJ_MIDI_BUS_REQUIRED_FUNCTIONS) !=
        OPENA8DJ_MIDI_BUS_REQUIRED_FUNCTIONS) {
        return OPENA8DJ_MIDI_BUS_CONTRACT_FUNCTIONS;
    }
    return OPENA8DJ_MIDI_BUS_CONTRACT_OK;
}

OPENA8DJ_MIDI_PORTCLS_WRITE_ACTION
OpenA8DJMidiClassifyBusWriteResult(
    OPENA8DJ_MIDI_BUS_WRITE_STATUS_CLASS statusClass,
    OPENA8DJ_MIDI_UINT32 requestedBytes,
    OPENA8DJ_MIDI_UINT32 reportedBytes)
{
    if (statusClass == OPENA8DJ_MIDI_BUS_WRITE_STATUS_SUCCESS) {
        return reportedBytes == requestedBytes
                   ? OPENA8DJ_MIDI_PORTCLS_WRITE_COMPLETE
                   : OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR;
    }
    if (statusClass == OPENA8DJ_MIDI_BUS_WRITE_STATUS_BUSY) {
        return reportedBytes == 0u
                   ? OPENA8DJ_MIDI_PORTCLS_WRITE_BACKPRESSURE
                   : OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR;
    }
    return reportedBytes == 0u
               ? OPENA8DJ_MIDI_PORTCLS_WRITE_TRANSPORT_ERROR
               : OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR;
}
