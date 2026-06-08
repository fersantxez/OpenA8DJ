#import "OpenA8DJUSB.h"

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/AppleUSBDefinitions.h>
#import <IOKit/usb/IOUSBHostFamilyDefinitions.h>
#import <IOUSBHost/AppleUSBDescriptorParsing.h>
#import <IOUSBHost/IOUSBHost.h>
#import <CoreMIDI/CoreMIDI.h>
#import <libkern/OSByteOrder.h>
#import <dispatch/dispatch.h>
#import <mach/mach_time.h>

#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#ifndef OPENA8DJ_ENABLE_TRACE
#define OPENA8DJ_ENABLE_TRACE 0
#endif

static const uint16_t kVendorID = 0x17cc;
static const uint16_t kProductID = 0x1978;
static const uint8_t kEndpointControlOut = 0x01;
static const uint8_t kEndpointControlIn = 0x81;
static const uint8_t kEndpointIsoCapture = 0x82;
static const uint8_t kEndpointIsoPlayback = 0x06;
static const uint8_t kCommandGetDeviceInfo = 0x01;
static const uint8_t kCommandReadIO = 0x04;
static const uint8_t kCommandWriteIO = 0x05;
static const uint8_t kCommandAudioParams = 0x09;
static const uint8_t kCommandMidiRead = 0x06;
static const uint8_t kCommandMidiWrite = 0x07;
static const uint8_t kCommandAutoMsg = 0x0b;
static const uint8_t kInterfaceNumber = 0;
static const uint8_t kConfigurationValue = 1;
static const uint8_t kAlternateSetting = 1;
enum {
    kChannels = 8,
    kStreams = 4,
    kChannelsPerStream = 2,
    kBytesPerSample = 3,
    kBytesPerSampleUSB = 4,
    kIsoFramesPerTransfer = 8,
    kIsoBytesPerFrame = 512,
    kIsoQueueDepth = 32,
    kRingFrames = 32768,
    kOutputTargetLatencyFrames = 4096,
    kClockDriftTolerance = 5,
    kA8DJControlStateBytes = 6
};

enum {
    kInputTransformPairMask = 0x0f
};

static const uint32_t kInputSourceIdentityMap = 0x3210;

static const char *kIPCSocketPath = "/tmp/opena8dj-control.sock";

enum {
    kIPCVersion = 1,
    kIPCMagic = 0x4a443841, /* "A8DJ" little-endian */
    kIPCTypeHello = 1,
    kIPCTypeMidiToDevice = 2,
    kIPCTypeMidiFromDevice = 3,
    kIPCTypeControlGet = 4,
    kIPCTypeControlSet = 5,
    kIPCTypeControlState = 6,
    kIPCTypeStatus = 7,
    kIPCTypeInputStatsGet = 8,
    kIPCTypeInputStats = 9
};

typedef struct OpenA8DJIPCHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t length;
} __attribute__((packed)) OpenA8DJIPCHeader;

typedef struct OpenA8DJControlPayload {
    uint8_t inputMode;
    uint8_t gndLiftTCVinyl;
    uint8_t gndLiftTCCDLine;
    uint8_t gndLiftPhono;
    uint8_t softwareLock;
    uint8_t inputSwapMask;
    uint8_t inputInvertLeftMask;
    uint8_t inputInvertRightMask;
    uint8_t inputSource[kStreams];
} __attribute__((packed)) OpenA8DJControlPayload;

typedef struct OpenA8DJInputStatsPayload {
    uint64_t frames[kStreams];
    double leftSquare[kStreams];
    double rightSquare[kStreams];
    double cross[kStreams];
    double leftPeak[kStreams];
    double rightPeak[kStreams];
} __attribute__((packed)) OpenA8DJInputStatsPayload;

typedef struct CaiaqDeviceSpec {
    uint16_t fwVersion;
    uint8_t hwSubtype;
    uint8_t numErp;
    uint8_t numAnalogIn;
    uint8_t numDigitalIn;
    uint8_t numDigitalOut;
    uint8_t numAnalogAudioOut;
    uint8_t numAnalogAudioIn;
    uint8_t numDigitalAudioOut;
    uint8_t numDigitalAudioIn;
    uint8_t numMidiOut;
    uint8_t numMidiIn;
    uint8_t dataAlignment;
} __attribute__((packed)) CaiaqDeviceSpec;

typedef struct FloatRing {
    pthread_mutex_t mutex;
    float *data;
    uint32_t capacityFrames;
    uint32_t channels;
    uint32_t readFrame;
    uint32_t writeFrame;
    uint32_t availableFrames;
} FloatRing;

static void USBTrace(const char *format, ...)
{
#if OPENA8DJ_ENABLE_TRACE
    FILE *file = fopen("/tmp/opena8dj-usb.log", "a");
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

static const char *NSErrorText(NSError *error)
{
    const char *text = error.localizedDescription.UTF8String;
    return text != NULL ? text : "";
}

static void RingInit(FloatRing *ring, uint32_t capacityFrames, uint32_t channels)
{
    memset(ring, 0, sizeof(*ring));
    pthread_mutex_init(&ring->mutex, NULL);
    ring->capacityFrames = capacityFrames;
    ring->channels = channels;
    ring->data = calloc((size_t)capacityFrames * channels, sizeof(float));
}

static void RingDestroy(FloatRing *ring)
{
    pthread_mutex_destroy(&ring->mutex);
    free(ring->data);
    memset(ring, 0, sizeof(*ring));
}

static void RingClear(FloatRing *ring)
{
    pthread_mutex_lock(&ring->mutex);
    ring->readFrame = 0;
    ring->writeFrame = 0;
    ring->availableFrames = 0;
    if (ring->data != NULL) {
        memset(ring->data, 0, (size_t)ring->capacityFrames * ring->channels * sizeof(float));
    }
    pthread_mutex_unlock(&ring->mutex);
}

static void RingTrimToLatest(FloatRing *ring, uint32_t maxFrames)
{
    pthread_mutex_lock(&ring->mutex);
    if (ring->availableFrames > maxFrames) {
        uint32_t dropFrames = ring->availableFrames - maxFrames;
        ring->readFrame = (ring->readFrame + dropFrames) % ring->capacityFrames;
        ring->availableFrames = maxFrames;
    }
    pthread_mutex_unlock(&ring->mutex);
}

static uint32_t RingWrite(FloatRing *ring, const float *frames, uint32_t frameCount)
{
    if (ring->data == NULL || frames == NULL || frameCount == 0) {
        return 0;
    }
    pthread_mutex_lock(&ring->mutex);
    uint32_t written = 0;
    for (; written < frameCount; written++) {
        if (ring->availableFrames == ring->capacityFrames) {
            ring->readFrame = (ring->readFrame + 1) % ring->capacityFrames;
            ring->availableFrames--;
        }
        memcpy(&ring->data[(size_t)ring->writeFrame * ring->channels],
               &frames[(size_t)written * ring->channels],
               (size_t)ring->channels * sizeof(float));
        ring->writeFrame = (ring->writeFrame + 1) % ring->capacityFrames;
        ring->availableFrames++;
    }
    pthread_mutex_unlock(&ring->mutex);
    return written;
}

static uint32_t RingRead(FloatRing *ring, float *frames, uint32_t frameCount, bool zeroFill)
{
    if (frames == NULL || frameCount == 0) {
        return 0;
    }
    uint32_t read = 0;
    pthread_mutex_lock(&ring->mutex);
    for (; read < frameCount && ring->availableFrames > 0; read++) {
        memcpy(&frames[(size_t)read * ring->channels],
               &ring->data[(size_t)ring->readFrame * ring->channels],
               (size_t)ring->channels * sizeof(float));
        ring->readFrame = (ring->readFrame + 1) % ring->capacityFrames;
        ring->availableFrames--;
    }
    pthread_mutex_unlock(&ring->mutex);
    if (zeroFill && read < frameCount) {
        memset(&frames[(size_t)read * ring->channels],
               0,
               (size_t)(frameCount - read) * ring->channels * sizeof(float));
    }
    return read;
}

static CFMutableDictionaryRef CreateDeviceMatchingDictionary(void)
{
    return [IOUSBHostDevice createMatchingDictionaryWithVendorID:@(kVendorID)
                                                       productID:@(kProductID)
                                                       bcdDevice:nil
                                                     deviceClass:nil
                                                  deviceSubclass:nil
                                                  deviceProtocol:nil
                                                           speed:nil
                                                  productIDArray:nil];
}

static CFMutableDictionaryRef CreateInterfaceMatchingDictionary(void)
{
    return [IOUSBHostInterface createMatchingDictionaryWithVendorID:@(kVendorID)
                                                          productID:@(kProductID)
                                                          bcdDevice:nil
                                                    interfaceNumber:@(kInterfaceNumber)
                                                 configurationValue:@(kConfigurationValue)
                                                     interfaceClass:nil
                                                  interfaceSubclass:nil
                                                  interfaceProtocol:nil
                                                              speed:nil
                                                     productIDArray:nil];
}

static io_service_t FindDeviceService(void)
{
    CFMutableDictionaryRef matching = CreateDeviceMatchingDictionary();
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (matching == NULL ||
        IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != KERN_SUCCESS ||
        iterator == IO_OBJECT_NULL) {
        return IO_OBJECT_NULL;
    }
    io_service_t service = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    return service;
}

static io_service_t FindInterfaceServiceWithRetry(NSTimeInterval timeout)
{
    const useconds_t stepUsec = 100000;
    int attempts = (int)((timeout * 1000000.0) / (double)stepUsec);
    if (attempts < 1) {
        attempts = 1;
    }

    for (int attempt = 0; attempt < attempts; attempt++) {
        CFMutableDictionaryRef matching = CreateInterfaceMatchingDictionary();
        io_iterator_t iterator = IO_OBJECT_NULL;
        if (matching != NULL &&
            IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) == KERN_SUCCESS &&
            iterator != IO_OBJECT_NULL) {
            io_service_t service = IOIteratorNext(iterator);
            IOObjectRelease(iterator);
            if (service != IO_OBJECT_NULL) {
                return service;
            }
        } else if (iterator != IO_OBJECT_NULL) {
            IOObjectRelease(iterator);
        }
        usleep(stepUsec);
    }

    return IO_OBJECT_NULL;
}

static uint16_t le16(uint16_t value)
{
    return OSSwapLittleToHostInt16(value);
}

static int SampleRateCode(double sampleRate)
{
    if (sampleRate == 44100.0) return 0;
    if (sampleRate == 48000.0) return 1;
    if (sampleRate == 96000.0) return 2;
    if (sampleRate == 88200.0) return 4;
    return -1;
}

static uint8_t MaxU8(uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

static bool ReadFull(int fd, void *buffer, size_t length)
{
    uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = read(fd, bytes + offset, length - offset);
        if (count <= 0) {
            return false;
        }
        offset += (size_t)count;
    }
    return true;
}

static bool WriteFull(int fd, const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = write(fd, bytes + offset, length - offset);
        if (count <= 0) {
            return false;
        }
        offset += (size_t)count;
    }
    return true;
}

static bool IPCSend(int fd, uint8_t type, const void *payload, uint16_t length)
{
    OpenA8DJIPCHeader header = {
        .magic = kIPCMagic,
        .version = kIPCVersion,
        .type = type,
        .length = length
    };
    return WriteFull(fd, &header, sizeof(header)) &&
           (length == 0 || WriteFull(fd, payload, length));
}

static int AudioStreamCount(const CaiaqDeviceSpec *spec)
{
    uint8_t inputChannels = MaxU8(spec->numAnalogAudioIn, spec->numDigitalAudioIn);
    uint8_t outputChannels = MaxU8(spec->numAnalogAudioOut, spec->numDigitalAudioOut);
    int inputStreams = inputChannels / kChannelsPerStream;
    int outputStreams = outputChannels / kChannelsPerStream;
    int streams = inputStreams > outputStreams ? inputStreams : outputStreams;
    return streams > 0 ? streams : 1;
}

static uint16_t CalculateBytesPerPacket(const CaiaqDeviceSpec *spec, double sampleRate)
{
    int bytesPerSample = kBytesPerSample;
    if (spec->dataAlignment >= 2) {
        bytesPerSample++;
    }
    int bpp = (((int)sampleRate / 8000) + kClockDriftTolerance) *
        bytesPerSample * kChannelsPerStream * AudioStreamCount(spec);
    if (bpp > (int)kIsoBytesPerFrame) {
        bpp = (int)kIsoBytesPerFrame;
    }
    return (uint16_t)bpp;
}

static int32_t S24BEToS32(const uint8_t *bytes)
{
    int32_t value = ((int32_t)bytes[0] << 16) | ((int32_t)bytes[1] << 8) | (int32_t)bytes[2];
    if (value & 0x00800000) {
        value |= (int32_t)0xff000000;
    }
    return value;
}

static void FloatToS24BE(float sample, uint8_t *bytes)
{
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    int32_t value = (int32_t)lrintf(sample * 8388607.0f);
    bytes[0] = (uint8_t)((value >> 16) & 0xff);
    bytes[1] = (uint8_t)((value >> 8) & 0xff);
    bytes[2] = (uint8_t)(value & 0xff);
}

static uint8_t Mode2CheckByte(uint32_t stream, NSUInteger byteIndex)
{
    NSUInteger group = byteIndex / (kStreams * kBytesPerSampleUSB);
    return (uint8_t)((stream << 1) | ((~group) & 1));
}

@class OpenA8DJIsoTransfer;

@interface OpenA8DJUSBEngine : NSObject
@property(nonatomic) double sampleRate;
- (instancetype)initWithSampleRate:(double)sampleRate;
- (BOOL)start;
- (BOOL)isStreaming;
- (BOOL)ensureOpen;
- (void)stop;
- (void)close;
- (uint32_t)readInput:(float *)outInterleaved frames:(uint32_t)frames channels:(uint32_t)channels;
- (void)writeOutput:(const float *)inInterleaved frames:(uint32_t)frames channels:(uint32_t)channels;
- (void)handleMIDIPacketList:(const MIDIPacketList *)packetList;
- (BOOL)sendCommandNoReply:(uint8_t)command payload:(const uint8_t *)payload payloadLength:(NSUInteger)payloadLength;
- (void)startIPCServer;
- (void)stopIPCServer;
- (void)broadcastIPCType:(uint8_t)type bytes:(const uint8_t *)bytes length:(NSUInteger)length;
- (void)queueCaptureTransfer;
- (void)handleCaptureTransfer:(OpenA8DJIsoTransfer *)transfer
                        status:(IOReturn)status
                  transactions:(IOUSBHostIsochronousTransaction *)transactions;
- (void)queuePlaybackWithRequests:(const uint32_t *)requests count:(NSUInteger)count;
- (void)handlePlaybackTransfer:(OpenA8DJIsoTransfer *)transfer
                         status:(IOReturn)status
                   transactions:(IOUSBHostIsochronousTransaction *)transactions;
@end

@interface OpenA8DJIsoTransfer : NSObject
@property(nonatomic, strong) NSMutableData *data;
@property(nonatomic, strong) NSMutableData *transactions;
@property(nonatomic) NSUInteger transactionCount;
@end

@implementation OpenA8DJIsoTransfer
@end

static OpenA8DJIsoTransfer *CreateIsoTransfer(const uint32_t *requests, NSUInteger count)
{
    if (requests == NULL || count == 0) {
        return nil;
    }

    NSUInteger dataLength = 0;
    for (NSUInteger index = 0; index < count; index++) {
        dataLength += requests[index];
    }

    OpenA8DJIsoTransfer *transfer = [OpenA8DJIsoTransfer new];
    transfer.data = [NSMutableData dataWithLength:dataLength];
    transfer.transactions = [NSMutableData dataWithLength:sizeof(IOUSBHostIsochronousTransaction) * count];
    transfer.transactionCount = count;

    IOUSBHostIsochronousTransaction *transactions = transfer.transactions.mutableBytes;
    uint32_t offset = 0;
    for (NSUInteger index = 0; index < count; index++) {
        transactions[index].status = kIOReturnInvalid;
        transactions[index].requestCount = requests[index];
        transactions[index].offset = offset;
        transactions[index].completeCount = 0;
        transactions[index].timeStamp = 0;
        transactions[index].options = IOUSBHostIsochronousTransactionOptionsNone;
        offset += requests[index];
    }

    return transfer;
}

@implementation OpenA8DJUSBEngine {
    dispatch_queue_t _queue;
    dispatch_queue_t _ep1Queue;
    dispatch_queue_t _ipcQueue;
    IOUSBHostDevice *_device;
    IOUSBHostInterface *_interface;
    IOUSBHostPipe *_bulkOutPipe;
    IOUSBHostPipe *_bulkInPipe;
    IOUSBHostPipe *_capturePipe;
    IOUSBHostPipe *_playbackPipe;
    CaiaqDeviceSpec _spec;
    FloatRing _inputRing;
    FloatRing _outputRing;
    uint64_t _inputMode2Index;
    uint8_t _inputBytes[kStreams][kChannelsPerStream * kBytesPerSample];
    uint8_t _inputByteCount[kStreams];
    float _pendingInput[kChannels];
    uint8_t _pendingInputMask;
    OpenA8DJInputStatsPayload _inputStats;
    pthread_mutex_t _inputStatsMutex;
    atomic_uint _inputSwapMask;
    atomic_uint _inputInvertLeftMask;
    atomic_uint _inputInvertRightMask;
    atomic_uint _inputSourceMap;
    uint8_t _outputFrameBytes[kStreams][kChannelsPerStream * kBytesPerSample];
    uint8_t _outputByteInFrame;
    bool _outputFrameLoaded;
#if OPENA8DJ_ENABLE_TRACE
    uint64_t _debugOutputFramesWritten;
    uint64_t _debugOutputFramesRead;
    uint64_t _debugOutputUnderruns;
    uint64_t _debugCaptureTransfers;
    uint64_t _debugPlaybackTransfers;
    uint64_t _debugCaptureQueueFailures;
    uint64_t _debugPlaybackQueueFailures;
#endif
    atomic_bool _running;
    atomic_bool _streaming;
    pthread_mutex_t _bulkOutMutex;
    pthread_mutex_t _ep1Mutex;
    pthread_cond_t _ep1Cond;
    uint8_t _pendingCommand;
    bool _pendingActive;
    bool _pendingReady;
    NSData *_pendingReply;
    uint8_t _controlState[256];
    pthread_mutex_t _ipcClientsMutex;
    int _ipcListenFd;
    int _ipcClients[16];
    size_t _ipcClientCount;
    MIDIClientRef _midiClient;
    MIDIEndpointRef _midiSource;
    MIDIEndpointRef _midiDestination;
}

- (instancetype)initWithSampleRate:(double)sampleRate
{
    self = [super init];
    if (self != nil) {
        _sampleRate = sampleRate;
        atomic_init(&_running, false);
        atomic_init(&_streaming, false);
        atomic_init(&_inputSwapMask, 0);
        atomic_init(&_inputInvertLeftMask, 0);
        atomic_init(&_inputInvertRightMask, 0);
        atomic_init(&_inputSourceMap, kInputSourceIdentityMap);
        _queue = dispatch_queue_create("org.opena8dj.driver.usb", DISPATCH_QUEUE_SERIAL);
        _ep1Queue = dispatch_queue_create("org.opena8dj.driver.ep1", DISPATCH_QUEUE_SERIAL);
        _ipcQueue = dispatch_queue_create("org.opena8dj.driver.ipc", DISPATCH_QUEUE_SERIAL);
        pthread_mutex_init(&_bulkOutMutex, NULL);
        pthread_mutex_init(&_ep1Mutex, NULL);
        pthread_cond_init(&_ep1Cond, NULL);
        pthread_mutex_init(&_inputStatsMutex, NULL);
        pthread_mutex_init(&_ipcClientsMutex, NULL);
        _ipcListenFd = -1;
        for (size_t i = 0; i < sizeof(_ipcClients) / sizeof(_ipcClients[0]); i++) {
            _ipcClients[i] = -1;
        }
        RingInit(&_inputRing, kRingFrames, kChannels);
        RingInit(&_outputRing, kRingFrames, kChannels);
    }
    return self;
}

- (void)dealloc
{
    [self close];
    pthread_mutex_destroy(&_ipcClientsMutex);
    pthread_mutex_destroy(&_inputStatsMutex);
    pthread_cond_destroy(&_ep1Cond);
    pthread_mutex_destroy(&_ep1Mutex);
    pthread_mutex_destroy(&_bulkOutMutex);
    RingDestroy(&_inputRing);
    RingDestroy(&_outputRing);
}

- (BOOL)sendCommand:(uint8_t)command
            payload:(const uint8_t *)payload
      payloadLength:(NSUInteger)payloadLength
              reply:(NSMutableData **)replyOut
        replyLength:(NSUInteger *)replyLengthOut
{
    for (int attempt = 0; attempt < 3; attempt++) {
        NSMutableData *outData = [NSMutableData dataWithLength:1 + payloadLength];
        uint8_t *outBytes = outData.mutableBytes;
        outBytes[0] = command;
        if (payload != NULL && payloadLength > 0) {
            memcpy(outBytes + 1, payload, payloadLength);
        }

        bool expectsReply = replyOut != NULL || replyLengthOut != NULL ||
                            command == kCommandGetDeviceInfo ||
                            command == kCommandAudioParams ||
                            command == kCommandReadIO;
        pthread_mutex_lock(&_ep1Mutex);
        while (_pendingActive) {
            pthread_cond_wait(&_ep1Cond, &_ep1Mutex);
        }
        if (expectsReply) {
            _pendingCommand = command;
            _pendingActive = true;
            _pendingReady = false;
            _pendingReply = nil;
        }
        pthread_mutex_unlock(&_ep1Mutex);

        NSUInteger bytesTransferred = 0;
        NSError *error = nil;
        pthread_mutex_lock(&_bulkOutMutex);
        BOOL sent = [_bulkOutPipe sendIORequestWithData:outData
                                       bytesTransferred:&bytesTransferred
                                      completionTimeout:1.0
                                                  error:&error];
        pthread_mutex_unlock(&_bulkOutMutex);
        if (!sent) {
            USBTrace("bulk OUT failed command=0x%02x error=%s", command, NSErrorText(error));
            pthread_mutex_lock(&_ep1Mutex);
            if (expectsReply && _pendingActive && _pendingCommand == command) {
                _pendingActive = false;
                _pendingReady = false;
                _pendingReply = nil;
                pthread_cond_broadcast(&_ep1Cond);
            }
            pthread_mutex_unlock(&_ep1Mutex);
            return NO;
        }

        if (!expectsReply) {
            return YES;
        }

        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += 2;

        pthread_mutex_lock(&_ep1Mutex);
        int waitStatus = 0;
        while (!_pendingReady && _pendingActive && waitStatus == 0) {
            waitStatus = pthread_cond_timedwait(&_ep1Cond, &_ep1Mutex, &deadline);
        }
        NSData *reply = _pendingReply;
        bool ok = _pendingReady && reply != nil;
        _pendingActive = false;
        _pendingReady = false;
        _pendingReply = nil;
        pthread_cond_broadcast(&_ep1Cond);
        pthread_mutex_unlock(&_ep1Mutex);

        if (ok) {
            if (replyOut != NULL) *replyOut = [reply mutableCopy];
            if (replyLengthOut != NULL) *replyLengthOut = reply.length;
            return YES;
        }

        USBTrace("bulk command timeout command=0x%02x wait=%d", command, waitStatus);
        usleep(100000);
    }
    return NO;
}

- (BOOL)sendCommandNoReply:(uint8_t)command payload:(const uint8_t *)payload payloadLength:(NSUInteger)payloadLength
{
    NSMutableData *outData = [NSMutableData dataWithLength:1 + payloadLength];
    uint8_t *outBytes = outData.mutableBytes;
    outBytes[0] = command;
    if (payload != NULL && payloadLength > 0) {
        memcpy(outBytes + 1, payload, payloadLength);
    }

    NSUInteger bytesTransferred = 0;
    NSError *error = nil;
    pthread_mutex_lock(&_bulkOutMutex);
    BOOL sent = [_bulkOutPipe sendIORequestWithData:outData
                                   bytesTransferred:&bytesTransferred
                                  completionTimeout:0.2
                                              error:&error];
    pthread_mutex_unlock(&_bulkOutMutex);
    if (!sent) {
        USBTrace("bulk OUT no-reply failed command=0x%02x error=%s", command, NSErrorText(error));
    }
    return sent;
}

- (BOOL)getDeviceInfo
{
    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (![self sendCommand:kCommandGetDeviceInfo payload:NULL payloadLength:0 reply:&reply replyLength:&replyLength]) {
        return NO;
    }
    if (replyLength < 1 + sizeof(CaiaqDeviceSpec)) {
        USBTrace("short GET_DEVICE_INFO reply length=%lu", (unsigned long)replyLength);
        return NO;
    }
    const uint8_t *bytes = reply.bytes;
    if (bytes[0] != kCommandGetDeviceInfo) {
        USBTrace("unexpected GET_DEVICE_INFO reply command=0x%02x", bytes[0]);
        return NO;
    }
    memcpy(&_spec, bytes + 1, sizeof(_spec));
    _spec.fwVersion = le16(_spec.fwVersion);
    USBTrace("device info fw=%u in=%u out=%u midi=%u/%u align=%u",
             _spec.fwVersion,
             _spec.numAnalogAudioIn,
             _spec.numAnalogAudioOut,
             _spec.numMidiIn,
             _spec.numMidiOut,
             _spec.dataAlignment);
    return YES;
}

- (BOOL)setAudioParams
{
    int rateCode = SampleRateCode(_sampleRate);
    if (rateCode < 0) {
        USBTrace("unsupported sample rate %.0f", _sampleRate);
        return NO;
    }
    uint16_t bpp = CalculateBytesPerPacket(&_spec, _sampleRate);
    uint8_t payload[5] = {
        (uint8_t)rateCode,
        2,
        (uint8_t)(bpp & 0xff),
        (uint8_t)(bpp >> 8),
        1
    };
    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (![self sendCommand:kCommandAudioParams payload:payload payloadLength:sizeof(payload) reply:&reply replyLength:&replyLength]) {
        return NO;
    }
    const uint8_t *bytes = reply.bytes;
    BOOL ok = replyLength >= 2 && bytes[0] == kCommandAudioParams && bytes[1] == 1;
    USBTrace("audio params rate=%.0f bpp=%u ok=%d", _sampleRate, bpp, ok);
    return ok;
}

- (void)loadControlPayload:(OpenA8DJControlPayload *)payload
{
    if (payload == NULL) {
        return;
    }
    pthread_mutex_lock(&_ep1Mutex);
    payload->inputMode = _controlState[0];
    payload->gndLiftTCVinyl = (_controlState[3] & (1u << 0)) ? 1 : 0;
    payload->gndLiftTCCDLine = (_controlState[3] & (1u << 1)) ? 1 : 0;
    payload->gndLiftPhono = (_controlState[3] & (1u << 2)) ? 1 : 0;
    payload->softwareLock = (_controlState[5] & (1u << 0)) ? 1 : 0;
    pthread_mutex_unlock(&_ep1Mutex);
    payload->inputSwapMask = (uint8_t)(atomic_load(&_inputSwapMask) & kInputTransformPairMask);
    payload->inputInvertLeftMask = (uint8_t)(atomic_load(&_inputInvertLeftMask) & kInputTransformPairMask);
    payload->inputInvertRightMask = (uint8_t)(atomic_load(&_inputInvertRightMask) & kInputTransformPairMask);
    uint32_t sourceMap = atomic_load(&_inputSourceMap);
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        uint8_t source = (uint8_t)((sourceMap >> (stream * 4)) & 0x0f);
        payload->inputSource[stream] = source < kStreams ? source : (uint8_t)stream;
    }
}

- (void)storeControlPayload:(const OpenA8DJControlPayload *)payload
{
    if (payload == NULL) {
        return;
    }
    pthread_mutex_lock(&_ep1Mutex);
    _controlState[0] = payload->inputMode <= 2 ? payload->inputMode : 0;
    if (payload->gndLiftTCVinyl) _controlState[3] |= (1u << 0); else _controlState[3] &= ~(1u << 0);
    if (payload->gndLiftTCCDLine) _controlState[3] |= (1u << 1); else _controlState[3] &= ~(1u << 1);
    if (payload->gndLiftPhono) _controlState[3] |= (1u << 2); else _controlState[3] &= ~(1u << 2);
    if (payload->softwareLock) _controlState[5] |= (1u << 0); else _controlState[5] &= ~(1u << 0);
    pthread_mutex_unlock(&_ep1Mutex);
    atomic_store(&_inputSwapMask, payload->inputSwapMask & kInputTransformPairMask);
    atomic_store(&_inputInvertLeftMask, payload->inputInvertLeftMask & kInputTransformPairMask);
    atomic_store(&_inputInvertRightMask, payload->inputInvertRightMask & kInputTransformPairMask);
    uint32_t sourceMap = 0;
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        uint8_t source = payload->inputSource[stream] < kStreams ? payload->inputSource[stream] : (uint8_t)stream;
        sourceMap |= ((uint32_t)source) << (stream * 4);
    }
    atomic_store(&_inputSourceMap, sourceMap);
}

- (BOOL)readControls
{
    const uint8_t autoMsgPayload[3] = {1, 0, 0};
    (void)[self sendCommandNoReply:kCommandAutoMsg payload:autoMsgPayload payloadLength:sizeof(autoMsgPayload)];

    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (![self sendCommand:kCommandReadIO payload:NULL payloadLength:0 reply:&reply replyLength:&replyLength]) {
        USBTrace("READ_IO failed");
        return NO;
    }
    if (replyLength < 2 || ((const uint8_t *)reply.bytes)[0] != kCommandReadIO) {
        USBTrace("READ_IO unexpected reply length=%lu", (unsigned long)replyLength);
        return NO;
    }
    NSUInteger copyLength = replyLength - 1;
    if (copyLength > sizeof(_controlState)) {
        copyLength = sizeof(_controlState);
    }
    pthread_mutex_lock(&_ep1Mutex);
    memset(_controlState, 0, sizeof(_controlState));
    memcpy(_controlState, ((const uint8_t *)reply.bytes) + 1, copyLength);
    bool needsDefaultFixup = _controlState[1] != 2 || _controlState[2] != 3 || _controlState[4] != 2;
    if (needsDefaultFixup) {
        _controlState[1] = 2;
        _controlState[2] = 3;
        _controlState[4] = 2;
    }
    pthread_mutex_unlock(&_ep1Mutex);
    USBTrace("READ_IO ok bytes=%lu", (unsigned long)copyLength);
    if (needsDefaultFixup) {
        uint8_t state[6];
        pthread_mutex_lock(&_ep1Mutex);
        memcpy(state, _controlState, sizeof(state));
        pthread_mutex_unlock(&_ep1Mutex);
        (void)[self sendCommandNoReply:kCommandWriteIO payload:state payloadLength:sizeof(state)];
    }
    return YES;
}

- (void)applyTimecodeVinylDefaults
{
    pthread_mutex_lock(&_ep1Mutex);
    _controlState[0] = 0;
    _controlState[3] = (uint8_t)((_controlState[3] & ~(uint8_t)0x07) | (1u << 0));
    _controlState[5] |= (1u << 0);
    pthread_mutex_unlock(&_ep1Mutex);
    (void)[self writeControls];
}

- (BOOL)writeControls
{
    uint8_t state[kA8DJControlStateBytes];
    pthread_mutex_lock(&_ep1Mutex);
    memcpy(state, _controlState, sizeof(state));
    pthread_mutex_unlock(&_ep1Mutex);
    BOOL ok = [self sendCommandNoReply:kCommandWriteIO payload:state payloadLength:sizeof(state)];
    USBTrace("WRITE_IO ok=%d", ok);
    return ok;
}

- (BOOL)openUSB
{
    io_service_t deviceService = FindDeviceService();
    if (deviceService == IO_OBJECT_NULL) {
        USBTrace("Audio 8 DJ USB device not found");
        return NO;
    }

    NSError *error = nil;
    _device = [[IOUSBHostDevice alloc] initWithIOService:deviceService
                                                 options:IOUSBHostObjectInitOptionsDeviceSeize
                                                   queue:_queue
                                                   error:&error
                                         interestHandler:nil];
    IOObjectRelease(deviceService);
    if (_device == nil) {
        USBTrace("open device failed: %s", NSErrorText(error));
        return NO;
    }

    error = nil;
    if (![_device configureWithValue:kConfigurationValue matchInterfaces:YES error:&error]) {
        USBTrace("configure failed: %s", NSErrorText(error));
        return NO;
    }

    io_service_t interfaceService = FindInterfaceServiceWithRetry(2.0);
    if (interfaceService == IO_OBJECT_NULL) {
        USBTrace("interface service not found");
        return NO;
    }

    error = nil;
    _interface = [[IOUSBHostInterface alloc] initWithIOService:interfaceService
                                                      options:IOUSBHostObjectInitOptionsDeviceSeize
                                                        queue:_queue
                                                        error:&error
                                              interestHandler:nil];
    IOObjectRelease(interfaceService);
    if (_interface == nil) {
        USBTrace("open interface failed: %s", NSErrorText(error));
        return NO;
    }

    error = nil;
    if (![_interface selectAlternateSetting:kAlternateSetting error:&error]) {
        USBTrace("select alt failed: %s", NSErrorText(error));
        return NO;
    }

    _bulkOutPipe = [_interface copyPipeWithAddress:kEndpointControlOut error:&error];
    if (_bulkOutPipe == nil) {
        USBTrace("open bulk OUT failed: %s", NSErrorText(error));
        return NO;
    }
    _bulkInPipe = [_interface copyPipeWithAddress:kEndpointControlIn error:&error];
    if (_bulkInPipe == nil) {
        USBTrace("open bulk IN failed: %s", NSErrorText(error));
        return NO;
    }

    atomic_store(&_running, true);
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_async(_ep1Queue, ^{
        [weakSelf ep1ReadLoop];
    });

    if (![self getDeviceInfo]) {
        return NO;
    }
    (void)[self readControls];
    [self applyTimecodeVinylDefaults];

    _capturePipe = [_interface copyPipeWithAddress:kEndpointIsoCapture error:&error];
    if (_capturePipe == nil) {
        USBTrace("open isoc IN failed: %s", NSErrorText(error));
        return NO;
    }
    _playbackPipe = [_interface copyPipeWithAddress:kEndpointIsoPlayback error:&error];
    if (_playbackPipe == nil) {
        USBTrace("open isoc OUT failed: %s", NSErrorText(error));
        return NO;
    }
    [self startIPCServer];
    return YES;
}

- (void)setupMIDI
{
    USBTrace("CoreMIDI bridge disabled in HAL helper; requires user-domain/DriverKit bridge");
    return;
#if 0
    if (_spec.numMidiIn == 0 && _spec.numMidiOut == 0) {
        return;
    }
    OSStatus status = MIDIClientCreate(CFSTR("OpenA8DJ MIDI Client"), NULL, NULL, &_midiClient);
    if (status != noErr) {
        USBTrace("MIDIClientCreate failed status=%d", (int)status);
        return;
    }
    if (_spec.numMidiIn > 0) {
        status = MIDISourceCreate(_midiClient, CFSTR("Open Audio 8 DJ MIDI Out"), &_midiSource);
        USBTrace("MIDISourceCreate status=%d source=%u", (int)status, (unsigned)_midiSource);
    }
    if (_spec.numMidiOut > 0) {
        status = MIDIDestinationCreate(_midiClient,
                                       CFSTR("Open Audio 8 DJ MIDI In"),
                                       MIDIReadCallback,
                                       (__bridge void *)self,
                                       &_midiDestination);
        USBTrace("MIDIDestinationCreate status=%d destination=%u", (int)status, (unsigned)_midiDestination);
    }
#endif
}

- (void)addIPCClient:(int)fd
{
    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    if (_ipcClientCount >= capacity) {
        pthread_mutex_unlock(&_ipcClientsMutex);
        close(fd);
        return;
    }
    for (size_t i = 0; i < capacity; i++) {
        if (_ipcClients[i] < 0) {
            _ipcClients[i] = fd;
            _ipcClientCount++;
            break;
        }
    }
    pthread_mutex_unlock(&_ipcClientsMutex);
}

- (void)removeIPCClient:(int)fd
{
    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    for (size_t i = 0; i < capacity; i++) {
        if (_ipcClients[i] == fd) {
            _ipcClients[i] = -1;
            if (_ipcClientCount > 0) {
                _ipcClientCount--;
            }
            break;
        }
    }
    pthread_mutex_unlock(&_ipcClientsMutex);
    close(fd);
}

- (void)broadcastIPCType:(uint8_t)type bytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (length > UINT16_MAX) {
        length = UINT16_MAX;
    }
    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    for (size_t i = 0; i < capacity; i++) {
        int fd = _ipcClients[i];
        if (fd < 0) {
            continue;
        }
        if (!IPCSend(fd, type, bytes, (uint16_t)length)) {
            close(fd);
            _ipcClients[i] = -1;
            if (_ipcClientCount > 0) {
                _ipcClientCount--;
            }
        }
    }
    pthread_mutex_unlock(&_ipcClientsMutex);
}

- (void)sendControlStateToClient:(int)fd
{
    OpenA8DJControlPayload state;
    [self loadControlPayload:&state];
    (void)IPCSend(fd, kIPCTypeControlState, &state, sizeof(state));
}

- (void)addInputStatsForStream:(uint32_t)stream left:(float)left right:(float)right
{
    if (stream >= kStreams) {
        return;
    }
    double l = left;
    double r = right;
    double la = fabs(l);
    double ra = fabs(r);
    pthread_mutex_lock(&_inputStatsMutex);
    _inputStats.frames[stream]++;
    _inputStats.leftSquare[stream] += l * l;
    _inputStats.rightSquare[stream] += r * r;
    _inputStats.cross[stream] += l * r;
    if (la > _inputStats.leftPeak[stream]) {
        _inputStats.leftPeak[stream] = la;
    }
    if (ra > _inputStats.rightPeak[stream]) {
        _inputStats.rightPeak[stream] = ra;
    }
    pthread_mutex_unlock(&_inputStatsMutex);
}

- (void)sendInputStatsToClient:(int)fd
{
    OpenA8DJInputStatsPayload stats;
    pthread_mutex_lock(&_inputStatsMutex);
    stats = _inputStats;
    memset(&_inputStats, 0, sizeof(_inputStats));
    pthread_mutex_unlock(&_inputStatsMutex);
    (void)IPCSend(fd, kIPCTypeInputStats, &stats, sizeof(stats));
}

- (void)handleIPCMessageType:(uint8_t)type payload:(const uint8_t *)payload length:(NSUInteger)length client:(int)fd
{
    switch (type) {
        case kIPCTypeHello: {
            const char status[] = "OpenA8DJ HAL USB bridge ready";
            (void)IPCSend(fd, kIPCTypeStatus, status, (uint16_t)(sizeof(status) - 1));
            [self sendControlStateToClient:fd];
            break;
        }
        case kIPCTypeMidiToDevice:
            [self sendMIDIBytes:payload length:length];
            break;
        case kIPCTypeControlGet:
            [self sendControlStateToClient:fd];
            break;
        case kIPCTypeControlSet:
            if (length >= sizeof(OpenA8DJControlPayload)) {
                [self storeControlPayload:(const OpenA8DJControlPayload *)payload];
                (void)[self writeControls];
                [self sendControlStateToClient:fd];
            }
            break;
        case kIPCTypeInputStatsGet:
            [self sendInputStatsToClient:fd];
            break;
        default:
            break;
    }
}

- (void)handleIPCClient:(int)fd
{
    @autoreleasepool {
        while (atomic_load(&_running)) {
            OpenA8DJIPCHeader header;
            if (!ReadFull(fd, &header, sizeof(header))) {
                break;
            }
            if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
                break;
            }
            uint8_t payload[4096];
            if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
                break;
            }
            [self handleIPCMessageType:header.type payload:payload length:header.length client:fd];
        }
        [self removeIPCClient:fd];
    }
}

- (void)startIPCServer
{
    if (_ipcListenFd >= 0) {
        return;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        USBTrace("IPC socket create failed");
        return;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strlcpy(address.sun_path, kIPCSocketPath, sizeof(address.sun_path));
    unlink(kIPCSocketPath);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        USBTrace("IPC bind failed errno=%d %s", errno, strerror(errno));
        close(fd);
        return;
    }
    chmod(kIPCSocketPath, 0666);
    if (listen(fd, 8) != 0) {
        USBTrace("IPC listen failed errno=%d %s", errno, strerror(errno));
        close(fd);
        unlink(kIPCSocketPath);
        return;
    }

    _ipcListenFd = fd;
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_async(_ipcQueue, ^{
        OpenA8DJUSBEngine *strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        USBTrace("IPC server listening at %s", kIPCSocketPath);
        while (atomic_load(&strongSelf->_running)) {
            int client = accept(fd, NULL, NULL);
            if (client < 0) {
                if (atomic_load(&strongSelf->_running)) {
                    USBTrace("IPC accept failed");
                }
                break;
            }
            [strongSelf addIPCClient:client];
            [strongSelf sendControlStateToClient:client];
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                [strongSelf handleIPCClient:client];
            });
        }
    });
}

- (void)stopIPCServer
{
    int fd = _ipcListenFd;
    _ipcListenFd = -1;
    if (fd >= 0) {
        close(fd);
    }

    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    for (size_t i = 0; i < capacity; i++) {
        if (_ipcClients[i] >= 0) {
            close(_ipcClients[i]);
            _ipcClients[i] = -1;
        }
    }
    _ipcClientCount = 0;
    pthread_mutex_unlock(&_ipcClientsMutex);
    unlink(kIPCSocketPath);
}

- (BOOL)ensureOpen
{
    if (atomic_load(&_running)) {
        return YES;
    }
    if (![self openUSB]) {
        [self close];
        return NO;
    }
    USBTrace("USB engine opened");
    return YES;
}

- (BOOL)start
{
    if (atomic_load(&_streaming)) {
        return YES;
    }
    if (![self ensureOpen]) {
        return NO;
    }
    if (![self setAudioParams]) {
        return NO;
    }
    RingClear(&_inputRing);
    RingClear(&_outputRing);
    memset(_inputBytes, 0, sizeof(_inputBytes));
    memset(_inputByteCount, 0, sizeof(_inputByteCount));
    if (_spec.dataAlignment == 2) {
        /* Mode 2 capture starts one 24-bit sample into the stereo byte stream. */
        for (uint32_t stream = 0; stream < kStreams; stream++) {
            _inputByteCount[stream] = kBytesPerSample;
        }
    }
    memset(_pendingInput, 0, sizeof(_pendingInput));
    _pendingInputMask = 0;
    _inputMode2Index = 0;
    memset(_outputFrameBytes, 0, sizeof(_outputFrameBytes));
    /* caiaq mode 2 playback begins four bytes into the per-stream PCM sequence. */
    _outputByteInFrame = kBytesPerSample + 1;
    _outputFrameLoaded = false;
#if OPENA8DJ_ENABLE_TRACE
    _debugOutputFramesWritten = 0;
    _debugOutputFramesRead = 0;
    _debugOutputUnderruns = 0;
    _debugCaptureTransfers = 0;
    _debugPlaybackTransfers = 0;
    _debugCaptureQueueFailures = 0;
    _debugPlaybackQueueFailures = 0;
#endif

    atomic_store(&_streaming, true);
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_async(_queue, ^{
        [weakSelf workerLoop];
    });
    USBTrace("USB engine started");
    return YES;
}

- (BOOL)isStreaming
{
    return atomic_load(&_streaming);
}

- (void)stop
{
    if (!atomic_exchange(&_streaming, false)) {
        return;
    }
    [_capturePipe abortWithError:nil];
    [_playbackPipe abortWithError:nil];
    dispatch_sync(_queue, ^{
    });
    USBTrace("USB engine stopped");
}

- (void)close
{
    [self stop];
    if (!atomic_exchange(&_running, false)) {
        return;
    }
    [self stopIPCServer];
    [_bulkInPipe abortWithError:nil];
    [_bulkOutPipe abortWithError:nil];
    dispatch_sync(_ep1Queue, ^{
    });
    [self closeUSB];
    USBTrace("USB engine closed");
}

- (void)closeUSB
{
    [_capturePipe abortWithError:nil];
    [_playbackPipe abortWithError:nil];
    [_bulkInPipe abortWithError:nil];
    [_bulkOutPipe abortWithError:nil];
    [_interface destroy];
    [_device destroy];
    _capturePipe = nil;
    _playbackPipe = nil;
    _bulkInPipe = nil;
    _bulkOutPipe = nil;
    _interface = nil;
    _device = nil;
    if (_midiSource != 0) {
        MIDIEndpointDispose(_midiSource);
        _midiSource = 0;
    }
    if (_midiDestination != 0) {
        MIDIEndpointDispose(_midiDestination);
        _midiDestination = 0;
    }
    if (_midiClient != 0) {
        MIDIClientDispose(_midiClient);
        _midiClient = 0;
    }
}

- (void)publishMIDIBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (bytes != NULL && length > 0) {
        [self broadcastIPCType:kIPCTypeMidiFromDevice bytes:bytes length:length];
    }
    if (_midiSource == 0 || bytes == NULL || length == 0) {
        return;
    }
    Byte packetStorage[sizeof(MIDIPacketList) + 256];
    MIDIPacketList *packetList = (MIDIPacketList *)packetStorage;
    MIDIPacket *packet = MIDIPacketListInit(packetList);
    packet = MIDIPacketListAdd(packetList,
                               sizeof(packetStorage),
                               packet,
                               mach_absolute_time(),
                               length > 256 ? 256 : (UInt16)length,
                               bytes);
    if (packet != NULL) {
        OSStatus status = MIDIReceived(_midiSource, packetList);
        if (status != noErr) {
            USBTrace("MIDIReceived failed status=%d", (int)status);
        }
    }
}

- (void)sendMIDIBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (_bulkOutPipe == nil || bytes == NULL || length == 0) {
        return;
    }
    NSUInteger offset = 0;
    while (offset < length) {
        NSUInteger chunk = length - offset;
        if (chunk > 61) {
            chunk = 61;
        }
        NSMutableData *outData = [NSMutableData dataWithLength:3 + chunk];
        uint8_t *out = outData.mutableBytes;
        out[0] = kCommandMidiWrite;
        out[1] = 0;
        out[2] = (uint8_t)chunk;
        memcpy(out + 3, bytes + offset, chunk);

        if (![self sendCommandNoReply:kCommandMidiWrite payload:out + 1 payloadLength:2 + chunk]) {
            USBTrace("MIDI_WRITE failed");
            return;
        }
        offset += chunk;
    }
}

- (void)handleMIDIPacketList:(const MIDIPacketList *)packetList
{
    if (packetList == NULL) {
        return;
    }
    const MIDIPacket *packet = &packetList->packet[0];
    for (UInt32 index = 0; index < packetList->numPackets; index++) {
        [self sendMIDIBytes:packet->data length:packet->length];
        packet = MIDIPacketNext(packet);
    }
}

- (void)handleEP1Bytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (bytes == NULL || length == 0) {
        return;
    }
    pthread_mutex_lock(&_ep1Mutex);
    if (_pendingActive && bytes[0] == _pendingCommand) {
        _pendingReply = [NSData dataWithBytes:bytes length:length];
        _pendingReady = true;
        pthread_cond_broadcast(&_ep1Cond);
        pthread_mutex_unlock(&_ep1Mutex);
        return;
    }
    pthread_mutex_unlock(&_ep1Mutex);

    if (bytes[0] == kCommandMidiRead && length >= 3) {
        NSUInteger midiLength = bytes[2];
        if (3 + midiLength > length) {
            midiLength = length - 3;
        }
        [self publishMIDIBytes:bytes + 3 length:midiLength];
    }
}

- (void)ep1ReadLoop
{
    while (atomic_load(&_running)) {
        @autoreleasepool {
            NSMutableData *inData = [NSMutableData dataWithLength:64];
            dispatch_semaphore_t readDone = dispatch_semaphore_create(0);
            __block IOReturn readStatus = kIOReturnInvalid;
            __block NSUInteger readBytes = 0;
            NSError *error = nil;
            BOOL queued = [_bulkInPipe enqueueIORequestWithData:inData
                                              completionTimeout:1.0
                                                          error:&error
                                              completionHandler:^(IOReturn status, NSUInteger bytesTransferred) {
                                                  readStatus = status;
                                                  readBytes = bytesTransferred;
                                                  dispatch_semaphore_signal(readDone);
                                              }];
            if (!queued) {
                if (atomic_load(&_running)) {
                    USBTrace("EP1 read queue failed: %s", NSErrorText(error));
                }
                usleep(100000);
                continue;
            }

            dispatch_semaphore_wait(readDone, DISPATCH_TIME_FOREVER);
            if (readStatus == kIOReturnSuccess && readBytes > 0) {
                [self handleEP1Bytes:inData.bytes length:readBytes];
            } else if (readStatus != kIOReturnTimeout &&
                       readStatus != kIOReturnAborted &&
                       atomic_load(&_running)) {
                USBTrace("EP1 read status=0x%08x bytes=%lu", readStatus, (unsigned long)readBytes);
            }
        }
    }
}

- (void)appendInputByte:(uint8_t)byte stream:(uint32_t)stream
{
    if (stream >= kStreams) {
        return;
    }
    uint8_t pos = _inputByteCount[stream];
    _inputBytes[stream][pos++] = byte;
    _inputByteCount[stream] = pos;
    if (pos < kChannelsPerStream * kBytesPerSample) {
        return;
    }

    const uint8_t *left = &_inputBytes[stream][0];
    const uint8_t *right = &_inputBytes[stream][3];
    float leftSample = (float)S24BEToS32(left) / 8388608.0f;
    float rightSample = (float)S24BEToS32(right) / 8388608.0f;
    uint32_t pairBit = 1u << stream;
    if ((atomic_load(&_inputSwapMask) & pairBit) != 0) {
        float tmp = leftSample;
        leftSample = rightSample;
        rightSample = tmp;
    }
    if ((atomic_load(&_inputInvertLeftMask) & pairBit) != 0) {
        leftSample = -leftSample;
    }
    if ((atomic_load(&_inputInvertRightMask) & pairBit) != 0) {
        rightSample = -rightSample;
    }
    _pendingInput[stream * 2] = leftSample;
    _pendingInput[stream * 2 + 1] = rightSample;
    [self addInputStatsForStream:stream left:leftSample right:rightSample];
    _inputByteCount[stream] = 0;
    _pendingInputMask |= (uint8_t)(1u << stream);
    if (_pendingInputMask == 0x0f) {
        float routedInput[kChannels];
        uint32_t sourceMap = atomic_load(&_inputSourceMap);
        for (uint32_t destination = 0; destination < kStreams; destination++) {
            uint32_t source = (sourceMap >> (destination * 4)) & 0x0f;
            if (source >= kStreams) {
                source = destination;
            }
            routedInput[destination * 2] = _pendingInput[source * 2];
            routedInput[destination * 2 + 1] = _pendingInput[source * 2 + 1];
        }
        RingWrite(&_inputRing, routedInput, 1);
        _pendingInputMask = 0;
    }
}

- (void)decodeCaptureBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (_spec.dataAlignment != 2) {
        return;
    }
    for (NSUInteger offset = 0; offset < length; offset++, _inputMode2Index++) {
        uint32_t groupOffset = (uint32_t)(_inputMode2Index % (kStreams * kBytesPerSampleUSB));
        if (groupOffset < kStreams) {
            continue;
        }
        uint32_t stream = groupOffset % kStreams;
        [self appendInputByte:bytes[offset] stream:stream];
    }
}

- (void)loadNextOutputFrame
{
    float frame[kChannels] = {0};
#if OPENA8DJ_ENABLE_TRACE
    uint32_t readFrames = RingRead(&_outputRing, frame, 1, true);
    _debugOutputFramesRead++;
    if (readFrames == 0) {
        _debugOutputUnderruns++;
    }
#else
    (void)RingRead(&_outputRing, frame, 1, true);
#endif
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        FloatToS24BE(frame[stream * 2], &_outputFrameBytes[stream][0]);
        FloatToS24BE(frame[stream * 2 + 1], &_outputFrameBytes[stream][3]);
    }
    _outputFrameLoaded = true;
}

- (void)loadNextOutputFrameIfNeeded
{
    if (!_outputFrameLoaded || _outputByteInFrame == 0) {
        [self loadNextOutputFrame];
    }
}

- (void)fillPlaybackBytes:(uint8_t *)bytes length:(NSUInteger)length
{
    if (_spec.dataAlignment != 2) {
        memset(bytes, 0, length);
        return;
    }
    NSUInteger i = 0;
    while (i < length) {
        if ((i % (kStreams * kBytesPerSampleUSB)) == (kStreams * kChannelsPerStream)) {
            for (uint32_t stream = 0; stream < kStreams && i < length; stream++, i++) {
                bytes[i] = Mode2CheckByte(stream, i);
            }
            continue;
        }

        [self loadNextOutputFrameIfNeeded];
        for (uint32_t stream = 0; stream < kStreams && i < length; stream++, i++) {
            bytes[i] = _outputFrameBytes[stream][_outputByteInFrame];
        }
        _outputByteInFrame++;
        if (_outputByteInFrame >= kChannelsPerStream * kBytesPerSample) {
            _outputByteInFrame = 0;
        }
    }
}

- (void)workerLoop
{
    for (uint32_t transfer = 0; transfer < kIsoQueueDepth && atomic_load(&_streaming); transfer++) {
        [self queueCaptureTransfer];
    }
    USBTrace("isoc async pipeline started depth=%u", kIsoQueueDepth);
}

- (void)queueCaptureTransfer
{
    if (!atomic_load(&_streaming) || _capturePipe == nil) {
        return;
    }

    uint32_t requests[kIsoFramesPerTransfer];
    for (uint32_t frame = 0; frame < kIsoFramesPerTransfer; frame++) {
        requests[frame] = kIsoBytesPerFrame;
    }

    OpenA8DJIsoTransfer *transfer = CreateIsoTransfer(requests, kIsoFramesPerTransfer);
    IOUSBHostIsochronousTransaction *transactions = transfer.transactions.mutableBytes;
    __weak OpenA8DJUSBEngine *weakSelf = self;
    NSError *error = nil;
    BOOL queued = [_capturePipe enqueueIORequestWithData:transfer.data
                                         transactionList:transactions
                                    transactionListCount:transfer.transactionCount
                                        firstFrameNumber:0
                                                 options:IOUSBHostIsochronousTransferOptionsNone
                                                   error:&error
                                       completionHandler:^(IOReturn status, IOUSBHostIsochronousTransaction transactionList[]) {
                                           OpenA8DJUSBEngine *strongSelf = weakSelf;
                                           if (strongSelf != nil) {
                                               [strongSelf handleCaptureTransfer:transfer
                                                                          status:status
                                                                    transactions:transactionList];
                                           }
                                       }];
    if (!queued) {
#if OPENA8DJ_ENABLE_TRACE
        _debugCaptureQueueFailures++;
#endif
        USBTrace("isoc IN queue failed: %s", NSErrorText(error));
        if (atomic_load(&_streaming)) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1000000), _queue, ^{
                [weakSelf queueCaptureTransfer];
            });
        }
    }
}

- (void)handleCaptureTransfer:(OpenA8DJIsoTransfer *)transfer
                        status:(IOReturn)status
                  transactions:(IOUSBHostIsochronousTransaction *)transactions
{
    uint32_t playbackRequests[kIsoFramesPerTransfer] = {0};
    NSUInteger playbackRequestCount = 0;
    uint32_t expectedBytes = CalculateBytesPerPacket(&_spec, _sampleRate);
#if OPENA8DJ_ENABLE_TRACE
    NSUInteger playbackLength = 0;
    uint32_t minCompleteCount = UINT32_MAX;
    uint32_t maxCompleteCount = 0;
    uint32_t failedTransactions = 0;
    uint32_t filteredTransactions = 0;
#endif

    if (status == kIOReturnSuccess && transactions != NULL) {
        const uint8_t *captureBytes = transfer.data.bytes;
        NSUInteger captureLength = transfer.data.length;
        for (NSUInteger frame = 0; frame < transfer.transactionCount; frame++) {
            IOUSBHostIsochronousTransaction *transaction = &transactions[frame];
            if (transaction->status != kIOReturnSuccess || transaction->completeCount == 0) {
#if OPENA8DJ_ENABLE_TRACE
                if (transaction->status != kIOReturnSuccess) {
                    failedTransactions++;
                }
#endif
                continue;
            }

            uint32_t count = transaction->completeCount;
            if ((NSUInteger)transaction->offset + count > captureLength) {
#if OPENA8DJ_ENABLE_TRACE
                failedTransactions++;
#endif
                continue;
            }

#if OPENA8DJ_ENABLE_TRACE
            if (count < minCompleteCount) minCompleteCount = count;
            if (count > maxCompleteCount) maxCompleteCount = count;
#endif
            /* Drop stale isochronous frames left over from the previous sample rate. */
            if (count != expectedBytes) {
#if OPENA8DJ_ENABLE_TRACE
                filteredTransactions++;
#endif
                continue;
            }
            [self decodeCaptureBytes:captureBytes + transaction->offset length:count];
            playbackRequests[playbackRequestCount++] = count;
#if OPENA8DJ_ENABLE_TRACE
            playbackLength += count;
#endif
        }
    } else if (status != kIOReturnAborted && atomic_load(&_streaming)) {
        USBTrace("isoc IN complete status=0x%08x", status);
    }

    if (playbackRequestCount > 0) {
        [self queuePlaybackWithRequests:playbackRequests count:playbackRequestCount];
    }

#if OPENA8DJ_ENABLE_TRACE
    _debugCaptureTransfers++;
    if ((_debugCaptureTransfers % 256) == 1) {
        uint32_t ringFrames = 0;
        pthread_mutex_lock(&_outputRing.mutex);
        ringFrames = _outputRing.availableFrames;
        pthread_mutex_unlock(&_outputRing.mutex);
        USBTrace("isoc stats cap=%llu play=%llu rate=%.0f requests=%lu bytes=%lu minmax=%u/%u written=%llu read=%llu underruns=%llu ring=%u bytepos=%u failtx=%u filtered=%u qfail=%llu/%llu",
                 _debugCaptureTransfers,
                 _debugPlaybackTransfers,
                 _sampleRate,
                 (unsigned long)playbackRequestCount,
                 (unsigned long)playbackLength,
                 minCompleteCount == UINT32_MAX ? 0 : minCompleteCount,
                 maxCompleteCount,
                 _debugOutputFramesWritten,
                 _debugOutputFramesRead,
                 _debugOutputUnderruns,
                 ringFrames,
                 _outputByteInFrame,
                 failedTransactions,
                 filteredTransactions,
                 _debugCaptureQueueFailures,
                 _debugPlaybackQueueFailures);
    }
#endif

    if (atomic_load(&_streaming)) {
        [self queueCaptureTransfer];
    }
}

- (void)queuePlaybackWithRequests:(const uint32_t *)requests count:(NSUInteger)count
{
    if (!atomic_load(&_streaming) || _playbackPipe == nil || requests == NULL || count == 0) {
        return;
    }

    OpenA8DJIsoTransfer *transfer = CreateIsoTransfer(requests, count);
    [self fillPlaybackBytes:transfer.data.mutableBytes length:transfer.data.length];

    IOUSBHostIsochronousTransaction *transactions = transfer.transactions.mutableBytes;
    __weak OpenA8DJUSBEngine *weakSelf = self;
    NSError *error = nil;
    BOOL queued = [_playbackPipe enqueueIORequestWithData:transfer.data
                                          transactionList:transactions
                                     transactionListCount:transfer.transactionCount
                                         firstFrameNumber:0
                                                  options:IOUSBHostIsochronousTransferOptionsNone
                                                    error:&error
                                        completionHandler:^(IOReturn status, IOUSBHostIsochronousTransaction transactionList[]) {
                                            OpenA8DJUSBEngine *strongSelf = weakSelf;
                                            if (strongSelf != nil) {
                                                [strongSelf handlePlaybackTransfer:transfer
                                                                            status:status
                                                                      transactions:transactionList];
                                            }
                                        }];
    if (!queued) {
#if OPENA8DJ_ENABLE_TRACE
        _debugPlaybackQueueFailures++;
#endif
        USBTrace("isoc OUT queue failed: %s", NSErrorText(error));
    }
}

- (void)handlePlaybackTransfer:(OpenA8DJIsoTransfer *)transfer
                         status:(IOReturn)status
                   transactions:(IOUSBHostIsochronousTransaction *)transactions
{
    (void)transfer;
#if OPENA8DJ_ENABLE_TRACE
    if (status == kIOReturnSuccess && transactions != NULL) {
        _debugPlaybackTransfers++;
    }
#else
    (void)transactions;
#endif
    if (status != kIOReturnSuccess && status != kIOReturnAborted && atomic_load(&_streaming)) {
        USBTrace("isoc OUT complete status=0x%08x", status);
    }
}

- (uint32_t)readInput:(float *)outInterleaved frames:(uint32_t)frames channels:(uint32_t)channels
{
    if (channels != kChannels) {
        memset(outInterleaved, 0, (size_t)frames * channels * sizeof(float));
        return 0;
    }
    return RingRead(&_inputRing, outInterleaved, frames, true);
}

- (void)writeOutput:(const float *)inInterleaved frames:(uint32_t)frames channels:(uint32_t)channels
{
    if (channels != kChannels || inInterleaved == NULL) {
        return;
    }
    RingWrite(&_outputRing, inInterleaved, frames);
    RingTrimToLatest(&_outputRing, kOutputTargetLatencyFrames);
#if OPENA8DJ_ENABLE_TRACE
    _debugOutputFramesWritten += frames;
#endif
}

@end

static pthread_mutex_t gEngineMutex = PTHREAD_MUTEX_INITIALIZER;
static OpenA8DJUSBEngine *gEngine = nil;

bool OpenA8DJUSBStart(double sampleRate)
{
    pthread_mutex_lock(&gEngineMutex);
    if (gEngine == nil) {
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    } else if (fabs(gEngine.sampleRate - sampleRate) > 0.5) {
        [gEngine close];
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    }
    gEngine.sampleRate = sampleRate;
    BOOL ok = [gEngine start];
    pthread_mutex_unlock(&gEngineMutex);
    return ok;
}

bool OpenA8DJUSBSetSampleRate(double sampleRate)
{
    pthread_mutex_lock(&gEngineMutex);
    if (gEngine == nil) {
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
        pthread_mutex_unlock(&gEngineMutex);
        return true;
    }

    if (fabs(gEngine.sampleRate - sampleRate) <= 0.5) {
        pthread_mutex_unlock(&gEngineMutex);
        return true;
    }

    BOOL wasStreaming = [gEngine isStreaming];
    [gEngine close];
    gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    BOOL ok = !wasStreaming || [gEngine start];
    pthread_mutex_unlock(&gEngineMutex);
    return ok;
}

bool OpenA8DJUSBEnsureOpen(double sampleRate)
{
    pthread_mutex_lock(&gEngineMutex);
    if (gEngine == nil) {
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    }
    gEngine.sampleRate = sampleRate;
    BOOL ok = [gEngine ensureOpen];
    pthread_mutex_unlock(&gEngineMutex);
    return ok;
}

bool OpenA8DJUSBDevicePresent(void)
{
    io_service_t service = FindDeviceService();
    if (service == IO_OBJECT_NULL) {
        return false;
    }
    IOObjectRelease(service);
    return true;
}

void OpenA8DJUSBStop(void)
{
    pthread_mutex_lock(&gEngineMutex);
    [gEngine stop];
    pthread_mutex_unlock(&gEngineMutex);
}

void OpenA8DJUSBClose(void)
{
    pthread_mutex_lock(&gEngineMutex);
    [gEngine close];
    gEngine = nil;
    pthread_mutex_unlock(&gEngineMutex);
}

uint32_t OpenA8DJUSBReadInput(float *outInterleaved, uint32_t frames, uint32_t channels)
{
    pthread_mutex_lock(&gEngineMutex);
    OpenA8DJUSBEngine *engine = gEngine;
    pthread_mutex_unlock(&gEngineMutex);
    if (engine == nil) {
        memset(outInterleaved, 0, (size_t)frames * channels * sizeof(float));
        return 0;
    }
    return [engine readInput:outInterleaved frames:frames channels:channels];
}

void OpenA8DJUSBWriteOutput(const float *inInterleaved, uint32_t frames, uint32_t channels)
{
    pthread_mutex_lock(&gEngineMutex);
    OpenA8DJUSBEngine *engine = gEngine;
    pthread_mutex_unlock(&gEngineMutex);
    if (engine != nil) {
        [engine writeOutput:inInterleaved frames:frames channels:channels];
    }
}

__attribute__((destructor))
static void OpenA8DJUSBDestructor(void)
{
    OpenA8DJUSBClose();
}
