#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const CFStringRef kTargetUID = CFSTR("org.opena8dj.Audio8DJ");

#define FourCC(a, b, c, d) \
    ((((UInt32)(a)) << 24) | (((UInt32)(b)) << 16) | (((UInt32)(c)) << 8) | ((UInt32)(d)))

enum {
    kOpenA8DJPropertyCycleFrameSize = FourCC('c', 'f', 's', 'z')
};

static OSStatus GetSize(AudioObjectID objectID,
                        AudioObjectPropertySelector selector,
                        AudioObjectPropertyScope scope,
                        AudioObjectPropertyElement element,
                        UInt32 *outSize)
{
    AudioObjectPropertyAddress address = {selector, scope, element};
    return AudioObjectGetPropertyDataSize(objectID, &address, 0, NULL, outSize);
}

static OSStatus GetData(AudioObjectID objectID,
                        AudioObjectPropertySelector selector,
                        AudioObjectPropertyScope scope,
                        AudioObjectPropertyElement element,
                        UInt32 *ioSize,
                        void *outData)
{
    AudioObjectPropertyAddress address = {selector, scope, element};
    return AudioObjectGetPropertyData(objectID, &address, 0, NULL, ioSize, outData);
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

static const char *ScopeName(AudioObjectPropertyScope scope)
{
    return scope == kAudioObjectPropertyScopeInput ? "input" : "output";
}

static AudioObjectID FindTargetDevice(void)
{
    UInt32 size = 0;
    OSStatus status = GetSize(kAudioObjectSystemObject,
                              kAudioHardwarePropertyDevices,
                              kAudioObjectPropertyScopeGlobal,
                              kAudioObjectPropertyElementMain,
                              &size);
    if (status != kAudioHardwareNoError || size == 0) {
        return kAudioObjectUnknown;
    }

    UInt32 count = size / (UInt32)sizeof(AudioObjectID);
    AudioObjectID *devices = (AudioObjectID *)calloc(count, sizeof(AudioObjectID));
    if (devices == NULL) {
        return kAudioObjectUnknown;
    }

    status = GetData(kAudioObjectSystemObject,
                     kAudioHardwarePropertyDevices,
                     kAudioObjectPropertyScopeGlobal,
                     kAudioObjectPropertyElementMain,
                     &size,
                     devices);
    if (status != kAudioHardwareNoError) {
        free(devices);
        return kAudioObjectUnknown;
    }

    AudioObjectID result = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        CFStringRef uid = NULL;
        UInt32 uidSize = sizeof(uid);
        status = GetData(devices[i],
                         kAudioDevicePropertyDeviceUID,
                         kAudioObjectPropertyScopeGlobal,
                         kAudioObjectPropertyElementMain,
                         &uidSize,
                         &uid);
        if (status == kAudioHardwareNoError && uid != NULL) {
            if (CFStringCompare(uid, kTargetUID, 0) == kCFCompareEqualTo) {
                result = devices[i];
            }
            CFRelease(uid);
        }
        if (result != kAudioObjectUnknown) {
            break;
        }
    }

    free(devices);
    return result;
}

static UInt32 PrintStreamConfiguration(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    UInt32 size = 0;
    OSStatus status = GetSize(deviceID,
                              kAudioDevicePropertyStreamConfiguration,
                              scope,
                              kAudioObjectPropertyElementMain,
                              &size);
    if (status != kAudioHardwareNoError || size == 0) {
        printf("%s stream configuration: unavailable\n", ScopeName(scope));
        return 0;
    }

    AudioBufferList *buffers = (AudioBufferList *)calloc(1, size);
    if (buffers == NULL) {
        printf("%s stream configuration: out of memory\n", ScopeName(scope));
        return 0;
    }

    status = GetData(deviceID,
                     kAudioDevicePropertyStreamConfiguration,
                     scope,
                     kAudioObjectPropertyElementMain,
                     &size,
                     buffers);
    if (status != kAudioHardwareNoError) {
        printf("%s stream configuration: error %d\n", ScopeName(scope), (int)status);
        free(buffers);
        return 0;
    }

    UInt32 channels = 0;
    printf("%s buffers: %u", ScopeName(scope), buffers->mNumberBuffers);
    for (UInt32 i = 0; i < buffers->mNumberBuffers; i++) {
        channels += buffers->mBuffers[i].mNumberChannels;
        printf(" [%u channels]", buffers->mBuffers[i].mNumberChannels);
    }
    printf(" total=%u\n", channels);

    free(buffers);
    return channels;
}

static void PrintStreams(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    UInt32 size = 0;
    OSStatus status = GetSize(deviceID,
                              kAudioDevicePropertyStreams,
                              scope,
                              kAudioObjectPropertyElementMain,
                              &size);
    if (status != kAudioHardwareNoError || size == 0) {
        printf("%s streams: none\n", ScopeName(scope));
        return;
    }

    UInt32 count = size / (UInt32)sizeof(AudioObjectID);
    AudioObjectID *streams = (AudioObjectID *)calloc(count, sizeof(AudioObjectID));
    if (streams == NULL) {
        printf("%s streams: out of memory\n", ScopeName(scope));
        return;
    }

    status = GetData(deviceID,
                     kAudioDevicePropertyStreams,
                     scope,
                     kAudioObjectPropertyElementMain,
                     &size,
                     streams);
    if (status != kAudioHardwareNoError) {
        printf("%s streams: error %d\n", ScopeName(scope), (int)status);
        free(streams);
        return;
    }

    printf("%s streams: %u\n", ScopeName(scope), count);
    for (UInt32 i = 0; i < count; i++) {
        UInt32 startingChannel = 0;
        UInt32 direction = 0;
        AudioStreamBasicDescription asbd;
        memset(&asbd, 0, sizeof(asbd));

        UInt32 valueSize = sizeof(startingChannel);
        (void)GetData(streams[i],
                      kAudioStreamPropertyStartingChannel,
                      kAudioObjectPropertyScopeGlobal,
                      kAudioObjectPropertyElementMain,
                      &valueSize,
                      &startingChannel);

        valueSize = sizeof(direction);
        (void)GetData(streams[i],
                      kAudioStreamPropertyDirection,
                      kAudioObjectPropertyScopeGlobal,
                      kAudioObjectPropertyElementMain,
                      &valueSize,
                      &direction);

        valueSize = sizeof(asbd);
        (void)GetData(streams[i],
                      kAudioStreamPropertyVirtualFormat,
                      kAudioObjectPropertyScopeGlobal,
                      kAudioObjectPropertyElementMain,
                      &valueSize,
                      &asbd);

        printf("  id=%u direction=%u start=%u channels/frame=%u rate=%.0f\n",
               streams[i],
               direction,
               startingChannel,
               asbd.mChannelsPerFrame,
               asbd.mSampleRate);
    }

    free(streams);
}

static void PrintPreferredStereo(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    UInt32 stereo[2] = {0, 0};
    UInt32 size = sizeof(stereo);
    OSStatus status = GetData(deviceID,
                              kAudioDevicePropertyPreferredChannelsForStereo,
                              scope,
                              kAudioObjectPropertyElementMain,
                              &size,
                              stereo);
    if (status == kAudioHardwareNoError) {
        printf("%s preferred stereo: %u/%u\n", ScopeName(scope), stereo[0], stereo[1]);
    }
}

static void PrintChannelNames(AudioObjectID deviceID, AudioObjectPropertyScope scope, UInt32 channels)
{
    printf("%s channel names:", ScopeName(scope));
    for (UInt32 channel = 1; channel <= channels; channel++) {
        CFStringRef name = NULL;
        UInt32 size = sizeof(name);
        OSStatus status = GetData(deviceID,
                                  kAudioObjectPropertyElementName,
                                  scope,
                                  channel,
                                  &size,
                                  &name);
        printf(" %u=", channel);
        if (status == kAudioHardwareNoError && name != NULL) {
            PrintCFString(name);
            CFRelease(name);
        } else {
            printf("<none>");
        }
        if (channel < channels) {
            printf(",");
        }
    }
    printf("\n");
}

static void PrintPreferredLayout(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    UInt32 size = 0;
    OSStatus status = GetSize(deviceID,
                              kAudioDevicePropertyPreferredChannelLayout,
                              scope,
                              kAudioObjectPropertyElementMain,
                              &size);
    if (status != kAudioHardwareNoError || size == 0) {
        printf("%s preferred layout: unavailable\n", ScopeName(scope));
        return;
    }

    AudioChannelLayout *layout = (AudioChannelLayout *)calloc(1, size);
    if (layout == NULL) {
        printf("%s preferred layout: out of memory\n", ScopeName(scope));
        return;
    }

    status = GetData(deviceID,
                     kAudioDevicePropertyPreferredChannelLayout,
                     scope,
                     kAudioObjectPropertyElementMain,
                     &size,
                     layout);
    if (status == kAudioHardwareNoError) {
        printf("%s preferred layout: descriptions=%u labels=",
               ScopeName(scope),
               layout->mNumberChannelDescriptions);
        for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++) {
            printf("%u", layout->mChannelDescriptions[i].mChannelLabel);
            if (i + 1 < layout->mNumberChannelDescriptions) {
                printf(",");
            }
        }
        printf("\n");
    } else {
        printf("%s preferred layout: error %d\n", ScopeName(scope), (int)status);
    }

    free(layout);
}

static void PrintDeviceTiming(AudioObjectID deviceID)
{
    Float64 rate = 0.0;
    UInt32 bufferFrames = 0;
    UInt32 cycleFrames = 0;
    UInt32 bufferBytes = 0;
    AudioValueRange bufferRange;
    AudioValueRange bufferByteRange;
    memset(&bufferRange, 0, sizeof(bufferRange));
    memset(&bufferByteRange, 0, sizeof(bufferByteRange));

    UInt32 size = sizeof(rate);
    OSStatus rateStatus = GetData(deviceID,
                                  kAudioDevicePropertyNominalSampleRate,
                                  kAudioObjectPropertyScopeGlobal,
                                  kAudioObjectPropertyElementMain,
                                  &size,
                                  &rate);

    size = sizeof(bufferFrames);
    OSStatus bufferStatus = GetData(deviceID,
                                    kAudioDevicePropertyBufferFrameSize,
                                    kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain,
                                    &size,
                                    &bufferFrames);

    size = sizeof(cycleFrames);
    OSStatus cycleStatus = GetData(deviceID,
                                   kOpenA8DJPropertyCycleFrameSize,
                                   kAudioObjectPropertyScopeGlobal,
                                   kAudioObjectPropertyElementMain,
                                   &size,
                                   &cycleFrames);

    size = sizeof(bufferRange);
    OSStatus rangeStatus = GetData(deviceID,
                                   kAudioDevicePropertyBufferFrameSizeRange,
                                   kAudioObjectPropertyScopeGlobal,
                                   kAudioObjectPropertyElementMain,
                                   &size,
                                   &bufferRange);

    size = sizeof(bufferBytes);
    OSStatus bufferBytesStatus = GetData(deviceID,
                                         kAudioDevicePropertyBufferSize,
                                         kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMain,
                                         &size,
                                         &bufferBytes);

    size = sizeof(bufferByteRange);
    OSStatus byteRangeStatus = GetData(deviceID,
                                       kAudioDevicePropertyBufferSizeRange,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain,
                                       &size,
                                       &bufferByteRange);

    printf("timing:");
    if (rateStatus == kAudioHardwareNoError) {
        printf(" rate=%.0f", rate);
    } else {
        printf(" rate-error=%d", (int)rateStatus);
    }
    if (bufferStatus == kAudioHardwareNoError) {
        printf(" buffer=%u", bufferFrames);
    } else {
        printf(" buffer-error=%d", (int)bufferStatus);
    }
    if (cycleStatus == kAudioHardwareNoError) {
        printf(" cycle=%u", cycleFrames);
    } else {
        printf(" cycle-error=%d", (int)cycleStatus);
    }
    if (rangeStatus == kAudioHardwareNoError) {
        printf(" buffer-range=%.0f-%.0f", bufferRange.mMinimum, bufferRange.mMaximum);
    } else {
        printf(" buffer-range-error=%d", (int)rangeStatus);
    }
    if (bufferBytesStatus == kAudioHardwareNoError) {
        printf(" buffer-bytes=%u", bufferBytes);
    } else {
        printf(" buffer-bytes-error=%d", (int)bufferBytesStatus);
    }
    if (byteRangeStatus == kAudioHardwareNoError) {
        printf(" buffer-byte-range=%.0f-%.0f", bufferByteRange.mMinimum, bufferByteRange.mMaximum);
    } else {
        printf(" buffer-byte-range-error=%d", (int)byteRangeStatus);
    }
    printf("\n");

    UInt32 ratesSize = 0;
    OSStatus sizeStatus = GetSize(deviceID,
                                  kAudioDevicePropertyAvailableNominalSampleRates,
                                  kAudioObjectPropertyScopeGlobal,
                                  kAudioObjectPropertyElementMain,
                                  &ratesSize);
    if (sizeStatus == kAudioHardwareNoError && ratesSize > 0) {
        AudioValueRange *rates = (AudioValueRange *)calloc(1, ratesSize);
        if (rates != NULL) {
            OSStatus ratesStatus = GetData(deviceID,
                                           kAudioDevicePropertyAvailableNominalSampleRates,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain,
                                           &ratesSize,
                                           rates);
            if (ratesStatus == kAudioHardwareNoError) {
                UInt32 count = ratesSize / (UInt32)sizeof(AudioValueRange);
                printf("available rates:");
                for (UInt32 i = 0; i < count; i++) {
                    printf(" %.0f", rates[i].mMinimum);
                }
                printf("\n");
            } else {
                printf("available rates: error %d\n", (int)ratesStatus);
            }
            free(rates);
        }
    } else {
        printf("available rates: error %d\n", (int)sizeStatus);
    }
}

static void InspectScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    UInt32 channels = PrintStreamConfiguration(deviceID, scope);
    PrintStreams(deviceID, scope);
    PrintPreferredStereo(deviceID, scope);
    PrintPreferredLayout(deviceID, scope);
    PrintChannelNames(deviceID, scope, channels);
}

int main(void)
{
    AudioObjectID deviceID = FindTargetDevice();
    if (deviceID == kAudioObjectUnknown) {
        fprintf(stderr, "Open Audio 8 DJ no aparece en Core Audio\n");
        return 2;
    }

    CFStringRef name = NULL;
    Float64 rate = 0.0;
    UInt32 size = sizeof(name);
    (void)GetData(deviceID,
                  kAudioObjectPropertyName,
                  kAudioObjectPropertyScopeGlobal,
                  kAudioObjectPropertyElementMain,
                  &size,
                  &name);
    size = sizeof(rate);
    (void)GetData(deviceID,
                  kAudioDevicePropertyNominalSampleRate,
                  kAudioObjectPropertyScopeGlobal,
                  kAudioObjectPropertyElementMain,
                  &size,
                  &rate);

    printf("device id=%u name=", deviceID);
    PrintCFString(name);
    printf(" rate=%.0f\n", rate);
    if (name != NULL) {
        CFRelease(name);
    }

    PrintDeviceTiming(deviceID);
    InspectScope(deviceID, kAudioObjectPropertyScopeInput);
    InspectScope(deviceID, kAudioObjectPropertyScopeOutput);

    return 0;
}
