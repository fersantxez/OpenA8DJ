#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>

static AudioObjectID DeviceForUID(const char *uidText)
{
    CFStringRef uid = CFStringCreateWithCString(NULL, uidText, kCFStringEncodingUTF8);
    if (uid == NULL) {
        return kAudioObjectUnknown;
    }

    AudioObjectID device = kAudioObjectUnknown;
    AudioValueTranslation translation = {
        .mInputData = &uid,
        .mInputDataSize = sizeof(uid),
        .mOutputData = &device,
        .mOutputDataSize = sizeof(device)
    };
    UInt32 size = sizeof(translation);
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyTranslateUIDToDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                 &address,
                                                 0,
                                                 NULL,
                                                 &size,
                                                 &translation);
    CFRelease(uid);
    if (status == kAudioHardwareNoError && device != kAudioObjectUnknown) {
        return device;
    }

    AudioObjectPropertyAddress devicesAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 devicesSize = 0;
    status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesAddress, 0, NULL, &devicesSize);
    if (status != kAudioHardwareNoError || devicesSize == 0) {
        return kAudioObjectUnknown;
    }
    AudioObjectID devices[128];
    if (devicesSize > sizeof(devices)) {
        devicesSize = sizeof(devices);
    }
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesAddress, 0, NULL, &devicesSize, devices);
    if (status != kAudioHardwareNoError) {
        return kAudioObjectUnknown;
    }
    UInt32 count = devicesSize / (UInt32)sizeof(AudioObjectID);
    for (UInt32 i = 0; i < count; i++) {
        CFStringRef currentUID = NULL;
        UInt32 uidSize = sizeof(currentUID);
        AudioObjectPropertyAddress uidAddress = {
            kAudioDevicePropertyDeviceUID,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        status = AudioObjectGetPropertyData(devices[i], &uidAddress, 0, NULL, &uidSize, &currentUID);
        if (status == kAudioHardwareNoError && currentUID != NULL) {
            CFStringRef requested = CFStringCreateWithCString(NULL, uidText, kCFStringEncodingUTF8);
            Boolean equal = requested != NULL && CFEqual(currentUID, requested);
            if (requested != NULL) {
                CFRelease(requested);
            }
            CFRelease(currentUID);
            if (equal) {
                return devices[i];
            }
        }
    }
    return kAudioObjectUnknown;
}

static OSStatus SetDefault(AudioObjectPropertySelector selector, AudioObjectID device)
{
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    return AudioObjectSetPropertyData(kAudioObjectSystemObject,
                                      &address,
                                      0,
                                      NULL,
                                      sizeof(device),
                                      &device);
}

static AudioObjectID GetDefault(AudioObjectPropertySelector selector)
{
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                &address,
                                                0,
                                                NULL,
                                                &size,
                                                &device);
    return status == kAudioHardwareNoError ? device : kAudioObjectUnknown;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: audio-route <uid> <output|system|both>\n");
        return 2;
    }

    AudioObjectID device = DeviceForUID(argv[1]);
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "device not found for uid %s\n", argv[1]);
        return 3;
    }

    int wantOutput = strcmp(argv[2], "output") == 0 || strcmp(argv[2], "both") == 0;
    int wantSystem = strcmp(argv[2], "system") == 0 || strcmp(argv[2], "both") == 0;
    if (!wantOutput && !wantSystem) {
        fprintf(stderr, "unknown route %s\n", argv[2]);
        return 4;
    }

    if (wantOutput) {
        OSStatus status = SetDefault(kAudioHardwarePropertyDefaultOutputDevice, device);
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "set default output failed: %d\n", (int)status);
            return 5;
        }
    }
    if (wantSystem) {
        OSStatus status = SetDefault(kAudioHardwarePropertyDefaultSystemOutputDevice, device);
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "set default system output failed: %d\n", (int)status);
            return 6;
        }
    }

    AudioObjectID defaultOutput = GetDefault(kAudioHardwarePropertyDefaultOutputDevice);
    AudioObjectID defaultSystem = GetDefault(kAudioHardwarePropertyDefaultSystemOutputDevice);
    printf("requested=%u default_output=%u default_system=%u\n",
           device,
           defaultOutput,
           defaultSystem);
    return 0;
}
