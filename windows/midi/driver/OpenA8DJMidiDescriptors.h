#ifndef OPENA8DJ_MIDI_DESCRIPTORS_H
#define OPENA8DJ_MIDI_DESCRIPTORS_H

#include <portcls.h>

#define OPENA8DJ_MIDI_PIN_RENDER 0u
#define OPENA8DJ_MIDI_PIN_CAPTURE 1u
#define OPENA8DJ_MIDI_PIN_COUNT 2u

extern const PCFILTER_DESCRIPTOR g_OpenA8DJMidiFilterDescriptor;

BOOLEAN
OpenA8DJMidiIsSupportedFormat(_In_ PKSDATAFORMAT dataFormat);

BOOLEAN
OpenA8DJMidiIsSupportedDataRange(_In_ PKSDATARANGE dataRange);

#endif
