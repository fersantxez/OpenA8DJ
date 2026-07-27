#include "OpenA8DJDriverMode.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool RejectPreflight(const OpenA8DJDriverModePolicy *policy, void *context)
{
    (void)policy;
    (void)context;
    return false;
}

static void CheckBalancedPolicy(void)
{
    OpenA8DJDriverModePolicy policy;
    assert(OpenA8DJDriverModeLookup(kOpenA8DJDriverModeBalanced, &policy));
    assert(policy.outputStartLatencyFrames == 8192);
    assert(policy.outputRestartLatencyFrames == 4096);
    assert(policy.outputTargetLatencyFrames == 8192);
    assert(policy.workerQoS == kOpenA8DJDriverModeWorkerQoSDefault);
}

static void CheckDefaultAndIdleApply(void)
{
    OpenA8DJDriverModeState state;
    OpenA8DJDriverModeStateInit(&state);
    assert(state.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(!state.pending);
    assert(state.generation == 0);
    CheckBalancedPolicy();

    assert(OpenA8DJDriverModeSet(&state,
                                 kOpenA8DJDriverModeBalanced,
                                 false,
                                 NULL,
                                 NULL));
    assert(state.lastResult == kOpenA8DJDriverModeResultUnchanged);
    assert(state.generation == 0);

    assert(OpenA8DJDriverModeSet(&state,
                                 kOpenA8DJDriverModePerformance,
                                 false,
                                 NULL,
                                 NULL));
    assert(state.requestedMode == kOpenA8DJDriverModePerformance);
    assert(state.effectiveMode == kOpenA8DJDriverModePerformance);
    assert(!state.pending);
    assert(state.lastResult == kOpenA8DJDriverModeResultApplied);
    assert(state.generation == 1);
    assert(state.acceptedRequests == 2);
    assert(state.appliedTransitions == 1);

    OpenA8DJDriverModeStatePayload payload;
    OpenA8DJDriverModeMakeStatePayload(&state, false, &payload);
    assert(payload.outputStartLatencyFrames == 4096);
    assert(payload.outputRestartLatencyFrames == 4096);
    assert(payload.outputTargetLatencyFrames == 4096);
    assert(payload.workerQoS == kOpenA8DJDriverModeWorkerQoSUserInteractive);
}

static void CheckRequestValidation(void)
{
    OpenA8DJDriverModeSetPayload request;
    OpenA8DJDriverModeSetPayload parsed;
    uint8_t rejection = 0xff;
    memset(&request, 0, sizeof(request));
    request.schemaVersion = kOpenA8DJDriverModeSchemaVersion;
    request.modeID = kOpenA8DJDriverModePerformance;
    assert(OpenA8DJDriverModeValidateSetPayload(&request,
                                                sizeof(request),
                                                &parsed,
                                                &rejection));
    assert(parsed.modeID == kOpenA8DJDriverModePerformance);
    assert(rejection == kOpenA8DJDriverModeRejectionNone);

    assert(!OpenA8DJDriverModeValidateSetPayload(&request,
                                                 sizeof(request) - 1,
                                                 NULL,
                                                 &rejection));
    assert(rejection == kOpenA8DJDriverModeRejectionBadLength);
    request.schemaVersion = 2;
    assert(!OpenA8DJDriverModeValidateSetPayload(&request,
                                                 sizeof(request),
                                                 NULL,
                                                 &rejection));
    assert(rejection == kOpenA8DJDriverModeRejectionUnsupportedSchema);
    request.schemaVersion = 1;
    request.reserved0 = 1;
    assert(!OpenA8DJDriverModeValidateSetPayload(&request,
                                                 sizeof(request),
                                                 NULL,
                                                 &rejection));
    assert(rejection == kOpenA8DJDriverModeRejectionReservedNonzero);
    request.reserved0 = 0;
    request.reserved[7] = 1;
    assert(!OpenA8DJDriverModeValidateSetPayload(&request,
                                                 sizeof(request),
                                                 NULL,
                                                 &rejection));
    assert(rejection == kOpenA8DJDriverModeRejectionReservedNonzero);
    request.reserved[7] = 0;
    request.modeID = 999;
    assert(!OpenA8DJDriverModeValidateSetPayload(&request,
                                                 sizeof(request),
                                                 NULL,
                                                 &rejection));
    assert(rejection == kOpenA8DJDriverModeRejectionUnknownMode);

    OpenA8DJDriverModeState state;
    OpenA8DJDriverModeStateInit(&state);
    assert(!OpenA8DJDriverModeSet(&state, 999, false, NULL, NULL));
    assert(state.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(state.rejectedRequests == 1);
    assert(state.lastResult == kOpenA8DJDriverModeResultInvalid);
    assert(state.rejectionReason == kOpenA8DJDriverModeRejectionUnknownMode);
}

static void CheckPendingCancelAndPromote(void)
{
    OpenA8DJDriverModeState state;
    OpenA8DJDriverModeStateInit(&state);
    assert(OpenA8DJDriverModeSet(&state,
                                 kOpenA8DJDriverModePerformance,
                                 true,
                                 NULL,
                                 NULL));
    assert(state.requestedMode == kOpenA8DJDriverModePerformance);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(state.pending);
    assert(state.lastResult == kOpenA8DJDriverModeResultPending);
    assert(state.pendingTransitions == 1);
    assert(state.generation == 0);

    assert(OpenA8DJDriverModeSet(&state,
                                 kOpenA8DJDriverModeBalanced,
                                 true,
                                 NULL,
                                 NULL));
    assert(state.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(!state.pending);
    assert(state.lastResult == kOpenA8DJDriverModeResultCancelled);
    assert(state.generation == 0);

    assert(OpenA8DJDriverModeSet(&state,
                                 kOpenA8DJDriverModePerformance,
                                 true,
                                 NULL,
                                 NULL));
    uint64_t generationBeforePromotion = state.generation;
    assert(OpenA8DJDriverModePromotePending(&state, NULL, NULL));
    assert(state.effectiveMode == kOpenA8DJDriverModePerformance);
    assert(!state.pending);
    assert(state.generation == generationBeforePromotion + 1);
    uint64_t applied = state.appliedTransitions;
    assert(OpenA8DJDriverModePromotePending(&state, NULL, NULL));
    assert(state.appliedTransitions == applied);
    assert(state.generation == generationBeforePromotion + 1);
}

static void CheckTransactionalFailures(void)
{
    OpenA8DJDriverModeState state;
    OpenA8DJDriverModeStateInit(&state);
    assert(!OpenA8DJDriverModeSet(&state,
                                  kOpenA8DJDriverModePerformance,
                                  false,
                                  RejectPreflight,
                                  NULL));
    assert(state.requestedMode == kOpenA8DJDriverModeBalanced);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(!state.pending);
    assert(state.generation == 0);
    assert(state.applyFailures == 1);
    assert(state.lastResult == kOpenA8DJDriverModeResultApplyFailed);
    CheckBalancedPolicy();

    assert(OpenA8DJDriverModeSet(&state,
                                 kOpenA8DJDriverModePerformance,
                                 true,
                                 NULL,
                                 NULL));
    uint64_t pendingGeneration = state.generation;
    assert(!OpenA8DJDriverModePromotePending(&state, RejectPreflight, NULL));
    assert(state.requestedMode == kOpenA8DJDriverModePerformance);
    assert(state.effectiveMode == kOpenA8DJDriverModeBalanced);
    assert(state.pending);
    assert(state.generation == pendingGeneration);
    assert(state.applyFailures == 2);
    assert(state.lastResult == kOpenA8DJDriverModeResultApplyFailed);
}

int main(void)
{
    CheckDefaultAndIdleApply();
    CheckRequestValidation();
    CheckPendingCancelAndPromote();
    CheckTransactionalFailures();
    puts("driver mode state machine: PASS");
    return 0;
}
