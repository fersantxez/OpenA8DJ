#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    kPairs = 4,
    kChannelsPerPair = 2
};

typedef struct PairStats {
    UInt64 frames;
    double leftSquare;
    double rightSquare;
    double cross;
    double leftPeak;
    double rightPeak;
} PairStats;

typedef struct MeterState {
    PairStats pairs[kPairs];
    UInt64 callbacks;
} MeterState;

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

static OSStatus SetIOProcStreamUsage(AudioDeviceID device,
                                     AudioDeviceIOProcID ioProcID,
                                     AudioObjectPropertyScope scope,
                                     bool enabled)
{
    AudioObjectPropertyAddress streamAddress = {
        kAudioDevicePropertyStreams,
        scope,
        kAudioObjectPropertyElementMain
    };
    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(device, &streamAddress, 0, NULL, &dataSize);
    if (status != kAudioHardwareNoError || dataSize == 0) {
        return status;
    }

    UInt32 count = dataSize / (UInt32)sizeof(AudioStreamID);
    size_t usageSize = offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) + sizeof(UInt32) * count;
    AudioHardwareIOProcStreamUsage *usage = calloc(1, usageSize);
    if (usage == NULL) {
        return kAudioHardwareUnspecifiedError;
    }

    usage->mIOProc = (void *)ioProcID;
    usage->mNumberStreams = count;
    for (UInt32 i = 0; i < count; i++) {
        usage->mStreamIsOn[i] = enabled ? 1 : 0;
    }

    AudioObjectPropertyAddress usageAddress = {
        kAudioDevicePropertyIOProcStreamUsage,
        scope,
        kAudioObjectPropertyElementMain
    };
    status = AudioObjectSetPropertyData(device, &usageAddress, 0, NULL, (UInt32)usageSize, usage);
    free(usage);
    return status;
}

static void AddStereoBuffer(PairStats *stats, const float *samples, UInt32 sampleCount, UInt32 channels)
{
    if (stats == NULL || samples == NULL || channels < kChannelsPerPair) {
        return;
    }

    for (UInt32 i = 0; i + 1 < sampleCount; i += channels) {
        double left = samples[i];
        double right = samples[i + 1];
        stats->leftSquare += left * left;
        stats->rightSquare += right * right;
        stats->cross += left * right;
        double leftAbs = fabs(left);
        double rightAbs = fabs(right);
        if (leftAbs > stats->leftPeak) {
            stats->leftPeak = leftAbs;
        }
        if (rightAbs > stats->rightPeak) {
            stats->rightPeak = rightAbs;
        }
        stats->frames++;
    }
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

    MeterState *state = (MeterState *)inClientData;
    if (state == NULL) {
        return kAudioHardwareNoError;
    }
    state->callbacks++;

    if (inInputData == NULL) {
        return kAudioHardwareNoError;
    }

    if (inInputData->mNumberBuffers >= kPairs) {
        for (UInt32 pair = 0; pair < kPairs; pair++) {
            const AudioBuffer *buffer = &inInputData->mBuffers[pair];
            const float *samples = (const float *)buffer->mData;
            UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(float);
            AddStereoBuffer(&state->pairs[pair], samples, sampleCount, buffer->mNumberChannels);
        }
    } else {
        for (UInt32 bufferIndex = 0; bufferIndex < inInputData->mNumberBuffers; bufferIndex++) {
            const AudioBuffer *buffer = &inInputData->mBuffers[bufferIndex];
            const float *samples = (const float *)buffer->mData;
            UInt32 channels = buffer->mNumberChannels;
            UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(float);
            if (samples == NULL || channels < kPairs * kChannelsPerPair) {
                continue;
            }
            for (UInt32 frame = 0; frame < sampleCount / channels; frame++) {
                const float *src = &samples[(size_t)frame * channels];
                for (UInt32 pair = 0; pair < kPairs; pair++) {
                    PairStats *stats = &state->pairs[pair];
                    double left = src[pair * 2];
                    double right = src[pair * 2 + 1];
                    stats->leftSquare += left * left;
                    stats->rightSquare += right * right;
                    stats->cross += left * right;
                    double leftAbs = fabs(left);
                    double rightAbs = fabs(right);
                    if (leftAbs > stats->leftPeak) stats->leftPeak = leftAbs;
                    if (rightAbs > stats->rightPeak) stats->rightPeak = rightAbs;
                    stats->frames++;
                }
            }
        }
    }

    return kAudioHardwareNoError;
}

static void PrintPair(char name, const PairStats *stats)
{
    double leftRMS = 0.0;
    double rightRMS = 0.0;
    double correlation = 0.0;
    if (stats->frames > 0) {
        leftRMS = sqrt(stats->leftSquare / (double)stats->frames);
        rightRMS = sqrt(stats->rightSquare / (double)stats->frames);
        double denom = sqrt(stats->leftSquare * stats->rightSquare);
        if (denom > 0.0) {
            correlation = stats->cross / denom;
        }
    }
    printf("Input %c: frames=%llu rmsL=%.8f rmsR=%.8f peakL=%.8f peakR=%.8f corr=%.4f\n",
           name,
           stats->frames,
           leftRMS,
           rightRMS,
           stats->leftPeak,
           stats->rightPeak,
           correlation);
}

int main(int argc, char **argv)
{
    int seconds = 5;
    if (argc > 1) {
        seconds = atoi(argv[1]);
        if (seconds <= 0 || seconds > 60) {
            seconds = 5;
        }
    }

    AudioDeviceID device = FindDeviceByUID(CFSTR("org.opena8dj.Audio8DJ"));
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "Open Audio 8 DJ not found.\n");
        return 2;
    }

    MeterState state;
    memset(&state, 0, sizeof(state));

    AudioDeviceIOProcID ioProcID = NULL;
    OSStatus status = AudioDeviceCreateIOProcID(device, IOProc, &state, &ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        return 3;
    }

    (void)SetIOProcStreamUsage(device, ioProcID, kAudioObjectPropertyScopeInput, true);
    (void)SetIOProcStreamUsage(device, ioProcID, kAudioObjectPropertyScopeOutput, false);

    status = AudioDeviceStart(device, ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        return 4;
    }

    sleep((unsigned int)seconds);

    AudioDeviceStop(device, ioProcID);
    AudioDeviceDestroyIOProcID(device, ioProcID);

    printf("callbacks=%llu seconds=%d\n", state.callbacks, seconds);
    PrintPair('A', &state.pairs[0]);
    PrintPair('B', &state.pairs[1]);
    PrintPair('C', &state.pairs[2]);
    PrintPair('D', &state.pairs[3]);
    return state.callbacks > 0 ? 0 : 5;
}
