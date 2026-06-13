#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
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
        if (status != kAudioHardwareNoError || currentUID == NULL) {
            continue;
        }
        char currentText[512] = {0};
        bool match = CFStringGetCString(currentUID, currentText, sizeof(currentText), kCFStringEncodingUTF8) &&
            strcmp(currentText, uidText) == 0;
        CFRelease(currentUID);
        if (match) {
            return devices[i];
        }
    }
    return kAudioObjectUnknown;
}

static OSStatus GetScalar(AudioObjectID device, AudioObjectPropertySelector selector, UInt32 size, void *outValue)
{
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    return AudioObjectGetPropertyData(device, &address, 0, NULL, &size, outValue);
}

static OSStatus SetScalar(AudioObjectID device, AudioObjectPropertySelector selector, UInt32 size, const void *value)
{
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    return AudioObjectSetPropertyData(device, &address, 0, NULL, size, value);
}

static int WaitForConfig(AudioObjectID device, double requestedRate, UInt32 requestedBuffer)
{
    for (int attempt = 0; attempt < 100; attempt++) {
        Float64 rate = 0.0;
        UInt32 buffer = 0;
        OSStatus rateStatus = GetScalar(device, kAudioDevicePropertyNominalSampleRate, sizeof(rate), &rate);
        OSStatus bufferStatus = GetScalar(device, kAudioDevicePropertyBufferFrameSize, sizeof(buffer), &buffer);
        if (rateStatus == kAudioHardwareNoError &&
            bufferStatus == kAudioHardwareNoError &&
            (requestedRate <= 0.0 || rate == requestedRate) &&
            (requestedBuffer == 0 || buffer == requestedBuffer)) {
            return 0;
        }
        usleep(50000);
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: audio-config <uid> [rate] [buffer-frames]\n");
        return 2;
    }

    AudioObjectID device = DeviceForUID(argv[1]);
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "device not found for uid %s\n", argv[1]);
        return 3;
    }

    double requestedRate = argc > 2 ? strtod(argv[2], NULL) : 0.0;
    UInt32 requestedBuffer = argc > 3 ? (UInt32)strtoul(argv[3], NULL, 10) : 0;

    if (requestedRate > 0.0) {
        Float64 rate = requestedRate;
        OSStatus status = SetScalar(device, kAudioDevicePropertyNominalSampleRate, sizeof(rate), &rate);
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "set sample rate failed: %d\n", (int)status);
            return 4;
        }
    }
    if (requestedBuffer > 0) {
        OSStatus status = SetScalar(device, kAudioDevicePropertyBufferFrameSize, sizeof(requestedBuffer), &requestedBuffer);
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "set buffer frames failed: %d\n", (int)status);
            return 5;
        }
    }

    if (WaitForConfig(device, requestedRate, requestedBuffer) != 0) {
        fprintf(stderr, "Core Audio did not confirm requested config\n");
        return 6;
    }

    Float64 rate = 0.0;
    UInt32 buffer = 0;
    (void)GetScalar(device, kAudioDevicePropertyNominalSampleRate, sizeof(rate), &rate);
    (void)GetScalar(device, kAudioDevicePropertyBufferFrameSize, sizeof(buffer), &buffer);
    printf("device=%u rate=%.0f buffer=%u\n", device, rate, buffer);
    return 0;
}
