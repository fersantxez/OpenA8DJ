#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct ToneState {
    UInt32 pairIndex;
    double frequency;
    double amplitude;
    double sampleRate;
    double phase;
    UInt64 callbacks;
} ToneState;

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
    (void)inInputData;
    (void)inInputTime;
    (void)inOutputTime;

    ToneState *state = (ToneState *)inClientData;
    state->callbacks++;
    if (outOutputData == NULL) {
        return kAudioHardwareNoError;
    }

    for (UInt32 bufferIndex = 0; bufferIndex < outOutputData->mNumberBuffers; bufferIndex++) {
        AudioBuffer *buffer = &outOutputData->mBuffers[bufferIndex];
        Float32 *samples = (Float32 *)buffer->mData;
        UInt32 channels = buffer->mNumberChannels;
        UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(Float32);
        if (samples == NULL || channels == 0) {
            continue;
        }
        memset(samples, 0, buffer->mDataByteSize);

        if (outOutputData->mNumberBuffers >= 4) {
            if (bufferIndex != state->pairIndex || channels < 2) {
                continue;
            }
            for (UInt32 i = 0; i + 1 < sampleCount; i += channels) {
                Float32 value = (Float32)(state->amplitude * sin(state->phase));
                state->phase += (2.0 * M_PI * state->frequency) / state->sampleRate;
                if (state->phase >= 2.0 * M_PI) {
                    state->phase -= 2.0 * M_PI;
                }
                samples[i] = value;
                samples[i + 1] = value;
            }
        } else {
            UInt32 left = state->pairIndex * 2;
            UInt32 right = left + 1;
            if (channels <= right) {
                continue;
            }
            for (UInt32 i = 0; i < sampleCount; i += channels) {
                Float32 value = (Float32)(state->amplitude * sin(state->phase));
                state->phase += (2.0 * M_PI * state->frequency) / state->sampleRate;
                if (state->phase >= 2.0 * M_PI) {
                    state->phase -= 2.0 * M_PI;
                }
                samples[i + left] = value;
                samples[i + right] = value;
            }
        }
    }
    return kAudioHardwareNoError;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: audio-pair-tone <A|B|C|D|0-3> [seconds] [frequency] [amplitude]\n");
        return 2;
    }

    UInt32 pairIndex = 0;
    if (argv[1][0] >= 'A' && argv[1][0] <= 'D') {
        pairIndex = (UInt32)(argv[1][0] - 'A');
    } else if (argv[1][0] >= 'a' && argv[1][0] <= 'd') {
        pairIndex = (UInt32)(argv[1][0] - 'a');
    } else {
        pairIndex = (UInt32)strtoul(argv[1], NULL, 10);
    }
    if (pairIndex > 3) {
        fprintf(stderr, "pair must be A-D or 0-3\n");
        return 3;
    }

    int seconds = argc > 2 ? atoi(argv[2]) : 5;
    if (seconds <= 0 || seconds > 60) {
        seconds = 5;
    }
    double frequency = argc > 3 ? strtod(argv[3], NULL) : (440.0 + (double)pairIndex * 220.0);
    double amplitude = argc > 4 ? strtod(argv[4], NULL) : 0.08;
    if (amplitude <= 0.0 || amplitude > 0.5) {
        amplitude = 0.08;
    }

    AudioObjectID device = FindDeviceByUID(CFSTR("org.opena8dj.Audio8DJ"));
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "Open Audio 8 DJ not found\n");
        return 4;
    }

    ToneState state;
    memset(&state, 0, sizeof(state));
    state.pairIndex = pairIndex;
    state.frequency = frequency;
    state.amplitude = amplitude;
    state.sampleRate = 48000.0;
    UInt32 size = sizeof(state.sampleRate);
    (void)GetProperty(device,
                      kAudioDevicePropertyNominalSampleRate,
                      kAudioObjectPropertyScopeGlobal,
                      &size,
                      &state.sampleRate);

    AudioDeviceIOProcID ioProcID = NULL;
    OSStatus status = AudioDeviceCreateIOProcID(device, IOProc, &state, &ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        return 5;
    }
    status = AudioDeviceStart(device, ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        return 6;
    }

    printf("Output %c tone %.1f Hz for %d seconds\n", 'A' + pairIndex, frequency, seconds);
    fflush(stdout);
    sleep((unsigned int)seconds);

    AudioDeviceStop(device, ioProcID);
    AudioDeviceDestroyIOProcID(device, ioProcID);
    printf("done callbacks=%llu\n", state.callbacks);
    return state.callbacks > 0 ? 0 : 7;
}
