#ifndef OPENA8DJ_MIDI_MINIPORT_H
#define OPENA8DJ_MIDI_MINIPORT_H

#include <portcls.h>

#include "OpenA8DJMidiBusInterface.h"

NTSTATUS
OpenA8DJMidiCreateMiniport(
    _In_ const OPENA8DJ_MIDI_BUS_INTERFACE_V1 *busInterface,
    _Out_ PMINIPORTMIDI *miniport);

/*
 * Ownership contract: on success this factory consumes exactly the one
 * InterfaceReference returned by IRP_MN_QUERY_INTERFACE. On failure it consumes
 * no reference. The caller must zero/forget its local interface only on success.
 */

#endif
