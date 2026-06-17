#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>

typedef struct RecorderState {
    int fd;
    atomic_bool active;
    double sampleRate;
    UInt32 leftChannel;
    UInt32 rightChannel;
    double energyThreshold;
    UInt64 framesWritten;
    atomic_ullong startNsec;
    atomic_ullong firstCallbackNsec;
    atomic_ullong firstEnergyFrame;
    atomic_ullong firstEnergyNsec;
    double squareSum;
    double peak;
    UInt64 clipped;
} RecorderState;

static uint64_t MonotonicNsec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

static int16_t ClampS16(float sample, UInt64 *clipped)
{
    if (sample > 1.0f) {
        sample = 1.0f;
        (*clipped)++;
    } else if (sample < -1.0f) {
        sample = -1.0f;
        (*clipped)++;
    }
    return (int16_t)lrintf(sample * 32767.0f);
}

static void WriteLE16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xff);
    p[1] = (uint8_t)(value >> 8);
}

static void WriteLE32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xff);
    p[1] = (uint8_t)((value >> 8) & 0xff);
    p[2] = (uint8_t)((value >> 16) & 0xff);
    p[3] = (uint8_t)(value >> 24);
}

static void WriteWAVHeader(int fd, double sampleRate, uint32_t dataBytes)
{
    uint8_t header[44];
    memset(header, 0, sizeof(header));
    memcpy(header + 0, "RIFF", 4);
    WriteLE32(header + 4, 36u + dataBytes);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    WriteLE32(header + 16, 16);
    WriteLE16(header + 20, 1);
    WriteLE16(header + 22, 2);
    WriteLE32(header + 24, (uint32_t)lrint(sampleRate));
    WriteLE32(header + 28, (uint32_t)lrint(sampleRate) * 2u * sizeof(int16_t));
    WriteLE16(header + 32, 2u * sizeof(int16_t));
    WriteLE16(header + 34, 16);
    memcpy(header + 36, "data", 4);
    WriteLE32(header + 40, dataBytes);
    (void)pwrite(fd, header, sizeof(header), 0);
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

static OSStatus GetAudioObjectIDProperty(AudioObjectID objectID,
                                         AudioObjectPropertySelector selector,
                                         AudioObjectID *outObjectID)
{
    UInt32 size = sizeof(*outObjectID);
    return GetProperty(objectID, selector, kAudioObjectPropertyScopeGlobal, &size, outObjectID);
}

static bool CopyCString(CFStringRef string, char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return false;
    }
    buffer[0] = 0;
    return string != NULL && CFStringGetCString(string, buffer, size, kCFStringEncodingUTF8);
}

static bool StringEqualsCString(CFStringRef string, const char *text)
{
    if (string == NULL || text == NULL) {
        return false;
    }
    CFStringRef target = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
    if (target == NULL) {
        return false;
    }
    Boolean equal = CFEqual(string, target);
    CFRelease(target);
    return equal;
}

static bool StringContainsCString(CFStringRef string, const char *text)
{
    if (string == NULL || text == NULL || text[0] == 0) {
        return false;
    }
    CFStringRef needle = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
    if (needle == NULL) {
        return false;
    }
    CFRange found = CFStringFind(string, needle, kCFCompareCaseInsensitive);
    CFRelease(needle);
    return found.location != kCFNotFound;
}

static UInt32 CountInputChannels(AudioObjectID deviceID)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(deviceID, &address, 0, NULL, &dataSize) != kAudioHardwareNoError ||
        dataSize == 0) {
        return 0;
    }

    AudioBufferList *buffers = (AudioBufferList *)calloc(1, dataSize);
    if (buffers == NULL) {
        return 0;
    }
    OSStatus status = AudioObjectGetPropertyData(deviceID, &address, 0, NULL, &dataSize, buffers);
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

static AudioObjectID FindInputDevice(const char *requested)
{
    if (requested == NULL ||
        requested[0] == 0 ||
        strcmp(requested, "default") == 0 ||
        strcmp(requested, "default-input") == 0) {
        AudioObjectID device = kAudioObjectUnknown;
        OSStatus status = GetAudioObjectIDProperty(kAudioObjectSystemObject,
                                                   kAudioHardwarePropertyDefaultInputDevice,
                                                   &device);
        return status == kAudioHardwareNoError ? device : kAudioObjectUnknown;
    }

    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &dataSize) != kAudioHardwareNoError ||
        dataSize == 0) {
        return kAudioObjectUnknown;
    }
    UInt32 count = dataSize / (UInt32)sizeof(AudioObjectID);
    AudioObjectID *devices = (AudioObjectID *)calloc(count, sizeof(AudioObjectID));
    if (devices == NULL) {
        return kAudioObjectUnknown;
    }
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &dataSize, devices) != kAudioHardwareNoError) {
        free(devices);
        return kAudioObjectUnknown;
    }

    AudioObjectID found = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        if (CountInputChannels(devices[i]) == 0) {
            continue;
        }
        CFStringRef uid = NULL;
        CFStringRef name = NULL;
        UInt32 size = sizeof(uid);
        (void)GetProperty(devices[i],
                          kAudioDevicePropertyDeviceUID,
                          kAudioObjectPropertyScopeGlobal,
                          &size,
                          &uid);
        size = sizeof(name);
        (void)GetProperty(devices[i],
                          kAudioObjectPropertyName,
                          kAudioObjectPropertyScopeGlobal,
                          &size,
                          &name);
        if (StringEqualsCString(uid, requested) ||
            StringEqualsCString(name, requested) ||
            StringContainsCString(name, requested)) {
            found = devices[i];
        }
        if (uid != NULL) {
            CFRelease(uid);
        }
        if (name != NULL) {
            CFRelease(name);
        }
        if (found != kAudioObjectUnknown) {
            break;
        }
    }

    free(devices);
    return found;
}

static UInt32 InputFrameCount(const AudioBufferList *inputData)
{
    if (inputData == NULL) {
        return 0;
    }
    UInt32 best = 0;
    for (UInt32 bufferIndex = 0; bufferIndex < inputData->mNumberBuffers; bufferIndex++) {
        const AudioBuffer *buffer = &inputData->mBuffers[bufferIndex];
        UInt32 channels = buffer->mNumberChannels > 0 ? buffer->mNumberChannels : 1;
        UInt32 samples = buffer->mDataByteSize / (UInt32)sizeof(float);
        UInt32 frames = samples / channels;
        if (frames > best) {
            best = frames;
        }
    }
    return best;
}

static bool ReadInputSample(const AudioBufferList *inputData,
                            UInt32 globalChannel,
                            UInt32 frame,
                            float *outSample)
{
    UInt32 baseChannel = 0;
    if (outSample != NULL) {
        *outSample = 0.0f;
    }
    if (inputData == NULL || outSample == NULL) {
        return false;
    }
    for (UInt32 bufferIndex = 0; bufferIndex < inputData->mNumberBuffers; bufferIndex++) {
        const AudioBuffer *buffer = &inputData->mBuffers[bufferIndex];
        const float *samples = (const float *)buffer->mData;
        UInt32 channels = buffer->mNumberChannels;
        if (samples == NULL || channels == 0) {
            baseChannel += channels;
            continue;
        }
        if (globalChannel < baseChannel + channels) {
            UInt32 localChannel = globalChannel - baseChannel;
            UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(float);
            UInt32 frames = sampleCount / channels;
            if (frame >= frames) {
                return false;
            }
            *outSample = samples[(size_t)frame * channels + localChannel];
            return true;
        }
        baseChannel += channels;
    }
    return false;
}

static OSStatus IOProc(AudioObjectID inDevice,
                       const AudioTimeStamp *inNow,
                       const AudioBufferList *inInputData,
                       const AudioTimeStamp *inInputTime,
                       AudioBufferList *outOutputData,
                       const AudioTimeStamp *inOutputTime,
                       void *inClientData)
{
    (void)inDevice;
    (void)inNow;
    (void)inInputTime;
    (void)outOutputData;
    (void)inOutputTime;

    RecorderState *state = (RecorderState *)inClientData;
    if (state == NULL || state->fd < 0 || inInputData == NULL || !atomic_load(&state->active)) {
        return kAudioHardwareNoError;
    }
    uint64_t expected = 0;
    uint64_t now = MonotonicNsec();
    atomic_compare_exchange_strong(&state->firstCallbackNsec, &expected, now);

    UInt32 frameCount = InputFrameCount(inInputData);
    int16_t converted[4096 * 2];
    UInt32 frameOffset = 0;
    while (frameOffset < frameCount) {
        UInt32 todo = frameCount - frameOffset;
        if (todo > 4096) {
            todo = 4096;
        }
        for (UInt32 frame = 0; frame < todo; frame++) {
            float left = 0.0f;
            float right = 0.0f;
            (void)ReadInputSample(inInputData, state->leftChannel, frameOffset + frame, &left);
            (void)ReadInputSample(inInputData, state->rightChannel, frameOffset + frame, &right);
            converted[frame * 2] = ClampS16(left, &state->clipped);
            converted[frame * 2 + 1] = ClampS16(right, &state->clipped);

            double dl = left;
            double dr = right;
            state->squareSum += dl * dl + dr * dr;
            double leftAbs = fabs(dl);
            double rightAbs = fabs(dr);
            if (leftAbs >= state->energyThreshold || rightAbs >= state->energyThreshold) {
                uint64_t unset = UINT64_MAX;
                uint64_t absoluteFrame = state->framesWritten + frameOffset + frame;
                if (atomic_compare_exchange_strong(&state->firstEnergyFrame, &unset, absoluteFrame)) {
                    atomic_store(&state->firstEnergyNsec, MonotonicNsec());
                }
            }
            if (leftAbs > state->peak) {
                state->peak = leftAbs;
            }
            if (rightAbs > state->peak) {
                state->peak = rightAbs;
            }
        }
        (void)write(state->fd, converted, (size_t)todo * 2u * sizeof(converted[0]));
        state->framesWritten += todo;
        frameOffset += todo;
    }

    return kAudioHardwareNoError;
}

static bool ParseChannels(const char *text, UInt32 *left, UInt32 *right)
{
    if (text == NULL || left == NULL || right == NULL) {
        return false;
    }
    char *end = NULL;
    unsigned long first = strtoul(text, &end, 10);
    if (end == text || first == 0 || (*end != ',' && *end != ':' && *end != 0)) {
        return false;
    }
    unsigned long second = first;
    if (*end != 0) {
        const char *secondText = end + 1;
        char *secondEnd = NULL;
        second = strtoul(secondText, &secondEnd, 10);
        if (secondEnd == secondText || second == 0 || *secondEnd != 0) {
            return false;
        }
    }
    *left = (UInt32)(first - 1);
    *right = (UInt32)(second - 1);
    return true;
}

static void PrintUsage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s <seconds> <output.wav> [device|default] [left,right] [energy_threshold]\n"
            "  device may be a Core Audio UID, exact name, or name substring.\n"
            "  channels are 1-based input channels and default to 1,2.\n",
            argv0);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 2;
    }

    int seconds = atoi(argv[1]);
    if (seconds <= 0 || seconds > 600) {
        fprintf(stderr, "seconds must be between 1 and 600\n");
        return 3;
    }

    const char *deviceText = argc > 3 ? argv[3] : "default";
    UInt32 leftChannel = 0;
    UInt32 rightChannel = 1;
    if (argc > 4 && !ParseChannels(argv[4], &leftChannel, &rightChannel)) {
        fprintf(stderr, "invalid channel selection: %s\n", argv[4]);
        return 4;
    }
    double energyThreshold = 0.005;
    if (argc > 5) {
        char *end = NULL;
        energyThreshold = strtod(argv[5], &end);
        if (end == argv[5] || *end != 0 || energyThreshold <= 0.0 || energyThreshold > 1.0) {
            fprintf(stderr, "invalid energy threshold: %s\n", argv[5]);
            return 4;
        }
    }

    AudioObjectID device = FindInputDevice(deviceText);
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "input device not found: %s\n", deviceText);
        return 5;
    }

    UInt32 inputChannels = CountInputChannels(device);
    if (inputChannels == 0 || leftChannel >= inputChannels || rightChannel >= inputChannels) {
        fprintf(stderr,
                "input device has %u channels; requested %u,%u\n",
                inputChannels,
                leftChannel + 1,
                rightChannel + 1);
        return 6;
    }

    CFStringRef name = NULL;
    CFStringRef uid = NULL;
    UInt32 size = sizeof(name);
    (void)GetProperty(device, kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, &size, &name);
    size = sizeof(uid);
    (void)GetProperty(device, kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, &size, &uid);
    char nameBuffer[512];
    char uidBuffer[512];
    (void)CopyCString(name, nameBuffer, sizeof(nameBuffer));
    (void)CopyCString(uid, uidBuffer, sizeof(uidBuffer));

    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    UInt32 formatSize = sizeof(format);
    OSStatus status = GetProperty(device,
                                  kAudioDevicePropertyStreamFormat,
                                  kAudioDevicePropertyScopeInput,
                                  &formatSize,
                                  &format);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "could not read input stream format: %d\n", (int)status);
        if (name != NULL) CFRelease(name);
        if (uid != NULL) CFRelease(uid);
        return 7;
    }
    if (format.mFormatID != kAudioFormatLinearPCM ||
        (format.mFormatFlags & kAudioFormatFlagIsFloat) == 0 ||
        format.mBitsPerChannel != 32) {
        fprintf(stderr,
                "unsupported input format id=%u flags=%u bits=%u\n",
                (unsigned)format.mFormatID,
                (unsigned)format.mFormatFlags,
                (unsigned)format.mBitsPerChannel);
        if (name != NULL) CFRelease(name);
        if (uid != NULL) CFRelease(uid);
        return 8;
    }

    int fd = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        perror("open");
        if (name != NULL) CFRelease(name);
        if (uid != NULL) CFRelease(uid);
        return 9;
    }
    WriteWAVHeader(fd, format.mSampleRate, 0);
    (void)lseek(fd, 44, SEEK_SET);

    RecorderState state;
    memset(&state, 0, sizeof(state));
    state.fd = fd;
    state.sampleRate = format.mSampleRate;
    state.leftChannel = leftChannel;
    state.rightChannel = rightChannel;
    state.energyThreshold = energyThreshold;
    atomic_init(&state.active, true);
    atomic_init(&state.startNsec, 0);
    atomic_init(&state.firstCallbackNsec, 0);
    atomic_init(&state.firstEnergyFrame, UINT64_MAX);
    atomic_init(&state.firstEnergyNsec, 0);

    AudioDeviceIOProcID ioProcID = NULL;
    status = AudioDeviceCreateIOProcID(device, IOProc, &state, &ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        close(fd);
        if (name != NULL) CFRelease(name);
        if (uid != NULL) CFRelease(uid);
        return 10;
    }
    atomic_store(&state.startNsec, MonotonicNsec());
    status = AudioDeviceStart(device, ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        close(fd);
        if (name != NULL) CFRelease(name);
        if (uid != NULL) CFRelease(uid);
        return 11;
    }

    sleep((unsigned int)seconds);
    atomic_store(&state.active, false);
    AudioDeviceStop(device, ioProcID);
    AudioDeviceDestroyIOProcID(device, ioProcID);

    uint64_t dataBytes64 = state.framesWritten * 2u * sizeof(int16_t);
    uint32_t dataBytes = dataBytes64 > UINT32_MAX ? UINT32_MAX : (uint32_t)dataBytes64;
    WriteWAVHeader(fd, state.sampleRate, dataBytes);
    close(fd);

    double rms = 0.0;
    if (state.framesWritten > 0) {
        rms = sqrt(state.squareSum / ((double)state.framesWritten * 2.0));
    }
    uint64_t startNsec = atomic_load(&state.startNsec);
    uint64_t firstCallbackNsec = atomic_load(&state.firstCallbackNsec);
    uint64_t firstEnergyFrame = atomic_load(&state.firstEnergyFrame);
    uint64_t firstEnergyNsec = atomic_load(&state.firstEnergyNsec);
    double firstCallbackSeconds = firstCallbackNsec > startNsec ?
        (double)(firstCallbackNsec - startNsec) / 1000000000.0 :
        -1.0;
    double firstEnergyRecordSeconds = firstEnergyFrame != UINT64_MAX ?
        (double)firstEnergyFrame / state.sampleRate :
        -1.0;
    double firstEnergyWallSeconds = firstEnergyNsec > startNsec ?
        (double)(firstEnergyNsec - startNsec) / 1000000000.0 :
        -1.0;
    printf("recorded path=%s seconds=%d device=\"%s\" uid=\"%s\" rate=%.0f channels=%u,%u frames=%llu rms=%.8f peak=%.8f clipped=%llu start_nsec=%llu first_callback_nsec=%llu first_callback_seconds=%.6f first_energy_frame=%lld first_energy_nsec=%llu first_energy_record_seconds=%.6f first_energy_wall_seconds=%.6f first_energy_threshold=%.8f\n",
           argv[2],
           seconds,
           nameBuffer,
           uidBuffer,
           state.sampleRate,
           leftChannel + 1,
           rightChannel + 1,
           state.framesWritten,
           rms,
           state.peak,
           state.clipped,
           (unsigned long long)startNsec,
           (unsigned long long)firstCallbackNsec,
           firstCallbackSeconds,
           firstEnergyFrame == UINT64_MAX ? -1ll : (long long)firstEnergyFrame,
           (unsigned long long)firstEnergyNsec,
           firstEnergyRecordSeconds,
           firstEnergyWallSeconds,
           state.energyThreshold);

    if (name != NULL) {
        CFRelease(name);
    }
    if (uid != NULL) {
        CFRelease(uid);
    }
    return state.framesWritten > 0 ? 0 : 12;
}
