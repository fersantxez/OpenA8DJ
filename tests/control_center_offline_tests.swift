import Darwin
import Foundation

private enum TestFailure: Error, CustomStringConvertible {
    case failed(String)
    var description: String {
        switch self { case .failed(let message): return message }
    }
}

private func check(
    _ condition: @autoclosure () -> Bool,
    _ message: String,
    file: StaticString = #filePath,
    line: UInt = #line
) throws {
    if !condition() {
        throw TestFailure.failed("\(file):\(line): \(message)")
    }
}

private func childHelperIfRequested() -> Bool {
    let name = URL(fileURLWithPath: CommandLine.arguments[0]).lastPathComponent
    guard name.hasPrefix("cc-helper-") else { return false }
    switch name {
    case "cc-helper-slow":
        Thread.sleep(forTimeInterval: 0.35)
        print("{}")
    case "cc-helper-timeout":
        Thread.sleep(forTimeInterval: 10)
        print("{}")
    case "cc-helper-overflow":
        FileHandle.standardOutput.write(Data(repeating: 0x61, count: 600 * 1024))
    case "cc-helper-stderr-overflow":
        FileHandle.standardError.write(Data(repeating: 0x62, count: 40 * 1024))
        print("{}")
    default:
        print("{}")
    }
    return true
}

private struct Fixtures {
    let root: URL

    func output(_ name: String, status: Int32 = 0) throws -> ProcessOutput {
        ProcessOutput(
            status: status,
            stdout: try Data(contentsOf: root.appendingPathComponent(name)),
            stderr: Data()
        )
    }

    func object(_ name: String) throws -> [String: Any] {
        try JSONSerialization.jsonObject(
            with: Data(contentsOf: root.appendingPathComponent(name))
        ) as! [String: Any]
    }

    func output(
        modifying name: String,
        status: Int32 = 0,
        _ mutation: (inout [String: Any]) -> Void
    ) throws -> ProcessOutput {
        var object = try object(name)
        mutation(&object)
        var data = try JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])
        data.append(0x0a)
        return ProcessOutput(status: status, stdout: data, stderr: Data())
    }
}

private func decoderTests(_ fixtures: Fixtures) throws {
    let version = try DashboardDecoder.version(fixtures.output("good_version.json"))
    try check(version.apiVersion == SemanticVersion(major: 1, minor: 1, patch: 0), "API 1.1 not decoded")
    try check(version.capabilities.contains("loopback.write"), "capability lost")

    let profiles = try DashboardDecoder.profiles(fixtures.output("good_profiles.json"))
    try check(profiles.count == 2 && profiles.allSatisfy(\.canonical), "canonical profiles not intersected")

    let modes = try DashboardDecoder.modeChoices(fixtures.output("good_modes.json"))
    try check(modes.contains(where: { $0.id == "vintage-compatible" }), "Vintage choice missing")

    let profile = try DashboardDecoder.profile(fixtures.output("good_profile.json"))
    try check(profile.inputMode == "timecode-vinyl" && profile.activeProfile == "traktor-dvs-vinyl", "profile mapping wrong")

    let stream = try DashboardDecoder.stream(fixtures.output("good_stats.json"))
    try check(stream.streaming && stream.sampleRateHz == 48_000, "stats stream mapping wrong")
    let partial = try DashboardDecoder.stream(fixtures.output("partial_stats.json"))
    try check(!partial.streaming && partial.generation == nil, "compatible partial stats tail fabricated state")

    let mode = try DashboardDecoder.driverMode(fixtures.output("good_driver_mode.json"))
    try check(mode.requestedMode == "performance" && mode.effectiveMode == "balanced" && mode.pending, "pending disagreement hidden")
    if case .known(let timecode) = mode.timecode {
        try check(timecode.armed && !timecode.optimizedActive, "armed was rendered as active")
        try check(
            DashboardReducer.timecodeWaitReason(timecode, pending: mode.pending) == "pending safe boundary",
            "qualified pending wait reason wrong"
        )
    } else {
        throw TestFailure.failed("timecode tail unavailable")
    }
    if case .known(let vintage) = mode.vintage {
        try check(vintage.status == "partial" && vintage.claim == "unverified" && vintage.experimental, "Vintage label weakened")
    } else {
        throw TestFailure.failed("Vintage tail unavailable")
    }

    func timecodeVariant(
        _ mutate: (inout [String: Any], inout [String: Any]) -> Void
    ) throws -> (DriverModeSnapshot, TimecodeSnapshot) {
        let decoded = try DashboardDecoder.driverMode(
            fixtures.output(modifying: "good_driver_mode.json") { root in
                var data = root["data"] as! [String: Any]
                var timecode = data["timecodeOptimized"] as! [String: Any]
                mutate(&data, &timecode)
                data["timecodeOptimized"] = timecode
                root["data"] = data
            }
        )
        guard case .known(let timecode) = decoded.timecode else {
            throw TestFailure.failed("timecode variant became unavailable")
        }
        return (decoded, timecode)
    }
    let wrongProfile = try timecodeVariant { _, timecode in
        timecode["profileVerified"] = false
        timecode["armState"] = "waiting_profile"
    }
    try check(
        DashboardReducer.timecodeWaitReason(wrongProfile.1, pending: wrongProfile.0.pending)
            == "wrong or unverified electrical profile",
        "wrong-profile wait reason hidden"
    )
    let staleEvidence = try timecodeVariant { _, timecode in
        timecode["windowFresh"] = false
        timecode["qualified"] = false
        timecode["eligibleWindows"] = 0
        timecode["armState"] = "qualifying"
    }
    try check(
        DashboardReducer.timecodeWaitReason(staleEvidence.1, pending: staleEvidence.0.pending)
            == "stale or missing evidence",
        "stale-evidence wait reason hidden"
    )
    let insufficient = try timecodeVariant { _, timecode in
        timecode["qualified"] = false
        timecode["eligibleWindows"] = 1
        timecode["armState"] = "qualifying"
    }
    try check(
        DashboardReducer.timecodeWaitReason(insufficient.1, pending: insufficient.0.pending)
            == "insufficient qualifying windows",
        "insufficient-window wait reason hidden"
    )
    let active = try timecodeVariant { data, timecode in
        data["requestedMode"] = "timecode-optimized"
        data["effectiveMode"] = "timecode-optimized"
        data["pending"] = false
        timecode["optimizedActive"] = true
        timecode["armState"] = "active"
    }
    try check(
        active.1.optimizedActive &&
            DashboardReducer.timecodeWaitReason(active.1, pending: false) == "active",
        "active Timecode state hidden"
    )
    let failOpen = try timecodeVariant { _, timecode in
        timecode["lastFailOpenReason"] = "xrun_or_transport_error"
    }
    try check(
        DashboardReducer.timecodeWaitReason(failOpen.1, pending: false)
            == "xrun_or_transport_error",
        "fail-open reason hidden"
    )

    let loopback = try DashboardDecoder.loopback(fixtures.output("good_loopback.json"))
    try check(!loopback.enabled && loopback.sourcePair == "A", "loopback default is not disabled")
    let enabledLoopback = try DashboardDecoder.loopback(
        fixtures.output(modifying: "good_loopback.json") {
            var data = $0["data"] as! [String: Any]
            data["enabled"] = true
            data["sourcePair"] = "C"
            data["physicalPlaybackPublishing"] = true
            $0["data"] = data
        }
    )
    try check(
        enabledLoopback.enabled && enabledLoopback.sourcePair == "C",
        "explicit loopback source not decoded"
    )
    let physicalOnly = try DashboardDecoder.loopback(
        fixtures.output(modifying: "good_loopback.json") {
            var data = $0["data"] as! [String: Any]
            data["physicalPlaybackPublishing"] = true
            $0["data"] = data
        }
    )
    try check(
        !physicalOnly.enabled && physicalOnly.physicalPlaybackPublishing,
        "physical publishing was falsely coupled to loopback"
    )

    let quality = try DashboardDecoder.quality(fixtures.output("good_quality.ndjson"))
    try check(quality.classification == "stable", "quality second sample not used")
    try check(quality.captureJitter.p95.label == "≤ 50 µs", "jitter upper bound fabricated")
    do {
        _ = try DashboardDecoder.quality(
            fixtures.output("good_quality.ndjson", status: 1)
        )
        throw TestFailure.failed("nonzero quality process accepted")
    } catch DecodeFailure.protocolViolation { }

    let profiler = try DashboardDecoder.profiler(fixtures.output("good_profiler.json"))
    try check(profiler.overallStatus == "PASS" && profiler.checks.count == 8, "profiler required checks missing")
    let firmwareEvidence = profiler.checks.first { $0.id == "device.firmware" }?.evidence.first
    try check(
        firmwareEvidence?.available == true &&
            firmwareEvidence?.key == "device.firmwareVersion" &&
            firmwareEvidence?.value == "31",
        "typed firmware evidence was discarded or reinterpreted"
    )
    let unknown = try DashboardDecoder.profiler(fixtures.output("profiler_unknown.json", status: 3))
    try check(unknown.overallStatus == "UNKNOWN", "profiler UNKNOWN collapsed")
    do {
        _ = try DashboardDecoder.profiler(
            fixtures.output("profiler_unknown.json", status: 70)
        )
        throw TestFailure.failed("profiler status 70 accepted")
    } catch DecodeFailure.protocolViolation { }

    let mutationProfile = try DashboardDecoder.profile(
        fixtures.output("profile_set_custom.json"),
        expectedOperation: "profile.set"
    )
    let readBackProfile = try DashboardDecoder.profile(
        fixtures.output("profile_get_custom.json")
    )
    try check(
        DashboardReducer.mutationMatches(
            expectation: .profile(mutationProfile), profile: readBackProfile
        ),
        "profile.set custom state falsely disagreed with identical GET"
    )
    try check(
        mutationProfile.activeProfile == "custom",
        "requestedProfile was substituted for activeProfile"
    )

    do {
        _ = try DashboardDecoder.profile(fixtures.output("action_error.json", status: 5), expectedOperation: "profile.set")
        throw TestFailure.failed("action error accepted")
    } catch DecodeFailure.api(let error) {
        try check(error.code == "profile_apply_failed" && error.retryable, "action error envelope lost")
    }
}

private func mismatchTests(_ fixtures: Fixtures) throws {
    let mutations: [String: (inout [String: Any]) -> Void] = [
        "schema": { $0["schema"] = "wrong" },
        "major": { $0["apiVersion"] = "2.0" },
        "minor": { $0["apiVersion"] = "1.0" },
        "operation": { $0["operation"] = "stats.get" },
        "required-type": {
            var data = $0["data"] as! [String: Any]
            data["transport"] = 1
            $0["data"] = data
        }
    ]
    for (name, mutation) in mutations {
        do {
            _ = try DashboardDecoder.version(fixtures.output(modifying: "good_version.json", mutation))
            throw TestFailure.failed("\(name) mismatch accepted")
        } catch DecodeFailure.protocolViolation { }
    }

    do {
        _ = try DashboardDecoder.driverMode(fixtures.output(modifying: "good_driver_mode.json") {
            var data = $0["data"] as! [String: Any]
            data["requestedMode"] = "future-turbo"
            $0["data"] = data
        })
        throw TestFailure.failed("unknown driver mode enum accepted")
    } catch DecodeFailure.protocolViolation { }

    do {
        _ = try DashboardDecoder.driverMode(fixtures.output(modifying: "good_driver_mode.json") {
            var data = $0["data"] as! [String: Any]
            data["pending"] = false
            $0["data"] = data
        })
        throw TestFailure.failed("pending cross-field mismatch accepted")
    } catch DecodeFailure.protocolViolation { }

    for key in ["schema", "schemaVersion"] {
        do {
            _ = try DashboardDecoder.profiler(fixtures.output(modifying: "good_profiler.json") {
                $0[key] = key == "schema" ? "wrong" : 2
            })
            throw TestFailure.failed("profiler \(key) mismatch accepted")
        } catch DecodeFailure.protocolViolation { }
    }

    let good = try fixtures.output("good_version.json").stdout
    for (name, data) in [
        ("missing-newline", Data(good.dropLast())),
        ("invalid-utf8", Data([0xff, 0x0a])),
        ("second-object", good + Data(" {}\n".utf8)),
        ("duplicate-key", Data("{\"a\":1,\"a\":2}\n".utf8))
    ] {
        do {
            _ = try StrictJSON.object(from: data)
            throw TestFailure.failed("\(name) accepted")
        } catch DecodeFailure.protocolViolation { }
    }
}

private func reducerTests(_ fixtures: Fixtures) throws {
    var reducer = DashboardReducer()
    let now = ContinuousClock.now
    try check(reducer.age(capturedAt: now.advanced(by: .seconds(-2)), now: now) == .current, "fresh threshold wrong")
    try check(reducer.age(capturedAt: now.advanced(by: .seconds(-4)), now: now) == .aging, "aging threshold wrong")
    try check(reducer.age(capturedAt: now.advanced(by: .seconds(-11)), now: now) == .stale, "stale threshold wrong")

    try check(reducer.counterDelta(previous: nil, current: 4, sameGeneration: true) == .baseline, "first delta not baseline")
    try check(reducer.counterDelta(previous: 4, current: 7, sameGeneration: true) == .value(3), "valid delta wrong")
    try check(reducer.counterDelta(previous: 7, current: 2, sameGeneration: true) == .counterReset, "counter decrease not reset")
    try check(reducer.counterDelta(previous: 7, current: 8, sameGeneration: false) == .backendMismatch, "generation mismatch hidden")
    reducer.resumeForeground()
    try check(reducer.counterDelta(previous: nil, current: 9, sameGeneration: true) == .baseline, "foreground resume retained baseline")

    var disabled = try DashboardDecoder.loopback(fixtures.output("good_loopback.json"))
    let baseline = reducer.loopbackDeltas(disabled)
    try check(baseline.gaps == .baseline && baseline.overrunEvents == .baseline, "loopback first values not baseline")
    disabled = LoopbackSnapshot(
        enabled: true, sourcePair: "B", sessionOnly: true, physicalPlaybackPublishing: true,
        ringCapacity: disabled.ringCapacity, generation: disabled.generation,
        registeredReaderCount: 1, sourceFramesPublished: 20, framesDelivered: 18,
        silenceFrames: 2, gapFrames: 2, overrunEvents: 1, overrunFrames: 3
    )
    let delta = reducer.loopbackDeltas(disabled)
    try check(delta.gaps == .value(2) && delta.overrunEvents == .value(1), "loopback deltas wrong")

    let reconnected = LoopbackSnapshot(
        enabled: false, sourcePair: "A", sessionOnly: true,
        physicalPlaybackPublishing: true, ringCapacity: disabled.ringCapacity,
        generation: disabled.generation + 1, registeredReaderCount: 0,
        sourceFramesPublished: 1, framesDelivered: 0, silenceFrames: 0,
        gapFrames: 0, overrunEvents: 0, overrunFrames: 0
    )
    let reconnectDelta = reducer.loopbackDeltas(reconnected)
    try check(
        reconnectDelta.gaps == .baseline &&
            reconnectDelta.overrunEvents == .baseline,
        "new generation did not start a safe baseline"
    )
    let rolledBackGeneration = LoopbackSnapshot(
        enabled: false, sourcePair: "A", sessionOnly: true,
        physicalPlaybackPublishing: false, ringCapacity: disabled.ringCapacity,
        generation: disabled.generation, registeredReaderCount: 0,
        sourceFramesPublished: 2, framesDelivered: 0, silenceFrames: 0,
        gapFrames: 0, overrunEvents: 0, overrunFrames: 0
    )
    let rollbackDelta = reducer.loopbackDeltas(rolledBackGeneration)
    try check(rollbackDelta.gaps == .backendMismatch, "generation rollback was not a mismatch")

    let backend = try DashboardDecoder.version(fixtures.output("good_version.json"))
    let profiles = try DashboardDecoder.profiles(fixtures.output("good_profiles.json"))
    let modes = try DashboardDecoder.modeChoices(fixtures.output("good_modes.json"))
    let profile = try DashboardDecoder.profile(fixtures.output("good_profile.json"))
    let stream = try DashboardDecoder.stream(fixtures.output("good_stats.json"))
    let quality = try DashboardDecoder.quality(fixtures.output("good_quality.ndjson"))
    let driverMode = try DashboardDecoder.driverMode(fixtures.output("good_driver_mode.json"))
    let loopback = try DashboardDecoder.loopback(fixtures.output("good_loopback.json"))
    let profiler = try DashboardDecoder.profiler(fixtures.output("good_profiler.json"))
    let snapshot = DashboardSnapshot(
        backend: SourceCapture(value: .known(backend), capturedAt: now),
        profiles: SourceCapture(value: .known(profiles), capturedAt: now),
        modeChoices: SourceCapture(value: .known(modes), capturedAt: now),
        profile: SourceCapture(value: .known(profile), capturedAt: now),
        stream: SourceCapture(value: .known(stream), capturedAt: now),
        quality: SourceCapture(value: .known(quality), capturedAt: now),
        driverMode: SourceCapture(value: .known(driverMode), capturedAt: now),
        loopback: SourceCapture(value: .known(loopback), capturedAt: now),
        profiler: SourceCapture(value: .known(profiler), capturedAt: now),
        phase: .online,
        sourceErrors: [:],
        mismatchReasons: []
    )
    let independentlyPublishing = LoopbackSnapshot(
        enabled: false, sourcePair: "A", sessionOnly: true,
        physicalPlaybackPublishing: true, ringCapacity: loopback.ringCapacity,
        generation: loopback.generation, registeredReaderCount: 0,
        sourceFramesPublished: 0, framesDelivered: 0, silenceFrames: 0,
        gapFrames: 0, overrunEvents: 0, overrunFrames: 0
    )
    let physicalPublishingReasons = reducer.mismatchReasons(
        backend: backend, stream: stream, quality: quality,
        mode: driverMode, profile: profile, loopback: independentlyPublishing,
        profiler: profiler
    )
    try check(
        !physicalPublishingReasons.contains(where: { $0.contains("publishing while disabled") }),
        "physical publishing was falsely coupled to loopback enablement"
    )
    reducer.accepted(snapshot)
    let failure = DashboardError(
        code: "backend_unavailable", message: "offline",
        retryable: true, phase: "stats"
    )
    guard let stale = reducer.staleSnapshot(for: failure) else {
        throw TestFailure.failed("last-good snapshot was discarded")
    }
    if case .stale(_, let reason) = stale.phase {
        try check(reason == "backend_unavailable", "stale reason lost")
    } else {
        throw TestFailure.failed("failed refresh returned healthy phase")
    }
    if case .known(let retainedStream) = stale.stream.value {
        try check(retainedStream == stream, "stale last-good stream changed")
    } else {
        throw TestFailure.failed("stale last-good stream hidden")
    }
    try check(stale.sourceErrors["refresh"] == failure, "stale source error hidden")
}

private func operationPolicyTests() throws {
    try check(ControlOperation.armTimecode.arguments == [
        "api", "driver-mode", "arm", "timecode-optimized", "--input-pairs", "A,B"
    ], "timecode arm vector drift")
    try check(ControlOperation.enableLoopback(.D).arguments == [
        "api", "loopback", "enable", "D"
    ], "loopback allowlist drift")
    try check(ControlOperation.quality.timeout == 7 && ControlOperation.profiler.timeout == 8, "timeout policy drift")
    try check(ControlOperation.version.isRead && !ControlOperation.disableLoopback.isRead, "NO_WAKE read policy wrong")
    let advertised = Set(["stats.read", "profile.read"])
    let reads: [ControlOperation] = [
        .stats, .quality, .profile, .driverMode, .loopbackGet
    ]
    let allowed = reads.filter {
        $0.requiredReadCapability.map(advertised.contains) ?? true
    }
    try check(
        allowed == [.stats, .profile],
        "missing read capabilities did not suppress unsupported operations"
    )
}

private func makeHelper(_ name: String, in directory: URL) throws -> URL {
    let source = URL(fileURLWithPath: CommandLine.arguments[0])
    let destination = directory.appendingPathComponent(name)
    try FileManager.default.copyItem(at: source, to: destination)
    try FileManager.default.setAttributes([.posixPermissions: 0o755], ofItemAtPath: destination.path)
    return destination
}

private func runnerTests() async throws {
    let directory = FileManager.default.temporaryDirectory
        .appendingPathComponent("opena8dj-control-center-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: false)
    defer { try? FileManager.default.removeItem(at: directory) }

    let slow = try makeHelper("cc-helper-slow", in: directory)
    let serialized = BoundedProcessRunner(resolver: { _ in slow })
    async let first = serialized.run(.version)
    async let second = serialized.run(.profile)
    _ = try await (first, second)
    let stats = await serialized.policyStatistics()
    try check(stats.launches == 2 && stats.maximumConcurrent == 1, "processes overlapped")

    let waitingRunner = BoundedProcessRunner(resolver: { _ in slow })
    let active = Task { try await waitingRunner.run(.version) }
    try await Task.sleep(for: .milliseconds(25))
    let waiter = Task { try await waitingRunner.run(.profile) }
    waiter.cancel()
    _ = try await active.value
    do {
        _ = try await waiter.value
        throw TestFailure.failed("cancelled waiter launched")
    } catch ProcessRunnerError.cancelled { }
    let waitingStats = await waitingRunner.policyStatistics()
    try check(waitingStats.launches == 1, "cancelled queued task started a process")

    for (helper, expected) in [
        ("cc-helper-overflow", ProcessRunnerError.truncated),
        ("cc-helper-stderr-overflow", ProcessRunnerError.truncated)
    ] {
        let executable = try makeHelper(helper, in: directory)
        let runner = BoundedProcessRunner(resolver: { _ in executable })
        do {
            _ = try await runner.run(.version)
            throw TestFailure.failed("\(helper) cap not enforced")
        } catch let error as ProcessRunnerError {
            try check(error == expected, "\(helper) wrong error \(error)")
        }
    }

    let timeout = try makeHelper("cc-helper-timeout", in: directory)
    let timeoutRunner = BoundedProcessRunner(resolver: { _ in timeout })
    do {
        _ = try await timeoutRunner.run(.version)
        throw TestFailure.failed("timeout not enforced")
    } catch ProcessRunnerError.timedOut { }

    let cancelRunner = BoundedProcessRunner(resolver: { _ in timeout })
    let cancelled = Task { try await cancelRunner.run(.version) }
    try await Task.sleep(for: .milliseconds(60))
    cancelled.cancel()
    do {
        _ = try await cancelled.value
        throw TestFailure.failed("running cancellation not reported")
    } catch ProcessRunnerError.cancelled { }

    let regular = try makeHelper("opena8dj-control", in: directory)
    _ = try BoundedProcessRunner.trustedTool(at: regular, expectedName: "opena8dj-control")
    let symlink = directory.appendingPathComponent("opena8dj-hardware-profiler")
    try FileManager.default.createSymbolicLink(at: symlink, withDestinationURL: regular)
    do {
        _ = try BoundedProcessRunner.trustedTool(
            at: symlink, expectedName: "opena8dj-hardware-profiler"
        )
        throw TestFailure.failed("symbolic link accepted as bundled tool")
    } catch ProcessRunnerError.untrustedTool { }
}

private actor Counter {
    var value = 0
    var cancelled = false
    func increment() { value += 1 }
    func markCancelled() { cancelled = true }
}

private func coordinatorTests() async throws {
    let coordinator = RefreshCoordinator()
    let counter = Counter()
    await coordinator.setVisible(true) {
        await counter.increment()
        try? await Task.sleep(for: .milliseconds(100))
    }
    try await Task.sleep(for: .milliseconds(20))
    await coordinator.requestRefresh {
        await counter.increment()
    }
    await coordinator.requestRefresh {
        await counter.increment()
    }
    try await Task.sleep(for: .milliseconds(240))
    let coalescedCount = await counter.value
    try check(coalescedCount == 2, "manual refresh did not coalesce")
    await coordinator.cancel()

    let foreground = RefreshCoordinator()
    let lifecycle = Counter()
    await foreground.setVisible(true) {
        do {
            try await Task.sleep(for: .seconds(2))
        } catch {
            await lifecycle.markCancelled()
        }
    }
    try await Task.sleep(for: .milliseconds(30))
    await foreground.cancel()
    try await Task.sleep(for: .milliseconds(30))
    let didCancel = await lifecycle.cancelled
    try check(didCancel, "background transition did not cancel current cycle")

    let backoff2 = await foreground.noteBackendContact(success: false)
    let backoff4 = await foreground.noteBackendContact(success: false)
    let backoff8 = await foreground.noteBackendContact(success: false)
    let backoff15 = await foreground.noteBackendContact(success: false)
    let reset = await foreground.noteBackendContact(success: true)
    try check(backoff2 == .seconds(2), "backoff 2s wrong")
    try check(backoff4 == .seconds(4), "backoff 4s wrong")
    try check(backoff8 == .seconds(8), "backoff 8s wrong")
    try check(backoff15 == .seconds(15), "backoff cap wrong")
    try check(reset == .seconds(1), "backoff did not reset")
    let paused = await foreground.pauseAutomaticRetries()
    let pausedCurrent = await foreground.currentBackoff()
    try check(
        paused == .seconds(15) && pausedCurrent == .seconds(15),
        "permission/protocol pause can spin faster than 15 seconds"
    )
}

@main
struct ControlCenterOfflineTests {
    static func main() async {
        if childHelperIfRequested() { return }
        do {
            guard CommandLine.arguments.count == 2 else {
                throw TestFailure.failed("usage: control-center-offline-tests FIXTURE_DIR")
            }
            let fixtures = Fixtures(root: URL(fileURLWithPath: CommandLine.arguments[1]))
            try decoderTests(fixtures)
            try mismatchTests(fixtures)
            try reducerTests(fixtures)
            try operationPolicyTests()
            try await runnerTests()
            try await coordinatorTests()
            print("control center offline tests: PASS")
        } catch {
            FileHandle.standardError.write(Data("control center offline tests: FAIL: \(error)\n".utf8))
            exit(1)
        }
    }
}
