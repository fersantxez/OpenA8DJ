#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <stdio.h>

static void PrintStringProperty(MIDIObjectRef object, CFStringRef property)
{
    CFStringRef value = NULL;
    OSStatus status = MIDIObjectGetStringProperty(object, property, &value);
    char buffer[512] = {0};
    if (status == noErr && value != NULL &&
        CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        printf("%s", buffer);
    } else {
        printf("<sin nombre>");
    }
    if (value != NULL) {
        CFRelease(value);
    }
}

static void PrintEndpoint(const char *kind, ItemCount index, MIDIEndpointRef endpoint)
{
    MIDIUniqueID uniqueID = 0;
    SInt32 isOffline = 0;
    (void)MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &uniqueID);
    (void)MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyOffline, &isOffline);
    printf("%s %lu: ", kind, (unsigned long)index + 1);
    PrintStringProperty(endpoint, kMIDIPropertyDisplayName);
    printf(" uid=%d offline=%d\n", (int)uniqueID, (int)isOffline);
}

int main(void)
{
    ItemCount sourceCount = MIDIGetNumberOfSources();
    ItemCount destinationCount = MIDIGetNumberOfDestinations();
    printf("MIDI sources: %lu\n", (unsigned long)sourceCount);
    for (ItemCount i = 0; i < sourceCount; i++) {
        PrintEndpoint("source", i, MIDIGetSource(i));
    }
    printf("MIDI destinations: %lu\n", (unsigned long)destinationCount);
    for (ItemCount i = 0; i < destinationCount; i++) {
        PrintEndpoint("destination", i, MIDIGetDestination(i));
    }
    return 0;
}
