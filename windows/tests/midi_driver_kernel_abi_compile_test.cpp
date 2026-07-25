#include <ntddk.h>
#include <portcls.h>
#include <ksmedia.h>

#include "../midi/driver/OpenA8DJMidiBusInterface.h"

#if !defined(_WIN64)
#error S1 is supported only on x64.
#endif

#if NTDDI_VERSION < NTDDI_WIN10_VB
#error S1 requires Windows 10 version 2004 or newer.
#endif

static_assert(sizeof(OPENA8DJ_MIDI_BUS_INTERFACE_V1) ==
                  OPENA8DJ_MIDI_BUS_INTERFACE_V1_X64_SIZE,
              "V1 bus interface x64 ABI drift");
static_assert(sizeof(OPENA8DJ_MIDI_BUS_INTERFACE_V1) == 80u,
              "reviewed V1 x64 ABI size changed");
static_assert(FIELD_OFFSET(OPENA8DJ_MIDI_BUS_INTERFACE_V1, Header) == 0u,
              "INTERFACE header must be first");
static_assert(FIELD_OFFSET(OPENA8DJ_MIDI_BUS_INTERFACE_V1, Magic) == 32u,
              "V1 Magic offset changed");
static_assert(FIELD_OFFSET(OPENA8DJ_MIDI_BUS_INTERFACE_V1, OpenStream) == 40u,
              "V1 function table offset changed");
static_assert(FIELD_OFFSET(OPENA8DJ_MIDI_BUS_INTERFACE_V1, WriteStream) == 72u,
              "V1 final function offset changed");

/* KSDATAFORMAT's LONGLONG alignment rounds the 92-byte payload to 96 on x64. */
static_assert(sizeof(KSDATARANGE_MUSIC) == 96u,
              "WDK KSDATARANGE_MUSIC ABI changed");
static_assert(FIELD_OFFSET(KSDATARANGE_MUSIC, Technology) == 64u,
              "KSDATARANGE_MUSIC Technology offset changed");
static_assert(FIELD_OFFSET(KSDATARANGE_MUSIC, Channels) == 80u,
              "KSDATARANGE_MUSIC Channels offset changed");
static_assert(FIELD_OFFSET(KSDATARANGE_MUSIC, ChannelMask) == 88u,
              "KSDATARANGE_MUSIC ChannelMask offset changed");

extern "C" int
OpenA8DJMidiKernelAbiCompileTestAnchor(void)
{
    return static_cast<int>(sizeof(OPENA8DJ_MIDI_BUS_INTERFACE_V1) +
                            sizeof(KSDATARANGE_MUSIC));
}
