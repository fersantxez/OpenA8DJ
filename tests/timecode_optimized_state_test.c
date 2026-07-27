#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "OpenA8DJTimecodeOptimized.h"

static OpenA8DJTimecodeWindow Window(double allowedRMS,
                                      double allowedPeak,
                                      uint32_t allowedMask,
                                      double forbiddenRMS,
                                      double forbiddenPeak,
                                      uint32_t forbiddenMask)
{
    OpenA8DJTimecodeWindow window;
    memset(&window, 0, sizeof(window));
    window.finite = 1;
    window.complete = 1;
    for (uint32_t pair = 0; pair < 4; pair++) {
        window.frames[pair] = 12000;
        for (uint32_t channel = 0; channel < 2; channel++) {
            bool allowed = pair < 2 &&
                (allowedMask & (1u << (pair * 2 + channel))) != 0;
            bool forbidden = pair >= 2 &&
                (forbiddenMask & (1u << (pair * 2 + channel))) != 0;
            double rms = allowed ? allowedRMS :
                (forbidden ? forbiddenRMS : 0.0);
            double peak = allowed ? allowedPeak :
                (forbidden ? forbiddenPeak : 0.0);
            window.square[pair][channel] =
                rms * rms * window.frames[pair];
            window.peak[pair][channel] = peak;
            window.minimum[pair][channel] = -peak;
            window.maximum[pair][channel] = peak;
        }
    }
    return window;
}

static OpenA8DJTimecodeState Armed(void)
{
    OpenA8DJTimecodeState state;
    OpenA8DJTimecodeStateInit(&state);
    assert(OpenA8DJTimecodeArm(
        &state, kOpenA8DJDriverModeBalanced,
        kOpenA8DJTimecodeProfileVinyl, 48000.0, 512));
    return state;
}

static void CheckPolicy(void)
{
    OpenA8DJDriverModePolicy policy;
    assert(OpenA8DJDriverModeLookup(
        kOpenA8DJDriverModeTimecodeOptimized, &policy));
    assert(policy.outputStartLatencyFrames == 4096);
    assert(policy.outputRestartLatencyFrames == 4096);
    assert(policy.outputTargetLatencyFrames == 4096);
    assert(policy.workerQoS ==
           kOpenA8DJDriverModeWorkerQoSUserInteractive);
    assert(policy.inputLeadGuardEnabled == 1);
    assert(policy.inputLeadCeilingFrames == 2048);
    assert(policy.timecodeEvidenceRequired == 1);
    assert(OpenA8DJDriverModePolicyIsSafe(&policy, 32768));
}

static void CheckCanonicalProfiles(void)
{
    uint8_t sources[4] = {0, 1, 2, 3};
    assert(OpenA8DJTimecodeProfileForElectricalState(
        0, 1, 0, 0, 1, 1, 0, 0, 0, sources) ==
        kOpenA8DJTimecodeProfileVinyl);
    assert(OpenA8DJTimecodeProfileForElectricalState(
        1, 0, 1, 0, 1, 1, 0, 0, 0, sources) ==
        kOpenA8DJTimecodeProfileCDLine);
    assert(OpenA8DJTimecodeProfileForElectricalState(
        2, 0, 0, 1, 1, 1, 0, 0, 0, sources) ==
        kOpenA8DJTimecodeProfilePhono);
    assert(OpenA8DJTimecodeProfileForElectricalState(
        0, 1, 0, 0, 0, 1, 0, 0, 0, sources) ==
        kOpenA8DJTimecodeProfileUnavailable);
    assert(OpenA8DJTimecodeProfileForElectricalState(
        0, 1, 0, 0, 1, 0, 0, 0, 0, sources) ==
        kOpenA8DJTimecodeProfileUnavailable);
    assert(OpenA8DJTimecodeProfileForElectricalState(
        0, 1, 0, 0, 1, 1, 1, 0, 0, sources) ==
        kOpenA8DJTimecodeProfileUnavailable);
    sources[0] = 1;
    assert(OpenA8DJTimecodeProfileForElectricalState(
        0, 1, 0, 0, 1, 1, 0, 0, 0, sources) ==
        kOpenA8DJTimecodeProfileUnavailable);
}

static void CheckArmValidation(void)
{
    OpenA8DJTimecodeArmPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.schemaVersion = 1;
    payload.modeID = kOpenA8DJDriverModeTimecodeOptimized;
    payload.allowedInputPairMask = 0x3;
    uint8_t rejection = 0xff;
    assert(OpenA8DJTimecodeValidateArmPayload(
        &payload, sizeof(payload), NULL));
    assert(!OpenA8DJTimecodeValidateArmPayloadDetailed(
        &payload, sizeof(payload) - 1, NULL, &rejection));
    assert(rejection == kOpenA8DJTimecodeRejectionBadLength);
    const uint8_t badMasks[] = {0, 1, 2, 4, 8, 7, 15, 0x83};
    for (size_t index = 0; index < sizeof(badMasks); index++) {
        payload.allowedInputPairMask = badMasks[index];
        assert(!OpenA8DJTimecodeValidateArmPayloadDetailed(
            &payload, sizeof(payload), NULL, &rejection));
        assert(rejection ==
               kOpenA8DJTimecodeRejectionInvalidPairMask);
    }
    payload.allowedInputPairMask = 3;
    payload.reserved[3] = 1;
    assert(!OpenA8DJTimecodeValidateArmPayloadDetailed(
        &payload, sizeof(payload), NULL, &rejection));
    assert(rejection ==
           kOpenA8DJTimecodeRejectionReservedNonzero);
    payload.reserved[3] = 0;
    payload.schemaVersion = 2;
    assert(!OpenA8DJTimecodeValidateArmPayloadDetailed(
        &payload, sizeof(payload), NULL, &rejection));
    assert(rejection ==
           kOpenA8DJTimecodeRejectionUnsupportedSchema);
    payload.schemaVersion = 1;
    payload.modeID = kOpenA8DJDriverModePerformance;
    assert(!OpenA8DJTimecodeValidateArmPayloadDetailed(
        &payload, sizeof(payload), NULL, &rejection));
    assert(rejection == kOpenA8DJTimecodeRejectionUnknownMode);

    OpenA8DJTimecodeState waiting;
    OpenA8DJTimecodeStateInit(&waiting);
    assert(OpenA8DJTimecodeArm(
        &waiting, kOpenA8DJDriverModeBalanced,
        kOpenA8DJTimecodeProfileUnavailable, 48000.0, 512));
    assert(waiting.armed);
    assert(!waiting.profileVerified);
    assert(!waiting.qualified && !waiting.optimizedActive);
    assert(waiting.armState == kOpenA8DJTimecodeWaitingProfile);
}

static void CheckQualification(void)
{
    OpenA8DJTimecodeState state = Armed();
    OpenA8DJTimecodeWindow silence = Window(0, 0, 0, 0, 0, 0);
    for (int i = 0; i < 32; i++) {
        assert(OpenA8DJTimecodeObserveWindow(
            &state, &silence, true) == kOpenA8DJTimecodeFailNone);
    }
    assert(!state.qualified && state.eligibleWindows == 0);

    OpenA8DJTimecodeWindow both = Window(
        OPENA8DJ_TIMECODE_ENTRY_RMS,
        OPENA8DJ_TIMECODE_ENTRY_PEAK, 0x0f, 0, 0, 0);
    uint64_t generationBeforeQualification = state.generation;
    for (int i = 0; i < 7; i++) {
        assert(OpenA8DJTimecodeObserveWindow(
            &state, &both, true) == kOpenA8DJTimecodeFailNone);
        assert(state.generation == generationBeforeQualification);
    }
    assert(!state.qualified && state.eligibleWindows == 7);
    assert(OpenA8DJTimecodeObserveWindow(&state, &both, true) == UINT8_MAX);
    assert(state.qualified);
    assert(state.generation ==
           generationBeforeQualification + 1);
    assert(state.armState ==
           kOpenA8DJTimecodeQualifiedPendingBoundary);

    state = Armed();
    OpenA8DJTimecodeWindow onlyA = Window(
        0.02, 0.04, 0x03, 0, 0, 0);
    OpenA8DJTimecodeWindow onlyB = Window(
        0.02, 0.04, 0x0c, 0, 0, 0);
    OpenA8DJTimecodeWindow mono = Window(
        0.02, 0.04, 0x05, 0, 0, 0);
    for (int i = 0; i < 12; i++) {
        assert(OpenA8DJTimecodeObserveWindow(
            &state, &onlyA, false) == kOpenA8DJTimecodeFailNone);
        assert(OpenA8DJTimecodeObserveWindow(
            &state, &onlyB, false) == kOpenA8DJTimecodeFailNone);
        assert(OpenA8DJTimecodeObserveWindow(
            &state, &mono, false) == kOpenA8DJTimecodeFailNone);
    }
    assert(!state.qualified);
}

static void CheckThresholdsAndHysteresis(void)
{
    OpenA8DJTimecodeWindow below = Window(
        nextafter(OPENA8DJ_TIMECODE_ENTRY_RMS, 0.0),
        OPENA8DJ_TIMECODE_ENTRY_PEAK, 0x0f, 0, 0, 0);
    assert(!OpenA8DJTimecodePairEntry(&below, 0));
    OpenA8DJTimecodeWindow peakBelow = Window(
        OPENA8DJ_TIMECODE_ENTRY_RMS,
        nextafter(OPENA8DJ_TIMECODE_ENTRY_PEAK, 0.0),
        0x0f, 0, 0, 0);
    assert(!OpenA8DJTimecodePairEntry(&peakBelow, 0));
    OpenA8DJTimecodeWindow at = Window(
        OPENA8DJ_TIMECODE_ENTRY_RMS,
        OPENA8DJ_TIMECODE_ENTRY_PEAK, 0x0f, 0, 0, 0);
    assert(OpenA8DJTimecodePairEntry(&at, 0));
    assert(OpenA8DJTimecodePairEntry(&at, 1));

    OpenA8DJTimecodeState state = Armed();
    state.qualified = 1;
    state.optimizedActive = 1;
    state.armState = kOpenA8DJTimecodeActive;
    OpenA8DJTimecodeWindow dropout = Window(0, 0, 0, 0, 0, 0);
    for (int i = 0; i < 3; i++) {
        assert(OpenA8DJTimecodeObserveWindow(
            &state, &dropout, true) == kOpenA8DJTimecodeFailNone);
    }
    assert(OpenA8DJTimecodeObserveWindow(&state, &dropout, true) ==
           kOpenA8DJTimecodeFailAllowedPairDropout);
}

static void CheckForbiddenAndInvalid(void)
{
    OpenA8DJTimecodeState state = Armed();
    OpenA8DJTimecodeWindow c = Window(
        0.01, 0.02, 0x0f,
        OPENA8DJ_TIMECODE_HOLD_RMS,
        OPENA8DJ_TIMECODE_HOLD_PEAK, 1u << 4);
    assert(OpenA8DJTimecodeObserveWindow(&state, &c, true) ==
           kOpenA8DJTimecodeFailOutsideAllowlist);

    state = Armed();
    OpenA8DJTimecodeWindow invalid = c;
    invalid.complete = 0;
    assert(OpenA8DJTimecodeObserveWindow(&state, &invalid, true) ==
           kOpenA8DJTimecodeFailStatsInvalid);
    invalid.complete = 1;
    invalid.finite = 0;
    assert(OpenA8DJTimecodeObserveWindow(&state, &invalid, true) ==
           kOpenA8DJTimecodeFailStatsInvalid);
}

static void CheckNonDestructiveAccumulator(void)
{
    OpenA8DJTimecodeClassifier classifier;
    OpenA8DJTimecodeClassifierInit(&classifier, 8.0);
    float frame[8] = {0.01f, 0.01f, 0.01f, 0.01f, 0, 0, 0, 0};
    OpenA8DJTimecodeWindow completed;
    assert(!OpenA8DJTimecodeClassifierFeedFrame(
        &classifier, frame, &completed));
    assert(OpenA8DJTimecodeClassifierFeedFrame(
        &classifier, frame, &completed));
    assert(completed.frames[0] == 2);
    OpenA8DJTimecodeWindow saved = classifier.latest;
    /* An operator-facing input-stats read has no reference to this state. */
    assert(memcmp(&saved, &classifier.latest, sizeof(saved)) == 0);
}

static void CheckDCAndNoise(void)
{
    OpenA8DJTimecodeClassifier classifier;
    OpenA8DJTimecodeClassifierInit(&classifier, 16.0);
    OpenA8DJTimecodeWindow completed;
    float dc[8] = {0.2f, 0.2f, 0.2f, 0.2f, 0, 0, 0, 0};
    for (int frame = 0; frame < 4; frame++) {
        bool ready = OpenA8DJTimecodeClassifierFeedFrame(
            &classifier, dc, &completed);
        assert(ready == (frame == 3));
    }
    assert(!OpenA8DJTimecodePairEntry(&completed, 0));
    assert(!OpenA8DJTimecodePairEntry(&completed, 1));
    OpenA8DJTimecodeState active;
    OpenA8DJTimecodeStateInit(&active);
    assert(OpenA8DJTimecodeArm(
        &active, kOpenA8DJDriverModeBalanced,
        kOpenA8DJTimecodeProfileVinyl, 16.0, 1));
    active.qualified = 1;
    active.optimizedActive = 1;
    active.armState = kOpenA8DJTimecodeActive;
    for (int index = 0; index < 3; index++) {
        assert(OpenA8DJTimecodeObserveWindow(
            &active, &completed, true) ==
            kOpenA8DJTimecodeFailNone);
    }
    assert(OpenA8DJTimecodeObserveWindow(
        &active, &completed, true) ==
        kOpenA8DJTimecodeFailAllowedPairDropout);

    OpenA8DJTimecodeClassifierInit(&classifier, 16.0);
    for (int frame = 0; frame < 4; frame++) {
        float sign = (frame & 1) ? 1.0f : -1.0f;
        float noise[8] = {
            sign * 0.009f, -sign * 0.009f,
            sign * 0.009f, -sign * 0.009f, 0, 0, 0, 0
        };
        (void)OpenA8DJTimecodeClassifierFeedFrame(
            &classifier, noise, &completed);
    }
    assert(!OpenA8DJTimecodePairEntry(&completed, 0));

    OpenA8DJTimecodeClassifierInit(&classifier, 16.0);
    for (int frame = 0; frame < 4; frame++) {
        float sign = (frame & 1) ? 1.0f : -1.0f;
        float noise[8] = {
            sign * 0.04f, -sign * 0.04f,
            sign * 0.04f, -sign * 0.04f, 0, 0, 0, 0
        };
        (void)OpenA8DJTimecodeClassifierFeedFrame(
            &classifier, noise, &completed);
    }
    assert(OpenA8DJTimecodePairEntry(&completed, 0));
    assert(OpenA8DJTimecodePairEntry(&completed, 1));
}

static bool RejectPreflight(const OpenA8DJDriverModePolicy *policy,
                            void *context)
{
    (void)policy;
    (void)context;
    return false;
}

static void CheckSafeBoundaryAndRollback(void)
{
    OpenA8DJDriverModeState mode;
    OpenA8DJDriverModeStateInit(&mode);
    assert(OpenA8DJDriverModeSet(
        &mode, kOpenA8DJDriverModePerformance,
        false, NULL, NULL));
    OpenA8DJTimecodeState timecode;
    OpenA8DJTimecodeStateInit(&timecode);
    assert(OpenA8DJTimecodeArm(
        &timecode, kOpenA8DJDriverModePerformance,
        kOpenA8DJTimecodeProfileVinyl, 48000.0, 512));
    OpenA8DJTimecodeWindow both = Window(
        0.02, 0.04, 0x0f, 0, 0, 0);
    for (int index = 0; index < 7; index++) {
        assert(OpenA8DJTimecodeObserveWindow(
            &timecode, &both, true) == kOpenA8DJTimecodeFailNone);
    }
    assert(OpenA8DJTimecodeObserveWindow(
        &timecode, &both, true) == UINT8_MAX);
    assert(OpenA8DJDriverModeSet(
        &mode, kOpenA8DJDriverModeTimecodeOptimized,
        true, NULL, NULL));
    assert(mode.pending);
    assert(mode.requestedMode ==
           kOpenA8DJDriverModeTimecodeOptimized);
    assert(mode.effectiveMode == kOpenA8DJDriverModePerformance);
    assert(OpenA8DJDriverModePromotePending(&mode, NULL, NULL));
    assert(mode.effectiveMode ==
           kOpenA8DJDriverModeTimecodeOptimized);

    timecode.optimizedActive = 1;
    timecode.armState = kOpenA8DJTimecodeActive;
    OpenA8DJTimecodeWindow outside = Window(
        0.02, 0.04, 0x0f, 0.002, 0.004, 1u << 4);
    assert(OpenA8DJTimecodeObserveWindow(
        &timecode, &outside, true) ==
        kOpenA8DJTimecodeFailOutsideAllowlist);
    assert(OpenA8DJDriverModeSet(
        &mode, timecode.fallbackMode, true, NULL, NULL));
    assert(mode.pending);
    assert(mode.effectiveMode ==
           kOpenA8DJDriverModeTimecodeOptimized);
    assert(!OpenA8DJDriverModePromotePending(
        &mode, RejectPreflight, NULL));
    assert(mode.pending);
    assert(mode.effectiveMode ==
           kOpenA8DJDriverModeTimecodeOptimized);
    assert(mode.lastResult ==
           kOpenA8DJDriverModeResultApplyFailed);
    assert(OpenA8DJDriverModePromotePending(&mode, NULL, NULL));
    assert(!mode.pending);
    assert(mode.effectiveMode ==
           kOpenA8DJDriverModePerformance);
}

int main(void)
{
    OpenA8DJTimecodeState restart;
    OpenA8DJTimecodeStateInit(&restart);
    assert(!restart.armed);
    assert(restart.armState == kOpenA8DJTimecodeDisarmed);
    assert(restart.fallbackMode == kOpenA8DJDriverModeBalanced);
    CheckPolicy();
    CheckCanonicalProfiles();
    CheckArmValidation();
    CheckQualification();
    CheckThresholdsAndHysteresis();
    CheckForbiddenAndInvalid();
    CheckNonDestructiveAccumulator();
    CheckDCAndNoise();
    CheckSafeBoundaryAndRollback();
    puts("timecode optimized state/classifier: PASS");
    return 0;
}
