#include "OpenA8DJVintageCompatible.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct PreflightContext {
    OpenA8DJVintageBuildDescriptor descriptor;
    uint64_t lastReasons;
    uint32_t calls;
} PreflightContext;

typedef struct MockBufferConfiguration {
    uint32_t current;
    uint32_t pending;
    uint32_t requests;
    uint32_t performs;
    uint32_t aborts;
} MockBufferConfiguration;

static bool MockRequestBufferChange(
    MockBufferConfiguration *configuration,
    uint32_t requested,
    bool vintagePresent,
    bool vintageEffective,
    bool streaming)
{
    if (vintagePresent && streaming) {
        return false;
    }
    uint32_t normalized = OpenA8DJVintageNormalizeBufferFrames(
        requested, vintageEffective);
    if (normalized == 0) {
        return false;
    }
    if (normalized == configuration->current) {
        return true;
    }
    configuration->pending = normalized;
    configuration->requests++;
    return true;
}

static void MockPerformBufferChange(
    MockBufferConfiguration *configuration,
    bool vintageEffective)
{
    uint32_t normalized = OpenA8DJVintageNormalizeBufferFrames(
        configuration->pending, vintageEffective);
    configuration->pending = 0;
    if (normalized != 0) {
        configuration->current = normalized;
        configuration->performs++;
    }
}

static void MockAbortBufferChange(
    MockBufferConfiguration *configuration)
{
    configuration->pending = 0;
    configuration->aborts++;
}

static OpenA8DJVintageBuildDescriptor GoodDescriptor(void)
{
    OpenA8DJVintageBuildDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.resetAudioParamsBeforeStream = 1;
    descriptor.capturePacedOutput = 1;
    descriptor.outputStartByte = 4;
    descriptor.timestampPeriodFrames =
        kOpenA8DJVintageRequiredTimestampPeriodFrames;
    descriptor.supportedRateMask =
        kOpenA8DJVintageRequiredRateMask;
    descriptor.transportRateMask =
        kOpenA8DJVintageRequiredRateMask;
    descriptor.effectiveRateMask =
        kOpenA8DJVintageRate48000;
    descriptor.currentBufferFrames =
        kOpenA8DJVintageRequiredBufferFrames;
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
    descriptor.effectiveSampleRate = 48000.0;
    descriptor.timestampSampleRate = 48000.0;
    return descriptor;
}

static bool VintagePreflight(
    const OpenA8DJDriverModePolicy *policy,
    void *opaque)
{
    PreflightContext *context = opaque;
    context->calls++;
    if (!policy->vintagePreflightRequired) {
        return OpenA8DJDriverModePolicyIsSafe(policy, 32768);
    }
    assert(policy->bufferNormalization ==
           kOpenA8DJVintageBufferNormalizationFixed);
    assert(policy->fixedBufferFrames == 512);
    OpenA8DJVintagePreflightResult result =
        OpenA8DJVintageEvaluatePreflight(&context->descriptor);
    context->lastReasons = result.reasons;
    return result.mandatoryPassed != 0;
}

static void CheckPolicyAndRates(void)
{
    OpenA8DJDriverModePolicy policy;
    assert(OpenA8DJDriverModeLookup(
        kOpenA8DJDriverModeVintageCompatible, &policy));
    assert(policy.outputStartLatencyFrames == 8192);
    assert(policy.outputRestartLatencyFrames == 4096);
    assert(policy.outputTargetLatencyFrames == 8192);
    assert(policy.workerQoS ==
           kOpenA8DJDriverModeWorkerQoSDefault);
    assert(policy.vintagePreflightRequired == 1);
    assert(policy.bufferNormalization ==
           kOpenA8DJVintageBufferNormalizationFixed);
    assert(policy.fixedBufferFrames == 512);
    assert(OpenA8DJDriverModePolicyIsSafe(&policy, 32768));

    assert(OpenA8DJVintageRateMaskForRate(44100.0) ==
           kOpenA8DJVintageRate44100);
    assert(OpenA8DJVintageRateMaskForRate(48000.0) ==
           kOpenA8DJVintageRate48000);
    assert(OpenA8DJVintageRateMaskForRate(88200.0) ==
           kOpenA8DJVintageRate88200);
    assert(OpenA8DJVintageRateMaskForRate(96000.0) ==
           kOpenA8DJVintageRate96000);
    assert(OpenA8DJVintageRateMaskForRate(192000.0) ==
           kOpenA8DJVintageRate192000);
    assert(OpenA8DJVintageRateMaskForRate(12345.0) == 0);
    assert(OpenA8DJVintageRateMaskForRate(NAN) == 0);
    assert(OpenA8DJVintageRateMaskForRate(INFINITY) == 0);
}

static void CheckPreflightFailures(void)
{
    OpenA8DJVintageBuildDescriptor good = GoodDescriptor();
    OpenA8DJVintagePreflightResult pass =
        OpenA8DJVintageEvaluatePreflight(&good);
    assert(pass.mandatoryPassed == 1);
    assert((pass.reasons &
            OPENA8DJ_VINTAGE_MANDATORY_REASON_MASK) == 0);
    assert((pass.reasons &
            kOpenA8DJVintageReasonRate192000NotImplemented) != 0);
    assert((pass.capabilities &
            kOpenA8DJVintageCapabilityRate88200) != 0);
    assert((pass.capabilities &
            kOpenA8DJVintageCapabilityRate192000) == 0);

#define CHECK_FAILURE(field, value, reason) do { \
    OpenA8DJVintageBuildDescriptor failed = good; \
    failed.field = (value); \
    OpenA8DJVintagePreflightResult result = \
        OpenA8DJVintageEvaluatePreflight(&failed); \
    assert(result.mandatoryPassed == 0); \
    assert((result.reasons & (reason)) != 0); \
} while (0)

    CHECK_FAILURE(resetAudioParamsBeforeStream, 0,
                  kOpenA8DJVintageReasonAudioParamsResetDisabled);
    CHECK_FAILURE(capturePacedOutput, 0,
                  kOpenA8DJVintageReasonCapturePacedOutputDisabled);
    CHECK_FAILURE(outputStartByte, 3,
                  kOpenA8DJVintageReasonOutputStartByteNot4);
    CHECK_FAILURE(explicitUSBScheduling, 1,
                  kOpenA8DJVintageReasonExplicitUSBSchedulingEnabled);
    CHECK_FAILURE(usbHALTimestampEnabled, 1,
                  kOpenA8DJVintageReasonUSBHALTimestampEnabled);
    CHECK_FAILURE(timestampPeriodFrames, 8192,
                  kOpenA8DJVintageReasonTimestampPeriodMismatch);
    CHECK_FAILURE(currentBufferFrames, 1024,
                  kOpenA8DJVintageReasonBufferNot512);
    CHECK_FAILURE(supportedRateMask,
                  kOpenA8DJVintageRequiredRateMask &
                      ~kOpenA8DJVintageRate88200,
                  kOpenA8DJVintageReasonRateSurfaceMismatch);
    CHECK_FAILURE(transportRateMask,
                  kOpenA8DJVintageRequiredRateMask &
                      ~kOpenA8DJVintageRate88200,
                  kOpenA8DJVintageReasonRateSurfaceMismatch);
    CHECK_FAILURE(effectiveRateMask, kOpenA8DJVintageRate192000,
                  kOpenA8DJVintageReasonRateSurfaceMismatch);
    CHECK_FAILURE(timestampSampleRate, 44100.0,
                  kOpenA8DJVintageReasonTimestampRateMismatch);

#undef CHECK_FAILURE
}

static void CheckNormalization(void)
{
    const uint32_t requests[] = {1, 512, 513, 1024, 4096};
    for (size_t index = 0;
         index < sizeof(requests) / sizeof(requests[0]);
         index++) {
        assert(OpenA8DJVintageNormalizeBufferFrames(
                   requests[index], true) == 512);
    }
    assert(OpenA8DJVintageNormalizeBufferFrames(0, true) == 0);
    assert(OpenA8DJVintageNormalizeBufferFrames(4097, true) == 0);
    assert(OpenA8DJVintageNormalizeBufferFrames(1, false) == 512);
    assert(OpenA8DJVintageNormalizeBufferFrames(512, false) == 512);
    assert(OpenA8DJVintageNormalizeBufferFrames(513, false) == 1024);
    assert(OpenA8DJVintageNormalizeBufferFrames(1024, false) == 1024);
    assert(OpenA8DJVintageNormalizeBufferFrames(1025, false) == 2048);
    assert(OpenA8DJVintageNormalizeBufferFrames(2049, false) == 4096);
    assert(OpenA8DJVintageNormalizeBufferFrames(4096, false) == 4096);
}

static void CheckBufferConfigurationTransactions(void)
{
    MockBufferConfiguration configuration = {.current = 512};

    /* After leaving Vintage, 1024 uses the normal Core Audio transaction. */
    assert(MockRequestBufferChange(
        &configuration, 1024, false, false, false));
    assert(configuration.current == 512);
    assert(configuration.pending == 1024);
    assert(configuration.requests == 1);
    MockPerformBufferChange(&configuration, false);
    assert(configuration.current == 1024);
    assert(configuration.pending == 0);
    assert(configuration.performs == 1);

    assert(MockRequestBufferChange(
        &configuration, 512, false, false, false));
    assert(configuration.pending == 512);
    MockAbortBufferChange(&configuration);
    assert(configuration.current == 1024);
    assert(configuration.pending == 0);
    assert(configuration.aborts == 1);

    /* Vintage streaming rejects even a request that would normalize to 512. */
    configuration.current = 512;
    assert(!MockRequestBufferChange(
        &configuration, 1024, true, true, true));
    assert(configuration.current == 512);
    assert(configuration.requests == 2);
}

static void CheckTransactionsAndRestart(void)
{
    OpenA8DJDriverModeState state;
    PreflightContext context = {
        .descriptor = GoodDescriptor()
    };
    OpenA8DJDriverModeStateInit(&state);
    assert(state.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(!state.pending);

    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeVintageCompatible,
        false, VintagePreflight, &context));
    assert(state.requestedMode ==
           kOpenA8DJDriverModeVintageCompatible);
    assert(state.effectiveMode ==
           kOpenA8DJDriverModeVintageCompatible);
    assert(!state.pending);
    assert(context.calls == 1);

    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeBalanced,
        false, VintagePreflight, &context));
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(OpenA8DJVintageNormalizeBufferFrames(513, false) == 1024);

    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModePerformance,
        false, VintagePreflight, &context));
    assert(state.effectiveMode == kOpenA8DJDriverModePerformance);
    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeVintageCompatible,
        true, VintagePreflight, &context));
    assert(state.requestedMode ==
           kOpenA8DJDriverModeVintageCompatible);
    assert(state.effectiveMode == kOpenA8DJDriverModePerformance);
    assert(state.pending);
    assert(OpenA8DJDriverModePromotePending(
        &state, VintagePreflight, &context));
    assert(state.effectiveMode ==
           kOpenA8DJDriverModeVintageCompatible);
    assert(!state.pending);

    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeBalanced,
        true, VintagePreflight, &context));
    assert(state.pending);
    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeVintageCompatible,
        true, VintagePreflight, &context));
    assert(!state.pending);
    assert(state.lastResult == kOpenA8DJDriverModeResultCancelled);

    OpenA8DJDriverModeState restarted;
    OpenA8DJDriverModeStateInit(&restarted);
    assert(restarted.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(restarted.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(!restarted.pending);
}

static void CheckRollback(void)
{
    OpenA8DJDriverModeState state;
    PreflightContext context = {
        .descriptor = GoodDescriptor()
    };
    context.descriptor.currentBufferFrames = 1024;
    OpenA8DJDriverModeStateInit(&state);
    assert(!OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeVintageCompatible,
        false, VintagePreflight, &context));
    assert(state.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(!state.pending);
    assert(state.lastResult ==
           kOpenA8DJDriverModeResultApplyFailed);
    assert((context.lastReasons &
            kOpenA8DJVintageReasonBufferNot512) != 0);
    assert(context.descriptor.currentBufferFrames == 1024);

    context.descriptor = GoodDescriptor();
    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeVintageCompatible,
        true, VintagePreflight, &context));
    context.descriptor.outputStartByte = 0;
    assert(!OpenA8DJDriverModePromotePending(
        &state, VintagePreflight, &context));
    assert(state.requestedMode ==
           kOpenA8DJDriverModeVintageCompatible);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(state.pending);
    assert((context.lastReasons &
            kOpenA8DJVintageReasonOutputStartByteNot4) != 0);
}

static OpenA8DJVintageStatePayload ValidPayload(void)
{
    OpenA8DJDriverModeState state;
    PreflightContext context = {
        .descriptor = GoodDescriptor()
    };
    OpenA8DJDriverModeStateInit(&state);
    assert(OpenA8DJDriverModeSet(
        &state, kOpenA8DJDriverModeVintageCompatible,
        false, VintagePreflight, &context));

    OpenA8DJVintageStatePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.schemaVersion = kOpenA8DJVintageSchemaVersion;
    payload.status = kOpenA8DJVintageConformancePartial;
    payload.experimental = 1;
    OpenA8DJVintagePreflightResult result =
        OpenA8DJVintageEvaluatePreflight(&context.descriptor);
    payload.reasons = result.reasons;
    payload.capabilities = result.capabilities;
    payload.knownCapabilities = result.knownCapabilities;
    payload.preflightGeneration = 1;
    payload.descriptor = context.descriptor;
    payload.bufferNormalization =
        kOpenA8DJVintageBufferNormalizationFixed;
    payload.normalizedBufferFrames = 512;
    OpenA8DJDriverModeMakeStatePayload(
        &state, false, &payload.driverMode);
    return payload;
}

static void CheckConformanceAndPayload(void)
{
    uint64_t gaps =
        kOpenA8DJVintageReasonRate192000NotImplemented |
        kOpenA8DJVintageReasonPhysicalMatrixMissing;
    assert(OpenA8DJVintageEvaluateConformance(
               false, false, gaps, false) ==
           kOpenA8DJVintageConformanceUnverified);
    assert(OpenA8DJVintageEvaluateConformance(
               true, false, gaps, false) ==
           kOpenA8DJVintageConformanceUnverified);
    assert(OpenA8DJVintageEvaluateConformance(
               true, true,
               gaps | kOpenA8DJVintageReasonBufferNot512,
               false) ==
           kOpenA8DJVintageConformanceUnverified);
    assert(OpenA8DJVintageEvaluateConformance(
               true, true, gaps, false) ==
           kOpenA8DJVintageConformancePartial);
    assert(OpenA8DJVintageEvaluateConformance(
               true, true, 0, true) ==
           kOpenA8DJVintageConformanceCompatible);
    assert(OpenA8DJVintageEvaluateConformance(
               true, true, 0, false) ==
           kOpenA8DJVintageConformancePartial);

    OpenA8DJVintageStatePayload payload = ValidPayload();
    assert(OpenA8DJVintageValidateStatePayload(&payload));
    payload.reasons |= 1ull << 63;
    assert(!OpenA8DJVintageValidateStatePayload(&payload));
    payload = ValidPayload();
    payload.capabilities |= 1ull << 63;
    assert(!OpenA8DJVintageValidateStatePayload(&payload));
    payload = ValidPayload();
    payload.status = 99;
    assert(!OpenA8DJVintageValidateStatePayload(&payload));
    payload = ValidPayload();
    payload.descriptor.outputStartByte = 3;
    assert(!OpenA8DJVintageValidateStatePayload(&payload));
    payload = ValidPayload();
    payload.driverMode.generation++;
    assert(OpenA8DJVintageValidateStatePayload(&payload));
}

int main(void)
{
    CheckPolicyAndRates();
    CheckPreflightFailures();
    CheckNormalization();
    CheckBufferConfigurationTransactions();
    CheckTransactionsAndRestart();
    CheckRollback();
    CheckConformanceAndPayload();
    puts("vintage compatible policy/preflight: PASS");
    return 0;
}
