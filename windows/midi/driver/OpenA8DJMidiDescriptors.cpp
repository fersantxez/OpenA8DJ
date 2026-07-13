#include "OpenA8DJMidiDescriptors.h"

#include <ksmedia.h>

static const KSDATARANGE_MUSIC g_OpenA8DJMidiDataRange = {
    {
        sizeof(KSDATARANGE_MUSIC),
        0u,
        0u,
        0u,
        STATICGUIDOF(KSDATAFORMAT_TYPE_MUSIC),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_MIDI),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    },
    STATICGUIDOF(KSMUSIC_TECHNOLOGY_PORT),
    0u,
    0u,
    0xffffu
};

static const PKSDATARANGE g_OpenA8DJMidiDataRanges[] = {
    const_cast<PKSDATARANGE>(
        reinterpret_cast<const KSDATARANGE *>(&g_OpenA8DJMidiDataRange))
};

static const PCPIN_DESCRIPTOR g_OpenA8DJMidiPins[OPENA8DJ_MIDI_PIN_COUNT] = {
    {
        1u,
        1u,
        0u,
        nullptr,
        {
            0u,
            nullptr,
            0u,
            nullptr,
            RTL_NUMBER_OF(g_OpenA8DJMidiDataRanges),
            g_OpenA8DJMidiDataRanges,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_MIDI,
            0
        }
    },
    {
        1u,
        1u,
        0u,
        nullptr,
        {
            0u,
            nullptr,
            0u,
            nullptr,
            RTL_NUMBER_OF(g_OpenA8DJMidiDataRanges),
            g_OpenA8DJMidiDataRanges,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_MIDI,
            0
        }
    }
};

static const GUID g_OpenA8DJMidiCategories[] = {
    STATICGUIDOF(KSCATEGORY_AUDIO),
    STATICGUIDOF(KSCATEGORY_RENDER),
    STATICGUIDOF(KSCATEGORY_CAPTURE)
};

const PCFILTER_DESCRIPTOR g_OpenA8DJMidiFilterDescriptor = {
    0u,
    nullptr,
    sizeof(PCPIN_DESCRIPTOR),
    RTL_NUMBER_OF(g_OpenA8DJMidiPins),
    g_OpenA8DJMidiPins,
    sizeof(PCNODE_DESCRIPTOR),
    0u,
    nullptr,
    0u,
    nullptr,
    RTL_NUMBER_OF(g_OpenA8DJMidiCategories),
    g_OpenA8DJMidiCategories
};

BOOLEAN
OpenA8DJMidiIsSupportedFormat(_In_ PKSDATAFORMAT dataFormat)
{
    if (dataFormat == nullptr || dataFormat->FormatSize < sizeof(KSDATAFORMAT)) {
        return FALSE;
    }
    return IsEqualGUIDAligned(dataFormat->MajorFormat, KSDATAFORMAT_TYPE_MUSIC) &&
           IsEqualGUIDAligned(dataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_MIDI) &&
           IsEqualGUIDAligned(dataFormat->Specifier, KSDATAFORMAT_SPECIFIER_NONE);
}

BOOLEAN
OpenA8DJMidiIsSupportedDataRange(_In_ PKSDATARANGE dataRange)
{
    const KSDATARANGE_MUSIC *musicRange;

    if (dataRange == nullptr ||
        dataRange->FormatSize < sizeof(KSDATARANGE_MUSIC) ||
        !OpenA8DJMidiIsSupportedFormat(dataRange)) {
        return FALSE;
    }

    musicRange = reinterpret_cast<const KSDATARANGE_MUSIC *>(dataRange);
    return IsEqualGUIDAligned(
               musicRange->Technology,
               KSMUSIC_TECHNOLOGY_PORT) &&
           musicRange->Channels == 0u &&
           musicRange->Notes == 0u &&
           musicRange->ChannelMask == 0xffffu;
}
