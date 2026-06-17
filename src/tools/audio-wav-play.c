#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

typedef struct WavData {
    double sampleRate;
    uint16_t channels;
    uint32_t frames;
    float *samples;
} WavData;

typedef struct PlayerState {
    WavData wav;
    UInt32 pairIndex;
    int useStreamUsage;
    atomic_uint frameIndex;
    atomic_bool done;
    atomic_ullong startNsec;
    atomic_ullong firstCallbackNsec;
    atomic_ullong doneNsec;
    UInt64 callbacks;
} PlayerState;

static uint64_t MonotonicNsec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

static uint16_t ReadLE16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ReadLE32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int ReadWav(const char *path, WavData *out)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("fopen");
        return 0;
    }
    uint8_t header[12];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0) {
        fclose(file);
        fprintf(stderr, "not a RIFF/WAVE file\n");
        return 0;
    }

    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bits = 0;
    uint8_t *data = NULL;
    uint32_t dataBytes = 0;

    for (;;) {
        uint8_t chunkHeader[8];
        if (fread(chunkHeader, 1, sizeof(chunkHeader), file) != sizeof(chunkHeader)) {
            break;
        }
        uint32_t chunkSize = ReadLE32(chunkHeader + 4);
        long next = ftell(file) + (long)chunkSize + (chunkSize & 1u);
        if (memcmp(chunkHeader, "fmt ", 4) == 0) {
            uint8_t fmt[40];
            if (chunkSize > sizeof(fmt)) {
                fclose(file);
                fprintf(stderr, "fmt chunk too large\n");
                return 0;
            }
            if (fread(fmt, 1, chunkSize, file) != chunkSize || chunkSize < 16) {
                fclose(file);
                fprintf(stderr, "short fmt chunk\n");
                return 0;
            }
            format = ReadLE16(fmt);
            channels = ReadLE16(fmt + 2);
            sampleRate = ReadLE32(fmt + 4);
            bits = ReadLE16(fmt + 14);
        } else if (memcmp(chunkHeader, "data", 4) == 0) {
            data = malloc(chunkSize);
            if (data == NULL || fread(data, 1, chunkSize, file) != chunkSize) {
                free(data);
                fclose(file);
                fprintf(stderr, "could not read data chunk\n");
                return 0;
            }
            dataBytes = chunkSize;
        }
        if (fseek(file, next, SEEK_SET) != 0) {
            break;
        }
    }
    fclose(file);

    if (data == NULL || channels == 0 || sampleRate == 0 || format != 1 || bits != 16) {
        free(data);
        fprintf(stderr, "unsupported WAV: need PCM16, got format=%u bits=%u channels=%u rate=%u\n",
                format,
                bits,
                channels,
                sampleRate);
        return 0;
    }

    uint32_t frames = dataBytes / ((uint32_t)channels * sizeof(int16_t));
    float *samples = calloc((size_t)frames * channels, sizeof(float));
    if (samples == NULL) {
        free(data);
        return 0;
    }
    for (uint32_t i = 0; i < frames * channels; i++) {
        int16_t value = (int16_t)ReadLE16(data + i * sizeof(int16_t));
        samples[i] = (float)value / 32768.0f;
    }
    free(data);

    out->sampleRate = (double)sampleRate;
    out->channels = channels;
    out->frames = frames;
    out->samples = samples;
    return 1;
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

static OSStatus SetProperty(AudioObjectID objectID,
                            AudioObjectPropertySelector selector,
                            AudioObjectPropertyScope scope,
                            UInt32 dataSize,
                            const void *data)
{
    AudioObjectPropertyAddress address = {
        selector,
        scope,
        kAudioObjectPropertyElementMain
    };
    return AudioObjectSetPropertyData(objectID, &address, 0, NULL, dataSize, data);
}

static void ConfigureOutputStreamUsage(AudioObjectID device,
                                       AudioDeviceIOProcID ioProcID,
                                       UInt32 pairIndex)
{
    struct UsagePayload {
        AudioDeviceIOProcID ioProcID;
        UInt32 numberStreams;
        UInt32 streamIsOn[4];
    } usage;
    memset(&usage, 0, sizeof(usage));
    usage.ioProcID = ioProcID;
    usage.numberStreams = 4;
    if (pairIndex < 4) {
        usage.streamIsOn[pairIndex] = 1;
    }
    OSStatus status = SetProperty(device,
                                  kAudioDevicePropertyIOProcStreamUsage,
                                  kAudioObjectPropertyScopeOutput,
                                  sizeof(usage),
                                  &usage);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr,
                "warning: output stream usage was not accepted: %d\n",
                (int)status);
    }
}

static void ConfigureInputStreamUsage(AudioObjectID device,
                                      AudioDeviceIOProcID ioProcID)
{
    struct UsagePayload {
        AudioDeviceIOProcID ioProcID;
        UInt32 numberStreams;
        UInt32 streamIsOn[4];
    } usage;
    memset(&usage, 0, sizeof(usage));
    usage.ioProcID = ioProcID;
    usage.numberStreams = 4;
    OSStatus status = SetProperty(device,
                                  kAudioDevicePropertyIOProcStreamUsage,
                                  kAudioObjectPropertyScopeInput,
                                  sizeof(usage),
                                  &usage);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr,
                "warning: input stream usage was not accepted: %d\n",
                (int)status);
    }
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
    (void)inInputData;
    (void)inInputTime;
    (void)inOutputTime;

    PlayerState *state = (PlayerState *)inClientData;
    state->callbacks++;
    uint64_t now = MonotonicNsec();
    uint64_t expected = 0;
    atomic_compare_exchange_strong(&state->firstCallbackNsec, &expected, now);
    if (outOutputData == NULL || atomic_load(&state->done)) {
        return kAudioHardwareNoError;
    }

    for (UInt32 bufferIndex = 0; bufferIndex < outOutputData->mNumberBuffers; bufferIndex++) {
        AudioBuffer *buffer = &outOutputData->mBuffers[bufferIndex];
        memset(buffer->mData, 0, buffer->mDataByteSize);
    }

    AudioBuffer *target = NULL;
    if (outOutputData->mNumberBuffers >= 4) {
        target = &outOutputData->mBuffers[state->pairIndex];
    } else if (outOutputData->mNumberBuffers > 0) {
        target = &outOutputData->mBuffers[0];
    }
    if (target == NULL || target->mData == NULL || target->mNumberChannels < 2) {
        return kAudioHardwareNoError;
    }

    float *out = (float *)target->mData;
    UInt32 outChannels = target->mNumberChannels;
    UInt32 outFrames = target->mDataByteSize / (UInt32)(sizeof(float) * outChannels);
    uint32_t frameIndex = atomic_load(&state->frameIndex);
    for (UInt32 frame = 0; frame < outFrames; frame++) {
        if (frameIndex >= state->wav.frames) {
            atomic_store(&state->done, true);
            expected = 0;
            atomic_compare_exchange_strong(&state->doneNsec, &expected, now);
            break;
        }
        float left = state->wav.samples[(size_t)frameIndex * state->wav.channels];
        float right = state->wav.channels > 1 ?
            state->wav.samples[(size_t)frameIndex * state->wav.channels + 1] :
            left;

        if (outOutputData->mNumberBuffers >= 4) {
            out[(size_t)frame * outChannels] = left;
            out[(size_t)frame * outChannels + 1] = right;
        } else {
            UInt32 leftChannel = state->pairIndex * 2;
            UInt32 rightChannel = leftChannel + 1;
            if (rightChannel < outChannels) {
                out[(size_t)frame * outChannels + leftChannel] = left;
                out[(size_t)frame * outChannels + rightChannel] = right;
            }
        }
        frameIndex++;
    }
    atomic_store(&state->frameIndex, frameIndex);
    return kAudioHardwareNoError;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: audio-wav-play <file.wav> [A|B|C|D|0-3] [--stream-usage|--no-stream-usage]\n");
        return 2;
    }

    PlayerState state;
    memset(&state, 0, sizeof(state));
    if (!ReadWav(argv[1], &state.wav)) {
        return 3;
    }

    state.pairIndex = 0;
    state.useStreamUsage = 1;
    for (int arg = 2; arg < argc; arg++) {
        const char *value = argv[arg];
        if (strcmp(value, "--stream-usage") == 0) {
            state.useStreamUsage = 1;
        } else if (strcmp(value, "--no-stream-usage") == 0) {
            state.useStreamUsage = 0;
        } else if (value[0] >= 'A' && value[0] <= 'D') {
            state.pairIndex = (UInt32)(value[0] - 'A');
        } else if (value[0] >= 'a' && value[0] <= 'd') {
            state.pairIndex = (UInt32)(value[0] - 'a');
        } else {
            char *end = NULL;
            unsigned long pair = strtoul(value, &end, 10);
            if (end == value || *end != '\0') {
                fprintf(stderr, "unknown argument: %s\n", value);
                free(state.wav.samples);
                return 2;
            }
            state.pairIndex = (UInt32)pair;
        }
    }
    if (state.pairIndex > 3) {
        fprintf(stderr, "pair must be A-D or 0-3\n");
        free(state.wav.samples);
        return 4;
    }
    atomic_init(&state.frameIndex, 0);
    atomic_init(&state.done, false);
    atomic_init(&state.startNsec, 0);
    atomic_init(&state.firstCallbackNsec, 0);
    atomic_init(&state.doneNsec, 0);

    AudioObjectID device = FindDeviceByUID(CFSTR("org.opena8dj.Audio8DJ"));
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "Open Audio 8 DJ not found\n");
        free(state.wav.samples);
        return 5;
    }

    double rate = state.wav.sampleRate;
    (void)SetProperty(device,
                      kAudioDevicePropertyNominalSampleRate,
                      kAudioObjectPropertyScopeGlobal,
                      sizeof(rate),
                      &rate);

    AudioDeviceIOProcID ioProcID = NULL;
    OSStatus status = AudioDeviceCreateIOProcID(device, IOProc, &state, &ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        free(state.wav.samples);
        return 6;
    }
    if (state.useStreamUsage) {
        ConfigureInputStreamUsage(device, ioProcID);
        ConfigureOutputStreamUsage(device, ioProcID, state.pairIndex);
    }
    atomic_store(&state.startNsec, MonotonicNsec());
    status = AudioDeviceStart(device, ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        free(state.wav.samples);
        return 7;
    }

    while (!atomic_load(&state.done)) {
        usleep(10000);
    }
    usleep(100000);
    AudioDeviceStop(device, ioProcID);
    AudioDeviceDestroyIOProcID(device, ioProcID);

    uint64_t startNsec = atomic_load(&state.startNsec);
    uint64_t firstCallbackNsec = atomic_load(&state.firstCallbackNsec);
    uint64_t doneNsec = atomic_load(&state.doneNsec);
    double firstCallbackSeconds = firstCallbackNsec > startNsec ?
        (double)(firstCallbackNsec - startNsec) / 1000000000.0 :
        -1.0;
    double doneSeconds = doneNsec > startNsec ?
        (double)(doneNsec - startNsec) / 1000000000.0 :
        -1.0;
    printf("played path=%s pair=%c frames=%u callbacks=%llu stream_usage=%s first_callback_seconds=%.6f done_seconds=%.6f\n",
           argv[1],
           'A' + state.pairIndex,
           atomic_load(&state.frameIndex),
           state.callbacks,
           state.useStreamUsage ? "on" : "off",
           firstCallbackSeconds,
           doneSeconds);
    free(state.wav.samples);
    return state.callbacks > 0 ? 0 : 8;
}
