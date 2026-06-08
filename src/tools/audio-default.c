#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static OSStatus CopyStringProperty(AudioObjectID objectID,
                                   AudioObjectPropertySelector selector,
                                   CFStringRef *outString)
{
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = sizeof(CFStringRef);
    *outString = NULL;
    return AudioObjectGetPropertyData(objectID, &address, 0, NULL, &size, outString);
}

static int HasOutput(AudioObjectID objectID)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(objectID, &address, 0, NULL, &size) != kAudioHardwareNoError || size == 0) {
        return 0;
    }
    AudioBufferList *list = (AudioBufferList *)calloc(1, size);
    if (list == NULL) {
        return 0;
    }
    int hasOutput = 0;
    if (AudioObjectGetPropertyData(objectID, &address, 0, NULL, &size, list) == kAudioHardwareNoError) {
        for (UInt32 i = 0; i < list->mNumberBuffers; i++) {
            if (list->mBuffers[i].mNumberChannels > 0) {
                hasOutput = 1;
                break;
            }
        }
    }
    free(list);
    return hasOutput;
}

static int StringEqualsCString(CFStringRef string, const char *text)
{
    if (string == NULL || text == NULL) {
        return 0;
    }
    CFStringRef target = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
    if (target == NULL) {
        return 0;
    }
    Boolean equal = CFEqual(string, target);
    CFRelease(target);
    return equal;
}

static void PrintCFString(CFStringRef string)
{
    char buffer[512];
    if (string != NULL && CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        printf("%s", buffer);
    }
}

static OSStatus GetAudioObjectIDProperty(AudioObjectID objectID,
                                         AudioObjectPropertySelector selector,
                                         AudioObjectID *outObjectID)
{
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = sizeof(AudioObjectID);
    return AudioObjectGetPropertyData(objectID, &address, 0, NULL, &size, outObjectID);
}

int main(int argc, char **argv)
{
    const char *requestedUID = argc > 1 ? argv[1] : NULL;
    AudioObjectPropertyAddress devicesAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesAddress, 0, NULL, &size);
    if (status != kAudioHardwareNoError || size == 0) {
        fprintf(stderr, "No se pudo leer la lista de dispositivos: %d\n", (int)status);
        return 2;
    }

    UInt32 count = size / (UInt32)sizeof(AudioObjectID);
    AudioObjectID *devices = (AudioObjectID *)calloc(count, sizeof(AudioObjectID));
    if (devices == NULL) {
        return 3;
    }
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesAddress, 0, NULL, &size, devices);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "No se pudo leer dispositivos: %d\n", (int)status);
        free(devices);
        return 4;
    }

    AudioObjectID selected = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        if (!HasOutput(devices[i])) {
            continue;
        }
        CFStringRef uid = NULL;
        CFStringRef name = NULL;
        (void)CopyStringProperty(devices[i], kAudioDevicePropertyDeviceUID, &uid);
        (void)CopyStringProperty(devices[i], kAudioObjectPropertyName, &name);

        printf("%u\t", devices[i]);
        PrintCFString(uid);
        printf("\t");
        PrintCFString(name);
        printf("\n");

        if (selected == kAudioObjectUnknown) {
            int isOpenA8DJ = StringEqualsCString(uid, "org.opena8dj.Audio8DJ");
            int matchesRequest = requestedUID != NULL && StringEqualsCString(uid, requestedUID);
            if ((requestedUID != NULL && matchesRequest) || (requestedUID == NULL && !isOpenA8DJ)) {
                selected = devices[i];
            }
        }

        if (uid != NULL) {
            CFRelease(uid);
        }
        if (name != NULL) {
            CFRelease(name);
        }
    }
    free(devices);

    if (selected == kAudioObjectUnknown) {
        fprintf(stderr, "No encontre una salida alternativa.\n");
        return 5;
    }

    AudioObjectPropertyAddress defaults[] = {
        {kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
        {kAudioHardwarePropertyDefaultSystemOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain}
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++) {
        status = AudioObjectSetPropertyData(kAudioObjectSystemObject,
                                            &defaults[i],
                                            0,
                                            NULL,
                                            sizeof(selected),
                                            &selected);
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "No se pudo cambiar default %zu: %d\n", i, (int)status);
            return 6;
        }
    }

    AudioObjectID defaultOutput = kAudioObjectUnknown;
    AudioObjectID defaultSystem = kAudioObjectUnknown;
    status = GetAudioObjectIDProperty(kAudioObjectSystemObject,
                                      kAudioHardwarePropertyDefaultOutputDevice,
                                      &defaultOutput);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "No se pudo verificar default output: %d\n", (int)status);
        return 7;
    }
    status = GetAudioObjectIDProperty(kAudioObjectSystemObject,
                                      kAudioHardwarePropertyDefaultSystemOutputDevice,
                                      &defaultSystem);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "No se pudo verificar default system output: %d\n", (int)status);
        return 8;
    }
    printf("Default output requested=%u actual=%u system_actual=%u\n", selected, defaultOutput, defaultSystem);
    if (defaultOutput != selected || defaultSystem != selected) {
        fprintf(stderr, "Core Audio no dejo seleccionado el dispositivo pedido.\n");
        return 9;
    }
    return 0;
}
