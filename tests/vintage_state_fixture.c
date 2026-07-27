#include "OpenA8DJVintageCompatible.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static OpenA8DJVintageBuildDescriptor GoodDescriptor(void)
{
    OpenA8DJVintageBuildDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.resetAudioParamsBeforeStream = 1;
    descriptor.capturePacedOutput = 1;
    descriptor.outputStartByte = 4;
    descriptor.inputChannels = 8;
    descriptor.outputChannels = 8;
    descriptor.inputStreams = 1;
    descriptor.outputStreams = 4;
    descriptor.clientSampleFormat =
        kOpenA8DJVintageClientSampleFormatFloat32;
    descriptor.captureQueueDepth = 8;
    descriptor.playbackQueueDepth = 8;
    descriptor.captureTransactions = 64;
    descriptor.playbackTransactions = 64;
    descriptor.timestampPeriodFrames = 16384;
    descriptor.supportedRateMask =
        kOpenA8DJVintageRequiredRateMask;
    descriptor.transportRateMask =
        kOpenA8DJVintageRequiredRateMask;
    descriptor.effectiveRateMask =
        kOpenA8DJVintageRate48000;
    descriptor.currentBufferFrames = 512;
    descriptor.effectiveSampleRate = 48000.0;
    descriptor.timestampSampleRate = 48000.0;
    return descriptor;
}

static OpenA8DJDriverModeState StateFor(const char *kind)
{
    OpenA8DJDriverModeState state;
    OpenA8DJDriverModeStateInit(&state);
    if (strcmp(kind, "effective") == 0) {
        if (!OpenA8DJDriverModeSet(
                &state,
                kOpenA8DJDriverModeVintageCompatible,
                false, NULL, NULL)) {
            abort();
        }
    } else if (strcmp(kind, "pending") == 0) {
        if (!OpenA8DJDriverModeSet(
                &state,
                kOpenA8DJDriverModeVintageCompatible,
                true, NULL, NULL)) {
            abort();
        }
    } else if (strcmp(kind, "conflict") == 0) {
        OpenA8DJDriverModeReject(
            &state, kOpenA8DJDriverModeRejectionConflict);
    }
    return state;
}

static OpenA8DJVintageStatePayload VintageFor(const char *kind)
{
    const char *stateKind =
        strcmp(kind, "balanced") == 0 ? "balanced" :
        strcmp(kind, "pending") == 0 ? "pending" :
        strcmp(kind, "conflict") == 0 ? "balanced" : "effective";
    OpenA8DJDriverModeState state = StateFor(stateKind);
    OpenA8DJVintageStatePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.schemaVersion = kOpenA8DJVintageSchemaVersion;
    payload.experimental = 1;
    OpenA8DJVintageBuildDescriptor descriptor = GoodDescriptor();
    payload.descriptor = descriptor;
    OpenA8DJVintagePreflightResult result =
        OpenA8DJVintageEvaluatePreflight(&descriptor);
    payload.reasons = result.reasons;
    payload.capabilities = result.capabilities;
    payload.knownCapabilities = result.knownCapabilities;
    OpenA8DJDriverModeMakeStatePayload(
        &state,
        strcmp(stateKind, "pending") == 0 ||
            strcmp(kind, "effective-streaming") == 0,
        &payload.driverMode);
    if (strcmp(stateKind, "balanced") == 0) {
        payload.status = kOpenA8DJVintageConformanceUnverified;
        payload.reasons |=
            kOpenA8DJVintageReasonNotRequested |
            kOpenA8DJVintageReasonPreflightNotRun;
        payload.bufferNormalization =
            kOpenA8DJVintageBufferNormalizationShippingTable;
        if (strcmp(kind, "conflict") == 0) {
            payload.reasons |=
                kOpenA8DJVintageReasonTimecodeModeConflict;
            OpenA8DJDriverModeState conflict =
                StateFor("conflict");
            OpenA8DJDriverModeMakeStatePayload(
                &conflict, false, &payload.driverMode);
        }
    } else {
        payload.status = kOpenA8DJVintageConformancePartial;
        payload.preflightGeneration = 1;
        if (strcmp(stateKind, "effective") == 0) {
            payload.bufferNormalization =
                kOpenA8DJVintageBufferNormalizationFixed;
            payload.normalizedBufferFrames = 512;
        }
    }
    if (strcmp(kind, "unknown-reason") == 0) {
        payload.reasons |= 1ull << 63;
    } else if (strcmp(kind, "capability-mismatch") == 0) {
        payload.capabilities ^=
            kOpenA8DJVintageCapabilityRate88200;
    } else if (strcmp(kind, "enum-mismatch") == 0) {
        payload.status = 99;
    } else if (strcmp(kind, "generation-mismatch") == 0) {
        payload.driverMode.generation++;
    } else if (strcmp(kind, "requested-mismatch") == 0) {
        payload.driverMode.requestedMode =
            kOpenA8DJDriverModeBalanced;
    }
    return payload;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }
    if (strncmp(argv[1], "driver-", 7) == 0) {
        const char *kind = argv[1] + 7;
        OpenA8DJDriverModeState state = StateFor(kind);
        OpenA8DJDriverModeStatePayload payload;
        OpenA8DJDriverModeMakeStatePayload(
            &state, strcmp(kind, "pending") == 0, &payload);
        return fwrite(&payload, sizeof(payload), 1, stdout) == 1 ?
            0 : 1;
    }
    const char *kind = strncmp(argv[1], "vintage-", 8) == 0 ?
        argv[1] + 8 : argv[1];
    OpenA8DJVintageStatePayload payload = VintageFor(kind);
    return fwrite(&payload, sizeof(payload), 1, stdout) == 1 ?
        0 : 1;
}
