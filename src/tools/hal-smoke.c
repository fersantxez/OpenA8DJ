#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

typedef void *(*OpenA8DJFactory)(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID);

static UInt32 gConfigurationChangeRequests = 0;
static AudioObjectID gLastConfigurationDevice = kAudioObjectUnknown;
static UInt64 gLastConfigurationAction = 0;

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
    (void)inChangeInfo;
    gConfigurationChangeRequests++;
    gLastConfigurationDevice = inDeviceObjectID;
    gLastConfigurationAction = inChangeAction;
    return kAudioHardwareNoError;
}

static const AudioServerPlugInHostInterface kSmokeHost = {
    HostPropertiesChanged,
    HostCopyFromStorage,
    HostWriteToStorage,
    HostDeleteFromStorage,
    HostRequestDeviceConfigurationChange
};

static void PrintCFString(CFStringRef string)
{
    char buffer[512];
    if (string != NULL &&
        CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        printf("%s", buffer);
    } else {
        printf("<null>");
    }
}

static OSStatus GetProperty(AudioServerPlugInDriverRef driver,
                            AudioObjectID objectID,
                            AudioObjectPropertySelector selector,
                            AudioObjectPropertyScope scope,
                            UInt32 dataSize,
                            UInt32 *outDataSize,
                            void *outData)
{
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain
    };
    return (*driver)->GetPropertyData(driver,
                                      objectID,
                                      0,
                                      &address,
                                      0,
                                      NULL,
                                      dataSize,
                                      outDataSize,
                                      outData);
}

static AudioObjectID FindDeviceByUID(AudioServerPlugInDriverRef driver,
                                    const AudioObjectID *devices,
                                    UInt32 count,
                                    CFStringRef expectedUID)
{
    for (UInt32 i = 0; i < count; ++i) {
        CFStringRef uid = NULL;
        UInt32 size = 0;
        if (GetProperty(driver, devices[i], kAudioDevicePropertyDeviceUID,
                        kAudioObjectPropertyScopeGlobal, sizeof(uid), &size,
                        &uid) == kAudioHardwareNoError && uid != NULL) {
            Boolean matches = CFEqual(uid, expectedUID);
            CFRelease(uid);
            if (matches) return devices[i];
        }
    }
    return kAudioObjectUnknown;
}

int main(int argc, char **argv)
{
    const char *bundlePath = argc > 1 ? argv[1] : "build/OpenA8DJ.driver";
    CFStringRef pathString = CFStringCreateWithCString(NULL, bundlePath, kCFStringEncodingUTF8);
    CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, pathString, kCFURLPOSIXPathStyle, true);
    CFRelease(pathString);
    if (bundleURL == NULL) {
        fprintf(stderr, "No se pudo crear URL para %s\n", bundlePath);
        return 2;
    }

    CFBundleRef bundle = CFBundleCreate(NULL, bundleURL);
    CFRelease(bundleURL);
    if (bundle == NULL) {
        fprintf(stderr, "No se pudo abrir bundle %s\n", bundlePath);
        return 3;
    }
    if (!CFBundleLoadExecutable(bundle)) {
        fprintf(stderr, "No se pudo cargar ejecutable del bundle.\n");
        CFRelease(bundle);
        return 4;
    }

    OpenA8DJFactory factory = (OpenA8DJFactory)CFBundleGetFunctionPointerForName(bundle, CFSTR("OpenA8DJ_Create"));
    if (factory == NULL) {
        fprintf(stderr, "No se encontro OpenA8DJ_Create.\n");
        CFRelease(bundle);
        return 5;
    }

    AudioServerPlugInDriverRef driver = (AudioServerPlugInDriverRef)factory(NULL, kAudioServerPlugInTypeUUID);
    if (driver == NULL) {
        fprintf(stderr, "La factory no devolvio driver.\n");
        CFRelease(bundle);
        return 6;
    }

    OSStatus status = (*driver)->Initialize(driver, &kSmokeHost);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "Initialize fallo: %d\n", (int)status);
        CFRelease(bundle);
        return 7;
    }

    UInt32 dataSize = 0;
    AudioObjectID devices[4] = {0};
    status = GetProperty(driver,
                         kAudioObjectPlugInObject,
                         kAudioPlugInPropertyDeviceList,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(devices),
                         &dataSize,
                         devices);
    if (status != kAudioHardwareNoError ||
        dataSize != 2 * sizeof(AudioObjectID)) {
        fprintf(stderr, "DeviceList fallo: status=%d size=%u\n", (int)status, dataSize);
        CFRelease(bundle);
        return 8;
    }
    AudioObjectID physicalDevice = FindDeviceByUID(
        driver, devices, 2, CFSTR("org.opena8dj.Audio8DJ"));
    AudioObjectID loopbackDevice = FindDeviceByUID(
        driver, devices, 2, CFSTR("org.opena8dj.Audio8DJ.loopback"));
    if (physicalDevice != 2 || loopbackDevice != 11) {
        fprintf(stderr, "UID/device mapping invalid: physical=%u loopback=%u\n",
                physicalDevice, loopbackDevice);
        CFRelease(bundle);
        return 16;
    }
    AudioObjectPropertyAddress translateAddress = {
        kAudioPlugInPropertyTranslateUIDToDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef translateUID = CFSTR("org.opena8dj.Audio8DJ");
    AudioObjectID translated = kAudioObjectUnknown;
    dataSize = 0;
    status = (*driver)->GetPropertyData(
        driver, kAudioObjectPlugInObject, 0, &translateAddress,
        sizeof(translateUID), &translateUID, sizeof(translated),
        &dataSize, &translated);
    if (status != kAudioHardwareNoError || translated != physicalDevice) {
        fprintf(stderr, "Physical UID translation failed: %u\n", translated);
        CFRelease(bundle);
        return 21;
    }
    translateUID = CFSTR("org.opena8dj.Audio8DJ.loopback");
    translated = kAudioObjectUnknown;
    dataSize = 0;
    status = (*driver)->GetPropertyData(
        driver, kAudioObjectPlugInObject, 0, &translateAddress,
        sizeof(translateUID), &translateUID, sizeof(translated),
        &dataSize, &translated);
    if (status != kAudioHardwareNoError || translated != loopbackDevice) {
        fprintf(stderr, "Loopback UID translation failed: %u\n", translated);
        CFRelease(bundle);
        return 22;
    }

    CFStringRef name = NULL;
    dataSize = 0;
    status = GetProperty(driver,
                         physicalDevice,
                         kAudioObjectPropertyName,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(name),
                         &dataSize,
                         &name);
    if (status != kAudioHardwareNoError || name == NULL) {
        fprintf(stderr, "Name fallo: status=%d size=%u\n", (int)status, dataSize);
        CFRelease(bundle);
        return 9;
    }

    Float64 sampleRate = 0;
    dataSize = 0;
    status = GetProperty(driver,
                         physicalDevice,
                         kAudioDevicePropertyNominalSampleRate,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(sampleRate),
                         &dataSize,
                         &sampleRate);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "SampleRate fallo: %d\n", (int)status);
        CFRelease(name);
        CFRelease(bundle);
        return 10;
    }

    AudioObjectID streams[16] = {0};
    dataSize = 0;
    status = GetProperty(driver,
                         physicalDevice,
                         kAudioDevicePropertyStreams,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(streams),
                         &dataSize,
                         streams);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "Streams fallo: %d\n", (int)status);
        CFRelease(name);
        CFRelease(bundle);
        return 11;
    }
    UInt32 streamCount = dataSize / (UInt32)sizeof(AudioObjectID);

    UInt32 bufferFrames = 0;
    dataSize = 0;
    status = GetProperty(driver,
                         physicalDevice,
                         kAudioDevicePropertyBufferFrameSize,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(bufferFrames),
                         &dataSize,
                         &bufferFrames);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "BufferFrameSize fallo: %d\n", (int)status);
        CFRelease(name);
        CFRelease(bundle);
        return 12;
    }

    UInt32 bufferBytes = 0;
    dataSize = 0;
    status = GetProperty(driver,
                         physicalDevice,
                         kAudioDevicePropertyBufferSize,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(bufferBytes),
                         &dataSize,
                         &bufferBytes);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "BufferSize fallo: %d\n", (int)status);
        CFRelease(name);
        CFRelease(bundle);
        return 13;
    }

    AudioValueRange bufferRange = {0};
    dataSize = 0;
    status = GetProperty(driver,
                         physicalDevice,
                         kAudioDevicePropertyBufferFrameSizeRange,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(bufferRange),
                         &dataSize,
                         &bufferRange);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "BufferFrameSizeRange fallo: %d\n", (int)status);
        CFRelease(name);
        CFRelease(bundle);
        return 14;
    }

    printf("HAL smoke OK: deviceID=%u name=", physicalDevice);
    PrintCFString(name);
    printf(" sampleRate=%.0f streams=%u buffer=%u bufferBytes=%u bufferRange=%.0f-%.0f\n",
           sampleRate,
           streamCount,
           bufferFrames,
           bufferBytes,
           bufferRange.mMinimum,
           bufferRange.mMaximum);

    if (streamCount != 5 ||
        bufferFrames != 512 ||
        bufferBytes != 16384 ||
        bufferRange.mMinimum != 512.0) {
        fprintf(stderr,
                "Unexpected HAL properties: streams=%u frames=%u bytes=%u range=%.0f-%.0f\n",
                streamCount,
                bufferFrames,
                bufferBytes,
                bufferRange.mMinimum,
                bufferRange.mMaximum);
        CFRelease(name);
        CFRelease(bundle);
        return 15;
    }

    AudioObjectID loopbackStreams[2] = {0};
    dataSize = 0;
    status = GetProperty(driver, loopbackDevice,
                         kAudioDevicePropertyStreams,
                         kAudioObjectPropertyScopeInput,
                         sizeof(loopbackStreams), &dataSize,
                         loopbackStreams);
    if (status != kAudioHardwareNoError ||
        dataSize != sizeof(AudioObjectID) || loopbackStreams[0] != 12) {
        fprintf(stderr, "Loopback input stream invalid status=%d size=%u id=%u\n",
                (int)status, dataSize, loopbackStreams[0]);
        CFRelease(name);
        CFRelease(bundle);
        return 17;
    }
    dataSize = 0;
    status = GetProperty(driver, loopbackDevice,
                         kAudioDevicePropertyStreams,
                         kAudioObjectPropertyScopeOutput,
                         sizeof(loopbackStreams), &dataSize,
                         loopbackStreams);
    if (status != kAudioHardwareNoError || dataSize != 0) {
        fprintf(stderr, "Loopback unexpectedly has output streams\n");
        CFRelease(name);
        CFRelease(bundle);
        return 18;
    }

    UInt32 physicalRunningBefore = 0;
    UInt32 virtualRunning = 0;
    dataSize = 0;
    status = GetProperty(driver, physicalDevice,
                         kAudioDevicePropertyDeviceIsRunning,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(physicalRunningBefore), &dataSize,
                         &physicalRunningBefore);
    if (status != kAudioHardwareNoError ||
        (*driver)->StartIO(driver, loopbackDevice, 9001) !=
            kAudioHardwareNoError ||
        (*driver)->StartIO(driver, loopbackDevice, 9001) !=
            kAudioHardwareNoError) {
        fprintf(stderr, "Loopback StartIO failed without physical playback\n");
        CFRelease(name);
        CFRelease(bundle);
        return 19;
    }
    dataSize = 0;
    status = GetProperty(driver, loopbackDevice,
                         kAudioDevicePropertyDeviceIsRunning,
                         kAudioObjectPropertyScopeGlobal,
                         sizeof(virtualRunning), &dataSize, &virtualRunning);
    UInt32 physicalRunningAfter = UINT32_MAX;
    dataSize = 0;
    (void)GetProperty(driver, physicalDevice,
                      kAudioDevicePropertyDeviceIsRunning,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(physicalRunningAfter), &dataSize,
                      &physicalRunningAfter);
    Boolean willRead = false;
    Boolean inPlace = false;
    (void)(*driver)->WillDoIOOperation(
        driver, loopbackDevice, 9001,
        kAudioServerPlugInIOOperationReadInput, &willRead, &inPlace);
    Float32 silence[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    AudioServerPlugInIOCycleInfo cycle = {0};
    status = (*driver)->DoIOOperation(
        driver, loopbackDevice, 12, 9001,
        kAudioServerPlugInIOOperationReadInput, 4, &cycle, silence, NULL);
    bool exactSilence = true;
    for (UInt32 i = 0; i < 8; ++i) {
        exactSilence = exactSilence && silence[i] == 0.0f;
    }
    Float64 virtualSample1 = -1.0;
    Float64 virtualSample2 = -1.0;
    UInt64 virtualHost1 = 0;
    UInt64 virtualHost2 = 0;
    UInt64 virtualSeed1 = 0;
    UInt64 virtualSeed2 = 0;
    (void)(*driver)->GetZeroTimeStamp(
        driver, loopbackDevice, 9001, &virtualSample1,
        &virtualHost1, &virtualSeed1);
    (void)(*driver)->GetZeroTimeStamp(
        driver, loopbackDevice, 9001, &virtualSample2,
        &virtualHost2, &virtualSeed2);
    (void)(*driver)->StopIO(driver, loopbackDevice, 9001);
    UInt32 virtualAfterOneStop = UINT32_MAX;
    dataSize = 0;
    (void)GetProperty(driver, loopbackDevice,
                      kAudioDevicePropertyDeviceIsRunning,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(virtualAfterOneStop), &dataSize,
                      &virtualAfterOneStop);
    (void)(*driver)->StopIO(driver, loopbackDevice, 9001);
    UInt32 virtualStopped = UINT32_MAX;
    dataSize = 0;
    (void)GetProperty(driver, loopbackDevice,
                      kAudioDevicePropertyDeviceIsRunning,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(virtualStopped), &dataSize, &virtualStopped);
    (void)(*driver)->StartIO(driver, loopbackDevice, 9001);
    UInt64 virtualRestartSeed = 0;
    Float64 virtualRestartSample = -1.0;
    UInt64 virtualRestartHost = 0;
    (void)(*driver)->GetZeroTimeStamp(
        driver, loopbackDevice, 9001, &virtualRestartSample,
        &virtualRestartHost, &virtualRestartSeed);
    (void)(*driver)->StopIO(driver, loopbackDevice, 9001);
    if (status != kAudioHardwareNoError || virtualRunning != 1 ||
        virtualAfterOneStop != 1 || virtualStopped != 0 ||
        physicalRunningAfter != physicalRunningBefore ||
        !willRead || !inPlace || !exactSilence ||
        virtualSample1 < 0.0 || virtualSample2 < virtualSample1 ||
        virtualHost1 == 0 || virtualHost2 < virtualHost1 ||
        virtualSeed1 == 0 || virtualSeed2 != virtualSeed1 ||
        virtualRestartSeed <= virtualSeed1 ||
        virtualRestartSample < 0.0 || virtualRestartHost == 0) {
        fprintf(stderr,
                "Loopback lifecycle/read invalid virtual=%u stopped=%u "
                "physical=%u/%u willRead=%u inPlace=%u silence=%d\n",
                virtualRunning, virtualStopped, physicalRunningBefore,
                physicalRunningAfter, willRead, inPlace, exactSilence);
        CFRelease(name);
        CFRelease(bundle);
        return 20;
    }

    UInt32 physicalBufferBefore = 0;
    dataSize = 0;
    (void)GetProperty(driver, physicalDevice,
                      kAudioDevicePropertyBufferFrameSize,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(physicalBufferBefore), &dataSize,
                      &physicalBufferBefore);
    AudioObjectPropertyAddress bufferAddress = {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 newLoopbackBuffer = 256;
    UInt32 requestsBefore = gConfigurationChangeRequests;
    status = (*driver)->SetPropertyData(
        driver, loopbackDevice, 0, &bufferAddress, 0, NULL,
        sizeof(newLoopbackBuffer), &newLoopbackBuffer);
    if (status != kAudioHardwareNoError ||
        gConfigurationChangeRequests != requestsBefore + 1 ||
        gLastConfigurationDevice != loopbackDevice ||
        gLastConfigurationAction != 3 ||
        (*driver)->PerformDeviceConfigurationChange(
            driver, loopbackDevice, 3, NULL) != kAudioHardwareNoError) {
        fprintf(stderr, "Loopback buffer configuration request invalid\n");
        CFRelease(name);
        CFRelease(bundle);
        return 23;
    }
    UInt32 loopbackBufferAfter = 0;
    UInt32 physicalBufferAfter = 0;
    dataSize = 0;
    (void)GetProperty(driver, loopbackDevice,
                      kAudioDevicePropertyBufferFrameSize,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(loopbackBufferAfter), &dataSize,
                      &loopbackBufferAfter);
    dataSize = 0;
    (void)GetProperty(driver, physicalDevice,
                      kAudioDevicePropertyBufferFrameSize,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(physicalBufferAfter), &dataSize,
                      &physicalBufferAfter);
    if (loopbackBufferAfter != newLoopbackBuffer ||
        physicalBufferAfter != physicalBufferBefore) {
        fprintf(stderr,
                "Loopback buffer changed physical geometry: loop=%u phys=%u/%u\n",
                loopbackBufferAfter, physicalBufferBefore,
                physicalBufferAfter);
        CFRelease(name);
        CFRelease(bundle);
        return 24;
    }

    CFRelease(name);
    CFRelease(bundle);
    return 0;
}
