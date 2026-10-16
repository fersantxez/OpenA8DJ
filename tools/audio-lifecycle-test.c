#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile unsigned long long gCallbacks = 0;
static volatile unsigned long long gFrames = 0;

static int CycleProgressAccepted(unsigned long long callbacksBefore,
                                 unsigned long long callbacksAfter,
                                 unsigned long long framesBefore,
                                 unsigned long long framesAfter)
{
    return callbacksAfter > callbacksBefore && framesAfter > framesBefore;
}

static int LifecycleSelfTest(void)
{
    if (CycleProgressAccepted(0, 0, 0, 0)) return 1;
    if (CycleProgressAccepted(0, 1, 0, 0)) return 1;
    if (CycleProgressAccepted(0, 0, 0, 1)) return 1;
    if (!CycleProgressAccepted(7, 8, 120, 512)) return 1;
    return 0;
}

static OSStatus LifecycleIOProc(AudioDeviceID device,
                                const AudioTimeStamp *now,
                                const AudioBufferList *input,
                                const AudioTimeStamp *inputTime,
                                AudioBufferList *output,
                                const AudioTimeStamp *outputTime,
                                void *context)
{
    (void)device; (void)now; (void)input; (void)inputTime; (void)outputTime; (void)context;
    if (output != NULL) {
        for (UInt32 i = 0; i < output->mNumberBuffers; ++i) {
            if (output->mBuffers[i].mData != NULL) {
                memset(output->mBuffers[i].mData, 0, output->mBuffers[i].mDataByteSize);
            }
            UInt32 channels = output->mBuffers[i].mNumberChannels;
            if (channels > 0) {
                __atomic_add_fetch(&gFrames,
                                   output->mBuffers[i].mDataByteSize / (sizeof(Float32) * channels),
                                   __ATOMIC_RELAXED);
            }
        }
    }
    __atomic_add_fetch(&gCallbacks, 1, __ATOMIC_RELAXED);
    return noErr;
}

static void SleepMilliseconds(unsigned milliseconds)
{
    struct timespec delay = { .tv_sec = milliseconds / 1000U,
                              .tv_nsec = (long)(milliseconds % 1000U) * 1000000L };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static AudioDeviceID FindDevice(const char *wantedUID)
{
    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDevices,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) != noErr ||
        size < sizeof(AudioDeviceID)) return kAudioObjectUnknown;
    AudioDeviceID *devices = calloc(size / sizeof(AudioDeviceID), sizeof(AudioDeviceID));
    if (devices == NULL || AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL,
                                                       &size, devices) != noErr) {
        free(devices); return kAudioObjectUnknown;
    }
    AudioDeviceID found = kAudioObjectUnknown;
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); ++i) {
        AudioObjectPropertyAddress uidAddress = { kAudioDevicePropertyDeviceUID,
                                                  kAudioObjectPropertyScopeGlobal,
                                                  kAudioObjectPropertyElementMain };
        CFStringRef uid = NULL;
        UInt32 uidSize = sizeof(uid);
        if (AudioObjectGetPropertyData(devices[i], &uidAddress, 0, NULL, &uidSize, &uid) == noErr && uid != NULL) {
            char buffer[512];
            if (CFStringGetCString(uid, buffer, sizeof(buffer), kCFStringEncodingUTF8) &&
                strcmp(buffer, wantedUID) == 0) found = devices[i];
            CFRelease(uid);
        }
        if (found != kAudioObjectUnknown) break;
    }
    free(devices);
    return found;
}

static int SetAndCheckUInt32(AudioDeviceID device, AudioObjectPropertySelector selector, UInt32 value)
{
    AudioObjectPropertyAddress address = { selector, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    OSStatus status = AudioObjectSetPropertyData(device, &address, 0, NULL, sizeof(value), &value);
    if (status != noErr) {
        fprintf(stderr, "set property 0x%08x failed: %d\n", selector, (int)status);
        return 0;
    }
    UInt32 actual = 0;
    for (unsigned attempt = 0; attempt < 50; ++attempt) {
        UInt32 size = sizeof(actual);
        status = AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &actual);
        if (status == noErr && actual == value) return 1;
        SleepMilliseconds(100);
    }
    fprintf(stderr, "property transaction timed out: selector=0x%08x status=%d expected=%u actual=%u\n",
            selector, (int)status, value, actual);
    return 0;
}

static int SetAndCheckRate(AudioDeviceID device, Float64 rate)
{
    AudioObjectPropertyAddress address = { kAudioDevicePropertyNominalSampleRate,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    OSStatus status = AudioObjectSetPropertyData(device, &address, 0, NULL, sizeof(rate), &rate);
    if (status != noErr) {
        fprintf(stderr, "set sample rate %.0f failed: %d\n", rate, (int)status);
        return 0;
    }
    // HAL nominal-rate changes are a host configuration transaction. The write
    // may return before PerformDeviceConfigurationChange publishes the result.
    for (unsigned attempt = 0; attempt < 50; ++attempt) {
        Float64 actual = 0; UInt32 size = sizeof(actual);
        status = AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &actual);
        if (status == noErr && actual == rate) return 1;
        SleepMilliseconds(100);
    }
    Float64 actual = 0; UInt32 size = sizeof(actual);
    status = AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &actual);
    fprintf(stderr, "sample-rate transaction timed out: status=%d requested=%.0f current=%.0f\n",
            (int)status, rate, actual);
    return 0;
}

int main(int argc, char **argv)
{
    const char *uid = "org.opena8dj.Audio8DJ";
    unsigned cyclesPerRate = 100, holdMs = 50, idleReopenMs = 11000;
    int selfTest = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--device-uid") == 0 && i + 1 < argc) uid = argv[++i];
        else if (strcmp(argv[i], "--cycles-per-rate") == 0 && i + 1 < argc) cyclesPerRate = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--hold-ms") == 0 && i + 1 < argc) holdMs = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--idle-reopen-ms") == 0 && i + 1 < argc) idleReopenMs = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--self-test") == 0) selfTest = 1;
        else { fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]); return 2; }
    }
    if (selfTest) {
        int pass = LifecycleSelfTest() == 0;
        printf("lifecycle_cycle_progress_self_test=%s\n", pass ? "PASS" : "FAIL");
        return pass ? 0 : 1;
    }
    AudioDeviceID device = FindDevice(uid);
    if (device == kAudioObjectUnknown) { fprintf(stderr, "device UID not found: %s\n", uid); return 3; }
    AudioObjectPropertyAddress rateAddress = { kAudioDevicePropertyNominalSampleRate,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain };
    Float64 originalRate = 0; UInt32 originalRateSize = sizeof(originalRate);
    if (AudioObjectGetPropertyData(device, &rateAddress, 0, NULL, &originalRateSize, &originalRate) != noErr) {
        fprintf(stderr, "read initial sample rate failed\n"); return 4;
    }
    AudioObjectPropertyAddress bufferAddress = { kAudioDevicePropertyBufferFrameSize,
                                                 kAudioObjectPropertyScopeGlobal,
                                                 kAudioObjectPropertyElementMain };
    UInt32 originalBuffer = 0, originalBufferSize = sizeof(originalBuffer);
    if (AudioObjectGetPropertyData(device, &bufferAddress, 0, NULL, &originalBufferSize, &originalBuffer) != noErr) {
        fprintf(stderr, "read initial buffer size failed\n"); return 4;
    }
    AudioDeviceIOProcID proc = NULL;
    OSStatus status = noErr;
    const Float64 rates[] = { 44100.0, 48000.0 };
    unsigned starts = 0, stops = 0;
    unsigned cyclesCompleted = 0;
    unsigned long long minimumCallbacksPerCycle = ~0ull;
    unsigned long long minimumFramesPerCycle = ~0ull;
    int failed = 0;
    int bufferChanged = originalBuffer != 512;
    int isRunning = 0;
    if (bufferChanged && !SetAndCheckUInt32(device, kAudioDevicePropertyBufferFrameSize, 512)) {
        failed = 1;
        goto cleanup;
    }
    status = AudioDeviceCreateIOProcID(device, LifecycleIOProc, NULL, &proc);
    if (status != noErr || proc == NULL) {
        fprintf(stderr, "create IOProc failed: %d\n", (int)status);
        failed = 1;
        goto cleanup;
    }
    for (unsigned r = 0; r < 2 && !failed; ++r) {
        if (!SetAndCheckRate(device, rates[r])) { failed = 1; goto cleanup; }
        for (unsigned cycle = 0; cycle < cyclesPerRate; ++cycle) {
            unsigned long long callbacksBeforeStart = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
            unsigned long long framesBeforeStart = __atomic_load_n(&gFrames, __ATOMIC_RELAXED);
            status = AudioDeviceStart(device, proc);
            if (status != noErr) { fprintf(stderr, "start rate=%.0f cycle=%u failed: %d\n", rates[r], cycle, (int)status); failed = 1; goto cleanup; }
            isRunning = 1;
            ++starts;
            SleepMilliseconds(holdMs);
            unsigned long long callbacksAfterStart = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
            unsigned long long framesAfterStart = __atomic_load_n(&gFrames, __ATOMIC_RELAXED);
            if (!CycleProgressAccepted(callbacksBeforeStart, callbacksAfterStart,
                                       framesBeforeStart, framesAfterStart)) {
                fprintf(stderr, "cycle made no IOProc progress: rate=%.0f cycle=%u callbacks=%llu frames=%llu\n",
                        rates[r], cycle, callbacksAfterStart - callbacksBeforeStart,
                        framesAfterStart - framesBeforeStart);
                failed = 1;
            }
            unsigned long long callbacksBeforeStop = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
            status = AudioDeviceStop(device, proc);
            if (status != noErr) { fprintf(stderr, "stop rate=%.0f cycle=%u failed: %d\n", rates[r], cycle, (int)status); failed = 1; goto cleanup; }
            isRunning = 0;
            ++stops;
            unsigned long long callbacksAtStopReturn = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
            SleepMilliseconds(20);
            unsigned long long callbacksAfterStop = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
            if (callbacksAfterStop != callbacksAtStopReturn) {
                fprintf(stderr, "IOProc entered after stop returned: at_return=%llu after_settle=%llu\n",
                        callbacksAtStopReturn, callbacksAfterStop);
                failed = 1;
                goto cleanup;
            }
            (void)callbacksBeforeStop;
            unsigned long long callbackDelta = callbacksAfterStart - callbacksBeforeStart;
            unsigned long long frameDelta = framesAfterStart - framesBeforeStart;
            if (callbackDelta < minimumCallbacksPerCycle) minimumCallbacksPerCycle = callbackDelta;
            if (frameDelta < minimumFramesPerCycle) minimumFramesPerCycle = frameDelta;
            ++cyclesCompleted;
            if (failed) goto cleanup;
        }
    }
    if (!failed && idleReopenMs > 0) {
        SleepMilliseconds(idleReopenMs);
        unsigned long long callbacksBeforeStart = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
        unsigned long long framesBeforeStart = __atomic_load_n(&gFrames, __ATOMIC_RELAXED);
        status = AudioDeviceStart(device, proc);
        if (status != noErr) { fprintf(stderr, "idle reopen start failed: %d\n", (int)status); failed = 1; goto cleanup; }
        else {
            isRunning = 1;
            ++starts; SleepMilliseconds(holdMs);
            unsigned long long callbacksAfterStart = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
            unsigned long long framesAfterStart = __atomic_load_n(&gFrames, __ATOMIC_RELAXED);
            if (!CycleProgressAccepted(callbacksBeforeStart, callbacksAfterStart,
                                       framesBeforeStart, framesAfterStart)) {
                fprintf(stderr, "idle reopen made no IOProc progress\n");
                failed = 1;
            }
            status = AudioDeviceStop(device, proc);
            if (status != noErr) { fprintf(stderr, "idle reopen stop failed: %d\n", (int)status); failed = 1; }
            else { isRunning = 0; ++stops; }
            if (callbacksAfterStart > callbacksBeforeStart && framesAfterStart > framesBeforeStart) {
                unsigned long long delta = callbacksAfterStart - callbacksBeforeStart;
                unsigned long long frameDelta = framesAfterStart - framesBeforeStart;
                if (delta < minimumCallbacksPerCycle) minimumCallbacksPerCycle = delta;
                if (frameDelta < minimumFramesPerCycle) minimumFramesPerCycle = frameDelta;
            }
        }
    }
cleanup:
    if (isRunning && proc != NULL) {
        OSStatus cleanupStop = AudioDeviceStop(device, proc);
        if (cleanupStop != noErr) {
            fprintf(stderr, "cleanup stop failed: %d\n", (int)cleanupStop);
            failed = 1;
        } else {
            ++stops;
        }
        isRunning = 0;
    }
    if (!SetAndCheckRate(device, originalRate)) failed = 1;
    if (bufferChanged && !SetAndCheckUInt32(device, kAudioDevicePropertyBufferFrameSize, originalBuffer)) failed = 1;
    if (proc != NULL) {
        OSStatus destroyStatus = AudioDeviceDestroyIOProcID(device, proc);
        if (destroyStatus != noErr) {
            fprintf(stderr, "destroy IOProc failed: %d\n", (int)destroyStatus);
            failed = 1;
        }
    }
    if (idleReopenMs > 0) SleepMilliseconds(idleReopenMs);
    unsigned long long callbacks = __atomic_load_n(&gCallbacks, __ATOMIC_RELAXED);
    unsigned long long frames = __atomic_load_n(&gFrames, __ATOMIC_RELAXED);
    int pass = !failed && starts == stops && cyclesCompleted == cyclesPerRate * 2 &&
               callbacks > 0 && frames > 0 && minimumCallbacksPerCycle != ~0ull &&
               minimumFramesPerCycle != ~0ull;
    printf("{\"result\":\"%s\",\"device_uid\":\"%s\",\"buffer_frames\":512,\"cycles_per_rate\":%u,\"cycles_completed\":%u,\"rates\":[44100,48000],\"original_sample_rate\":%.0f,\"original_buffer_frames\":%u,\"starts\":%u,\"stops\":%u,\"callbacks\":%llu,\"frames\":%llu,\"minimum_callbacks_per_cycle\":%llu,\"minimum_frames_per_cycle\":%llu,\"idle_reopen_ms\":%u}\n",
           pass ? "PASS" : "FAIL", uid, cyclesPerRate, cyclesCompleted,
           originalRate, originalBuffer, starts, stops, callbacks, frames,
           minimumCallbacksPerCycle == ~0ull ? 0 : minimumCallbacksPerCycle,
           minimumFramesPerCycle == ~0ull ? 0 : minimumFramesPerCycle, idleReopenMs);
    return pass ? 0 : 6;
}
