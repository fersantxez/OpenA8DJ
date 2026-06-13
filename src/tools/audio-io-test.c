#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct TestState {
    double phase;
    double sampleRate;
    UInt64 callbacks;
    UInt64 inputFrames;
    double inputSquareSum;
    double inputPeak;
    UInt64 outputBuffers;
    UInt64 outputSamples;
    UInt64 outputFrames;
    double outputPeak;
    double amplitude;
} TestState;

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

static OSStatus WaitForNominalSampleRate(AudioObjectID device, double requestedRate, double *outRate)
{
    double actualRate = 0.0;
    OSStatus status = kAudioHardwareNoError;
    for (int attempt = 0; attempt < 100; attempt++) {
        UInt32 dataSize = sizeof(actualRate);
        status = GetProperty(device,
                             kAudioDevicePropertyNominalSampleRate,
                             kAudioObjectPropertyScopeGlobal,
                             &dataSize,
                             &actualRate);
        if (status != kAudioHardwareNoError) {
            break;
        }
        if (fabs(actualRate - requestedRate) < 1.0) {
            if (outRate != NULL) {
                *outRate = actualRate;
            }
            return kAudioHardwareNoError;
        }
        usleep(20000);
    }
    if (outRate != NULL) {
        *outRate = actualRate;
    }
    return status == kAudioHardwareNoError ? kAudioHardwareIllegalOperationError : status;
}

static OSStatus WaitForBufferFrameSize(AudioObjectID device, UInt32 requestedBufferFrames)
{
    UInt32 actualBufferFrames = 0;
    OSStatus status = kAudioHardwareNoError;
    for (int attempt = 0; attempt < 100; attempt++) {
        UInt32 dataSize = sizeof(actualBufferFrames);
        status = GetProperty(device,
                             kAudioDevicePropertyBufferFrameSize,
                             kAudioObjectPropertyScopeGlobal,
                             &dataSize,
                             &actualBufferFrames);
        if (status != kAudioHardwareNoError) {
            break;
        }
        if (actualBufferFrames == requestedBufferFrames) {
            return kAudioHardwareNoError;
        }
        usleep(20000);
    }
    return status == kAudioHardwareNoError ? kAudioHardwareIllegalOperationError : status;
}

static UInt32 BufferFramesForStereoBytes(UInt32 bytes)
{
    UInt32 bytesPerFrame = (UInt32)(sizeof(float) * 2);
    return bytesPerFrame > 0 ? (bytes + bytesPerFrame - 1) / bytesPerFrame : 0;
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
    (void)inOutputTime;
    TestState *state = (TestState *)inClientData;
    state->callbacks++;

    if (inInputData != NULL) {
        for (UInt32 bufferIndex = 0; bufferIndex < inInputData->mNumberBuffers; bufferIndex++) {
            const AudioBuffer *buffer = &inInputData->mBuffers[bufferIndex];
            const float *samples = (const float *)buffer->mData;
            UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(float);
            state->inputFrames += buffer->mNumberChannels > 0 ? sampleCount / buffer->mNumberChannels : 0;
            for (UInt32 i = 0; i < sampleCount; i++) {
                double value = samples[i];
                state->inputSquareSum += value * value;
                double absValue = fabs(value);
                if (absValue > state->inputPeak) {
                    state->inputPeak = absValue;
                }
            }
        }
    }

    if (outOutputData != NULL) {
        for (UInt32 bufferIndex = 0; bufferIndex < outOutputData->mNumberBuffers; bufferIndex++) {
            AudioBuffer *buffer = &outOutputData->mBuffers[bufferIndex];
            float *samples = (float *)buffer->mData;
            UInt32 channels = buffer->mNumberChannels;
            UInt32 sampleCount = buffer->mDataByteSize / (UInt32)sizeof(float);
            if (samples == NULL || channels == 0 || sampleCount == 0) {
                continue;
            }
            state->outputBuffers++;
            state->outputSamples += sampleCount;
            state->outputFrames += sampleCount / channels;
            for (UInt32 i = 0; i < sampleCount; i += channels) {
                float value = (float)(state->amplitude * sin(state->phase));
                state->phase += (2.0 * M_PI * 440.0) / state->sampleRate;
                if (state->phase >= 2.0 * M_PI) {
                    state->phase -= 2.0 * M_PI;
                }
                for (UInt32 channel = 0; channel < channels && i + channel < sampleCount; channel++) {
                    samples[i + channel] = channel < 2 ? value : 0.0f;
                    double absValue = fabs(samples[i + channel]);
                    if (absValue > state->outputPeak) {
                        state->outputPeak = absValue;
                    }
                }
            }
        }
    }
    return kAudioHardwareNoError;
}

int main(int argc, char **argv)
{
    int seconds = 3;
    double requestedRate = 0.0;
    UInt32 requestedBufferFrames = 0;
    UInt32 requestedBufferBytes = 0;
    double amplitude = 0.02;
    if (argc > 1) {
        seconds = atoi(argv[1]);
        if (seconds <= 0 || seconds > 120) {
            fprintf(stderr, "seconds must be between 1 and 120\n");
            return 11;
        }
    }
    if (argc > 2) {
        requestedRate = strtod(argv[2], NULL);
    }
    if (argc > 3) {
        if (strncmp(argv[3], "bytes:", 6) == 0) {
            requestedBufferBytes = (UInt32)strtoul(argv[3] + 6, NULL, 10);
            requestedBufferFrames = BufferFramesForStereoBytes(requestedBufferBytes);
        } else {
            requestedBufferFrames = (UInt32)strtoul(argv[3], NULL, 10);
        }
    }
    if (argc > 4) {
        amplitude = strtod(argv[4], NULL);
        if (!isfinite(amplitude) || amplitude < 0.0 || amplitude > 1.0) {
            fprintf(stderr, "amplitude must be between 0.0 and 1.0\n");
            return 10;
        }
    }

    AudioObjectID device = FindDeviceByUID(CFSTR("org.opena8dj.Audio8DJ"));
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "No se encontro Open Audio 8 DJ en Core Audio.\n");
        return 2;
    }

    TestState state;
    memset(&state, 0, sizeof(state));
    state.sampleRate = 48000.0;
    state.amplitude = amplitude;
    if (requestedRate > 0.0) {
        OSStatus setRate = SetProperty(device,
                                       kAudioDevicePropertyNominalSampleRate,
                                       kAudioObjectPropertyScopeGlobal,
                                       sizeof(requestedRate),
                                       &requestedRate);
        if (setRate != kAudioHardwareNoError) {
            fprintf(stderr, "No se pudo cambiar sample rate a %.0f: %d\n", requestedRate, (int)setRate);
            return 6;
        }
        OSStatus waitRate = WaitForNominalSampleRate(device, requestedRate, &state.sampleRate);
        if (waitRate != kAudioHardwareNoError) {
            fprintf(stderr, "Core Audio no confirmo sample rate %.0f; actual %.0f: %d\n",
                    requestedRate,
                    state.sampleRate,
                    (int)waitRate);
            return 8;
        }
    }
    if (requestedBufferFrames > 0) {
        UInt32 bufferValue = requestedBufferBytes > 0 ? requestedBufferBytes : requestedBufferFrames;
        AudioObjectPropertySelector bufferSelector = requestedBufferBytes > 0 ?
            kAudioDevicePropertyBufferSize :
            kAudioDevicePropertyBufferFrameSize;
        OSStatus setBuffer = SetProperty(device,
                                         bufferSelector,
                                         kAudioObjectPropertyScopeGlobal,
                                         sizeof(bufferValue),
                                         &bufferValue);
        if (setBuffer != kAudioHardwareNoError) {
            fprintf(stderr,
                    "No se pudo cambiar buffer a %u %s: %d\n",
                    bufferValue,
                    requestedBufferBytes > 0 ? "bytes" : "frames",
                    (int)setBuffer);
            return 7;
        }
        OSStatus waitBuffer = WaitForBufferFrameSize(device, requestedBufferFrames);
        if (waitBuffer != kAudioHardwareNoError) {
            fprintf(stderr, "Core Audio no confirmo buffer de %u frames: %d\n",
                    requestedBufferFrames,
                    (int)waitBuffer);
            return 9;
        }
    }
    UInt32 dataSize = sizeof(state.sampleRate);
    (void)GetProperty(device,
                      kAudioDevicePropertyNominalSampleRate,
                      kAudioObjectPropertyScopeGlobal,
                      &dataSize,
                      &state.sampleRate);

    AudioDeviceIOProcID ioProcID = NULL;
    OSStatus status = AudioDeviceCreateIOProcID(device, IOProc, &state, &ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceCreateIOProcID fallo: %d\n", (int)status);
        return 3;
    }

    status = AudioDeviceStart(device, ioProcID);
    if (status != kAudioHardwareNoError) {
        fprintf(stderr, "AudioDeviceStart fallo: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        return 4;
    }

    sleep((unsigned int)seconds);

    AudioDeviceStop(device, ioProcID);
    AudioDeviceDestroyIOProcID(device, ioProcID);

    double rms = 0.0;
    if (state.inputFrames > 0) {
        rms = sqrt(state.inputSquareSum / ((double)state.inputFrames * 8.0));
    }
    printf("I/O OK: rate=%.0f callbacks=%llu outputBuffers=%llu outputFrames=%llu outputSamples=%llu outputPeak=%.8f inputFrames=%llu inputRMS=%.8f inputPeak=%.8f\n",
           state.sampleRate,
           state.callbacks,
           state.outputBuffers,
           state.outputFrames,
           state.outputSamples,
           state.outputPeak,
           state.inputFrames,
           rms,
           state.inputPeak);
    return state.callbacks > 0 && state.outputSamples > 0 && state.outputPeak > 0.0 ? 0 : 5;
}
