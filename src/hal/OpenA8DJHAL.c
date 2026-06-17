#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach_time.h>

#include "OpenA8DJUSB.h"

#ifndef OPENA8DJ_ENABLE_TRACE
#define OPENA8DJ_ENABLE_TRACE 0
#endif

#ifndef OPENA8DJ_ENABLE_INPUT_IO
#define OPENA8DJ_ENABLE_INPUT_IO 1
#endif

#ifndef OPENA8DJ_INPUT_STREAM_COUNT
#define OPENA8DJ_INPUT_STREAM_COUNT 1
#endif

#ifndef OPENA8DJ_PROPERTY_BACKOFF_USEC
#define OPENA8DJ_PROPERTY_BACKOFF_USEC 0
#endif

#ifndef OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
#define OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY 0
#endif

#ifndef OPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP
#define OPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP 0
#endif

#ifndef OPENA8DJ_IGNORE_OUTPUT_SAMPLE_TIME
#define OPENA8DJ_IGNORE_OUTPUT_SAMPLE_TIME 0
#endif

#ifndef OPENA8DJ_BACKGROUND_WARM_OPEN
#define OPENA8DJ_BACKGROUND_WARM_OPEN 0
#endif

#ifndef OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT
#define OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT 0
#endif

#ifndef OPENA8DJ_STOP_GRACE_USEC
#define OPENA8DJ_STOP_GRACE_USEC 4000000
#endif

#ifndef OPENA8DJ_STOP_ISOC_ON_STOP
#define OPENA8DJ_STOP_ISOC_ON_STOP 0
#endif

#ifndef OPENA8DJ_OUTPUT_STREAM_COUNT
#define OPENA8DJ_OUTPUT_STREAM_COUNT 4
#endif

#if OPENA8DJ_OUTPUT_STREAM_COUNT < 1 || OPENA8DJ_OUTPUT_STREAM_COUNT > 4
#error OPENA8DJ_OUTPUT_STREAM_COUNT must be between 1 and 4
#endif

#if (8 % OPENA8DJ_OUTPUT_STREAM_COUNT) != 0
#error OPENA8DJ_OUTPUT_STREAM_COUNT must divide the 8 hardware output channels evenly
#endif

#if OPENA8DJ_INPUT_STREAM_COUNT < 1 || OPENA8DJ_INPUT_STREAM_COUNT > 4
#error OPENA8DJ_INPUT_STREAM_COUNT must be between 1 and 4
#endif

#if (8 % OPENA8DJ_INPUT_STREAM_COUNT) != 0
#error OPENA8DJ_INPUT_STREAM_COUNT must divide the 8 hardware input channels evenly
#endif

enum {
    kOpenA8DJDeviceObjectID = 2,
    kOpenA8DJInputStreamAObjectID = 3,
    kOpenA8DJInputStreamBObjectID = 4,
    kOpenA8DJInputStreamCObjectID = 5,
    kOpenA8DJInputStreamDObjectID = 6,
    kOpenA8DJOutputStreamAObjectID = 7,
    kOpenA8DJOutputStreamBObjectID = 8,
    kOpenA8DJOutputStreamCObjectID = 9,
    kOpenA8DJOutputStreamDObjectID = 10,
    kOpenA8DJStreamCount = 4,
    kOpenA8DJInputStreamCount = OPENA8DJ_INPUT_STREAM_COUNT,
    kOpenA8DJOutputStreamCount = OPENA8DJ_OUTPUT_STREAM_COUNT,
    kOpenA8DJChannelsPerStream = 2,
    kOpenA8DJChannels = 8,
    kOpenA8DJSupportedRateCount = 4,
    kOpenA8DJConfigChangeSampleRate = 1,
    kOpenA8DJConfigChangeBufferFrames = 2,
    kOpenA8DJPreferredBufferFrames = 512,
    kOpenA8DJMinBufferFrames = 512,
    kOpenA8DJMaxAdvertisedBufferFrames = 4096,
    kOpenA8DJMaxBufferFrames = 4096,
    kOpenA8DJZeroTimeStampPeriodFrames = 16384
};

static const CFStringRef kDriverBundleID = CFSTR("org.opena8dj.driver.hal");
static const CFStringRef kDeviceUID = CFSTR("org.opena8dj.Audio8DJ");
static const CFStringRef kModelUID = CFSTR("org.opena8dj.Audio8DJ.model");
static const CFStringRef kDeviceName = CFSTR("Open Audio 8 DJ");
static const CFStringRef kManufacturer = CFSTR("OpenA8DJ");

static AudioServerPlugInHostRef gHost = NULL;
static atomic_uint gRefCount = 1;
static Float64 gSampleRate = 48000.0;
static UInt32 gBufferFrames = kOpenA8DJPreferredBufferFrames;
static UInt32 gRunningClients = 0;
static UInt64 gZeroTimeStampSeed = 1;
static Float64 gSampleTime = 0.0;
static UInt64 gHostTime = 0;
static Float64 gPendingSampleRate = 0.0;
static UInt32 gPendingBufferFrames = 0;
static atomic_bool gDevicePresent = false;
static atomic_bool gClockRunning = false;
static atomic_uint gStopGeneration = 1;
static pthread_mutex_t gClockMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gIOMutex = PTHREAD_MUTEX_INITIALIZER;
static Float32 gOutputCycleBuffer[kOpenA8DJMaxBufferFrames * kOpenA8DJChannels];
static UInt32 gOutputCycleFrames = 0;
static UInt64 gOutputCycleCounter = 0;
static Float64 gOutputCycleSampleTime = 0.0;
static bool gOutputCycleSampleTimeValid = false;
static UInt32 gOutputCycleStreamMask = 0;
static bool gOutputCycleTouched = false;

#if OPENA8DJ_ENABLE_INPUT_IO
static Float32 gInputCycleBuffer[kOpenA8DJMaxBufferFrames * kOpenA8DJChannels];
static UInt32 gInputCycleFrames = 0;
static UInt64 gInputCycleCounter = 0;
static bool gInputCycleValid = false;
#endif

enum {
    kOpenA8DJMaxStreamUsageRecords = 32,
    kOpenA8DJAllOutputStreamMask = (1u << kOpenA8DJOutputStreamCount) - 1u
};

static atomic_uint gExpectedOutputStreamMask = kOpenA8DJAllOutputStreamMask;
static const useconds_t kOpenA8DJStopGraceUsec = OPENA8DJ_STOP_GRACE_USEC;

#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
typedef struct {
    bool valid;
    void *ioProc;
    AudioObjectPropertyScope scope;
    UInt32 streamMask;
} OpenA8DJStreamUsageRecord;

static OpenA8DJStreamUsageRecord gStreamUsageRecords[kOpenA8DJMaxStreamUsageRecords];
static pthread_mutex_t gStreamUsageMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void Trace(const char *format, ...)
{
#if OPENA8DJ_ENABLE_TRACE
    FILE *file = fopen("/tmp/opena8dj-hal-trace.log", "a");
    if (file == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
#else
    (void)format;
#endif
}

static void TraceProperty(const char *operation,
                          AudioObjectID objectID,
                          const AudioObjectPropertyAddress *address,
                          OSStatus status)
{
#if OPENA8DJ_ENABLE_TRACE
    static atomic_uint propertyTraceCount = 0;
    if (atomic_fetch_add(&propertyTraceCount, 1) >= 2000) {
        return;
    }
    if (address == NULL) {
        Trace("%s object=%u address=NULL status=%d", operation, objectID, (int)status);
        return;
    }
    char selector[5] = {
        (char)((address->mSelector >> 24) & 0xff),
        (char)((address->mSelector >> 16) & 0xff),
        (char)((address->mSelector >> 8) & 0xff),
        (char)(address->mSelector & 0xff),
        0
    };
    char scope[5] = {
        (char)((address->mScope >> 24) & 0xff),
        (char)((address->mScope >> 16) & 0xff),
        (char)((address->mScope >> 8) & 0xff),
        (char)(address->mScope & 0xff),
        0
    };
    for (int i = 0; i < 4; i++) {
        if (selector[i] < 32 || selector[i] > 126) selector[i] = '.';
        if (scope[i] < 32 || scope[i] > 126) scope[i] = '.';
    }
    Trace("%s object=%u selector=%s scope=%s element=%u status=%d",
          operation,
          objectID,
          selector,
          scope,
          address->mElement,
          (int)status);
#else
    (void)operation;
    (void)objectID;
    (void)status;
    if (address != NULL && OPENA8DJ_PROPERTY_BACKOFF_USEC > 0) {
        static atomic_uint propertyBackoffCount = 0;
        UInt32 count = atomic_fetch_add(&propertyBackoffCount, 1);
        if (count < 2500) {
            usleep(OPENA8DJ_PROPERTY_BACKOFF_USEC);
        }
    } else {
        (void)address;
    }
#endif
}

static HRESULT STDMETHODCALLTYPE OpenA8DJ_QueryInterface(void *inDriver, REFIID inUUID, LPVOID *outInterface);
static ULONG STDMETHODCALLTYPE OpenA8DJ_AddRef(void *inDriver);
static ULONG STDMETHODCALLTYPE OpenA8DJ_Release(void *inDriver);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_CreateDevice(AudioServerPlugInDriverRef inDriver, CFDictionaryRef inDescription, const AudioServerPlugInClientInfo *inClientInfo, AudioObjectID *outDeviceObjectID);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_DestroyDevice(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_AddDeviceClient(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, const AudioServerPlugInClientInfo *inClientInfo);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_RemoveDeviceClient(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, const AudioServerPlugInClientInfo *inClientInfo);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt64 inChangeAction, void *inChangeInfo);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt64 inChangeAction, void *inChangeInfo);
static Boolean STDMETHODCALLTYPE OpenA8DJ_HasProperty(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_IsPropertySettable(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, Boolean *outIsSettable);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_GetPropertyDataSize(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *outDataSize);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_GetPropertyData(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize, UInt32 *outDataSize, void *outData);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_SetPropertyData(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize, const void *inData);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_StartIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_StopIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, Float64 *outSampleTime, UInt64 *outHostTime, UInt64 *outSeed);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_WillDoIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, UInt32 inOperationID, Boolean *outWillDo, Boolean *outWillDoInPlace);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_BeginIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_DoIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, AudioObjectID inStreamObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo, void *ioMainBuffer, void *ioSecondaryBuffer);
static OSStatus STDMETHODCALLTYPE OpenA8DJ_EndIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo);

static AudioServerPlugInDriverInterface gDriverInterface = {
    NULL,
    OpenA8DJ_QueryInterface,
    OpenA8DJ_AddRef,
    OpenA8DJ_Release,
    OpenA8DJ_Initialize,
    OpenA8DJ_CreateDevice,
    OpenA8DJ_DestroyDevice,
    OpenA8DJ_AddDeviceClient,
    OpenA8DJ_RemoveDeviceClient,
    OpenA8DJ_PerformDeviceConfigurationChange,
    OpenA8DJ_AbortDeviceConfigurationChange,
    OpenA8DJ_HasProperty,
    OpenA8DJ_IsPropertySettable,
    OpenA8DJ_GetPropertyDataSize,
    OpenA8DJ_GetPropertyData,
    OpenA8DJ_SetPropertyData,
    OpenA8DJ_StartIO,
    OpenA8DJ_StopIO,
    OpenA8DJ_GetZeroTimeStamp,
    OpenA8DJ_WillDoIOOperation,
    OpenA8DJ_BeginIOOperation,
    OpenA8DJ_DoIOOperation,
    OpenA8DJ_EndIOOperation
};

static AudioServerPlugInDriverInterface *gDriverInterfacePtr = &gDriverInterface;
static AudioServerPlugInDriverRef gDriverRef = &gDriverInterfacePtr;

static AudioStreamBasicDescription MakeASBD(Float64 rate, UInt32 channels)
{
    AudioStreamBasicDescription asbd;
    memset(&asbd, 0, sizeof(asbd));
    asbd.mSampleRate = rate;
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
    asbd.mBytesPerPacket = sizeof(Float32) * channels;
    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerFrame = sizeof(Float32) * channels;
    asbd.mChannelsPerFrame = channels;
    asbd.mBitsPerChannel = 32;
    return asbd;
}

static OSStatus CopyScalar(const void *source, UInt32 sourceSize, UInt32 inDataSize, UInt32 *outDataSize, void *outData)
{
    if (inDataSize < sourceSize) {
        return kAudioHardwareBadPropertySizeError;
    }
    memcpy(outData, source, sourceSize);
    *outDataSize = sourceSize;
    return kAudioHardwareNoError;
}

static OSStatus CopyCFString(CFStringRef string, UInt32 inDataSize, UInt32 *outDataSize, void *outData)
{
    if (inDataSize < sizeof(CFStringRef)) {
        return kAudioHardwareBadPropertySizeError;
    }
    CFRetain(string);
    *((CFStringRef *)outData) = string;
    *outDataSize = sizeof(CFStringRef);
    return kAudioHardwareNoError;
}

static bool IsChannelElement(const AudioObjectPropertyAddress *address)
{
    if (address == NULL || address->mElement < 1 || address->mElement > kOpenA8DJChannels) {
        return false;
    }
    if (address->mScope == kAudioObjectPropertyScopeOutput) {
        return true;
    }
#if OPENA8DJ_ENABLE_INPUT_IO
    if (address->mScope == kAudioObjectPropertyScopeInput) {
        return true;
    }
#endif
    return false;
}

static CFStringRef ChannelElementName(const AudioObjectPropertyAddress *address)
{
    static const CFStringRef inputNames[8] = {
        CFSTR("Input A Left"),
        CFSTR("Input A Right"),
        CFSTR("Input B Left"),
        CFSTR("Input B Right"),
        CFSTR("Input C Left"),
        CFSTR("Input C Right"),
        CFSTR("Input D Left"),
        CFSTR("Input D Right")
    };
    static const CFStringRef outputNames[8] = {
        CFSTR("Output A Left"),
        CFSTR("Output A Right"),
        CFSTR("Output B Left"),
        CFSTR("Output B Right"),
        CFSTR("Output C Left"),
        CFSTR("Output C Right"),
        CFSTR("Output D Left"),
        CFSTR("Output D Right")
    };

    if (!IsChannelElement(address)) {
        return NULL;
    }
    if (address->mScope == kAudioObjectPropertyScopeInput) {
        return inputNames[address->mElement - 1];
    }
    return outputNames[address->mElement - 1];
}

static CFStringRef ChannelElementCategoryName(const AudioObjectPropertyAddress *address)
{
    if (!IsChannelElement(address)) {
        return NULL;
    }
    return address->mScope == kAudioObjectPropertyScopeInput ? CFSTR("Input") : CFSTR("Output");
}

static CFStringRef ChannelElementNumberName(const AudioObjectPropertyAddress *address)
{
    static const CFStringRef numbers[8] = {
        CFSTR("1"),
        CFSTR("2"),
        CFSTR("3"),
        CFSTR("4"),
        CFSTR("5"),
        CFSTR("6"),
        CFSTR("7"),
        CFSTR("8")
    };

    if (!IsChannelElement(address)) {
        return NULL;
    }
    return numbers[address->mElement - 1];
}

#if OPENA8DJ_ENABLE_INPUT_IO
static const AudioObjectID kInputStreamIDs[kOpenA8DJStreamCount] = {
    kOpenA8DJInputStreamAObjectID,
    kOpenA8DJInputStreamBObjectID,
    kOpenA8DJInputStreamCObjectID,
    kOpenA8DJInputStreamDObjectID
};
#endif

static const AudioObjectID kOutputStreamIDs[kOpenA8DJStreamCount] = {
    kOpenA8DJOutputStreamAObjectID,
    kOpenA8DJOutputStreamBObjectID,
    kOpenA8DJOutputStreamCObjectID,
    kOpenA8DJOutputStreamDObjectID
};

static bool IsInputStreamObject(AudioObjectID objectID)
{
#if !OPENA8DJ_ENABLE_INPUT_IO
    (void)objectID;
    return false;
#else
    for (UInt32 i = 0; i < kOpenA8DJInputStreamCount; i++) {
        if (objectID == kInputStreamIDs[i]) {
            return true;
        }
    }
    return false;
#endif
}

static bool IsOutputStreamObject(AudioObjectID objectID)
{
    for (UInt32 i = 0; i < kOpenA8DJOutputStreamCount; i++) {
        if (objectID == kOutputStreamIDs[i]) {
            return true;
        }
    }
    return false;
}

static bool IsStreamObject(AudioObjectID objectID)
{
    return IsInputStreamObject(objectID) || IsOutputStreamObject(objectID);
}

static UInt32 StreamIndex(AudioObjectID objectID)
{
#if OPENA8DJ_ENABLE_INPUT_IO
    for (UInt32 i = 0; i < kOpenA8DJInputStreamCount; i++) {
        if (objectID == kInputStreamIDs[i]) {
            return i;
        }
    }
#endif
    for (UInt32 i = 0; i < kOpenA8DJOutputStreamCount; i++) {
        if (objectID == kOutputStreamIDs[i]) {
            return i;
        }
    }
    return UINT32_MAX;
}

static UInt32 StreamDirection(AudioObjectID objectID)
{
    if (IsOutputStreamObject(objectID)) {
        return 0;
    }
    return IsInputStreamObject(objectID) ? 1 : 0;
}

static UInt32 ActiveInputStreamCount(void)
{
#if OPENA8DJ_ENABLE_INPUT_IO
    return kOpenA8DJInputStreamCount;
#else
    return 0;
#endif
}

static UInt32 ActiveOwnedStreamCount(void)
{
    return ActiveInputStreamCount() + kOpenA8DJOutputStreamCount;
}

static UInt32 OutputChannelsPerStream(void)
{
    return kOpenA8DJChannels / kOpenA8DJOutputStreamCount;
}

static UInt32 InputChannelsPerStream(void)
{
    UInt32 count = ActiveInputStreamCount();
    return count > 0 ? kOpenA8DJChannels / count : 0;
}

static bool IsKnownObject(AudioObjectID objectID)
{
    return objectID == kAudioObjectPlugInObject ||
           objectID == kOpenA8DJDeviceObjectID ||
           IsStreamObject(objectID);
}

static AudioClassID ClassForObject(AudioObjectID objectID)
{
    switch (objectID) {
        case kAudioObjectPlugInObject:
            return kAudioPlugInClassID;
        case kOpenA8DJDeviceObjectID:
            return kAudioDeviceClassID;
        default:
            return IsStreamObject(objectID) ? kAudioStreamClassID : kAudioObjectClassID;
    }
}

static AudioObjectID OwnerForObject(AudioObjectID objectID)
{
    switch (objectID) {
        case kAudioObjectPlugInObject:
            return kAudioObjectUnknown;
        case kOpenA8DJDeviceObjectID:
            return kAudioObjectPlugInObject;
        default:
            return IsStreamObject(objectID) ? kOpenA8DJDeviceObjectID : kAudioObjectUnknown;
    }
}

static bool HasObjectProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *address)
{
    if (!IsKnownObject(objectID)) {
        return false;
    }
    switch (address->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyName:
            return true;
        case kAudioObjectPropertyModelName:
            return objectID == kOpenA8DJDeviceObjectID;
        case kAudioObjectPropertyManufacturer:
            return true;
        case kAudioObjectPropertyElementName:
        case kAudioObjectPropertyElementCategoryName:
        case kAudioObjectPropertyElementNumberName:
            return objectID == kOpenA8DJDeviceObjectID && IsChannelElement(address);
        case kAudioObjectPropertyOwnedObjects:
            return objectID == kAudioObjectPlugInObject ||
                   objectID == kOpenA8DJDeviceObjectID;
        case kAudioObjectPropertySerialNumber:
        case kAudioObjectPropertyFirmwareVersion:
            return objectID == kOpenA8DJDeviceObjectID;
        default:
            return false;
    }
}

static bool HasPluginProperty(const AudioObjectPropertyAddress *address)
{
    switch (address->mSelector) {
        case kAudioPlugInPropertyBundleID:
        case kAudioPlugInPropertyDeviceList:
        case kAudioPlugInPropertyTranslateUIDToDevice:
        case kAudioPlugInPropertyBoxList:
        case kAudioPlugInPropertyClockDeviceList:
            return true;
        default:
            return false;
    }
}

static UInt32 PreferredChannelLayoutSize(AudioObjectPropertyScope scope);
#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
static UInt32 StreamUsageStreamCount(AudioObjectPropertyScope scope);
#endif

static bool HasDeviceProperty(const AudioObjectPropertyAddress *address)
{
    switch (address->mSelector) {
        case kAudioDevicePropertyDeviceUID:
        case kAudioDevicePropertyModelUID:
        case kAudioDevicePropertyTransportType:
        case kAudioDevicePropertyRelatedDevices:
        case kAudioDevicePropertyClockIsStable:
        case kAudioDevicePropertyClockAlgorithm:
        case kAudioDevicePropertyClockDomain:
        case kAudioDevicePropertyDeviceIsAlive:
        case kAudioDevicePropertyIsHidden:
        case kAudioDevicePropertyDeviceIsRunning:
        case kAudioDevicePropertyDeviceIsRunningSomewhere:
        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
        case kAudioDevicePropertyLatency:
        case kAudioDevicePropertyStreams:
        case kAudioDevicePropertyZeroTimeStampPeriod:
        case kAudioDevicePropertySafetyOffset:
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDevicePropertyActualSampleRate:
        case kAudioDevicePropertyAvailableNominalSampleRates:
        case kAudioDevicePropertyBufferSize:
        case kAudioDevicePropertyBufferSizeRange:
        case kAudioDevicePropertyBufferFrameSize:
        case kAudioDevicePropertyBufferFrameSizeRange:
        case kAudioDevicePropertyUsesVariableBufferFrameSizes:
        case kAudioDevicePropertyStreamConfiguration:
#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
        case kAudioDevicePropertyIOProcStreamUsage:
            return StreamUsageStreamCount(address->mScope) > 0;
#endif
        case kAudioDevicePropertyPreferredChannelsForStereo:
        case kAudioObjectPropertyControlList:
            return true;
        case kAudioDevicePropertyPreferredChannelLayout:
            return PreferredChannelLayoutSize(address->mScope) > 0;
        default:
            return false;
    }
}

static bool HasStreamProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *address)
{
    if (!IsStreamObject(objectID)) {
        return false;
    }
    switch (address->mSelector) {
        case kAudioStreamPropertyIsActive:
        case kAudioStreamPropertyDirection:
        case kAudioStreamPropertyTerminalType:
        case kAudioStreamPropertyStartingChannel:
        case kAudioStreamPropertyLatency:
        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyPhysicalFormat:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            return true;
        default:
            return false;
    }
}

static UInt32 StreamConfigurationBufferCount(AudioObjectPropertyScope scope)
{
    if (scope == kAudioObjectPropertyScopeInput) {
        return ActiveInputStreamCount();
    }
    if (scope == kAudioObjectPropertyScopeOutput) {
        return kOpenA8DJOutputStreamCount;
    }
    return ActiveOwnedStreamCount();
}

static UInt32 StreamConfigurationSize(AudioObjectPropertyScope scope)
{
    return offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * StreamConfigurationBufferCount(scope));
}

#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
static UInt32 StreamUsageStreamCount(AudioObjectPropertyScope scope)
{
    if (scope == kAudioObjectPropertyScopeInput) {
        return ActiveInputStreamCount();
    }
    if (scope == kAudioObjectPropertyScopeOutput) {
        return kOpenA8DJOutputStreamCount;
    }
    return ActiveOwnedStreamCount();
}

static UInt32 StreamUsageSize(AudioObjectPropertyScope scope)
{
    return offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) +
           (sizeof(UInt32) * StreamUsageStreamCount(scope));
}

static UInt32 StreamUsageAllMask(AudioObjectPropertyScope scope)
{
    UInt32 count = StreamUsageStreamCount(scope);
    if (count == 0) {
        return 0;
    }
    if (count >= 32) {
        return UINT32_MAX;
    }
    return (1u << count) - 1u;
}

static UInt32 OutputMaskFromStreamUsage(AudioObjectPropertyScope scope, UInt32 mask)
{
    if (scope == kAudioObjectPropertyScopeOutput) {
        return mask & kOpenA8DJAllOutputStreamMask;
    }
    if (scope == kAudioObjectPropertyScopeGlobal) {
#if OPENA8DJ_ENABLE_INPUT_IO
        return (mask >> ActiveInputStreamCount()) & kOpenA8DJAllOutputStreamMask;
#else
        return mask & kOpenA8DJAllOutputStreamMask;
#endif
    }
    return 0;
}
#endif

static OSStatus WriteStreamConfiguration(const AudioObjectPropertyAddress *address,
                                         UInt32 inDataSize,
                                         UInt32 *outDataSize,
                                         void *outData)
{
    AudioObjectPropertyScope scope = address != NULL ? address->mScope : kAudioObjectPropertyScopeGlobal;
    UInt32 bufferCount = StreamConfigurationBufferCount(scope);
    UInt32 size = StreamConfigurationSize(scope);
    if (inDataSize < size) {
        return kAudioHardwareBadPropertySizeError;
    }
    AudioBufferList *abl = (AudioBufferList *)outData;
    abl->mNumberBuffers = bufferCount;
    for (UInt32 i = 0; i < bufferCount; i++) {
        UInt32 channels = kOpenA8DJChannelsPerStream;
        if (scope == kAudioObjectPropertyScopeOutput) {
            channels = OutputChannelsPerStream();
        }
#if OPENA8DJ_ENABLE_INPUT_IO
        if (scope == kAudioObjectPropertyScopeInput ||
            (scope == kAudioObjectPropertyScopeGlobal && i < ActiveInputStreamCount())) {
            channels = InputChannelsPerStream();
        } else if (scope == kAudioObjectPropertyScopeGlobal) {
            channels = OutputChannelsPerStream();
        }
#else
        (void)i;
        if (scope == kAudioObjectPropertyScopeGlobal) {
            channels = OutputChannelsPerStream();
        }
#endif
        abl->mBuffers[i].mNumberChannels = channels;
        abl->mBuffers[i].mDataByteSize = 0;
        abl->mBuffers[i].mData = NULL;
    }
    *outDataSize = size;
    return kAudioHardwareNoError;
}

static UInt32 PreferredChannelLayoutChannelCount(AudioObjectPropertyScope scope)
{
    if (scope == kAudioObjectPropertyScopeInput) {
#if OPENA8DJ_ENABLE_INPUT_IO
        return kOpenA8DJChannels;
#else
        return 0;
#endif
    }
    return kOpenA8DJChannels;
}

static UInt32 PreferredChannelLayoutSize(AudioObjectPropertyScope scope)
{
    UInt32 channels = PreferredChannelLayoutChannelCount(scope);
    if (channels == 0) {
        return 0;
    }
    return (UInt32)offsetof(AudioChannelLayout, mChannelDescriptions) +
           (sizeof(AudioChannelDescription) * channels);
}

static OSStatus WritePreferredChannelLayout(const AudioObjectPropertyAddress *address,
                                            UInt32 inDataSize,
                                            UInt32 *outDataSize,
                                            void *outData)
{
    AudioObjectPropertyScope scope = address != NULL ? address->mScope : kAudioObjectPropertyScopeGlobal;
    UInt32 channels = PreferredChannelLayoutChannelCount(scope);
    UInt32 size = PreferredChannelLayoutSize(scope);
    if (channels == 0 || size == 0) {
        *outDataSize = 0;
        return kAudioHardwareUnknownPropertyError;
    }
    if (inDataSize < size) {
        return kAudioHardwareBadPropertySizeError;
    }
    AudioChannelLayout *layout = (AudioChannelLayout *)outData;
    memset(layout, 0, size);
    layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    layout->mNumberChannelDescriptions = channels;
    for (UInt32 i = 0; i < channels; i++) {
        layout->mChannelDescriptions[i].mChannelLabel = kAudioChannelLabel_Discrete_0 + i;
    }
    *outDataSize = size;
    return kAudioHardwareNoError;
}

#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
static UInt32 GetStreamUsageMask(void *ioProc, AudioObjectPropertyScope scope)
{
    UInt32 mask = StreamUsageAllMask(scope);
    pthread_mutex_lock(&gStreamUsageMutex);
    for (UInt32 i = 0; i < kOpenA8DJMaxStreamUsageRecords; i++) {
        OpenA8DJStreamUsageRecord *record = &gStreamUsageRecords[i];
        if (record->valid && record->ioProc == ioProc && record->scope == scope) {
            mask = record->streamMask;
            break;
        }
    }
    pthread_mutex_unlock(&gStreamUsageMutex);
    return mask;
}

static void SetStreamUsageMask(void *ioProc, AudioObjectPropertyScope scope, UInt32 mask)
{
    UInt32 normalizedMask = mask & StreamUsageAllMask(scope);
    pthread_mutex_lock(&gStreamUsageMutex);
    OpenA8DJStreamUsageRecord *slot = NULL;
    for (UInt32 i = 0; i < kOpenA8DJMaxStreamUsageRecords; i++) {
        OpenA8DJStreamUsageRecord *record = &gStreamUsageRecords[i];
        if (record->valid && record->ioProc == ioProc && record->scope == scope) {
            slot = record;
            break;
        }
        if (!record->valid && slot == NULL) {
            slot = record;
        }
    }
    if (slot != NULL) {
        slot->valid = true;
        slot->ioProc = ioProc;
        slot->scope = scope;
        slot->streamMask = normalizedMask;
    }
    pthread_mutex_unlock(&gStreamUsageMutex);
    if (scope == kAudioObjectPropertyScopeOutput ||
        scope == kAudioObjectPropertyScopeGlobal) {
        atomic_store(&gExpectedOutputStreamMask, OutputMaskFromStreamUsage(scope, normalizedMask));
    }
}

static OSStatus WriteStreamUsage(const AudioObjectPropertyAddress *address,
                                 UInt32 inDataSize,
                                 UInt32 *outDataSize,
                                 void *outData)
{
    AudioObjectPropertyScope scope = address != NULL ? address->mScope : kAudioObjectPropertyScopeGlobal;
    UInt32 streamCount = StreamUsageStreamCount(scope);
    UInt32 size = StreamUsageSize(scope);
    if (inDataSize < size) {
        return kAudioHardwareBadPropertySizeError;
    }
    AudioHardwareIOProcStreamUsage *usage = (AudioHardwareIOProcStreamUsage *)outData;
    UInt32 mask = GetStreamUsageMask(usage->mIOProc, scope);
    usage->mNumberStreams = streamCount;
    for (UInt32 i = 0; i < streamCount; i++) {
        usage->mStreamIsOn[i] = (mask & (1u << i)) ? 1 : 0;
    }
    *outDataSize = size;
    return kAudioHardwareNoError;
}
#endif

static OSStatus GetObjectPropertyDataSize(AudioObjectID objectID, const AudioObjectPropertyAddress *address, UInt32 *outDataSize)
{
    switch (address->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
            *outDataSize = sizeof(UInt32);
            return kAudioHardwareNoError;
        case kAudioObjectPropertyName:
        case kAudioObjectPropertyModelName:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertySerialNumber:
        case kAudioObjectPropertyFirmwareVersion:
        case kAudioObjectPropertyElementName:
        case kAudioObjectPropertyElementCategoryName:
        case kAudioObjectPropertyElementNumberName:
            *outDataSize = sizeof(CFStringRef);
            return kAudioHardwareNoError;
        case kAudioObjectPropertyOwnedObjects:
            if (objectID == kAudioObjectPlugInObject) {
                *outDataSize = sizeof(AudioObjectID);
            } else if (objectID == kOpenA8DJDeviceObjectID) {
                *outDataSize = sizeof(AudioObjectID) * ActiveOwnedStreamCount();
            } else {
                *outDataSize = 0;
            }
            return kAudioHardwareNoError;
        default:
            return kAudioHardwareUnknownPropertyError;
    }
}

static OSStatus GetPluginPropertyDataSize(const AudioObjectPropertyAddress *address, UInt32 *outDataSize)
{
    switch (address->mSelector) {
        case kAudioPlugInPropertyBundleID:
            *outDataSize = sizeof(CFStringRef);
            return kAudioHardwareNoError;
        case kAudioPlugInPropertyDeviceList:
            *outDataSize = sizeof(AudioObjectID);
            return kAudioHardwareNoError;
        case kAudioPlugInPropertyTranslateUIDToDevice:
            *outDataSize = sizeof(AudioObjectID);
            return kAudioHardwareNoError;
        case kAudioPlugInPropertyBoxList:
        case kAudioPlugInPropertyClockDeviceList:
            *outDataSize = 0;
            return kAudioHardwareNoError;
        default:
            return kAudioHardwareUnknownPropertyError;
    }
}

static OSStatus GetDevicePropertyDataSize(const AudioObjectPropertyAddress *address, UInt32 *outDataSize)
{
    switch (address->mSelector) {
        case kAudioDevicePropertyDeviceUID:
        case kAudioDevicePropertyModelUID:
            *outDataSize = sizeof(CFStringRef);
            return kAudioHardwareNoError;
        case kAudioDevicePropertyRelatedDevices:
            *outDataSize = sizeof(AudioObjectID);
            return kAudioHardwareNoError;
        case kAudioDevicePropertyStreams:
            if (address->mScope == kAudioObjectPropertyScopeInput ||
                address->mScope == kAudioObjectPropertyScopeOutput) {
                UInt32 count = address->mScope == kAudioObjectPropertyScopeInput ?
                    ActiveInputStreamCount() : kOpenA8DJOutputStreamCount;
                *outDataSize = sizeof(AudioObjectID) * count;
                return kAudioHardwareNoError;
            }
            *outDataSize = sizeof(AudioObjectID) * ActiveOwnedStreamCount();
            return kAudioHardwareNoError;
        case kAudioDevicePropertyAvailableNominalSampleRates:
            *outDataSize = sizeof(AudioValueRange) * kOpenA8DJSupportedRateCount;
            return kAudioHardwareNoError;
        case kAudioDevicePropertyStreamConfiguration:
            *outDataSize = StreamConfigurationSize(address->mScope);
            return kAudioHardwareNoError;
#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
        case kAudioDevicePropertyIOProcStreamUsage:
            *outDataSize = StreamUsageSize(address->mScope);
            return kAudioHardwareNoError;
#endif
        case kAudioDevicePropertyPreferredChannelsForStereo:
            *outDataSize = sizeof(UInt32) * 2;
            return kAudioHardwareNoError;
        case kAudioDevicePropertyPreferredChannelLayout:
            *outDataSize = PreferredChannelLayoutSize(address->mScope);
            return *outDataSize > 0 ? kAudioHardwareNoError : kAudioHardwareUnknownPropertyError;
        case kAudioObjectPropertyControlList:
            *outDataSize = 0;
            return kAudioHardwareNoError;
        default:
            *outDataSize = sizeof(UInt32);
            if (address->mSelector == kAudioDevicePropertyNominalSampleRate ||
                address->mSelector == kAudioDevicePropertyActualSampleRate) {
                *outDataSize = sizeof(Float64);
            } else if (address->mSelector == kAudioDevicePropertyBufferFrameSizeRange ||
                       address->mSelector == kAudioDevicePropertyBufferSizeRange) {
                *outDataSize = sizeof(AudioValueRange);
            }
            return kAudioHardwareNoError;
    }
}

static OSStatus GetStreamPropertyDataSize(const AudioObjectPropertyAddress *address, UInt32 *outDataSize)
{
    switch (address->mSelector) {
        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            *outDataSize = sizeof(AudioStreamBasicDescription);
            return kAudioHardwareNoError;
        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            *outDataSize = sizeof(AudioStreamRangedDescription) * kOpenA8DJSupportedRateCount;
            return kAudioHardwareNoError;
        default:
            *outDataSize = sizeof(UInt32);
            return kAudioHardwareNoError;
    }
}

static void FillRates(AudioValueRange *rates)
{
    rates[0] = (AudioValueRange){44100.0, 44100.0};
    rates[1] = (AudioValueRange){48000.0, 48000.0};
    rates[2] = (AudioValueRange){88200.0, 88200.0};
    rates[3] = (AudioValueRange){96000.0, 96000.0};
}

static void FillFormats(AudioStreamRangedDescription *formats, UInt32 channels)
{
    AudioValueRange rates[kOpenA8DJSupportedRateCount];
    FillRates(rates);
    for (UInt32 i = 0; i < kOpenA8DJSupportedRateCount; i++) {
        formats[i].mFormat = MakeASBD(rates[i].mMinimum, channels);
        formats[i].mSampleRateRange = rates[i];
    }
}

static UInt32 BytesPerOutputFrame(void)
{
    return sizeof(Float32) * kOpenA8DJChannels;
}

static UInt32 BufferBytesForFrames(UInt32 frames)
{
    return frames * BytesPerOutputFrame();
}

static UInt32 BufferFramesForBytes(UInt32 bytes)
{
    UInt32 bytesPerFrame = BytesPerOutputFrame();
    return bytesPerFrame > 0 ? (bytes + bytesPerFrame - 1) / bytesPerFrame : 0;
}

static UInt32 NormalizeBufferFrames(UInt32 frames)
{
    if (frames <= 512) return 512;
    if (frames <= 1024) return 1024;
    if (frames <= 2048) return 2048;
    return 4096;
}

static bool IsSupportedRate(Float64 rate)
{
    return rate == 44100.0 || rate == 48000.0 || rate == 88200.0 || rate == 96000.0;
}

static UInt32 RecommendedBufferFramesForRate(Float64 rate)
{
    (void)rate;
    return kOpenA8DJPreferredBufferFrames;
}

static UInt64 CycleCounterFromInfo(const AudioServerPlugInIOCycleInfo *cycleInfo)
{
    return cycleInfo != NULL ? cycleInfo->mIOCycleCounter : 0;
}

static bool OutputSampleTimeFromInfo(const AudioServerPlugInIOCycleInfo *cycleInfo, Float64 *outSampleTime)
{
#if OPENA8DJ_IGNORE_OUTPUT_SAMPLE_TIME
    (void)cycleInfo;
    (void)outSampleTime;
    return false;
#else
    if (cycleInfo == NULL || outSampleTime == NULL) {
        return false;
    }
    if ((cycleInfo->mOutputTime.mFlags & kAudioTimeStampSampleTimeValid) == 0) {
        return false;
    }
    *outSampleTime = cycleInfo->mOutputTime.mSampleTime;
    return isfinite(*outSampleTime);
#endif
}

static void FlushOutputCycle(void);

static void ResetOutputCycle(UInt32 frameCount,
                             UInt64 cycleCounter,
                             Float64 outputSampleTime,
                             bool outputSampleTimeValid)
{
    gOutputCycleFrames = frameCount <= kOpenA8DJMaxBufferFrames ? frameCount : kOpenA8DJMaxBufferFrames;
    memset(gOutputCycleBuffer, 0, (size_t)gOutputCycleFrames * kOpenA8DJChannels * sizeof(Float32));
    gOutputCycleCounter = cycleCounter;
    gOutputCycleSampleTime = outputSampleTime;
    gOutputCycleSampleTimeValid = outputSampleTimeValid;
    gOutputCycleStreamMask = 0;
    gOutputCycleTouched = false;
}

static void EnsureOutputCycle(UInt32 frameCount, const AudioServerPlugInIOCycleInfo *cycleInfo)
{
    UInt64 cycleCounter = CycleCounterFromInfo(cycleInfo);
    Float64 outputSampleTime = 0.0;
    bool outputSampleTimeValid = OutputSampleTimeFromInfo(cycleInfo, &outputSampleTime);
    if (cycleCounter != 0 && gOutputCycleTouched && gOutputCycleCounter != 0 && gOutputCycleCounter != cycleCounter) {
        FlushOutputCycle();
    }
    if (gOutputCycleFrames == 0 ||
        (cycleCounter != 0 && gOutputCycleCounter != cycleCounter)) {
        ResetOutputCycle(frameCount, cycleCounter, outputSampleTime, outputSampleTimeValid);
    }
}

static void CopyClientOutputToOutput(UInt32 streamIndex, const Float32 *inInterleaved)
{
    if (streamIndex >= kOpenA8DJOutputStreamCount || inInterleaved == NULL) {
        return;
    }
    UInt32 sourceChannels = OutputChannelsPerStream();
    UInt32 startChannel = streamIndex * kOpenA8DJChannelsPerStream;
    for (UInt32 frame = 0; frame < gOutputCycleFrames; frame++) {
        const Float32 *src = &inInterleaved[(size_t)frame * sourceChannels];
        Float32 *dst = &gOutputCycleBuffer[(size_t)frame * kOpenA8DJChannels];
        if (kOpenA8DJOutputStreamCount == 1) {
            memcpy(dst, src, kOpenA8DJChannels * sizeof(Float32));
        } else {
            dst[startChannel] = src[0];
            dst[startChannel + 1] = src[1];
        }
    }
    gOutputCycleStreamMask |= (1u << streamIndex);
    gOutputCycleTouched = true;
}

#if OPENA8DJ_ENABLE_INPUT_IO
static void PrepareInputCycle(UInt32 frameCount, UInt64 cycleCounter)
{
    OpenA8DJUSBSetInputDecodeActive(true);
    UInt32 clampedFrames = frameCount <= kOpenA8DJMaxBufferFrames ? frameCount : kOpenA8DJMaxBufferFrames;
    if (cycleCounter == 0) {
        gInputCycleFrames = clampedFrames;
        gInputCycleValid = false;
        return;
    }
    if (gInputCycleFrames != clampedFrames || gInputCycleCounter != cycleCounter) {
        gInputCycleFrames = clampedFrames;
        gInputCycleCounter = cycleCounter;
        gInputCycleValid = false;
    }
}

static void EnsureInputCycle(void)
{
    if (gInputCycleValid) {
        return;
    }
    if (gInputCycleFrames == 0) {
        return;
    }
    (void)OpenA8DJUSBReadInput(gInputCycleBuffer, gInputCycleFrames, kOpenA8DJChannels);
    gInputCycleValid = true;
}

static void CopyInputStreamToClient(UInt32 streamIndex, Float32 *outInterleaved)
{
    UInt32 channels = InputChannelsPerStream();
    if (streamIndex >= ActiveInputStreamCount() || outInterleaved == NULL || channels == 0) {
        return;
    }
    EnsureInputCycle();
    UInt32 startChannel = streamIndex * channels;
    for (UInt32 frame = 0; frame < gInputCycleFrames; frame++) {
        const Float32 *src = &gInputCycleBuffer[(size_t)frame * kOpenA8DJChannels + startChannel];
        Float32 *dst = &outInterleaved[(size_t)frame * channels];
        for (UInt32 channel = 0; channel < channels; channel++) {
            dst[channel] = src[channel];
        }
    }
}
#endif

static void FlushOutputCycle(void)
{
    if (!gOutputCycleTouched || gOutputCycleFrames == 0) {
        return;
    }
    OpenA8DJUSBWriteOutputAtSampleTime(gOutputCycleBuffer,
                                       gOutputCycleFrames,
                                       kOpenA8DJChannels,
                                       gOutputCycleSampleTime,
                                       gOutputCycleSampleTimeValid);
    gOutputCycleFrames = 0;
    gOutputCycleCounter = 0;
    gOutputCycleSampleTime = 0.0;
    gOutputCycleSampleTimeValid = false;
    gOutputCycleStreamMask = 0;
    gOutputCycleTouched = false;
}

static bool OutputCycleHasExpectedStreams(void)
{
    UInt32 expectedMask = atomic_load(&gExpectedOutputStreamMask) & kOpenA8DJAllOutputStreamMask;
    if (expectedMask == 0) {
        expectedMask = kOpenA8DJAllOutputStreamMask;
    }
    return (gOutputCycleStreamMask & expectedMask) == expectedMask;
}

static Float64 HostTicksPerFrame(Float64 sampleRate)
{
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        (void)mach_timebase_info(&timebase);
    }
    if (sampleRate <= 0.0 || timebase.numer == 0) {
        return 0.0;
    }
    Float64 nsPerFrame = 1000000000.0 / sampleRate;
    return nsPerFrame * (Float64)timebase.denom / (Float64)timebase.numer;
}

static void StartClock(void)
{
    if (atomic_exchange(&gClockRunning, true)) {
        return;
    }
    pthread_mutex_lock(&gClockMutex);
    gSampleTime = 0.0;
    gHostTime = mach_absolute_time();
    gZeroTimeStampSeed++;
    pthread_mutex_unlock(&gClockMutex);
}

static void StopClock(void)
{
    atomic_store(&gClockRunning, false);
}

#if OPENA8DJ_BACKGROUND_WARM_OPEN
static void *BackgroundWarmOpenThread(void *arg)
{
    unsigned int generation = (unsigned int)(uintptr_t)arg;
    pthread_mutex_lock(&gIOMutex);
    if (gRunningClients == 0 && atomic_load(&gStopGeneration) == generation) {
        if (OpenA8DJUSBEnsureOpen(gSampleRate)) {
            Trace("StopIO USB warm-opened in background");
        } else {
            Trace("StopIO USB warm-open in background failed");
        }
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
    }
    pthread_mutex_unlock(&gIOMutex);
    return NULL;
}

static void ScheduleBackgroundWarmOpen(void)
{
    unsigned int generation = atomic_fetch_add(&gStopGeneration, 1) + 1;
    pthread_t thread;
    if (pthread_create(&thread, NULL, BackgroundWarmOpenThread, (void *)(uintptr_t)generation) == 0) {
        pthread_detach(thread);
    } else {
        if (OpenA8DJUSBEnsureOpen(gSampleRate)) {
            Trace("StopIO USB warm-opened after background thread failure");
        }
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
    }
}
#else
static void *DelayedCloseThread(void *arg)
{
    unsigned int generation = (unsigned int)(uintptr_t)arg;
    usleep(kOpenA8DJStopGraceUsec);
    pthread_mutex_lock(&gIOMutex);
    if (gRunningClients == 0 && atomic_load(&gStopGeneration) == generation) {
        FlushOutputCycle();
        OpenA8DJUSBSetInputDecodeActive(false);
#if OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
        Trace("StopIO USB kept open after grace");
#else
        OpenA8DJUSBClose();
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
        Trace("StopIO USB closed after grace");
#endif
    }
    pthread_mutex_unlock(&gIOMutex);
    return NULL;
}

static void ScheduleDelayedClose(void)
{
    unsigned int generation = atomic_fetch_add(&gStopGeneration, 1) + 1;
    pthread_t thread;
    if (pthread_create(&thread, NULL, DelayedCloseThread, (void *)(uintptr_t)generation) == 0) {
        pthread_detach(thread);
    } else {
        OpenA8DJUSBSetInputDecodeActive(false);
#if OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
        Trace("StopIO USB kept open after grace thread failure");
#else
        OpenA8DJUSBClose();
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
        Trace("StopIO USB closed immediately after grace thread failure");
#endif
    }
}
#endif

#if OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT
static void *BackgroundPreopenThread(void *arg)
{
    unsigned int generation = (unsigned int)(uintptr_t)arg;
    pthread_mutex_lock(&gIOMutex);
    if (gRunningClients == 0 && atomic_load(&gStopGeneration) == generation) {
        if (OpenA8DJUSBEnsureOpen(gSampleRate)) {
            Trace("Initialize USB pre-opened in background");
        } else {
            Trace("Initialize USB pre-open in background failed");
        }
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
    }
    pthread_mutex_unlock(&gIOMutex);
    return NULL;
}

static void ScheduleBackgroundPreopen(void)
{
    unsigned int generation = atomic_load(&gStopGeneration);
    pthread_t thread;
    if (pthread_create(&thread, NULL, BackgroundPreopenThread, (void *)(uintptr_t)generation) == 0) {
        pthread_detach(thread);
    } else {
        pthread_mutex_lock(&gIOMutex);
        if (gRunningClients == 0 && atomic_load(&gStopGeneration) == generation) {
            if (OpenA8DJUSBEnsureOpen(gSampleRate)) {
                Trace("Initialize USB pre-opened after background thread failure");
            }
            atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
        }
        pthread_mutex_unlock(&gIOMutex);
    }
}
#endif

static void NotifySampleRateChanged(void)
{
    return;
    if (gHost == NULL) {
        return;
    }
    AudioObjectPropertyAddress deviceChanged[] = {
        {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
        {kAudioDevicePropertyActualSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain}
    };
    AudioObjectPropertyAddress streamChanged[] = {
        {kAudioStreamPropertyVirtualFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
        {kAudioStreamPropertyPhysicalFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain}
    };
    gHost->PropertiesChanged(gHost, kOpenA8DJDeviceObjectID, 2, deviceChanged);
    for (UInt32 i = 0; i < kOpenA8DJOutputStreamCount; i++) {
        gHost->PropertiesChanged(gHost, kOutputStreamIDs[i], 2, streamChanged);
    }
#if OPENA8DJ_ENABLE_INPUT_IO
    for (UInt32 i = 0; i < ActiveInputStreamCount(); i++) {
        gHost->PropertiesChanged(gHost, kInputStreamIDs[i], 2, streamChanged);
    }
#endif
}

static void ApplySampleRate(Float64 newRate)
{
    bool changed = false;
    pthread_mutex_lock(&gClockMutex);
    if (gSampleRate != newRate) {
        gSampleRate = newRate;
        gBufferFrames = RecommendedBufferFramesForRate(newRate);
        gSampleTime = 0.0;
        gHostTime = mach_absolute_time();
        gZeroTimeStampSeed++;
        changed = true;
    }
    pthread_mutex_unlock(&gClockMutex);
    if (changed) {
        NotifySampleRateChanged();
    }
}

static void NotifyBufferFrameSizeChanged(void)
{
    return;
    if (gHost == NULL) {
        return;
    }
    AudioObjectPropertyAddress changed[] = {
        {kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
        {kAudioDevicePropertyZeroTimeStampPeriod, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain}
    };
    gHost->PropertiesChanged(gHost, kOpenA8DJDeviceObjectID, 2, changed);
}

static void ApplyBufferFrameSize(UInt32 newSize)
{
    bool changed = false;
    pthread_mutex_lock(&gClockMutex);
    if (gBufferFrames != newSize) {
        gBufferFrames = newSize;
        gZeroTimeStampSeed++;
        changed = true;
    }
    pthread_mutex_unlock(&gClockMutex);
    if (changed) {
        NotifyBufferFrameSizeChanged();
    }
}

static HRESULT STDMETHODCALLTYPE OpenA8DJ_QueryInterface(void *inDriver, REFIID inUUID, LPVOID *outInterface)
{
    (void)inDriver;
    Trace("QueryInterface");
    if (outInterface == NULL) {
        return E_POINTER;
    }
    CFUUIDRef requestedUUID = CFUUIDCreateFromUUIDBytes(NULL, inUUID);
    bool matches = CFEqual(requestedUUID, kAudioServerPlugInDriverInterfaceUUID) ||
                   CFEqual(requestedUUID, IUnknownUUID);
    CFRelease(requestedUUID);
    if (!matches) {
        *outInterface = NULL;
        return E_NOINTERFACE;
    }
    *outInterface = gDriverRef;
    OpenA8DJ_AddRef(inDriver);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE OpenA8DJ_AddRef(void *inDriver)
{
    (void)inDriver;
    return atomic_fetch_add(&gRefCount, 1) + 1;
}

static ULONG STDMETHODCALLTYPE OpenA8DJ_Release(void *inDriver)
{
    (void)inDriver;
    UInt32 oldValue = atomic_fetch_sub(&gRefCount, 1);
    return oldValue > 0 ? oldValue - 1 : 0;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost)
{
    (void)inDriver;
    Trace("Initialize host=%p", inHost);
    gHost = inHost;
    pthread_mutex_lock(&gClockMutex);
    gSampleTime = 0.0;
    gHostTime = mach_absolute_time();
    gBufferFrames = RecommendedBufferFramesForRate(gSampleRate);
    pthread_mutex_unlock(&gClockMutex);
    atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
#if OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT
    ScheduleBackgroundPreopen();
#endif
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_CreateDevice(AudioServerPlugInDriverRef inDriver, CFDictionaryRef inDescription, const AudioServerPlugInClientInfo *inClientInfo, AudioObjectID *outDeviceObjectID)
{
    (void)inDriver;
    (void)inDescription;
    (void)inClientInfo;
    Trace("CreateDevice");
    if (outDeviceObjectID == NULL) {
        return kAudioHardwareBadPropertySizeError;
    }
    *outDeviceObjectID = kOpenA8DJDeviceObjectID;
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_DestroyDevice(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID)
{
    (void)inDriver;
    return inDeviceObjectID == kOpenA8DJDeviceObjectID ? kAudioHardwareNoError : kAudioHardwareBadDeviceError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_AddDeviceClient(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, const AudioServerPlugInClientInfo *inClientInfo)
{
    (void)inDriver;
    (void)inDeviceObjectID;
    (void)inClientInfo;
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_RemoveDeviceClient(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, const AudioServerPlugInClientInfo *inClientInfo)
{
    (void)inDriver;
    (void)inDeviceObjectID;
    (void)inClientInfo;
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt64 inChangeAction, void *inChangeInfo)
{
    (void)inDriver;
    (void)inChangeInfo;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    if (inChangeAction == kOpenA8DJConfigChangeSampleRate) {
        Float64 newRate = 0.0;
        pthread_mutex_lock(&gClockMutex);
        newRate = gPendingSampleRate;
        gPendingSampleRate = 0.0;
        pthread_mutex_unlock(&gClockMutex);
        if (newRate > 0.0) {
            ApplySampleRate(newRate);
            if (gRunningClients > 0) {
                (void)OpenA8DJUSBSetSampleRate(newRate);
            }
        }
        return kAudioHardwareNoError;
    }
    if (inChangeAction == kOpenA8DJConfigChangeBufferFrames) {
        UInt32 newSize = 0;
        pthread_mutex_lock(&gClockMutex);
        newSize = gPendingBufferFrames;
        gPendingBufferFrames = 0;
        pthread_mutex_unlock(&gClockMutex);
        if (newSize > 0) {
            ApplyBufferFrameSize(newSize);
        }
        return kAudioHardwareNoError;
    }
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt64 inChangeAction, void *inChangeInfo)
{
    (void)inDriver;
    (void)inChangeInfo;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    pthread_mutex_lock(&gClockMutex);
    if (inChangeAction == kOpenA8DJConfigChangeSampleRate) {
        gPendingSampleRate = 0.0;
    } else if (inChangeAction == kOpenA8DJConfigChangeBufferFrames) {
        gPendingBufferFrames = 0;
    }
    pthread_mutex_unlock(&gClockMutex);
    return kAudioHardwareNoError;
}

static Boolean STDMETHODCALLTYPE OpenA8DJ_HasProperty(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress)
{
    (void)inDriver;
    (void)inClientProcessID;
    if (inAddress == NULL) {
        TraceProperty("HasProperty", inObjectID, inAddress, 0);
        return false;
    }
    Boolean result = HasObjectProperty(inObjectID, inAddress) ||
                     (inObjectID == kAudioObjectPlugInObject && HasPluginProperty(inAddress)) ||
                     (inObjectID == kOpenA8DJDeviceObjectID && HasDeviceProperty(inAddress)) ||
                     HasStreamProperty(inObjectID, inAddress);
    TraceProperty(result ? "HasProperty=yes" : "HasProperty=no", inObjectID, inAddress, 0);
    return result;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_IsPropertySettable(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, Boolean *outIsSettable)
{
    (void)inDriver;
    (void)inClientProcessID;
    if (outIsSettable == NULL || inAddress == NULL) {
        return kAudioHardwareBadPropertySizeError;
    }
    if (!IsKnownObject(inObjectID)) {
        return kAudioHardwareBadObjectError;
    }
    bool isSettable = (inObjectID == kOpenA8DJDeviceObjectID &&
                       (inAddress->mSelector == kAudioDevicePropertyNominalSampleRate ||
                        inAddress->mSelector == kAudioDevicePropertyBufferFrameSize ||
                        inAddress->mSelector == kAudioDevicePropertyBufferSize));
#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
    if (inObjectID == kOpenA8DJDeviceObjectID &&
        inAddress->mSelector == kAudioDevicePropertyIOProcStreamUsage &&
        StreamUsageStreamCount(inAddress->mScope) > 0) {
        isSettable = true;
    }
#endif
    *outIsSettable = isSettable;
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_GetPropertyDataSize(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *outDataSize)
{
    (void)inDriver;
    (void)inClientProcessID;
    (void)inQualifierDataSize;
    (void)inQualifierData;
    if (inAddress == NULL || outDataSize == NULL) {
        TraceProperty("GetSize bad", inObjectID, inAddress, kAudioHardwareBadPropertySizeError);
        return kAudioHardwareBadPropertySizeError;
    }
    if (!IsKnownObject(inObjectID)) {
        TraceProperty("GetSize bad-object", inObjectID, inAddress, kAudioHardwareBadObjectError);
        return kAudioHardwareBadObjectError;
    }
    TraceProperty("GetSize", inObjectID, inAddress, 0);
    if (HasObjectProperty(inObjectID, inAddress)) {
        return GetObjectPropertyDataSize(inObjectID, inAddress, outDataSize);
    }
    if (inObjectID == kAudioObjectPlugInObject && HasPluginProperty(inAddress)) {
        return GetPluginPropertyDataSize(inAddress, outDataSize);
    }
    if (inObjectID == kOpenA8DJDeviceObjectID && HasDeviceProperty(inAddress)) {
        return GetDevicePropertyDataSize(inAddress, outDataSize);
    }
    if (HasStreamProperty(inObjectID, inAddress)) {
        return GetStreamPropertyDataSize(inAddress, outDataSize);
    }
    return kAudioHardwareUnknownPropertyError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_GetPropertyData(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize, UInt32 *outDataSize, void *outData)
{
    (void)inDriver;
    (void)inClientProcessID;
    if (inAddress == NULL || outDataSize == NULL || outData == NULL) {
        TraceProperty("GetData bad", inObjectID, inAddress, kAudioHardwareBadPropertySizeError);
        return kAudioHardwareBadPropertySizeError;
    }
    if (!IsKnownObject(inObjectID)) {
        TraceProperty("GetData bad-object", inObjectID, inAddress, kAudioHardwareBadObjectError);
        return kAudioHardwareBadObjectError;
    }
    TraceProperty("GetData", inObjectID, inAddress, 0);

    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass: {
            AudioClassID classID = ClassForObject(inObjectID);
            if (classID == kAudioPlugInClassID) classID = kAudioPlugInClassID;
            else if (classID == kAudioDeviceClassID) classID = kAudioDeviceClassID;
            else if (classID == kAudioStreamClassID) classID = kAudioStreamClassID;
            return CopyScalar(&classID, sizeof(classID), inDataSize, outDataSize, outData);
        }
        case kAudioObjectPropertyClass: {
            AudioClassID classID = ClassForObject(inObjectID);
            return CopyScalar(&classID, sizeof(classID), inDataSize, outDataSize, outData);
        }
        case kAudioObjectPropertyOwner: {
            AudioObjectID owner = OwnerForObject(inObjectID);
            return CopyScalar(&owner, sizeof(owner), inDataSize, outDataSize, outData);
        }
        case kAudioObjectPropertyName:
            if (inObjectID == kOpenA8DJInputStreamAObjectID) return CopyCFString(CFSTR("Audio 8 DJ Input A"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJInputStreamBObjectID) return CopyCFString(CFSTR("Audio 8 DJ Input B"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJInputStreamCObjectID) return CopyCFString(CFSTR("Audio 8 DJ Input C"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJInputStreamDObjectID) return CopyCFString(CFSTR("Audio 8 DJ Input D"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJOutputStreamAObjectID) return CopyCFString(CFSTR("Audio 8 DJ Output A"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJOutputStreamBObjectID) return CopyCFString(CFSTR("Audio 8 DJ Output B"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJOutputStreamCObjectID) return CopyCFString(CFSTR("Audio 8 DJ Output C"), inDataSize, outDataSize, outData);
            if (inObjectID == kOpenA8DJOutputStreamDObjectID) return CopyCFString(CFSTR("Audio 8 DJ Output D"), inDataSize, outDataSize, outData);
            return CopyCFString(kDeviceName, inDataSize, outDataSize, outData);
        case kAudioObjectPropertyModelName:
            return CopyCFString(CFSTR("Audio 8 DJ"), inDataSize, outDataSize, outData);
        case kAudioObjectPropertyManufacturer:
            return CopyCFString(kManufacturer, inDataSize, outDataSize, outData);
        case kAudioObjectPropertyElementName: {
            CFStringRef name = ChannelElementName(inAddress);
            if (inObjectID == kOpenA8DJDeviceObjectID && name != NULL) {
                return CopyCFString(name, inDataSize, outDataSize, outData);
            }
            return kAudioHardwareUnknownPropertyError;
        }
        case kAudioObjectPropertyElementCategoryName: {
            CFStringRef name = ChannelElementCategoryName(inAddress);
            if (inObjectID == kOpenA8DJDeviceObjectID && name != NULL) {
                return CopyCFString(name, inDataSize, outDataSize, outData);
            }
            return kAudioHardwareUnknownPropertyError;
        }
        case kAudioObjectPropertyElementNumberName: {
            CFStringRef name = ChannelElementNumberName(inAddress);
            if (inObjectID == kOpenA8DJDeviceObjectID && name != NULL) {
                return CopyCFString(name, inDataSize, outDataSize, outData);
            }
            return kAudioHardwareUnknownPropertyError;
        }
        case kAudioObjectPropertySerialNumber:
            return CopyCFString(CFSTR("17cc:1978"), inDataSize, outDataSize, outData);
        case kAudioObjectPropertyFirmwareVersion:
            return CopyCFString(CFSTR("14"), inDataSize, outDataSize, outData);
        case kAudioObjectPropertyOwnedObjects: {
            if (inObjectID == kAudioObjectPlugInObject) {
                AudioObjectID ids[] = {kOpenA8DJDeviceObjectID};
                return CopyScalar(ids, sizeof(ids), inDataSize, outDataSize, outData);
            }
            if (inObjectID == kOpenA8DJDeviceObjectID) {
                AudioObjectID ids[kOpenA8DJStreamCount + kOpenA8DJOutputStreamCount];
                UInt32 count = 0;
#if OPENA8DJ_ENABLE_INPUT_IO
                memcpy(ids, kInputStreamIDs, sizeof(AudioObjectID) * ActiveInputStreamCount());
                count += ActiveInputStreamCount();
#endif
                memcpy(ids + count,
                       kOutputStreamIDs,
                       sizeof(AudioObjectID) * kOpenA8DJOutputStreamCount);
                count += kOpenA8DJOutputStreamCount;
                return CopyScalar(ids, sizeof(AudioObjectID) * count, inDataSize, outDataSize, outData);
            }
            *outDataSize = 0;
            return kAudioHardwareNoError;
        }
        default:
            break;
    }

    if (inObjectID == kAudioObjectPlugInObject) {
        switch (inAddress->mSelector) {
            case kAudioPlugInPropertyBundleID:
                return CopyCFString(kDriverBundleID, inDataSize, outDataSize, outData);
            case kAudioPlugInPropertyDeviceList: {
                Trace("Get DeviceList");
                AudioObjectID ids[] = {kOpenA8DJDeviceObjectID};
                return CopyScalar(ids, sizeof(ids), inDataSize, outDataSize, outData);
            }
            case kAudioPlugInPropertyTranslateUIDToDevice: {
                AudioObjectID objectID = kAudioObjectUnknown;
                if (inQualifierDataSize == sizeof(CFStringRef) && inQualifierData != NULL) {
                    CFStringRef requested = *((const CFStringRef *)inQualifierData);
                    if (requested != NULL && CFEqual(requested, kDeviceUID)) {
                        objectID = kOpenA8DJDeviceObjectID;
                    }
                }
                return CopyScalar(&objectID, sizeof(objectID), inDataSize, outDataSize, outData);
            }
            case kAudioPlugInPropertyBoxList:
            case kAudioPlugInPropertyClockDeviceList:
                *outDataSize = 0;
                return kAudioHardwareNoError;
            default:
                return kAudioHardwareUnknownPropertyError;
        }
    }

    if (inObjectID == kOpenA8DJDeviceObjectID) {
        switch (inAddress->mSelector) {
            case kAudioDevicePropertyDeviceUID:
                return CopyCFString(kDeviceUID, inDataSize, outDataSize, outData);
            case kAudioDevicePropertyModelUID:
                return CopyCFString(kModelUID, inDataSize, outDataSize, outData);
            case kAudioDevicePropertyTransportType: {
                UInt32 value = kAudioDeviceTransportTypeUSB;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyRelatedDevices: {
                AudioObjectID ids[] = {kOpenA8DJDeviceObjectID};
                return CopyScalar(ids, sizeof(AudioObjectID), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyClockDomain: {
                UInt32 value = 0;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyClockIsStable: {
                UInt32 value = 1;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyClockAlgorithm: {
                UInt32 value = kAudioDeviceClockAlgorithmRaw;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyDeviceIsAlive:
            {
                bool present = atomic_load(&gDevicePresent);
                UInt32 value = present ? 1 : 0;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyDeviceCanBeDefaultDevice: {
                UInt32 value = inAddress->mScope == kAudioObjectPropertyScopeInput ? 0 : 1;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice: {
                UInt32 value = inAddress->mScope == kAudioObjectPropertyScopeInput ? 0 : 1;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyIsHidden: {
                UInt32 value = 0;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyDeviceIsRunning:
            case kAudioDevicePropertyDeviceIsRunningSomewhere: {
                UInt32 value = gRunningClients > 0 ? 1 : 0;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyLatency:
            case kAudioDevicePropertySafetyOffset:
            case kAudioDevicePropertyUsesVariableBufferFrameSizes: {
                UInt32 value = 0;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
        case kAudioDevicePropertyZeroTimeStampPeriod:
        {
            UInt32 value = kOpenA8DJZeroTimeStampPeriodFrames;
            return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
        }
            case kAudioDevicePropertyStreams: {
                if (inAddress->mScope == kAudioObjectPropertyScopeInput) {
#if OPENA8DJ_ENABLE_INPUT_IO
                    return CopyScalar(kInputStreamIDs, sizeof(AudioObjectID) * ActiveInputStreamCount(), inDataSize, outDataSize, outData);
#else
                    *outDataSize = 0;
                    return kAudioHardwareNoError;
#endif
                }
                if (inAddress->mScope == kAudioObjectPropertyScopeOutput) {
                    return CopyScalar(kOutputStreamIDs,
                                      sizeof(AudioObjectID) * kOpenA8DJOutputStreamCount,
                                      inDataSize,
                                      outDataSize,
                                      outData);
                }
                AudioObjectID ids[kOpenA8DJStreamCount + kOpenA8DJOutputStreamCount];
                UInt32 count = 0;
#if OPENA8DJ_ENABLE_INPUT_IO
                memcpy(ids, kInputStreamIDs, sizeof(AudioObjectID) * ActiveInputStreamCount());
                count += ActiveInputStreamCount();
#endif
                memcpy(ids + count,
                       kOutputStreamIDs,
                       sizeof(AudioObjectID) * kOpenA8DJOutputStreamCount);
                count += kOpenA8DJOutputStreamCount;
                return CopyScalar(ids, sizeof(AudioObjectID) * count, inDataSize, outDataSize, outData);
            }
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDevicePropertyActualSampleRate:
            Trace("Get sample rate %.0f", gSampleRate);
            return CopyScalar(&gSampleRate, sizeof(gSampleRate), inDataSize, outDataSize, outData);
        case kAudioDevicePropertyAvailableNominalSampleRates: {
            AudioValueRange rates[kOpenA8DJSupportedRateCount];
            FillRates(rates);
            return CopyScalar(rates, sizeof(rates), inDataSize, outDataSize, outData);
        }
        case kAudioDevicePropertyBufferFrameSize:
            Trace("Get buffer frames %u", gBufferFrames);
            return CopyScalar(&gBufferFrames, sizeof(gBufferFrames), inDataSize, outDataSize, outData);
        case kAudioDevicePropertyBufferSize: {
            UInt32 value = BufferBytesForFrames(gBufferFrames);
            Trace("Get buffer bytes %u", value);
            return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
        }
        case kAudioDevicePropertyBufferFrameSizeRange: {
            AudioValueRange range = {kOpenA8DJMinBufferFrames, kOpenA8DJMaxAdvertisedBufferFrames};
            Trace("Get buffer range %.0f %.0f size=%u", range.mMinimum, range.mMaximum, (unsigned int)sizeof(range));
            return CopyScalar(&range, sizeof(range), inDataSize, outDataSize, outData);
        }
        case kAudioDevicePropertyBufferSizeRange: {
            AudioValueRange range = {
                BufferBytesForFrames(kOpenA8DJMinBufferFrames),
                BufferBytesForFrames(kOpenA8DJMaxAdvertisedBufferFrames)
            };
            Trace("Get buffer byte range %.0f %.0f size=%u", range.mMinimum, range.mMaximum, (unsigned int)sizeof(range));
            return CopyScalar(&range, sizeof(range), inDataSize, outDataSize, outData);
        }
            case kAudioDevicePropertyStreamConfiguration:
                return WriteStreamConfiguration(inAddress, inDataSize, outDataSize, outData);
#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
            case kAudioDevicePropertyIOProcStreamUsage:
                return WriteStreamUsage(inAddress, inDataSize, outDataSize, outData);
#endif
            case kAudioDevicePropertyPreferredChannelsForStereo: {
                UInt32 channels[] = {1, 2};
                return CopyScalar(channels, sizeof(channels), inDataSize, outDataSize, outData);
            }
            case kAudioDevicePropertyPreferredChannelLayout:
                return WritePreferredChannelLayout(inAddress, inDataSize, outDataSize, outData);
            case kAudioObjectPropertyControlList:
                *outDataSize = 0;
                return kAudioHardwareNoError;
            default:
                return kAudioHardwareUnknownPropertyError;
        }
    }

    if (HasStreamProperty(inObjectID, inAddress)) {
        switch (inAddress->mSelector) {
            case kAudioStreamPropertyIsActive: {
                UInt32 value = 1;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioStreamPropertyDirection: {
                UInt32 value = StreamDirection(inObjectID);
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioStreamPropertyTerminalType: {
                UInt32 value = kAudioStreamTerminalTypeLine;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioStreamPropertyStartingChannel: {
                UInt32 index = StreamIndex(inObjectID);
                UInt32 channels = IsOutputStreamObject(inObjectID) ?
                    OutputChannelsPerStream() : InputChannelsPerStream();
                UInt32 value = index == UINT32_MAX || channels == 0 ? 1 : (index * channels) + 1;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioStreamPropertyLatency: {
                UInt32 value = 0;
                return CopyScalar(&value, sizeof(value), inDataSize, outDataSize, outData);
            }
            case kAudioStreamPropertyVirtualFormat:
            case kAudioStreamPropertyPhysicalFormat: {
                UInt32 channels = IsOutputStreamObject(inObjectID) ?
                    OutputChannelsPerStream() : InputChannelsPerStream();
                AudioStreamBasicDescription asbd = MakeASBD(gSampleRate, channels);
                return CopyScalar(&asbd, sizeof(asbd), inDataSize, outDataSize, outData);
            }
            case kAudioStreamPropertyAvailableVirtualFormats:
            case kAudioStreamPropertyAvailablePhysicalFormats: {
                AudioStreamRangedDescription formats[kOpenA8DJSupportedRateCount];
                UInt32 channels = IsOutputStreamObject(inObjectID) ?
                    OutputChannelsPerStream() : InputChannelsPerStream();
                FillFormats(formats, channels);
                return CopyScalar(formats, sizeof(formats), inDataSize, outDataSize, outData);
            }
            default:
                return kAudioHardwareUnknownPropertyError;
        }
    }

    return kAudioHardwareUnknownPropertyError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_SetPropertyData(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize, const void *inData)
{
    (void)inDriver;
    (void)inClientProcessID;
    (void)inQualifierDataSize;
    (void)inQualifierData;
    if (inObjectID != kOpenA8DJDeviceObjectID || inAddress == NULL || inData == NULL) {
        return kAudioHardwareIllegalOperationError;
    }
    if (inAddress->mSelector == kAudioDevicePropertyNominalSampleRate) {
        if (inDataSize != sizeof(Float64)) {
            return kAudioHardwareBadPropertySizeError;
        }
        Float64 newRate = *((const Float64 *)inData);
        Trace("Set sample rate requested %.0f current %.0f", newRate, gSampleRate);
        if (!IsSupportedRate(newRate)) {
            return kAudioHardwareUnsupportedOperationError;
        }
        pthread_mutex_lock(&gClockMutex);
        bool sameRate = (gSampleRate == newRate);
        if (!sameRate) {
            gPendingSampleRate = newRate;
        }
        pthread_mutex_unlock(&gClockMutex);
        if (sameRate) {
            return kAudioHardwareNoError;
        }
        if (gHost != NULL) {
            return gHost->RequestDeviceConfigurationChange(gHost,
                                                           kOpenA8DJDeviceObjectID,
                                                           kOpenA8DJConfigChangeSampleRate,
                                                           NULL);
        }
        ApplySampleRate(newRate);
        return kAudioHardwareNoError;
    }
    if (inAddress->mSelector == kAudioDevicePropertyBufferFrameSize ||
        inAddress->mSelector == kAudioDevicePropertyBufferSize) {
        if (inDataSize != sizeof(UInt32)) {
            return kAudioHardwareBadPropertySizeError;
        }
        UInt32 newSize = *((const UInt32 *)inData);
        if (inAddress->mSelector == kAudioDevicePropertyBufferSize) {
            newSize = BufferFramesForBytes(newSize);
        }
        UInt32 requestedSize = newSize;
        if (requestedSize == 0 || requestedSize > kOpenA8DJMaxAdvertisedBufferFrames) {
            return kAudioHardwareUnsupportedOperationError;
        }
        newSize = NormalizeBufferFrames(requestedSize);
        Trace("Set buffer frames requested %u normalized %u current %u", requestedSize, newSize, gBufferFrames);
        pthread_mutex_lock(&gClockMutex);
        bool sameSize = (gBufferFrames == newSize);
        if (!sameSize) {
            gPendingBufferFrames = newSize;
        }
        pthread_mutex_unlock(&gClockMutex);
        if (sameSize) {
            return kAudioHardwareNoError;
        }
        if (gHost != NULL) {
            return gHost->RequestDeviceConfigurationChange(gHost,
                                                           kOpenA8DJDeviceObjectID,
                                                           kOpenA8DJConfigChangeBufferFrames,
                                                           NULL);
        }
        ApplyBufferFrameSize(newSize);
        return kAudioHardwareNoError;
    }
#if OPENA8DJ_ENABLE_STREAM_USAGE_PROPERTY
    if (inAddress->mSelector == kAudioDevicePropertyIOProcStreamUsage) {
        UInt32 minSize = offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn);
        if (inDataSize < minSize) {
            return kAudioHardwareBadPropertySizeError;
        }
        const AudioHardwareIOProcStreamUsage *usage = (const AudioHardwareIOProcStreamUsage *)inData;
        UInt32 streamCount = StreamUsageStreamCount(inAddress->mScope);
        if (usage->mNumberStreams != streamCount ||
            inDataSize < minSize + (sizeof(UInt32) * streamCount)) {
            return kAudioHardwareBadPropertySizeError;
        }
        UInt32 mask = 0;
        for (UInt32 i = 0; i < streamCount; i++) {
            if (usage->mStreamIsOn[i] != 0) {
                mask |= (1u << i);
            }
        }
        SetStreamUsageMask(usage->mIOProc, inAddress->mScope, mask);
        Trace("Set stream usage scope=%u mask=0x%x", inAddress->mScope, mask);
        return kAudioHardwareNoError;
    }
#endif
    return kAudioHardwareIllegalOperationError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_StartIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID)
{
    (void)inDriver;
    (void)inClientID;
    Trace("StartIO device=%u client=%u", inDeviceObjectID, inClientID);
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    pthread_mutex_lock(&gIOMutex);
    atomic_fetch_add(&gStopGeneration, 1);
    if (gRunningClients == 0) {
        StartClock();
        if (!OpenA8DJUSBStart(gSampleRate)) {
            Trace("StartIO USB start failed");
            atomic_store(&gDevicePresent, false);
            StopClock();
            pthread_mutex_unlock(&gIOMutex);
            return kAudioHardwareUnspecifiedError;
        } else {
            atomic_store(&gDevicePresent, true);
            Trace("StartIO USB started");
        }
    }
    gRunningClients++;
    pthread_mutex_unlock(&gIOMutex);
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_StopIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID)
{
    (void)inDriver;
    (void)inClientID;
    Trace("StopIO device=%u client=%u", inDeviceObjectID, inClientID);
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    pthread_mutex_lock(&gIOMutex);
    if (gRunningClients > 0) {
        gRunningClients--;
    }
    if (gRunningClients == 0) {
        FlushOutputCycle();
        OpenA8DJUSBSetInputDecodeActive(false);
#if OPENA8DJ_STOP_ISOC_ON_STOP
        OpenA8DJUSBStop();
#endif
        StopClock();
#if OPENA8DJ_BACKGROUND_WARM_OPEN
        OpenA8DJUSBClose();
        atomic_store(&gDevicePresent, OpenA8DJUSBDevicePresent());
        ScheduleBackgroundWarmOpen();
        Trace("StopIO USB closed; warm-open scheduled");
#else
        ScheduleDelayedClose();
        Trace("StopIO USB close scheduled after grace");
#endif
    }
    pthread_mutex_unlock(&gIOMutex);
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, Float64 *outSampleTime, UInt64 *outHostTime, UInt64 *outSeed)
{
    (void)inDriver;
    (void)inClientID;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    if (outSampleTime == NULL || outHostTime == NULL || outSeed == NULL) {
        return kAudioHardwareBadPropertySizeError;
    }
    pthread_mutex_lock(&gClockMutex);
    Float64 anchorSampleTime = gSampleTime;
    UInt64 anchorHostTime = gHostTime;
    Float64 sampleRate = gSampleRate;
    UInt32 periodFrames = kOpenA8DJZeroTimeStampPeriodFrames;
    UInt64 seed = gZeroTimeStampSeed;
    pthread_mutex_unlock(&gClockMutex);

#if OPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP
    OpenA8DJUSBClockAnchor usbAnchor;
    if (OpenA8DJUSBGetClockAnchor(&usbAnchor) &&
        usbAnchor.valid &&
        usbAnchor.sampleRate == sampleRate &&
        usbAnchor.hostTime != 0) {
        Float64 ticksPerFrame = HostTicksPerFrame(sampleRate);
        UInt64 now = mach_absolute_time();
        Float64 ageFrames = 0.0;
        if (ticksPerFrame > 0.0 && now > usbAnchor.hostTime) {
            ageFrames = ((Float64)(now - usbAnchor.hostTime)) / ticksPerFrame;
        }
        if (ageFrames >= 0.0 && ageFrames < sampleRate) {
            anchorSampleTime = usbAnchor.sampleTime;
            anchorHostTime = usbAnchor.hostTime;
            seed = gZeroTimeStampSeed + usbAnchor.seed;
        }
    }
#endif

    UInt64 hostTime = anchorHostTime != 0 ? anchorHostTime : mach_absolute_time();
    Float64 ticksPerFrame = HostTicksPerFrame(sampleRate);
    if (ticksPerFrame > 0.0 && periodFrames > 0) {
        UInt64 now = mach_absolute_time();
        Float64 elapsedFrames = 0.0;
        if (now > hostTime) {
            elapsedFrames = floor(((Float64)(now - hostTime)) / ticksPerFrame);
        }
        Float64 periods = floor(elapsedFrames / (Float64)periodFrames);
        Float64 advancedFrames = periods * (Float64)periodFrames;
        hostTime += (UInt64)llround(advancedFrames * ticksPerFrame);
        anchorSampleTime += advancedFrames;
    }

    *outSampleTime = anchorSampleTime;
    *outHostTime = hostTime;
    *outSeed = seed;
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_WillDoIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, UInt32 inOperationID, Boolean *outWillDo, Boolean *outWillDoInPlace)
{
    (void)inDriver;
    (void)inClientID;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    if (outWillDo == NULL || outWillDoInPlace == NULL) {
        return kAudioHardwareBadPropertySizeError;
    }
    *outWillDo = inOperationID == kAudioServerPlugInIOOperationWriteMix;
#if OPENA8DJ_ENABLE_INPUT_IO
    if (inOperationID == kAudioServerPlugInIOOperationReadInput) {
        *outWillDo = true;
    }
#endif
    *outWillDoInPlace = true;
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_BeginIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo)
{
    (void)inDriver;
    (void)inClientID;
    (void)inIOCycleInfo;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    if (inIOBufferFrameSize > kOpenA8DJMaxBufferFrames) {
        return kAudioHardwareUnsupportedOperationError;
    }
#if OPENA8DJ_ENABLE_INPUT_IO
    if (inOperationID == kAudioServerPlugInIOOperationReadInput) {
        PrepareInputCycle(inIOBufferFrameSize, CycleCounterFromInfo(inIOCycleInfo));
    } else if (inOperationID == kAudioServerPlugInIOOperationWriteMix) {
#else
    if (inOperationID == kAudioServerPlugInIOOperationWriteMix) {
#endif
        EnsureOutputCycle(inIOBufferFrameSize, inIOCycleInfo);
    }
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_DoIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, AudioObjectID inStreamObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo, void *ioMainBuffer, void *ioSecondaryBuffer)
{
    (void)inDriver;
    (void)inClientID;
    (void)inIOCycleInfo;
    (void)ioSecondaryBuffer;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    if (ioMainBuffer == NULL) {
        return kAudioHardwareNoError;
    }
    if (inIOBufferFrameSize > kOpenA8DJMaxBufferFrames) {
        return kAudioHardwareUnsupportedOperationError;
    }
    if (inOperationID == kAudioServerPlugInIOOperationReadInput &&
        IsInputStreamObject(inStreamObjectID)) {
#if !OPENA8DJ_ENABLE_INPUT_IO
        memset(ioMainBuffer,
               0,
               (size_t)inIOBufferFrameSize * InputChannelsPerStream() * sizeof(Float32));
        return kAudioHardwareNoError;
#else
        if (gInputCycleFrames == 0) {
            PrepareInputCycle(inIOBufferFrameSize, CycleCounterFromInfo(inIOCycleInfo));
        }
        CopyInputStreamToClient(StreamIndex(inStreamObjectID), (Float32 *)ioMainBuffer);
        return kAudioHardwareNoError;
#endif
    }
    if (inOperationID == kAudioServerPlugInIOOperationWriteMix &&
        IsOutputStreamObject(inStreamObjectID)) {
        const Float32 *output = (const Float32 *)ioMainBuffer;
        EnsureOutputCycle(inIOBufferFrameSize, inIOCycleInfo);
        CopyClientOutputToOutput(StreamIndex(inStreamObjectID), output);
        if (OutputCycleHasExpectedStreams()) {
            FlushOutputCycle();
        }
        return kAudioHardwareNoError;
    }
    return kAudioHardwareNoError;
}

static OSStatus STDMETHODCALLTYPE OpenA8DJ_EndIOOperation(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo)
{
    (void)inDriver;
    (void)inClientID;
    (void)inOperationID;
    (void)inIOBufferFrameSize;
    (void)inIOCycleInfo;
    if (inDeviceObjectID != kOpenA8DJDeviceObjectID) {
        return kAudioHardwareBadDeviceError;
    }
    if (inOperationID == kAudioServerPlugInIOOperationWriteMix &&
        (OutputCycleHasExpectedStreams() || gOutputCycleCounter == 0)) {
        FlushOutputCycle();
    }
    return kAudioHardwareNoError;
}

void *OpenA8DJ_Create(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID)
{
    (void)allocator;
    Trace("Factory requestedTypeUUID=%p", requestedTypeUUID);
    if (requestedTypeUUID == NULL) {
        return NULL;
    }
    if (CFEqual(requestedTypeUUID, kAudioServerPlugInTypeUUID)) {
        Trace("Factory returning driver");
        OpenA8DJ_AddRef(NULL);
        return gDriverRef;
    }
    Trace("Factory rejected type");
    return NULL;
}
