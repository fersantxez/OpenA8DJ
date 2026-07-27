import AppKit
import Foundation
import SwiftUI

@MainActor
final class ControlCenterStore: ObservableObject {
    @Published private(set) var phase: RefreshPhase = .starting
    @Published private(set) var backend: Evidence<BackendIdentity> = .unavailable(reason: .notYetObserved)
    @Published private(set) var profiles: Evidence<[ProfileChoice]> = .unavailable(reason: .notYetObserved)
    @Published private(set) var modeChoices: Evidence<[DriverModeChoice]> = .unavailable(reason: .notYetObserved)
    @Published private(set) var profile: Evidence<ProfileSnapshot> = .unavailable(reason: .notYetObserved)
    @Published private(set) var stream: Evidence<StreamSnapshot> = .unavailable(reason: .notYetObserved)
    @Published private(set) var quality: Evidence<USBQualitySnapshot> = .unavailable(reason: .notYetObserved)
    @Published private(set) var driverMode: Evidence<DriverModeSnapshot> = .unavailable(reason: .notYetObserved)
    @Published private(set) var loopback: Evidence<LoopbackSnapshot> = .unavailable(reason: .notYetObserved)
    @Published private(set) var profiler: Evidence<ProfilerSnapshot> = .unavailable(reason: .notYetObserved)
    @Published private(set) var sourceErrors: [String: DashboardError] = [:]
    @Published private(set) var mismatchReasons: [String] = []
    @Published private(set) var actionOutcome: ActionOutcome = .idle
    @Published private(set) var loopbackDeltas = LoopbackDeltas(
        gaps: .baseline, overrunEvents: .baseline, overrunFrames: .baseline
    )
    @Published private(set) var qualityXrunDelta: DeltaValue = .baseline
    @Published private(set) var isBusy = false
    @Published var selectedSection: DashboardSection? = .overview
    @Published var pendingConfirmation: PendingConfirmation?
    @Published var selectedLoopbackPair: OutputPair = .A

    private let runner: BoundedProcessRunner
    private let coordinator: RefreshCoordinator
    private var reducer = DashboardReducer()
    private let clock = ContinuousClock()
    private var captures: [String: ContinuousClock.Instant] = [:]
    private var bootstrapped = false
    private var isSceneActive = false
    private var isWindowVisible = false
    private var profilerCapturedAt: ContinuousClock.Instant?
    private var cadenceTask: Task<Void, Never>?
    private var profilerRefreshRequested = false

    init(
        runner: BoundedProcessRunner = BoundedProcessRunner(),
        coordinator: RefreshCoordinator = RefreshCoordinator()
    ) {
        self.runner = runner
        self.coordinator = coordinator
    }

#if OPENA8DJ_CONTROL_CENTER_TESTING
    func loadOfflineFixtures(from directory: URL) throws {
        func output(_ name: String, status: Int32 = 0) throws -> ProcessOutput {
            ProcessOutput(
                status: status,
                stdout: try Data(contentsOf: directory.appendingPathComponent(name)),
                stderr: Data()
            )
        }
        backend = .known(try DashboardDecoder.version(output("good_version.json")))
        profiles = .known(try DashboardDecoder.profiles(output("good_profiles.json")))
        modeChoices = .known(try DashboardDecoder.modeChoices(output("good_modes.json")))
        profile = .known(try DashboardDecoder.profile(output("good_profile.json")))
        stream = .known(try DashboardDecoder.stream(output("good_stats.json")))
        quality = .known(try DashboardDecoder.quality(output("good_quality.ndjson")))
        driverMode = .known(try DashboardDecoder.driverMode(output("good_driver_mode.json")))
        loopback = .known(try DashboardDecoder.loopback(output("good_loopback.json")))
        profiler = .known(try DashboardDecoder.profiler(output("good_profiler.json")))
        let now = clock.now
        for source in [
            "backend", "profiles", "modeChoices", "profile", "stream",
            "quality", "driverMode", "loopback", "profiler"
        ] {
            captures[source] = now
        }
        bootstrapped = true
        sourceErrors = [:]
        reduceCurrentSnapshot()
    }

    func runRefreshCycleForTesting() async {
        await refreshCycle(explicitProfiler: false)
    }

    func processStatisticsForTesting() async -> (launches: Int, maximumConcurrent: Int) {
        await runner.policyStatistics()
    }
#endif

    func setSceneActive(_ active: Bool) {
        isSceneActive = active
        updateVisibility()
    }

    func setWindowVisible(_ visible: Bool) {
        isWindowVisible = visible
        updateVisibility()
    }

    func manualRefresh() {
        profilerRefreshRequested = true
        Task {
            await coordinator.requestRefresh { [weak self] in
                await self?.refreshCycle(explicitProfiler: true)
            }
        }
    }

    func ageLabel(for source: String) -> String {
        guard let captured = captures[source] else { return "not yet observed" }
        let duration = captured.duration(to: clock.now)
        let seconds = max(0, Int(duration.components.seconds))
        if seconds >= 10 { return "stale · \(seconds)s" }
        if seconds >= 3 { return "aging · \(seconds)s" }
        return "current · \(seconds)s"
    }

    func requestProfile(_ id: String) {
        guard let canonical = CanonicalProfile(rawValue: id),
              case .known(let choices) = profiles,
              choices.contains(where: { $0.id == id && $0.canonical }) else { return }
        pendingConfirmation = PendingConfirmation(
            operation: .setProfile(canonical),
            title: "Apply electrical profile?",
            message: "The requested profile may remain pending until a safe boundary. The panel will read the active profile back separately."
        )
    }

    func requestDriverMode(_ id: DriverModeID) {
        let vintage = id == .vintageCompatible
        pendingConfirmation = PendingConfirmation(
            operation: .setDriverMode(id),
            title: vintage ? "Use Vintage Compatible?" : "Change driver mode?",
            message: vintage
                ? "Experimental — Unverified. The requested and effective modes can differ while pending."
                : "The requested and effective modes can differ while pending. The panel will verify the result separately."
        )
    }

    func requestTimecode(armed: Bool) {
        pendingConfirmation = PendingConfirmation(
            operation: armed ? .armTimecode : .disarmTimecode,
            title: armed ? "Arm Timecode Optimized for A,B?" : "Disarm Timecode Optimized?",
            message: armed
                ? "Armed is not active. Qualification evidence and a safe boundary are still required."
                : "The driver will fail open to its requested non-timecode mode."
        )
    }

    func requestLoopbackEnable(_ pair: OutputPair) {
        pendingConfirmation = PendingConfirmation(
            operation: .enableLoopback(pair),
            title: "Expose output pair \(pair.rawValue)?",
            message: "Output pair \(pair.rawValue) will be exposed as an application-readable virtual input for this session. It is not called a master."
        )
    }

    func requestLoopbackDisable() {
        Task { await perform(.disableLoopback) }
    }

    func confirmPendingAction() {
        guard let confirmation = pendingConfirmation else { return }
        pendingConfirmation = nil
        Task { await perform(confirmation.operation) }
    }

    func cancelPendingAction() {
        pendingConfirmation = nil
    }

    private func updateVisibility() {
        let visible = isSceneActive && isWindowVisible && selectedSection != nil
        if visible {
            reducer.resumeForeground()
            loopbackDeltas = LoopbackDeltas(
                gaps: .baseline, overrunEvents: .baseline, overrunFrames: .baseline
            )
            qualityXrunDelta = .baseline
            cadenceTask?.cancel()
            Task {
                await coordinator.setVisible(true) { [weak self] in
                    await self?.refreshCycle(explicitProfiler: false)
                }
            }
            cadenceTask = Task { [weak self] in
                while !Task.isCancelled {
                    let delay = await self?.coordinator.currentBackoff() ?? .seconds(1)
                    try? await Task.sleep(for: delay)
                    if Task.isCancelled { break }
                    await self?.coordinator.requestRefresh { [weak self] in
                        await self?.refreshCycle(explicitProfiler: false)
                    }
                }
            }
        } else {
            cadenceTask?.cancel()
            cadenceTask = nil
            Task { await coordinator.cancel() }
        }
    }

    private func refreshCycle(explicitProfiler: Bool) async {
        guard !isBusy else { return }
        let cycleStart = clock.now
        var errors: [String: DashboardError] = [:]

        if !bootstrapped {
            await bootstrap(errors: &errors)
            if Task.isCancelled { return }
        }

        let shouldProfile: Bool
        if explicitProfiler || profilerRefreshRequested || profilerCapturedAt == nil {
            shouldProfile = true
        } else {
            shouldProfile = profilerCapturedAt!.duration(to: cycleStart) >= .seconds(60)
        }
        if shouldProfile {
            profilerRefreshRequested = false
            await readProfiler(errors: &errors)
            if Task.isCancelled { return }
        }

        await readQuality(errors: &errors)
        if Task.isCancelled { return }
        await readStats(errors: &errors)
        if Task.isCancelled { return }
        await readProfile(errors: &errors)
        if Task.isCancelled { return }
        await readMode(errors: &errors)
        if Task.isCancelled { return }
        await readLoopback(errors: &errors)
        if Task.isCancelled { return }

        sourceErrors = errors
        if let actionError = reducer.lastActionError {
            sourceErrors["action"] = actionError
        }
        if errors.values.contains(where: {
            $0.evidenceReason == .permissionDenied ||
                $0.evidenceReason == .protocolMismatch
        }) {
            _ = await coordinator.pauseAutomaticRetries()
        } else {
            let backendContact = !errors.values.contains(where: {
                $0.code == "backend_unavailable"
            })
            _ = await coordinator.noteBackendContact(success: backendContact)
        }
        reduceCurrentSnapshot()
    }

    private func bootstrap(errors: inout [String: DashboardError]) async {
        do {
            let output = try await runner.run(.version)
            backend = .known(try DashboardDecoder.version(output))
            captures["backend"] = clock.now
        } catch {
            record(error, source: "version", into: &errors)
        }
        if Task.isCancelled { return }
        if supports(.profiles) {
            do {
                let output = try await runner.run(.profiles)
                profiles = .known(try DashboardDecoder.profiles(output))
                captures["profiles"] = clock.now
            } catch {
                record(error, source: "profiles", into: &errors)
            }
        } else {
            profiles = .unavailable(reason: .unsupportedTail)
        }
        if Task.isCancelled { return }
        if supports(.driverModes) {
            do {
                let output = try await runner.run(.driverModes)
                modeChoices = .known(try DashboardDecoder.modeChoices(output))
                captures["modeChoices"] = clock.now
            } catch {
                record(error, source: "driverModes", into: &errors)
            }
        } else {
            modeChoices = .unavailable(reason: .unsupportedTail)
        }
        if Task.isCancelled { return }
        if case .known = backend { bootstrapped = true } else { bootstrapped = false }
    }

    private func readQuality(errors: inout [String: DashboardError]) async {
        guard supports(.quality) else {
            quality = .unavailable(reason: .unsupportedTail)
            return
        }
        do {
            let value = try DashboardDecoder.quality(try await runner.run(.quality))
            let generation: UInt64?
            if case .known(let stream) = stream { generation = stream.generation } else { generation = nil }
            qualityXrunDelta = reducer.qualityDelta(value, generation: generation)
            quality = .known(value)
            captures["quality"] = clock.now
        } catch { record(error, source: "quality", into: &errors) }
    }

    private func readStats(errors: inout [String: DashboardError]) async {
        guard supports(.stats) else {
            stream = .unavailable(reason: .unsupportedTail)
            return
        }
        do {
            stream = .known(try DashboardDecoder.stream(try await runner.run(.stats)))
            captures["stream"] = clock.now
        } catch { record(error, source: "stats", into: &errors) }
    }

    private func readProfile(errors: inout [String: DashboardError]) async {
        guard supports(.profile) else {
            profile = .unavailable(reason: .unsupportedTail)
            return
        }
        do {
            profile = .known(try DashboardDecoder.profile(try await runner.run(.profile)))
            captures["profile"] = clock.now
        } catch { record(error, source: "profile", into: &errors) }
    }

    private func readMode(errors: inout [String: DashboardError]) async {
        guard supports(.driverMode) else {
            driverMode = .unavailable(reason: .unsupportedTail)
            return
        }
        do {
            driverMode = .known(try DashboardDecoder.driverMode(try await runner.run(.driverMode)))
            captures["driverMode"] = clock.now
        } catch { record(error, source: "driverMode", into: &errors) }
    }

    private func readLoopback(errors: inout [String: DashboardError]) async {
        guard supports(.loopbackGet) else {
            loopback = .unavailable(reason: .unsupportedTail)
            return
        }
        do {
            let value = try DashboardDecoder.loopback(try await runner.run(.loopbackGet))
            loopbackDeltas = reducer.loopbackDeltas(value)
            loopback = .known(value)
            captures["loopback"] = clock.now
        } catch { record(error, source: "loopback", into: &errors) }
    }

    private func readProfiler(errors: inout [String: DashboardError]) async {
        do {
            profiler = .known(try DashboardDecoder.profiler(try await runner.run(.profiler)))
            profilerCapturedAt = clock.now
            captures["profiler"] = clock.now
        } catch { record(error, source: "profiler", into: &errors) }
    }

    private func reduceCurrentSnapshot() {
        guard case .known(let backendValue) = backend,
              case .known(let profileValue) = profile,
              case .known(let streamValue) = stream,
              case .known(let qualityValue) = quality,
              case .known(let modeValue) = driverMode,
              case .known(let loopbackValue) = loopback else {
            if sourceErrors.isEmpty, hasUnsupportedReadEvidence {
                phase = .partial
            } else if sourceErrors.values.contains(where: { $0.code == "backend_permission_denied" }) {
                phase = .permissionDenied
            } else if sourceErrors.values.contains(where: { $0.evidenceReason == .protocolMismatch }) {
                phase = .mismatch
            } else {
                phase = .offline
            }
            return
        }
        let profilerValue: ProfilerSnapshot?
        if case .known(let value) = profiler { profilerValue = value } else { profilerValue = nil }
        mismatchReasons = reducer.mismatchReasons(
            backend: backendValue,
            stream: streamValue,
            quality: qualityValue,
            mode: modeValue,
            profile: profileValue,
            loopback: loopbackValue,
            profiler: profilerValue
        )
        if !mismatchReasons.isEmpty {
            phase = .mismatch
        } else if !sourceErrors.isEmpty {
            let first = sourceErrors.values.sorted { $0.phase < $1.phase }.first!
            phase = .stale(lastUpdatedDescription: "last validated values retained", reason: first.code)
        } else if backendValue.newerMinor || profilerValue == nil {
            phase = .partial
        } else {
            phase = .online
        }
        guard case .known(let profileChoices) = profiles,
              case .known(let modes) = modeChoices else { return }
        let now = clock.now
        let snapshot = DashboardSnapshot(
            backend: SourceCapture(value: backend, capturedAt: captures["backend"] ?? now),
            profiles: SourceCapture(value: .known(profileChoices), capturedAt: captures["profiles"] ?? now),
            modeChoices: SourceCapture(value: .known(modes), capturedAt: captures["modeChoices"] ?? now),
            profile: SourceCapture(value: .known(profileValue), capturedAt: captures["profile"] ?? now),
            stream: SourceCapture(value: .known(streamValue), capturedAt: captures["stream"] ?? now),
            quality: SourceCapture(value: .known(qualityValue), capturedAt: captures["quality"] ?? now),
            driverMode: SourceCapture(value: .known(modeValue), capturedAt: captures["driverMode"] ?? now),
            loopback: SourceCapture(value: .known(loopbackValue), capturedAt: captures["loopback"] ?? now),
            profiler: SourceCapture(value: profiler, capturedAt: captures["profiler"] ?? now),
            phase: phase,
            sourceErrors: sourceErrors,
            mismatchReasons: mismatchReasons
        )
        reducer.accepted(snapshot)
    }

    private func perform(_ operation: ControlOperation) async {
        guard !isBusy else { return }
        isBusy = true
        actionOutcome = .idle
        await coordinator.cancel()
        let previousProfile = profile
        let previousMode = driverMode
        let previousLoopback = loopback
        do {
            let output = try await runner.run(operation)
            let expectation = try validateMutation(output, operation: operation)
            let verified = try await readBackMatches(
                operation, expectation: expectation
            )
            if verified {
                actionOutcome = pendingResult(operation)
                reducer.clearActionErrorAfterNewSuccess()
            } else {
                actionOutcome = try await compensate(
                    operation,
                    previousProfile: previousProfile,
                    previousMode: previousMode,
                    previousLoopback: previousLoopback
                )
            }
        } catch {
            let dashboardError = normalized(error, phase: "mutation")
            reducer.preserveActionError(dashboardError)
            sourceErrors["action"] = dashboardError
            actionOutcome = .indeterminate(dashboardError)
        }
        isBusy = false
        await refreshCycle(explicitProfiler: false)
        updateVisibility()
    }

    private func validateMutation(
        _ output: ProcessOutput,
        operation: ControlOperation
    ) throws -> MutationExpectation {
        guard let expected = operation.expectedOperation else {
            throw DecodeFailure.protocolViolation("read operation used as mutation")
        }
        switch operation {
        case .setProfile:
            return .profile(
                try DashboardDecoder.profile(output, expectedOperation: expected)
            )
        case .setDriverMode, .armTimecode, .disarmTimecode:
            return .driverMode(
                try DashboardDecoder.driverMode(output, expectedOperation: expected)
            )
        case .enableLoopback, .disableLoopback:
            return .loopback(
                try DashboardDecoder.loopback(output, expectedOperation: expected)
            )
        default:
            throw DecodeFailure.protocolViolation("operation is not mutable")
        }
    }

    private func readBackMatches(
        _ operation: ControlOperation,
        expectation: MutationExpectation
    ) async throws -> Bool {
        switch operation {
        case .setProfile:
            let value = try DashboardDecoder.profile(try await runner.run(.profile))
            profile = .known(value)
            return DashboardReducer.mutationMatches(
                expectation: expectation, profile: value
            )
        case .setDriverMode, .armTimecode, .disarmTimecode:
            let value = try DashboardDecoder.driverMode(try await runner.run(.driverMode))
            driverMode = .known(value)
            return DashboardReducer.mutationMatches(
                expectation: expectation, driverMode: value
            )
        case .enableLoopback, .disableLoopback:
            let value = try DashboardDecoder.loopback(try await runner.run(.loopbackGet))
            loopback = .known(value)
            return DashboardReducer.mutationMatches(
                expectation: expectation, loopback: value
            )
        default: return false
        }
    }

    private func compensate(
        _ operation: ControlOperation,
        previousProfile: Evidence<ProfileSnapshot>,
        previousMode: Evidence<DriverModeSnapshot>,
        previousLoopback: Evidence<LoopbackSnapshot>
    ) async throws -> ActionOutcome {
        let disagreement = DashboardError(
            code: "read_back_disagreement",
            message: "The separate read-back disagreed with the successful mutation.",
            retryable: true,
            phase: "read-back"
        )
        let rollback: ControlOperation?
        switch operation {
        case .setProfile:
            if case .known(let prior) = previousProfile,
               let canonical = CanonicalProfile(rawValue: prior.activeProfile) {
                rollback = .setProfile(canonical)
            } else {
                rollback = nil
            }
        case .setDriverMode:
            if case .known(let prior) = previousMode,
               let mode = DriverModeID(rawValue: prior.requestedMode) {
                rollback = .setDriverMode(mode)
            } else { rollback = nil }
        case .armTimecode, .disarmTimecode:
            if case .known(let prior) = previousMode,
               case .known(let timecode) = prior.timecode {
                rollback = timecode.armed ? .armTimecode : .disarmTimecode
            } else { rollback = nil }
        case .enableLoopback, .disableLoopback:
            if case .known(let prior) = previousLoopback {
                rollback = prior.enabled
                    ? OutputPair(rawValue: prior.sourcePair).map(ControlOperation.enableLoopback)
                    : .disableLoopback
            } else { rollback = nil }
        default: rollback = nil
        }
        guard let rollback else {
            reducer.preserveActionError(disagreement)
            return .rollbackUnavailable(disagreement)
        }
        do {
            let output = try await runner.run(rollback)
            let expectation = try validateMutation(output, operation: rollback)
            if try await readBackMatches(rollback, expectation: expectation) {
                reducer.preserveActionError(disagreement)
                return .rolledBack(disagreement)
            }
            reducer.preserveActionError(disagreement)
            return .rollbackFailed(disagreement)
        } catch {
            let failure = normalized(error, phase: "rollback")
            reducer.preserveActionError(failure)
            return .rollbackFailed(failure)
        }
    }

    private func pendingResult(_ operation: ControlOperation) -> ActionOutcome {
        if case .known(let value) = driverMode,
           (operation == .armTimecode ||
            operation == .disarmTimecode ||
            {
                if case .setDriverMode = operation { return true }
                return false
            }()),
           value.pending {
            return .pending("Request accepted; effective mode remains \(value.effectiveMode) until a safe boundary.")
        }
        return .applied("Action applied and verified by a separate read-back.")
    }

    private func record(
        _ error: Error,
        source: String,
        into errors: inout [String: DashboardError]
    ) {
        let value = normalized(error, phase: source)
        errors[source] = value
        let evidence: EvidenceReason = value.evidenceReason
        switch source {
        case "version": backend = .unavailable(reason: evidence)
        case "profiles": profiles = .unavailable(reason: evidence)
        case "driverModes": modeChoices = .unavailable(reason: evidence)
        case "quality": quality = staleOrUnavailable(quality, reason: evidence)
        case "stats": stream = staleOrUnavailable(stream, reason: evidence)
        case "profile": profile = staleOrUnavailable(profile, reason: evidence)
        case "driverMode": driverMode = staleOrUnavailable(driverMode, reason: evidence)
        case "loopback": loopback = staleOrUnavailable(loopback, reason: evidence)
        case "profiler": profiler = staleOrUnavailable(profiler, reason: evidence)
        default: break
        }
    }

    private func staleOrUnavailable<T: Sendable>(
        _ old: Evidence<T>,
        reason: EvidenceReason
    ) -> Evidence<T> {
        if case .known = old {
            return old
        }
        return .unavailable(reason: reason)
    }

    private func normalized(_ error: Error, phase: String) -> DashboardError {
        if case DecodeFailure.api(let apiError) = error { return apiError }
        if let process = error as? ProcessRunnerError {
            switch process {
            case .timedOut:
                return DashboardError(code: "timeout", message: "The operation exceeded its bounded deadline.", retryable: true, phase: phase)
            case .truncated:
                return DashboardError(code: "truncated", message: "The tool output exceeded the accepted bound.", retryable: false, phase: phase)
            case .cancelled:
                return DashboardError(code: "cancelled", message: "The foreground operation was cancelled.", retryable: true, phase: phase)
            default:
                return DashboardError(code: "tool_unavailable", message: process.description, retryable: false, phase: phase)
            }
        }
        return DashboardError(
            code: "protocol_mismatch",
            message: String(describing: error).prefix(240).description,
            retryable: false,
            phase: phase
        )
    }

    private func supports(_ operation: ControlOperation) -> Bool {
        guard let capability = operation.requiredReadCapability else { return true }
        guard case .known(let identity) = backend else { return false }
        return identity.capabilities.contains(capability)
    }

    private var hasUnsupportedReadEvidence: Bool {
        func unsupported<T>(_ evidence: Evidence<T>) -> Bool {
            if case .unavailable(reason: .unsupportedTail) = evidence {
                return true
            }
            return false
        }
        return unsupported(profiles) || unsupported(modeChoices) ||
            unsupported(profile) || unsupported(stream) || unsupported(quality) ||
            unsupported(driverMode) || unsupported(loopback)
    }
}
