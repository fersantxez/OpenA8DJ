#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cf_string_to_c(CFStringRef string, char *buffer, size_t size) {
    if (buffer == NULL || size == 0) return;
    buffer[0] = '\0';
    if (string == NULL) return;
    CFStringGetCString(string, buffer, size, kCFStringEncodingUTF8);
}

static int get_device_name(AudioObjectID device, char *buffer, size_t size) {
    CFStringRef name = NULL;
    UInt32 dataSize = sizeof(name);
    AudioObjectPropertyAddress address = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    OSStatus status = AudioObjectGetPropertyData(device, &address, 0, NULL, &dataSize, &name);
    if (status != noErr || name == NULL) return 0;
    cf_string_to_c(name, buffer, size);
    CFRelease(name);
    return 1;
}

static AudioObjectID find_device(const char *needle) {
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &dataSize) != noErr) {
        return kAudioObjectUnknown;
    }
    AudioObjectID *devices = malloc(dataSize);
    if (devices == NULL) return kAudioObjectUnknown;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &dataSize, devices) != noErr) {
        free(devices);
        return kAudioObjectUnknown;
    }
    UInt32 count = dataSize / sizeof(AudioObjectID);
    AudioObjectID found = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        char name[256];
        if (!get_device_name(devices[i], name, sizeof(name))) continue;
        if (strstr(name, needle) != NULL) {
            found = devices[i];
            break;
        }
    }
    free(devices);
    return found;
}

static int get_bool_property(AudioObjectID device,
                             AudioObjectPropertySelector selector,
                             AudioObjectPropertyScope scope,
                             AudioObjectPropertyElement element,
                             UInt32 *value) {
    AudioObjectPropertyAddress address = { selector, scope, element };
    UInt32 dataSize = sizeof(*value);
    if (!AudioObjectHasProperty(device, &address)) return 0;
    OSStatus status = AudioObjectGetPropertyData(device, &address, 0, NULL, &dataSize, value);
    return status == noErr;
}

static int get_float_property(AudioObjectID device,
                              AudioObjectPropertySelector selector,
                              AudioObjectPropertyScope scope,
                              AudioObjectPropertyElement element,
                              Float32 *value) {
    AudioObjectPropertyAddress address = { selector, scope, element };
    UInt32 dataSize = sizeof(*value);
    if (!AudioObjectHasProperty(device, &address)) return 0;
    OSStatus status = AudioObjectGetPropertyData(device, &address, 0, NULL, &dataSize, value);
    return status == noErr;
}

static int set_float_property(AudioObjectID device,
                              AudioObjectPropertySelector selector,
                              AudioObjectPropertyScope scope,
                              AudioObjectPropertyElement element,
                              Float32 value) {
    AudioObjectPropertyAddress address = { selector, scope, element };
    if (!AudioObjectHasProperty(device, &address)) return 0;
    Boolean settable = false;
    if (AudioObjectIsPropertySettable(device, &address, &settable) != noErr || !settable) {
        return 0;
    }
    UInt32 dataSize = sizeof(value);
    OSStatus status = AudioObjectSetPropertyData(device, &address, 0, NULL, dataSize, &value);
    return status == noErr;
}

static void print_scope(AudioObjectID device, AudioObjectPropertyScope scope, const char *label) {
    AudioObjectPropertyAddress streamAddress = {
        kAudioDevicePropertyStreams,
        scope,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    UInt32 channelCount = 0;
    if (AudioObjectGetPropertyDataSize(device, &streamAddress, 0, NULL, &dataSize) == noErr) {
        AudioStreamID *streams = malloc(dataSize);
        if (streams != NULL &&
            AudioObjectGetPropertyData(device, &streamAddress, 0, NULL, &dataSize, streams) == noErr) {
            UInt32 streamCount = dataSize / sizeof(AudioStreamID);
            for (UInt32 i = 0; i < streamCount; i++) {
                AudioObjectPropertyAddress formatAddress = {
                    kAudioStreamPropertyVirtualFormat,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                AudioStreamBasicDescription asbd;
                UInt32 formatSize = sizeof(asbd);
                if (AudioObjectGetPropertyData(streams[i], &formatAddress, 0, NULL, &formatSize, &asbd) == noErr) {
                    channelCount += asbd.mChannelsPerFrame;
                }
            }
        }
        free(streams);
    }

    printf("%s channels=%u\n", label, channelCount);
    for (UInt32 channel = 0; channel <= channelCount; channel++) {
        AudioObjectPropertyElement element = channel == 0 ? kAudioObjectPropertyElementMain : channel;
        Boolean settable = false;
        Float32 scalar = 0.0f;
        UInt32 mute = 0;
        AudioObjectPropertyAddress volumeAddress = {
            kAudioDevicePropertyVolumeScalar,
            scope,
            element
        };
        int hasVolume = get_float_property(device, kAudioDevicePropertyVolumeScalar, scope, element, &scalar);
        int canSetVolume = 0;
        if (AudioObjectHasProperty(device, &volumeAddress) &&
            AudioObjectIsPropertySettable(device, &volumeAddress, &settable) == noErr) {
            canSetVolume = settable != 0;
        }
        int hasMute = get_bool_property(device, kAudioDevicePropertyMute, scope, element, &mute);
        if (hasVolume || hasMute) {
            printf("  element=%u", element);
            if (hasVolume) printf(" volume=%.6f settable=%s", scalar, canSetVolume ? "yes" : "no");
            if (hasMute) printf(" mute=%u", mute);
            printf("\n");
        }
    }
}

int main(int argc, char **argv) {
    const char *needle = argc > 1 ? argv[1] : "iRig Stream";
    AudioObjectID device = find_device(needle);
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "device not found: %s\n", needle);
        return 1;
    }
    char name[256];
    get_device_name(device, name, sizeof(name));
    if (argc >= 5 && strcmp(argv[2], "set-input-volume") == 0) {
        AudioObjectPropertyElement element = (AudioObjectPropertyElement)strtoul(argv[3], NULL, 10);
        Float32 value = (Float32)strtod(argv[4], NULL);
        if (!set_float_property(device, kAudioDevicePropertyVolumeScalar,
                                kAudioDevicePropertyScopeInput, element, value)) {
            fprintf(stderr, "failed to set input volume element=%u value=%.6f\n", element, value);
            return 1;
        }
    }
    if (argc >= 5 && strcmp(argv[2], "set-output-volume") == 0) {
        AudioObjectPropertyElement element = (AudioObjectPropertyElement)strtoul(argv[3], NULL, 10);
        Float32 value = (Float32)strtod(argv[4], NULL);
        if (!set_float_property(device, kAudioDevicePropertyVolumeScalar,
                                kAudioDevicePropertyScopeOutput, element, value)) {
            fprintf(stderr, "failed to set output volume element=%u value=%.6f\n", element, value);
            return 1;
        }
    }
    printf("device id=%u name=%s\n", device, name);
    print_scope(device, kAudioDevicePropertyScopeInput, "input");
    print_scope(device, kAudioDevicePropertyScopeOutput, "output");
    return 0;
}
