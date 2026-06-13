#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*OpenA8DJFactory)(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID);

static const UInt32 kUsageMissing = UINT32_MAX - 1u;
static const UInt32 kUsageInvalid = UINT32_MAX;

static UInt32 gConfigurationChangeRequests = 0;

static OSStatus HostPropertiesChanged(AudioServerPlugInHostRef inHost,
                                      AudioObjectID inObjectID,
                                      UInt32 inNumberAddresses,
                                      const AudioObjectPropertyAddress *inAddresses)
{
    (void)inHost;
    (void)inObjectID;
    (void)inNumberAddresses;
    (void)inAddresses;
    return kAudioHardwareNoError;
}

static OSStatus HostCopyFromStorage(AudioServerPlugInHostRef inHost,
                                    CFStringRef inKey,
                                    CFPropertyListRef *outData)
{
    (void)inHost;
    (void)inKey;
    if (outData != NULL) {
        *outData = NULL;
    }
    return kAudioHardwareNoError;
}

static OSStatus HostWriteToStorage(AudioServerPlugInHostRef inHost, CFStringRef inKey, CFPropertyListRef inData)
{
    (void)inHost;
    (void)inKey;
    (void)inData;
    return kAudioHardwareNoError;
}

static OSStatus HostDeleteFromStorage(AudioServerPlugInHostRef inHost, CFStringRef inKey)
{
    (void)inHost;
    (void)inKey;
    return kAudioHardwareNoError;
}

static OSStatus HostRequestDeviceConfigurationChange(AudioServerPlugInHostRef inHost,
                                                     AudioObjectID inDeviceObjectID,
                                                     UInt64 inChangeAction,
                                                     void *inChangeInfo)
{
    (void)inHost;
    (void)inDeviceObjectID;
    (void)inChangeAction;
    (void)inChangeInfo;
    gConfigurationChangeRequests++;
    return kAudioHardwareNoError;
}

static const AudioServerPlugInHostInterface kParityHost = {
    HostPropertiesChanged,
    HostCopyFromStorage,
    HostWriteToStorage,
    HostDeleteFromStorage,
    HostRequestDeviceConfigurationChange
};

static AudioObjectPropertyAddress Address(AudioObjectPropertySelector selector,
                                          AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain
    };
    return address;
}

static int QueryProperty(AudioServerPlugInDriverRef driver,
                         AudioObjectID objectID,
                         AudioObjectPropertySelector selector,
                         AudioObjectPropertyScope scope,
                         void **outData,
                         UInt32 *outSize)
{
    AudioObjectPropertyAddress address = Address(selector, scope);
    Boolean has = (*driver)->HasProperty(driver, objectID, 0, &address);
    if (!has) {
        if (outData != NULL) *outData = NULL;
        if (outSize != NULL) *outSize = 0;
        return 1;
    }

    Boolean settable = false;
    OSStatus status = (*driver)->IsPropertySettable(driver, objectID, 0, &address, &settable);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "IsPropertySettable failed object=%u selector=%u scope=%u status=%d\n",
                objectID, selector, scope, (int)status);
        return -1;
    }

    UInt32 size = 0;
    status = (*driver)->GetPropertyDataSize(driver, objectID, 0, &address, 0, NULL, &size);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "GetPropertyDataSize failed object=%u selector=%u scope=%u status=%d\n",
                objectID, selector, scope, (int)status);
        return -1;
    }

    void *data = NULL;
    if (size > 0) {
        data = calloc(1, size);
        if (data == NULL) {
            fprintf(stderr, "calloc failed size=%u\n", size);
            return -1;
        }
    } else {
        if (outData != NULL) {
            *outData = NULL;
        }
        if (outSize != NULL) {
            *outSize = 0;
        }
        return 0;
    }

    UInt32 dataSize = size;
    status = (*driver)->GetPropertyData(driver,
                                        objectID,
                                        0,
                                        &address,
                                        0,
                                        NULL,
                                        size,
                                        &dataSize,
                                        data);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "GetPropertyData failed object=%u selector=%u scope=%u status=%d size=%u\n",
                objectID, selector, scope, (int)status, size);
        free(data);
        return -1;
    }

    if (dataSize > size) {
        fprintf(stderr, "GetPropertyData returned oversized data object=%u selector=%u scope=%u size=%u returned=%u\n",
                objectID, selector, scope, size, dataSize);
        free(data);
        return -1;
    }

    if (outData != NULL) {
        *outData = data;
    } else {
        free(data);
    }
    if (outSize != NULL) {
        *outSize = dataSize;
    }
    return 0;
}

static UInt32 QueryObjectList(AudioServerPlugInDriverRef driver,
                              AudioObjectID objectID,
                              AudioObjectPropertySelector selector,
                              AudioObjectPropertyScope scope,
                              AudioObjectID **outIDs)
{
    void *data = NULL;
    UInt32 size = 0;
    int rc = QueryProperty(driver, objectID, selector, scope, &data, &size);
    if (rc != 0 || size == 0) {
        if (outIDs != NULL) *outIDs = NULL;
        return 0;
    }
    if ((size % sizeof(AudioObjectID)) != 0) {
        fprintf(stderr, "object list has invalid size object=%u selector=%u scope=%u size=%u\n",
                objectID, selector, scope, size);
        free(data);
        if (outIDs != NULL) *outIDs = NULL;
        return UINT32_MAX;
    }
    if (outIDs != NULL) {
        *outIDs = (AudioObjectID *)data;
    } else {
        free(data);
    }
    return size / (UInt32)sizeof(AudioObjectID);
}

static UInt32 QueryStreamConfiguration(AudioServerPlugInDriverRef driver,
                                       AudioObjectID deviceID,
                                       AudioObjectPropertyScope scope,
                                       UInt32 *outChannels)
{
    void *data = NULL;
    UInt32 size = 0;
    int rc = QueryProperty(driver,
                           deviceID,
                           kAudioDevicePropertyStreamConfiguration,
                           scope,
                           &data,
                           &size);
    if (rc != 0 || data == NULL) {
        if (outChannels != NULL) *outChannels = 0;
        return 0;
    }
    if (size < offsetof(AudioBufferList, mBuffers)) {
        fprintf(stderr, "stream configuration too small scope=%u size=%u\n", scope, size);
        free(data);
        if (outChannels != NULL) *outChannels = 0;
        return UINT32_MAX;
    }
    AudioBufferList *list = (AudioBufferList *)data;
    UInt32 expectedSize = offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer) * list->mNumberBuffers;
    if (size < expectedSize) {
        fprintf(stderr, "stream configuration truncated scope=%u size=%u expected=%u buffers=%u\n",
                scope, size, expectedSize, list->mNumberBuffers);
        free(data);
        if (outChannels != NULL) *outChannels = 0;
        return UINT32_MAX;
    }
    UInt32 channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; i++) {
        channels += list->mBuffers[i].mNumberChannels;
    }
    UInt32 buffers = list->mNumberBuffers;
    free(data);
    if (outChannels != NULL) *outChannels = channels;
    return buffers;
}

static UInt32 QueryIOProcStreamUsage(AudioServerPlugInDriverRef driver,
                                     AudioObjectID deviceID,
                                     AudioObjectPropertyScope scope)
{
    void *data = NULL;
    UInt32 size = 0;
    int rc = QueryProperty(driver,
                           deviceID,
                           kAudioDevicePropertyIOProcStreamUsage,
                           scope,
                           &data,
                           &size);
    if (rc != 0 || data == NULL) {
        return rc > 0 ? kUsageMissing : kUsageInvalid;
    }
    if (size < offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn)) {
        fprintf(stderr, "stream usage too small scope=%u size=%u\n", scope, size);
        free(data);
        return kUsageInvalid;
    }
    AudioHardwareIOProcStreamUsage *usage = (AudioHardwareIOProcStreamUsage *)data;
    UInt32 expectedSize = offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) +
        sizeof(UInt32) * usage->mNumberStreams;
    if (size < expectedSize) {
        fprintf(stderr, "stream usage truncated scope=%u size=%u expected=%u streams=%u\n",
                scope, size, expectedSize, usage->mNumberStreams);
        free(data);
        return kUsageInvalid;
    }
    UInt32 streams = usage->mNumberStreams;
    free(data);
    return streams;
}

static int QueryBoolean(AudioServerPlugInDriverRef driver,
                        AudioObjectID objectID,
                        AudioObjectPropertySelector selector,
                        AudioObjectPropertyScope scope,
                        UInt32 *outValue)
{
    void *data = NULL;
    UInt32 size = 0;
    int rc = QueryProperty(driver, objectID, selector, scope, &data, &size);
    if (rc != 0) {
        if (outValue != NULL) *outValue = 0;
        return rc;
    }
    if (size != sizeof(UInt32) || data == NULL) {
        fprintf(stderr, "boolean property invalid size object=%u selector=%u scope=%u size=%u\n",
                objectID, selector, scope, size);
        free(data);
        return -1;
    }
    if (outValue != NULL) {
        *outValue = *(UInt32 *)data;
    }
    free(data);
    return 0;
}

static int QueryScalarFloat64(AudioServerPlugInDriverRef driver,
                              AudioObjectID objectID,
                              AudioObjectPropertySelector selector,
                              AudioObjectPropertyScope scope,
                              Float64 *outValue)
{
    void *data = NULL;
    UInt32 size = 0;
    int rc = QueryProperty(driver, objectID, selector, scope, &data, &size);
    if (rc != 0) {
        if (outValue != NULL) *outValue = 0;
        return rc;
    }
    if (size != sizeof(Float64) || data == NULL) {
        fprintf(stderr, "Float64 property invalid size object=%u selector=%u scope=%u size=%u\n",
                objectID, selector, scope, size);
        free(data);
        return -1;
    }
    if (outValue != NULL) {
        *outValue = *(Float64 *)data;
    }
    free(data);
    return 0;
}

int main(int argc, char **argv)
{
    const char *bundlePath = argc > 1 ? argv[1] : "build/OpenA8DJ.driver";
    int failures = 0;

    CFStringRef pathString = CFStringCreateWithCString(NULL, bundlePath, kCFStringEncodingUTF8);
    CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, pathString, kCFURLPOSIXPathStyle, true);
    CFRelease(pathString);
    if (bundleURL == NULL) {
        fprintf(stderr, "could not create bundle URL for %s\n", bundlePath);
        return 2;
    }

    CFBundleRef bundle = CFBundleCreate(NULL, bundleURL);
    CFRelease(bundleURL);
    if (bundle == NULL) {
        fprintf(stderr, "could not open bundle %s\n", bundlePath);
        return 3;
    }
    if (!CFBundleLoadExecutable(bundle)) {
        fprintf(stderr, "could not load bundle executable\n");
        CFRelease(bundle);
        return 4;
    }

    OpenA8DJFactory factory = (OpenA8DJFactory)CFBundleGetFunctionPointerForName(bundle, CFSTR("OpenA8DJ_Create"));
    if (factory == NULL) {
        fprintf(stderr, "OpenA8DJ_Create not found\n");
        CFRelease(bundle);
        return 5;
    }

    AudioServerPlugInDriverRef driver = (AudioServerPlugInDriverRef)factory(NULL, kAudioServerPlugInTypeUUID);
    if (driver == NULL) {
        fprintf(stderr, "factory returned NULL\n");
        CFRelease(bundle);
        return 6;
    }

    OSStatus status = (*driver)->Initialize(driver, &kParityHost);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "Initialize failed status=%d\n", (int)status);
        CFRelease(bundle);
        return 7;
    }

    AudioObjectID *devices = NULL;
    UInt32 deviceCount = QueryObjectList(driver,
                                         kAudioObjectPlugInObject,
                                         kAudioPlugInPropertyDeviceList,
                                         kAudioObjectPropertyScopeGlobal,
                                         &devices);
    if (deviceCount != 1 || devices == NULL) {
        fprintf(stderr, "expected one device, got %u\n", deviceCount);
        failures++;
        free(devices);
        CFRelease(bundle);
        return 8;
    }
    AudioObjectID deviceID = devices[0];
    free(devices);

    AudioObjectID *ownedObjects = NULL;
    UInt32 ownedCount = QueryObjectList(driver,
                                        deviceID,
                                        kAudioObjectPropertyOwnedObjects,
                                        kAudioObjectPropertyScopeGlobal,
                                        &ownedObjects);
    free(ownedObjects);

    AudioObjectID *inputStreams = NULL;
    AudioObjectID *outputStreams = NULL;
    UInt32 inputStreamCount = QueryObjectList(driver,
                                              deviceID,
                                              kAudioDevicePropertyStreams,
                                              kAudioObjectPropertyScopeInput,
                                              &inputStreams);
    UInt32 outputStreamCount = QueryObjectList(driver,
                                               deviceID,
                                               kAudioDevicePropertyStreams,
                                               kAudioObjectPropertyScopeOutput,
                                               &outputStreams);
    UInt32 globalStreamCount = QueryObjectList(driver,
                                               deviceID,
                                               kAudioDevicePropertyStreams,
                                               kAudioObjectPropertyScopeGlobal,
                                               NULL);
    free(inputStreams);
    free(outputStreams);

    UInt32 inputChannels = 0;
    UInt32 outputChannels = 0;
    UInt32 inputBuffers = QueryStreamConfiguration(driver,
                                                   deviceID,
                                                   kAudioObjectPropertyScopeInput,
                                                   &inputChannels);
    UInt32 outputBuffers = QueryStreamConfiguration(driver,
                                                    deviceID,
                                                    kAudioObjectPropertyScopeOutput,
                                                    &outputChannels);
    UInt32 globalChannels = 0;
    UInt32 globalBuffers = QueryStreamConfiguration(driver,
                                                    deviceID,
                                                    kAudioObjectPropertyScopeGlobal,
                                                    &globalChannels);

    UInt32 inputUsageStreams = QueryIOProcStreamUsage(driver, deviceID, kAudioObjectPropertyScopeInput);
    UInt32 outputUsageStreams = QueryIOProcStreamUsage(driver, deviceID, kAudioObjectPropertyScopeOutput);

    UInt32 canDefaultOutput = 0;
    UInt32 canDefaultInput = 0;
    (void)QueryBoolean(driver,
                       deviceID,
                       kAudioDevicePropertyDeviceCanBeDefaultDevice,
                       kAudioObjectPropertyScopeOutput,
                       &canDefaultOutput);
    (void)QueryBoolean(driver,
                       deviceID,
                       kAudioDevicePropertyDeviceCanBeDefaultDevice,
                       kAudioObjectPropertyScopeInput,
                       &canDefaultInput);

    Float64 sampleRate = 0.0;
    (void)QueryScalarFloat64(driver,
                             deviceID,
                             kAudioDevicePropertyNominalSampleRate,
                             kAudioObjectPropertyScopeGlobal,
                             &sampleRate);

    printf("HAL parity bundle=%s\n", bundlePath);
    printf("device=%u sampleRate=%.0f configChangeRequests=%u\n",
           deviceID, sampleRate, gConfigurationChangeRequests);
    printf("ownedObjects=%u streams input=%u output=%u global=%u\n",
           ownedCount, inputStreamCount, outputStreamCount, globalStreamCount);
    printf("streamConfig buffers input=%u output=%u global=%u channels input=%u output=%u global=%u\n",
           inputBuffers, outputBuffers, globalBuffers, inputChannels, outputChannels, globalChannels);
    printf("ioProcStreamUsage input=");
    if (inputUsageStreams == kUsageMissing) printf("missing");
    else if (inputUsageStreams == kUsageInvalid) printf("invalid");
    else printf("%u", inputUsageStreams);
    printf(" output=");
    if (outputUsageStreams == kUsageMissing) printf("missing");
    else if (outputUsageStreams == kUsageInvalid) printf("invalid");
    else printf("%u", outputUsageStreams);
    printf("\n");
    printf("defaultDevice input=%u output=%u\n", canDefaultInput, canDefaultOutput);

    if (outputStreamCount == 0 || outputChannels != 8) {
        fprintf(stderr, "invalid output topology: streams=%u channels=%u\n",
                outputStreamCount, outputChannels);
        failures++;
    }
    if (inputStreamCount != 1 || inputChannels != 8) {
        fprintf(stderr, "invalid input topology: streams=%u channels=%u\n",
                inputStreamCount, inputChannels);
        failures++;
    }
    if (globalStreamCount != inputStreamCount + outputStreamCount) {
        fprintf(stderr, "global stream count mismatch: global=%u input+output=%u\n",
                globalStreamCount, inputStreamCount + outputStreamCount);
        failures++;
    }
    if (globalChannels != inputChannels + outputChannels) {
        fprintf(stderr, "global channel count mismatch: global=%u input+output=%u\n",
                globalChannels, inputChannels + outputChannels);
        failures++;
    }
    if (outputUsageStreams != kUsageMissing && outputUsageStreams != outputStreamCount) {
        fprintf(stderr, "output IOProcStreamUsage mismatch: usage=%u outputStreams=%u\n",
                outputUsageStreams, outputStreamCount);
        failures++;
    }
    if (inputUsageStreams != kUsageMissing && inputUsageStreams != inputStreamCount) {
        fprintf(stderr, "input IOProcStreamUsage mismatch: usage=%u inputStreams=%u\n",
                inputUsageStreams, inputStreamCount);
        failures++;
    }
    if (canDefaultOutput == 0 || canDefaultInput != 0) {
        fprintf(stderr, "default-device flags invalid: input=%u output=%u\n",
                canDefaultInput, canDefaultOutput);
        failures++;
    }

    CFRelease(bundle);
    if (failures == 0) {
        printf("HAL parity OK\n");
        return 0;
    }
    fprintf(stderr, "HAL parity FAIL failures=%d\n", failures);
    return 1;
}
