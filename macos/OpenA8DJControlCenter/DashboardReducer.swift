import Foundation

struct DashboardReducer {
    private(set) var previousLoopback: LoopbackSnapshot?
    private(set) var previousQuality: USBQualitySnapshot?
    private(set) var previousGeneration: UInt64?
    private(set) var lastValidSnapshot: DashboardSnapshot?
    private(set) var lastActionError: DashboardError?

    mutating func age(
        capturedAt: ContinuousClock.Instant,
        now: ContinuousClock.Instant = .now
    ) -> MetricAge {
        let elapsed = capturedAt.duration(to: now)
        if elapsed >= .seconds(10) { return .stale }
        if elapsed >= .seconds(3) { return .aging }
        return .current
    }

    mutating func counterDelta(
        previous: UInt64?,
        current: UInt64,
        sameGeneration: Bool
    ) -> DeltaValue {
        guard sameGeneration else { return .backendMismatch }
        guard let previous else { return .baseline }
        guard current >= previous else { return .counterReset }
        return .value(current - previous)
    }

    mutating func loopbackDeltas(_ current: LoopbackSnapshot) -> LoopbackDeltas {
        let relation: Int
        if let previous = previousLoopback {
            relation = current.generation == previous.generation ? 0 :
                (current.generation > previous.generation ? 1 : -1)
        } else {
            relation = 1
        }
        func delta(_ previous: UInt64?, _ current: UInt64) -> DeltaValue {
            if relation < 0 { return .backendMismatch }
            if relation > 0 { return .baseline }
            guard let previous else { return .baseline }
            return current >= previous ? .value(current - previous) : .counterReset
        }
        let result = LoopbackDeltas(
            gaps: delta(previousLoopback?.gapFrames, current.gapFrames),
            overrunEvents: delta(previousLoopback?.overrunEvents, current.overrunEvents),
            overrunFrames: delta(previousLoopback?.overrunFrames, current.overrunFrames)
        )
        previousLoopback = current
        return result
    }

    mutating func qualityDelta(_ current: USBQualitySnapshot, generation: UInt64?) -> DeltaValue {
        let result: DeltaValue
        if let previousGeneration, let generation {
            if generation < previousGeneration {
                result = .backendMismatch
            } else if generation > previousGeneration {
                result = .baseline
            } else {
                result = counterDelta(
                    previous: previousQuality?.xruns.totalHardXruns,
                    current: current.xruns.totalHardXruns,
                    sameGeneration: true
                )
            }
        } else {
            result = .baseline
        }
        previousQuality = current
        previousGeneration = generation
        return result
    }

    mutating func resumeForeground() {
        previousLoopback = nil
        previousQuality = nil
        previousGeneration = nil
    }

    func mismatchReasons(
        backend: BackendIdentity,
        stream: StreamSnapshot,
        quality: USBQualitySnapshot,
        mode: DriverModeSnapshot,
        profile: ProfileSnapshot,
        loopback: LoopbackSnapshot,
        profiler: ProfilerSnapshot?
    ) -> [String] {
        var reasons: [String] = []
        if stream.streaming != quality.streaming {
            reasons.append("quality/stats streaming disagreement")
        }
        if stream.streaming && abs(stream.sampleRateHz - quality.sampleRateHz) > 0.5 {
            reasons.append("quality/stats sample-rate disagreement")
        }
        if stream.outputTargetLatencyFrames != mode.effectivePolicy.outputTargetLatencyFrames {
            reasons.append("stats/mode latency-policy disagreement")
        }
        if case .known(let timecode) = mode.timecode,
           timecode.profileVerified &&
            timecode.electricalProfile != profile.activeProfile {
            reasons.append("timecode/profile disagreement")
        }
        if let profiler,
           let pairing = profiler.checks.first(where: { $0.id == "driver.api-pairing" }),
           pairing.status == "FAIL" {
            reasons.append("profiler/backend pairing mismatch")
        }
        if backend.apiVersion.major != 1 {
            reasons.append("backend API major mismatch")
        }
        return reasons
    }

    mutating func accepted(_ snapshot: DashboardSnapshot) {
        lastValidSnapshot = snapshot
    }

    mutating func staleSnapshot(for error: DashboardError) -> DashboardSnapshot? {
        guard let previous = lastValidSnapshot else { return nil }
        return DashboardSnapshot(
            backend: previous.backend,
            profiles: previous.profiles,
            modeChoices: previous.modeChoices,
            profile: previous.profile,
            stream: previous.stream,
            quality: previous.quality,
            driverMode: previous.driverMode,
            loopback: previous.loopback,
            profiler: previous.profiler,
            phase: .stale(lastUpdatedDescription: "last validated snapshot", reason: error.code),
            sourceErrors: previous.sourceErrors.merging(["refresh": error]) { _, new in new },
            mismatchReasons: previous.mismatchReasons
        )
    }

    mutating func preserveActionError(_ error: DashboardError) {
        lastActionError = error
    }

    mutating func clearActionErrorAfterNewSuccess() {
        lastActionError = nil
    }

    static func timecodeWaitReason(_ value: TimecodeSnapshot, pending: Bool) -> String {
        if !value.armed { return "explicitly disarmed" }
        if !value.profileVerified { return "wrong or unverified electrical profile" }
        if !value.windowFresh { return "stale or missing evidence" }
        if value.eligibleWindows < value.requiredEligibleWindows {
            return "insufficient qualifying windows"
        }
        if value.lastFailOpenReason != "none" && !value.lastFailOpenReason.isEmpty {
            return value.lastFailOpenReason
        }
        if pending { return "pending safe boundary" }
        return value.optimizedActive ? "active" : "waiting for qualification"
    }

    static func deltaLabel(_ value: DeltaValue) -> String {
        switch value {
        case .baseline: return "baseline"
        case .value(let amount): return "+\(amount)"
        case .counterReset: return "counter reset"
        case .backendMismatch: return "backend mismatch"
        }
    }

    static func mutationMatches(
        expectation: MutationExpectation,
        profile: ProfileSnapshot? = nil,
        driverMode: DriverModeSnapshot? = nil,
        loopback: LoopbackSnapshot? = nil
    ) -> Bool {
        switch expectation {
        case .profile(let expected):
            return profile == expected
        case .driverMode(let expected):
            guard let actual = driverMode else { return false }
            return actual.requestedMode == expected.requestedMode &&
                actual.effectiveMode == expected.effectiveMode &&
                actual.pending == expected.pending &&
                actual.generation == expected.generation
        case .loopback(let expected):
            guard let actual = loopback else { return false }
            return actual.enabled == expected.enabled &&
                actual.sourcePair == expected.sourcePair &&
                actual.sessionOnly == expected.sessionOnly &&
                actual.generation == expected.generation
        }
    }
}
