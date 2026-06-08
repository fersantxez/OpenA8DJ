#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static void PrintOSStatus(OSStatus status)
{
    char code[5] = {
        (char)((status >> 24) & 0xff),
        (char)((status >> 16) & 0xff),
        (char)((status >> 8) & 0xff),
        (char)(status & 0xff),
        0
    };
    for (int i = 0; i < 4; i++) {
        if (code[i] < 32 || code[i] > 126) {
            code[i] = '.';
        }
    }
    fprintf(stderr, "OSStatus %d ('%s')", (int)status, code);
}

static OSStatus GetProperty(AudioObjectID objectID,
                            AudioObjectPropertySelector selector,
                            AudioObjectPropertyScope scope,
                            UInt32 *ioDataSize,
                            void *outData)
{
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain
    };
    return AudioObjectGetPropertyData(objectID, &address, 0, NULL, ioDataSize, outData);
}

static void PrintCFString(CFStringRef string)
{
    char buffer[512];
    if (string != NULL &&
        CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        printf("%s", buffer);
    } else {
        printf("<sin nombre>");
    }
}

static UInt32 CountChannels(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        scope,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(deviceID, &address, 0, NULL, &dataSize);
    if (status != kAudioHardwareNoError || dataSize == 0) {
        return 0;
    }

    AudioBufferList *buffers = (AudioBufferList *)calloc(1, dataSize);
    if (buffers == NULL) {
        return 0;
    }

    status = AudioObjectGetPropertyData(deviceID, &address, 0, NULL, &dataSize, buffers);
    if (status != kAudioHardwareNoError) {
        free(buffers);
        return 0;
    }

    UInt32 channels = 0;
    for (UInt32 i = 0; i < buffers->mNumberBuffers; i++) {
        channels += buffers->mBuffers[i].mNumberChannels;
    }
    free(buffers);
    return channels;
}

int main(void)
{
    AudioObjectPropertyAddress devicesAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                     &devicesAddress,
                                                     0,
                                                     NULL,
                                                     &dataSize);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "No se pudo consultar la lista de dispositivos: ");
        PrintOSStatus(status);
        fprintf(stderr, "\n");
        return 2;
    }

    UInt32 count = dataSize / (UInt32)sizeof(AudioObjectID);
    AudioObjectID *devices = (AudioObjectID *)calloc(count, sizeof(AudioObjectID));
    if (devices == NULL) {
        fprintf(stderr, "Sin memoria para %u dispositivos\n", count);
        return 3;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &devicesAddress,
                                        0,
                                        NULL,
                                        &dataSize,
                                        devices);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "No se pudo leer la lista de dispositivos: ");
        PrintOSStatus(status);
        fprintf(stderr, "\n");
        free(devices);
        return 4;
    }

    printf("Dispositivos Core Audio: %u\n", count);
    for (UInt32 i = 0; i < count; i++) {
        CFStringRef name = NULL;
        CFStringRef uid = NULL;
        Float64 rate = 0.0;

        dataSize = sizeof(name);
        (void)GetProperty(devices[i],
                          kAudioObjectPropertyName,
                          kAudioObjectPropertyScopeGlobal,
                          &dataSize,
                          &name);

        dataSize = sizeof(uid);
        (void)GetProperty(devices[i],
                          kAudioDevicePropertyDeviceUID,
                          kAudioObjectPropertyScopeGlobal,
                          &dataSize,
                          &uid);

        dataSize = sizeof(rate);
        (void)GetProperty(devices[i],
                          kAudioDevicePropertyNominalSampleRate,
                          kAudioObjectPropertyScopeGlobal,
                          &dataSize,
                          &rate);

        printf("%3u  id=%u  ", i + 1, devices[i]);
        PrintCFString(name);
        printf("  uid=");
        PrintCFString(uid);
        printf("  in=%u out=%u rate=%.0f\n",
               CountChannels(devices[i], kAudioObjectPropertyScopeInput),
               CountChannels(devices[i], kAudioObjectPropertyScopeOutput),
               rate);

        if (name != NULL) {
            CFRelease(name);
        }
        if (uid != NULL) {
            CFRelease(uid);
        }
    }

    free(devices);
    return 0;
}
