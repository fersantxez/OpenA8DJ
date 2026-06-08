#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct RecorderState {
    int fd;
    atomic_bool active;
    double sampleRate;
    UInt32 channels;
    UInt64 framesWritten;
    double squareSum;
    double peak;
} RecorderState;

static uint16_t ClampS16(float sample)
{
    if (sample > 1.0f) {
        sample = 1.0f;
    } else if (sample < -1.0f) {
        sample = -1.0f;
    }
    int32_t value = (int32_t)lrintf(sample * 32767.0f);
    return (uint16_t)(int16_t)value;
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

static void WriteWAVHeader(int fd, double sampleRate, UInt32 channels, uint32_t dataBytes)
{
    uint8_t header[44];
    memset(header, 0, sizeof(header));
    memcpy(header + 0, "RIFF", 4);
    WriteLE32(header + 4, 36u + dataBytes);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    WriteLE32(header + 16, 16);
    WriteLE16(header + 20, 1);
    WriteLE16(header + 22, (uint16_t)channels);
    WriteLE32(header + 24, (uint32_t)lrint(sampleRate));
    WriteLE32(header + 28, (uint32_t)lrint(sampleRate) * channels * sizeof(int16_t));
    WriteLE16(header + 32, (uint16_t)(channels * sizeof(int16_t)));
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

static AudioObjectID FindDeviceByUID(CFStringRef targetUID)
{
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &dataSize) != kAudioHardwareNoError) {
        return kAudioObjectUnknown;
    }
    UInt32 count = dataSize / (UInt32)sizeof(AudioObjectID);
    AudioObjectID *devices = calloc(count, sizeof(AudioObjectID));
    if (devices == NULL) {
        return kAudioObjectUnknown;
    }
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &dataSize, devices) != kAudioHardwareNoError) {
        free(devices);
        return kAudioObjectUnknown;
    }

    AudioObjectID found = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        CFStringRef uid = NULL;
        UInt32 uidSize = sizeof(uid);
        if (GetProperty(devices[i],
                        kAudioDevicePropertyDeviceUID,
                        kAudioObjectPropertyScopeGlobal,
                        &uidSize,
                        &uid) == kAudioHardwareNoError &&
            uid != NULL &&
            CFEqual(uid, targetUID)) {
            found = devices[i];
            CFRelease(uid);
            break;
        }
        if (uid != NULL) {
            CFRelease(uid);
        }
    }
    free(devices);
    return found;
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

    int16_t converted[4096];
    for (UInt32 bufferIndex = 0; bufferIndex < inInputData->mNumberBuffers; bufferIndex++) {
        const AudioBuffer *buffer = &inInputData->mBuffers[bufferIndex];
        const float *samples = (const float *)buffer->mData;
        UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(float);
        if (samples == NULL || sampleCount == 0) {
            continue;
        }

        UInt32 offset = 0;
        while (offset < sampleCount) {
            UInt32 todo = sampleCount - offset;
            if (todo > (UInt32)(sizeof(converted) / sizeof(converted[0]))) {
                todo = (UInt32)(sizeof(converted) / sizeof(converted[0]));
            }
            for (UInt32 i = 0; i < todo; i++) {
                float sample = samples[offset + i];
                converted[i] = (int16_t)ClampS16(sample);
                double value = sample;
                state->squareSum += value * value;
                double absValue = fabs(value);
                if (absValue > state->peak) {
                    state->peak = absValue;
                }
            }
            (void)write(state->fd, converted, (size_t)todo * sizeof(converted[0]));
            offset += todo;
        }
        UInt32 channels = buffer->mNumberChannels > 0 ? buffer->mNumberChannels : 1;
        state->framesWritten += sampleCount / channels;
    }
    return kAudioHardwareNoError;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: macbook-mic-record <seconds> <output.wav>\n");
        return 2;
    }

    int seconds = atoi(argv[1]);
    if (seconds <= 0 || seconds > 120) {
        fprintf(stderr, "seconds must be between 1 and 120\n");
        return 3;
    }

    fprintf(stderr, "finding BuiltInMicrophoneDevice\n");
    fflush(stderr);
    AudioObjectID device = FindDeviceByUID(CFSTR("BuiltInMicrophoneDevice"));
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "BuiltInMicrophoneDevice not found\n");
        return 4;
    }
    fprintf(stderr, "device id=%u\n", device);
    fflush(stderr);

    AudioStreamBasicDescription format;
    UInt32 formatSize = sizeof(format);
    memset(&format, 0, sizeof(format));
    fprintf(stderr, "reading input stream format\n");
    fflush(stderr);
    OSStatus status = GetProperty(device,
                                  kAudioDevicePropertyStreamFormat,
                                  kAudioDevicePropertyScopeInput,
                                  &formatSize,
                                  &format);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "could not read mic stream format: %d\n", (int)status);
        return 5;
    }
    if (format.mFormatID != kAudioFormatLinearPCM ||
        (format.mFormatFlags & kAudioFormatFlagIsFloat) == 0 ||
        format.mBitsPerChannel != 32) {
        fprintf(stderr,
                "unsupported mic format id=%u flags=%u bits=%u\n",
                (unsigned)format.mFormatID,
                (unsigned)format.mFormatFlags,
                (unsigned)format.mBitsPerChannel);
        return 6;
    }

    int fd = creat(argv[2], 0644);
    if (fd < 0) {
        perror("creat");
        return 7;
    }
    UInt32 channels = format.mChannelsPerFrame > 0 ? format.mChannelsPerFrame : 1;
    WriteWAVHeader(fd, format.mSampleRate, channels, 0);
    (void)lseek(fd, 44, SEEK_SET);

    RecorderState state;
    memset(&state, 0, sizeof(state));
    state.fd = fd;
    atomic_init(&state.active, true);
    state.sampleRate = format.mSampleRate;
    state.channels = channels;

    fprintf(stderr, "creating IOProc rate=%.0f channels=%u\n", format.mSampleRate, (unsigned)channels);
    fflush(stderr);
    AudioDeviceIOProcID ioProcID = NULL;
    status = AudioDeviceCreateIOProcID(device, IOProc, &state, &ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        close(fd);
        return 8;
    }
    fprintf(stderr, "starting AudioDevice\n");
    fflush(stderr);
    status = AudioDeviceStart(device, ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        close(fd);
        return 9;
    }

    fprintf(stderr, "recording\n");
    fflush(stderr);
    sleep((unsigned int)seconds);

    fprintf(stderr, "stopping\n");
    fflush(stderr);
    atomic_store(&state.active, false);

    uint64_t dataBytes64 = state.framesWritten * state.channels * sizeof(int16_t);
    uint32_t dataBytes = dataBytes64 > UINT32_MAX ? UINT32_MAX : (uint32_t)dataBytes64;
    WriteWAVHeader(fd, state.sampleRate, state.channels, dataBytes);
    close(fd);

    double rms = 0.0;
    if (state.framesWritten > 0) {
        rms = sqrt(state.squareSum / ((double)state.framesWritten * state.channels));
    }
    printf("recorded path=%s seconds=%d rate=%.0f channels=%u frames=%llu rms=%.8f peak=%.8f\n",
           argv[2],
           seconds,
           state.sampleRate,
           (unsigned)state.channels,
           state.framesWritten,
           rms,
           state.peak);
    fflush(stdout);
    fflush(stderr);
    _exit(state.framesWritten > 0 ? 0 : 10);
}
