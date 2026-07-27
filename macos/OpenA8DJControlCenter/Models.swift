import Foundation

enum EvidenceReason: Equatable, Sendable, CustomStringConvertible {
    case backendUnavailable
    case permissionDenied
    case protocolMismatch
    case unsupportedTail
    case notStreaming
    case insufficientData
    case profilerUnknown(code: String)
    case timedOut
    case truncated
    case stale
    case notYetObserved
    case unrecognized(code: String)

    var description: String {
        switch self {
        case .backendUnavailable: return "backend unavailable"
        case .permissionDenied: return "permission denied"
        case .protocolMismatch: return "protocol/backend mismatch"
        case .unsupportedTail: return "unsupported by this backend"
        case .notStreaming: return "not streaming"
        case .insufficientData: return "insufficient data"
        case .profilerUnknown(let code): return "UNKNOWN (\(code))"
        case .timedOut: return "timed out"
        case .truncated: return "truncated output"
        case .stale: return "stale"
        case .notYetObserved: return "not yet observed"
        case .unrecognized(let code): return "unrecognized (\(code))"
        }
    }
}

enum Evidence<Value: Sendable>: Sendable {
    case known(Value)
    case unavailable(reason: EvidenceReason)
}

extension Evidence: Equatable where Value: Equatable {
    static func == (lhs: Evidence<Value>, rhs: Evidence<Value>) -> Bool {
        switch (lhs, rhs) {
        case (.known(let a), .known(let b)): return a == b
        case (.unavailable(let a), .unavailable(let b)): return a == b
        default: return false
        }
    }
}

struct SemanticVersion: Equatable, Sendable, CustomStringConvertible {
    let major: Int
    let minor: Int
    let patch: Int
    var description: String { "\(major).\(minor)" + (patch == 0 ? "" : ".\(patch)") }
}

struct BackendIdentity: Equatable, Sendable {
    let apiVersion: SemanticVersion
    let capabilities: Set<String>
    let newerMinor: Bool
    let fingerprint: String
}

struct ProfileChoice: Identifiable, Equatable, Sendable {
    let id: String
    let title: String
    let surface: String
    let summary: String
    let canonical: Bool
}

struct DriverModeChoice: Identifiable, Equatable, Sendable {
    let id: String
    let title: String
    let summary: String
}

struct ProfileSnapshot: Equatable, Sendable {
    let activeProfile: String
    let inputMode: String
    let inputModeValue: UInt64
    let inputDecode: Bool
    let softwareLock: Bool
    let groundLiftVinyl: Bool
    let groundLiftCDLine: Bool
    let groundLiftPhono: Bool
    let inputSources: [String: String]
    let inputTransforms: [String: String]
}

struct StreamSnapshot: Equatable, Sendable {
    let streaming: Bool
    let sampleRateHz: Double
    let outputRingFrames: UInt64
    let outputTargetLatencyFrames: UInt64
    let generation: UInt64?
}

struct PercentileBound: Equatable, Sendable {
    let upperBoundUs: UInt64?
    let overflow: Bool

    var label: String {
        overflow ? "overflow" : upperBoundUs.map { "≤ \($0) µs" } ?? "UNKNOWN"
    }
}

struct DirectionJitter: Equatable, Sendable {
    let p95: PercentileBound
    let p99: PercentileBound
    let samples: UInt64
    let active: Bool
}

struct ISOErrors: Equatable, Sendable {
    let captureTotal: UInt64
    let playbackTotal: UInt64
    let components: [String: UInt64]
}

struct Xruns: Equatable, Sendable {
    let totalHardXruns: UInt64
    let activeUnderruns: UInt64
    let ringOverruns: UInt64
    let lateWriteBatches: UInt64
    let lateWriteFrames: UInt64
}

struct USBQualitySnapshot: Equatable, Sendable {
    let classification: String
    let reasons: [String]
    let captureJitter: DirectionJitter
    let playbackJitter: DirectionJitter
    let isoErrors: ISOErrors
    let xruns: Xruns
    let windowMilliseconds: UInt64
    let streaming: Bool
    let sampleRateHz: Double
    let instrumentationAvailable: Bool
}

struct EffectivePolicy: Equatable, Sendable {
    let outputStartLatencyFrames: UInt64
    let outputRestartLatencyFrames: UInt64
    let outputTargetLatencyFrames: UInt64
    let workerQoS: String
    let inputLeadGuardEnabled: Bool
    let inputLeadCeilingFrames: UInt64
    let timecodeEvidenceRequired: Bool
}

struct PairWindow: Equatable, Sendable {
    let active: Bool
    let leftACRMS: Double
    let rightACRMS: Double
    let leftACPeak: Double
    let rightACPeak: Double
}

struct TimecodeSnapshot: Equatable, Sendable {
    let armed: Bool
    let armState: String
    let allowedInputPairs: [String]
    let electricalProfile: String
    let profileVerified: Bool
    let evidenceKind: String
    let windowFresh: Bool
    let qualified: Bool
    let optimizedActive: Bool
    let eligibleWindows: UInt64
    let requiredEligibleWindows: UInt64
    let dropoutWindows: UInt64
    let pairWindows: [String: Evidence<PairWindow>]
    let lastFailOpenReason: String
    let counters: [String: UInt64]
}

struct VintageSnapshot: Equatable, Sendable {
    let status: String
    let claim: String
    let experimental: Bool
    let reasons: [String]
    let capabilities: [String: Bool]
    let preflight: [String: String]
}

struct DriverModeSnapshot: Equatable, Sendable {
    let requestedMode: String
    let effectiveMode: String
    let pending: Bool
    let streaming: Bool
    let lastResult: String
    let rejectionReason: String
    let generation: UInt64
    let counters: [String: UInt64]
    let effectivePolicy: EffectivePolicy
    let timecode: Evidence<TimecodeSnapshot>
    let vintage: Evidence<VintageSnapshot>
}

struct LoopbackSnapshot: Equatable, Sendable {
    let enabled: Bool
    let sourcePair: String
    let sessionOnly: Bool
    let physicalPlaybackPublishing: Bool
    let ringCapacity: UInt64
    let generation: UInt64
    let registeredReaderCount: UInt64
    let sourceFramesPublished: UInt64
    let framesDelivered: UInt64
    let silenceFrames: UInt64
    let gapFrames: UInt64
    let overrunEvents: UInt64
    let overrunFrames: UInt64
}

struct ProfilerCheck: Identifiable, Equatable, Sendable {
    let id: String
    let status: String
    let code: String
    let summary: String
    let remediation: String?
    let evidence: [String: String]
}

struct ProfilerSnapshot: Equatable, Sendable {
    let overallStatus: String
    let summaryCodes: [String]
    let checks: [ProfilerCheck]
    let fingerprint: String
}

struct SourceCapture<Value: Sendable>: Sendable {
    let value: Evidence<Value>
    let capturedAt: ContinuousClock.Instant
}

enum RefreshPhase: Equatable, Sendable {
    case starting
    case online
    case partial
    case offline
    case permissionDenied
    case mismatch
    case stale(lastUpdatedDescription: String, reason: String)
}

struct DashboardSnapshot: Sendable {
    let backend: SourceCapture<BackendIdentity>
    let profiles: SourceCapture<[ProfileChoice]>
    let modeChoices: SourceCapture<[DriverModeChoice]>
    let profile: SourceCapture<ProfileSnapshot>
    let stream: SourceCapture<StreamSnapshot>
    let quality: SourceCapture<USBQualitySnapshot>
    let driverMode: SourceCapture<DriverModeSnapshot>
    let loopback: SourceCapture<LoopbackSnapshot>
    let profiler: SourceCapture<ProfilerSnapshot>
    let phase: RefreshPhase
    let sourceErrors: [String: DashboardError]
    let mismatchReasons: [String]
}

struct DashboardError: Error, Equatable, Sendable, CustomStringConvertible {
    let code: String
    let message: String
    let retryable: Bool
    let phase: String

    var description: String { "\(code): \(message)" }

    var evidenceReason: EvidenceReason {
        switch code {
        case "backend_unavailable": return .backendUnavailable
        case "backend_permission_denied": return .permissionDenied
        case "timeout": return .timedOut
        case "truncated": return .truncated
        default: return .protocolMismatch
        }
    }
}

enum MetricAge: Equatable, Sendable {
    case current
    case aging
    case stale
}

enum DeltaValue: Equatable, Sendable {
    case baseline
    case value(UInt64)
    case counterReset
    case backendMismatch
}

struct LoopbackDeltas: Equatable, Sendable {
    let gaps: DeltaValue
    let overrunEvents: DeltaValue
    let overrunFrames: DeltaValue
}

enum RollbackOutcome: Equatable, Sendable {
    case notNeeded
    case succeeded
    case failed
    case unavailable
}

enum ActionOutcome: Equatable, Sendable {
    case idle
    case applied(String)
    case pending(String)
    case rolledBack(DashboardError)
    case rollbackFailed(DashboardError)
    case rollbackUnavailable(DashboardError)
    case indeterminate(DashboardError)
}

enum DashboardSection: String, CaseIterable, Identifiable {
    case overview = "Overview"
    case usbQuality = "USB Quality"
    case driverModes = "Driver Modes"
    case loopback = "Loopback"
    case diagnostics = "Diagnostics"
    var id: String { rawValue }
}

enum CanonicalProfile: String, CaseIterable, Sendable {
    case playback = "playback-4out"
    case dvsVinyl = "traktor-dvs-vinyl"
    case dvsCDLine = "traktor-dvs-cd-line"
    case vinylRecording = "vinyl-recording"
    case djSetRecording = "dj-set-recording"
    case effectsLoop = "effects-loop"
    case microphone
    case midiOnly = "midi-only"
    case groundDiagnostics = "ground-diagnostics"
    case engineeringDiagnostics = "engineering-diagnostics"
}

enum DriverModeID: String, CaseIterable, Sendable {
    case balanced
    case performance
    case vintageCompatible = "vintage-compatible"
}

enum OutputPair: String, CaseIterable, Sendable {
    case A, B, C, D
}

enum ControlOperation: Equatable, Sendable {
    case version, profiles, profile, driverModes, driverMode, stats, loopbackGet
    case quality, profiler
    case setProfile(CanonicalProfile)
    case setDriverMode(DriverModeID)
    case armTimecode
    case disarmTimecode
    case enableLoopback(OutputPair)
    case disableLoopback

    var tool: BundledTool {
        self == .profiler ? .profiler : .control
    }

    var arguments: [String] {
        switch self {
        case .version: return ["api", "version"]
        case .profiles: return ["api", "profiles"]
        case .profile: return ["api", "profile"]
        case .driverModes: return ["api", "driver-modes"]
        case .driverMode: return ["api", "driver-mode"]
        case .stats: return ["api", "stats"]
        case .loopbackGet: return ["api", "loopback", "get"]
        case .quality: return ["usb-quality", "--json", "--interval-ms", "1000", "--count", "2"]
        case .profiler: return ["--json"]
        case .setProfile(let id): return ["api", "profile", "set", id.rawValue]
        case .setDriverMode(let id): return ["api", "driver-mode", "set", id.rawValue]
        case .armTimecode:
            return ["api", "driver-mode", "arm", "timecode-optimized", "--input-pairs", "A,B"]
        case .disarmTimecode:
            return ["api", "driver-mode", "disarm", "timecode-optimized"]
        case .enableLoopback(let pair): return ["api", "loopback", "enable", pair.rawValue]
        case .disableLoopback: return ["api", "loopback", "disable"]
        }
    }

    var expectedOperation: String? {
        switch self {
        case .version: return "version.get"
        case .profiles: return "profiles.list"
        case .profile: return "profile.get"
        case .driverModes: return "driver_modes.list"
        case .driverMode: return "driver_mode.get"
        case .stats: return "stats.get"
        case .loopbackGet: return "loopback.get"
        case .setProfile: return "profile.set"
        case .setDriverMode: return "driver_mode.set"
        case .armTimecode: return "driver_mode.arm"
        case .disarmTimecode: return "driver_mode.disarm"
        case .enableLoopback: return "loopback.enable"
        case .disableLoopback: return "loopback.disable"
        case .quality, .profiler: return nil
        }
    }

    var timeout: TimeInterval {
        switch self {
        case .quality: return 7
        case .profiler: return 8
        default: return 3
        }
    }

    var isRead: Bool {
        switch self {
        case .version, .profiles, .profile, .driverModes, .driverMode, .stats,
             .loopbackGet, .quality, .profiler: return true
        default: return false
        }
    }
}

enum BundledTool: String, Sendable {
    case control = "opena8dj-control"
    case profiler = "opena8dj-hardware-profiler"
}

struct ProcessOutput: Equatable, Sendable {
    let status: Int32
    let stdout: Data
    let stderr: Data
}

struct PendingConfirmation: Identifiable, Equatable {
    let id = UUID()
    let operation: ControlOperation
    let title: String
    let message: String
}
