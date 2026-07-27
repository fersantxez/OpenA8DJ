import Foundation

enum DecodeFailure: Error, Equatable, CustomStringConvertible {
    case protocolViolation(String)
    case api(DashboardError)

    var description: String {
        switch self {
        case .protocolViolation(let detail): return detail
        case .api(let error): return error.description
        }
    }
}

typealias JSONObject = [String: Any]

enum StrictJSON {
    static func object(from data: Data) throws -> JSONObject {
        guard data.last == 0x0a else {
            throw DecodeFailure.protocolViolation("missing final newline")
        }
        guard String(data: data, encoding: .utf8) != nil else {
            throw DecodeFailure.protocolViolation("invalid UTF-8")
        }
        let body = data.dropLast()
        guard !body.contains(0x0a), !body.contains(0x0d) else {
            throw DecodeFailure.protocolViolation("multiple JSON values")
        }
        guard !JSONDuplicateKeyDetector.hasDuplicateKey(in: Data(body)) else {
            throw DecodeFailure.protocolViolation("duplicate JSON key")
        }
        let value = try JSONSerialization.jsonObject(with: body, options: [])
        guard let object = value as? JSONObject else {
            throw DecodeFailure.protocolViolation("JSON root is not an object")
        }
        return object
    }

    static func ndjsonPair(from data: Data) throws -> [JSONObject] {
        guard data.last == 0x0a else {
            throw DecodeFailure.protocolViolation("missing final newline")
        }
        guard let text = String(data: data, encoding: .utf8) else {
            throw DecodeFailure.protocolViolation("invalid UTF-8")
        }
        let lines = text.split(separator: "\n", omittingEmptySubsequences: false)
        guard lines.count == 3, lines[2].isEmpty else {
            throw DecodeFailure.protocolViolation("quality output must contain exactly two objects")
        }
        return try lines.prefix(2).map { line in
            try object(from: Data((String(line) + "\n").utf8))
        }
    }
}

private enum JSONDuplicateKeyDetector {
    static func hasDuplicateKey(in data: Data) -> Bool {
        var parser = Parser(bytes: Array(data))
        return parser.parseValue().duplicate || !parser.atEnd
    }

    private struct Result { let duplicate: Bool }
    private struct Parser {
        let bytes: [UInt8]
        var index = 0
        var atEnd: Bool { skipIndex(index) == bytes.count }

        mutating func parseValue() -> Result {
            skip()
            guard index < bytes.count else { return Result(duplicate: false) }
            switch bytes[index] {
            case 0x7b: return parseObject()
            case 0x5b: return parseArray()
            case 0x22:
                _ = parseString()
                return Result(duplicate: false)
            default:
                while index < bytes.count && ![0x2c, 0x5d, 0x7d].contains(bytes[index]) { index += 1 }
                return Result(duplicate: false)
            }
        }

        mutating func parseObject() -> Result {
            index += 1
            var keys = Set<String>()
            var duplicate = false
            skip()
            if consume(0x7d) { return Result(duplicate: false) }
            while index < bytes.count {
                skip()
                guard let key = parseString() else { return Result(duplicate: duplicate) }
                if !keys.insert(key).inserted { duplicate = true }
                skip()
                guard consume(0x3a) else { return Result(duplicate: duplicate) }
                duplicate = parseValue().duplicate || duplicate
                skip()
                if consume(0x7d) { break }
                guard consume(0x2c) else { break }
            }
            return Result(duplicate: duplicate)
        }

        mutating func parseArray() -> Result {
            index += 1
            var duplicate = false
            skip()
            if consume(0x5d) { return Result(duplicate: false) }
            while index < bytes.count {
                duplicate = parseValue().duplicate || duplicate
                skip()
                if consume(0x5d) { break }
                guard consume(0x2c) else { break }
            }
            return Result(duplicate: duplicate)
        }

        mutating func parseString() -> String? {
            guard consume(0x22) else { return nil }
            var result = ""
            while index < bytes.count {
                let byte = bytes[index]
                index += 1
                if byte == 0x22 { return result }
                if byte == 0x5c {
                    guard index < bytes.count else { return nil }
                    result.append("\\")
                    result.append(Character(UnicodeScalar(bytes[index])))
                    index += 1
                } else {
                    result.append(Character(UnicodeScalar(byte)))
                }
            }
            return nil
        }

        mutating func skip() {
            while index < bytes.count && [0x20, 0x09, 0x0a, 0x0d].contains(bytes[index]) { index += 1 }
        }
        func skipIndex(_ start: Int) -> Int {
            var cursor = start
            while cursor < bytes.count && [0x20, 0x09, 0x0a, 0x0d].contains(bytes[cursor]) { cursor += 1 }
            return cursor
        }
        mutating func consume(_ byte: UInt8) -> Bool {
            guard index < bytes.count, bytes[index] == byte else { return false }
            index += 1
            return true
        }
    }
}

struct APIEnvelope {
    let version: SemanticVersion
    let operation: String
    let data: [String: Any]
    let newerMinor: Bool
}

enum DashboardDecoder {
    static let apiSchema = "org.opena8dj.public-api.response.v1"
    static let profilerSchema = "org.opena8dj.hardware-profiler.report.v1"
    static let testedMinor = 1

    static func envelope(_ output: ProcessOutput, expected: String) throws -> APIEnvelope {
        let object = try StrictJSON.object(from: output.stdout)
        guard string(object, "schema") == apiSchema,
              let version = string(object, "apiVersion").flatMap(parseVersion),
              version.major == 1,
              string(object, "operation") == expected,
              let ok = bool(object, "ok") else {
            throw DecodeFailure.protocolViolation("API schema, major, operation, or ok mismatch")
        }
        let hasData = object["data"] is JSONObject
        let hasError = object["error"] is JSONObject
        guard hasData != hasError, ok == hasData, (output.status == 0) == ok else {
            throw DecodeFailure.protocolViolation("exit/envelope invariant mismatch")
        }
        if !ok {
            let error = try dictionary(object, "error")
            throw DecodeFailure.api(DashboardError(
                code: try requiredString(error, "code"),
                message: redactedMessage(try requiredString(error, "message")),
                retryable: try requiredBool(error, "retryable"),
                phase: expected
            ))
        }
        return APIEnvelope(
            version: version,
            operation: expected,
            data: try dictionary(object, "data"),
            newerMinor: version.minor > testedMinor
        )
    }

    static func version(_ output: ProcessOutput) throws -> BackendIdentity {
        let envelope = try envelope(output, expected: "version.get")
        let data = envelope.data
        guard try requiredString(data, "schema") == apiSchema,
              try requiredString(data, "transport") == "process-json",
              let nested = parseVersion(try requiredString(data, "apiVersion")),
              nested == envelope.version else {
            throw DecodeFailure.protocolViolation("version response disagreement")
        }
        let capabilities = Set(try stringArray(data, "capabilities"))
        return BackendIdentity(
            apiVersion: nested,
            capabilities: capabilities,
            newerMinor: envelope.newerMinor,
            fingerprint: "\(nested.description):\(capabilities.sorted().joined(separator: ","))"
        )
    }

    static func profiles(_ output: ProcessOutput) throws -> [ProfileChoice] {
        let data = try envelope(output, expected: "profiles.list").data
        let rows = try objectArray(data, "profiles")
        let canonical = Set(CanonicalProfile.allCases.map(\.rawValue))
        return try rows.map {
            let id = try requiredID($0, "id")
            return ProfileChoice(
                id: id,
                title: try requiredString($0, "title"),
                surface: try requiredString($0, "surface"),
                summary: try requiredString($0, "summary"),
                canonical: canonical.contains(id)
            )
        }
    }

    static func modeChoices(_ output: ProcessOutput) throws -> [DriverModeChoice] {
        let data = try envelope(output, expected: "driver_modes.list").data
        guard try requiredUInt(data, "schemaVersion") == 1 else {
            throw DecodeFailure.protocolViolation("driver mode schema mismatch")
        }
        return try objectArray(data, "driverModes").map {
            DriverModeChoice(
                id: try requiredID($0, "id"),
                title: try requiredString($0, "name"),
                summary: string($0, "summary") ?? ""
            )
        }
    }

    static func profile(_ output: ProcessOutput, expectedOperation: String = "profile.get") throws -> ProfileSnapshot {
        try decodeProfile(envelope(output, expected: expectedOperation).data)
    }

    static func decodeProfile(_ data: JSONObject) throws -> ProfileSnapshot {
        let mode = try requiredString(data, "inputMode")
        guard ["playback", "vinyl", "cd-line", "microphone", "custom"].contains(mode) else {
            throw DecodeFailure.protocolViolation("unknown input mode")
        }
        return ProfileSnapshot(
            activeProfile: try requiredID(data, "activeProfile", allowCustom: true),
            inputMode: mode,
            inputModeValue: try requiredUInt(data, "inputModeValue"),
            inputDecode: try requiredBool(data, "inputDecode"),
            softwareLock: try requiredBool(data, "softwareLock"),
            groundLiftVinyl: try requiredBool(data, "groundLiftVinyl"),
            groundLiftCDLine: try requiredBool(data, "groundLiftCDLine"),
            groundLiftPhono: try requiredBool(data, "groundLiftPhono"),
            inputSources: try stringMap(data, "inputSources", keys: ["A", "B", "C", "D"]),
            inputTransforms: try stringMap(data, "inputTransforms", keys: ["A", "B", "C", "D"])
        )
    }

    static func stream(_ output: ProcessOutput) throws -> StreamSnapshot {
        let data = try envelope(output, expected: "stats.get").data
        let stream = try dictionary(data, "stream")
        let rate = try requiredDouble(stream, "sampleRate")
        guard rate >= 0, rate.isFinite else {
            throw DecodeFailure.protocolViolation("invalid sample rate")
        }
        let generation = (data["driverMode"] as? JSONObject).flatMap { uint($0, "generation") }
        return StreamSnapshot(
            streaming: try requiredBool(stream, "streaming"),
            sampleRateHz: rate,
            outputRingFrames: try requiredUInt(stream, "outputRingFrames"),
            outputTargetLatencyFrames: try requiredUInt(stream, "outputTargetLatencyFrames"),
            generation: generation
        )
    }

    static func driverMode(_ output: ProcessOutput, expectedOperation: String = "driver_mode.get") throws -> DriverModeSnapshot {
        try decodeDriverMode(envelope(output, expected: expectedOperation).data)
    }

    static func decodeDriverMode(_ data: JSONObject) throws -> DriverModeSnapshot {
        guard try requiredUInt(data, "schemaVersion") == 1 else {
            throw DecodeFailure.protocolViolation("driver mode schema mismatch")
        }
        let requested = try requiredEnum(data, "requestedMode", ["balanced", "performance", "vintage-compatible"])
        let effective = try requiredEnum(data, "effectiveMode", ["balanced", "performance", "vintage-compatible"])
        let pending = try requiredBool(data, "pending")
        guard pending == (requested != effective) else {
            throw DecodeFailure.protocolViolation("pending/requested/effective invariant mismatch")
        }
        let policy = try dictionary(data, "effectivePolicy")
        let timecode: Evidence<TimecodeSnapshot>
        if data["timecodeOptimized"] is NSNull || data["timecodeOptimized"] == nil {
            timecode = .unavailable(reason: .unsupportedTail)
        } else {
            timecode = .known(try decodeTimecode(try dictionary(data, "timecodeOptimized")))
        }
        let vintage: Evidence<VintageSnapshot>
        if data["vintageCompatible"] is NSNull || data["vintageCompatible"] == nil {
            vintage = .unavailable(reason: .unsupportedTail)
        } else {
            vintage = .known(try decodeVintage(try dictionary(data, "vintageCompatible")))
        }
        return DriverModeSnapshot(
            requestedMode: requested,
            effectiveMode: effective,
            pending: pending,
            streaming: try requiredBool(data, "streaming"),
            lastResult: try requiredString(data, "lastResult"),
            rejectionReason: try requiredString(data, "rejectionReason"),
            generation: try requiredUInt(data, "generation"),
            counters: try uintMap(try dictionary(data, "counters")),
            effectivePolicy: EffectivePolicy(
                outputStartLatencyFrames: try requiredUInt(policy, "outputStartLatencyFrames"),
                outputRestartLatencyFrames: try requiredUInt(policy, "outputRestartLatencyFrames"),
                outputTargetLatencyFrames: try requiredUInt(policy, "outputTargetLatencyFrames"),
                workerQoS: try requiredString(policy, "workerQoS"),
                inputLeadGuardEnabled: try requiredBool(policy, "inputLeadGuardEnabled"),
                inputLeadCeilingFrames: try requiredUInt(policy, "inputLeadCeilingFrames"),
                timecodeEvidenceRequired: try requiredBool(policy, "timecodeEvidenceRequired")
            ),
            timecode: timecode,
            vintage: vintage
        )
    }

    static func loopback(_ output: ProcessOutput, expectedOperation: String = "loopback.get") throws -> LoopbackSnapshot {
        try decodeLoopback(envelope(output, expected: expectedOperation).data)
    }

    static func decodeLoopback(_ data: JSONObject) throws -> LoopbackSnapshot {
        let source = try requiredEnum(data, "sourcePair", ["A", "B", "C", "D"])
        let enabled = try requiredBool(data, "enabled")
        let publishing = try requiredBool(data, "physicalPlaybackPublishing")
        guard try requiredBool(data, "sessionOnly"), !publishing || enabled else {
            throw DecodeFailure.protocolViolation("loopback privacy/state invariant mismatch")
        }
        return LoopbackSnapshot(
            enabled: enabled,
            sourcePair: source,
            sessionOnly: true,
            physicalPlaybackPublishing: publishing,
            ringCapacity: try requiredUInt(data, "ringCapacity"),
            generation: try requiredUInt(data, "generation"),
            registeredReaderCount: try requiredUInt(data, "registeredReaderCount"),
            sourceFramesPublished: try requiredUInt(data, "sourceFramesPublished"),
            framesDelivered: try requiredUInt(data, "framesDelivered"),
            silenceFrames: try requiredUInt(data, "silenceFrames"),
            gapFrames: try requiredUInt(data, "gapFrames"),
            overrunEvents: try requiredUInt(data, "overrunEvents"),
            overrunFrames: try requiredUInt(data, "overrunFrames")
        )
    }

    static func quality(_ output: ProcessOutput) throws -> USBQualitySnapshot {
        let objects = try StrictJSON.ndjsonPair(from: output.stdout)
        let second = objects[1]
        let stability = try dictionary(second, "stability")
        let classification = try requiredEnum(
            stability, "classification",
            ["stable", "degraded", "unstable", "not-streaming", "insufficient-data", "warming-up"]
        )
        let jitter = try dictionary(second, "jitter")
        let iso = try dictionary(second, "isochronousErrors")
        let xruns = try dictionary(second, "xruns")
        let captureISO = try dictionary(iso, "capture")
        let playbackISO = try dictionary(iso, "playback")
        let captureJitter = try decodeJitter(try dictionary(jitter, "capture"))
        let playbackJitter = try decodeJitter(try dictionary(jitter, "playback"))
        let instrumentation = try requiredBool(second, "instrumentationAvailable")
        if classification == "stable" &&
            (!instrumentation ||
             (captureJitter.active && captureJitter.samples < 20) ||
             (playbackJitter.active && playbackJitter.samples < 20)) {
            throw DecodeFailure.protocolViolation("stable classification lacks sufficient instrumentation")
        }
        var components = try uintMap(captureISO).mapKeys { "capture.\($0)" }
        components.merge(try uintMap(playbackISO).mapKeys { "playback.\($0)" }) { a, _ in a }
        return USBQualitySnapshot(
            classification: classification,
            reasons: try stringArray(stability, "reasons"),
            captureJitter: captureJitter,
            playbackJitter: playbackJitter,
            isoErrors: ISOErrors(
                captureTotal: try requiredUInt(captureISO, "totalEvents"),
                playbackTotal: try requiredUInt(playbackISO, "totalEvents"),
                components: components
            ),
            xruns: Xruns(
                totalHardXruns: try requiredUInt(xruns, "totalHardXruns"),
                activeUnderruns: try requiredUInt(xruns, "activeOutputUnderruns"),
                ringOverruns: try requiredUInt(xruns, "outputRingOverruns"),
                lateWriteBatches: try requiredUInt(xruns, "outputLateWriteBatches"),
                lateWriteFrames: try requiredUInt(xruns, "outputLateWriteFrames")
            ),
            windowMilliseconds: try requiredUInt(second, "windowMilliseconds"),
            streaming: try requiredBool(second, "streaming"),
            sampleRateHz: try requiredDouble(second, "sampleRateHz"),
            instrumentationAvailable: instrumentation
        )
    }

    static func profiler(_ output: ProcessOutput) throws -> ProfilerSnapshot {
        let root = try StrictJSON.object(from: output.stdout)
        guard string(root, "schema") == profilerSchema,
              try requiredUInt(root, "schemaVersion") == 1 else {
            throw DecodeFailure.protocolViolation("profiler schema/version mismatch")
        }
        let overall = try dictionary(root, "overall")
        let status = try requiredEnum(overall, "status", ["PASS", "WARN", "FAIL", "UNKNOWN"])
        let checks = try objectArray(root, "checks").map { check -> ProfilerCheck in
            let checkStatus = try requiredEnum(check, "status", ["PASS", "WARN", "FAIL", "UNKNOWN"])
            return ProfilerCheck(
                id: try requiredString(check, "id"),
                status: checkStatus,
                code: try requiredString(check, "code"),
                summary: try requiredString(check, "summary"),
                remediation: string(check, "remediation"),
                evidence: stringMapLoose(check["evidence"])
            )
        }
        let required = Set([
            "usb.identity", "usb.enumeration", "usb.link-speed", "usb.power",
            "device.firmware", "coreaudio.device", "driver.api-pairing", "usb.stream-quality"
        ])
        guard required.isSubset(of: Set(checks.map(\.id))) else {
            throw DecodeFailure.protocolViolation("profiler required checks missing")
        }
        let codes = try stringArray(overall, "summaryCodes")
        return ProfilerSnapshot(
            overallStatus: status,
            summaryCodes: codes,
            checks: checks,
            fingerprint: "\(status):\(checks.map { "\($0.id)=\($0.code)" }.sorted().joined(separator: ","))"
        )
    }

    private static func decodeJitter(_ data: JSONObject) throws -> DirectionJitter {
        DirectionJitter(
            p95: try percentile(data, "p95"),
            p99: try percentile(data, "p99"),
            samples: try requiredUInt(data, "samples"),
            active: try requiredBool(data, "active")
        )
    }

    private static func percentile(_ object: JSONObject, _ key: String) throws -> PercentileBound {
        if object[key] is NSNull {
            return PercentileBound(upperBoundUs: nil, overflow: false)
        }
        let value = try dictionary(object, key)
        let overflow = try requiredBool(value, "overflow")
        let bound = uint(value, "upperBoundUs")
        guard overflow != (bound != nil) else {
            throw DecodeFailure.protocolViolation("percentile bound invariant mismatch")
        }
        return PercentileBound(upperBoundUs: bound, overflow: overflow)
    }

    private static func decodeTimecode(_ data: JSONObject) throws -> TimecodeSnapshot {
        let pairObject = try dictionary(data, "pairWindows")
        var pairs: [String: Evidence<PairWindow>] = [:]
        for pair in ["A", "B", "C", "D"] {
            if pairObject[pair] is NSNull {
                pairs[pair] = .unavailable(reason: .insufficientData)
            } else {
                let value = try dictionary(pairObject, pair)
                pairs[pair] = .known(PairWindow(
                    active: try requiredBool(value, "active"),
                    leftACRMS: try requiredDouble(value, "leftACRMS"),
                    rightACRMS: try requiredDouble(value, "rightACRMS"),
                    leftACPeak: try requiredDouble(value, "leftACPeak"),
                    rightACPeak: try requiredDouble(value, "rightACPeak")
                ))
            }
        }
        let armed = try requiredBool(data, "armed")
        let optimized = try requiredBool(data, "optimizedActive")
        guard !optimized || armed else {
            throw DecodeFailure.protocolViolation("optimized mode active while disarmed")
        }
        return TimecodeSnapshot(
            armed: armed,
            armState: try requiredString(data, "armState"),
            allowedInputPairs: try stringArray(data, "allowedInputPairs"),
            electricalProfile: try requiredString(data, "electricalProfile"),
            profileVerified: try requiredBool(data, "profileVerified"),
            evidenceKind: try requiredString(data, "evidenceKind"),
            windowFresh: try requiredBool(data, "windowFresh"),
            qualified: try requiredBool(data, "qualified"),
            optimizedActive: optimized,
            eligibleWindows: try requiredUInt(data, "eligibleWindows"),
            requiredEligibleWindows: try requiredUInt(data, "requiredEligibleWindows"),
            dropoutWindows: try requiredUInt(data, "dropoutWindows"),
            pairWindows: pairs,
            lastFailOpenReason: try requiredString(data, "lastFailOpenReason"),
            counters: (try? uintMap(try dictionary(data, "counters"))) ?? [:]
        )
    }

    private static func decodeVintage(_ data: JSONObject) throws -> VintageSnapshot {
        let status = try requiredEnum(data, "status", ["unverified", "partial", "compatible"])
        guard try requiredBool(data, "experimental"),
              try requiredString(data, "claim") == "unverified" else {
            throw DecodeFailure.protocolViolation("Vintage experimental claim mismatch")
        }
        return VintageSnapshot(
            status: status,
            claim: "unverified",
            experimental: true,
            reasons: try stringArray(data, "reasons"),
            capabilities: try boolMap(try dictionary(data, "capabilities")),
            preflight: stringifyMap(try dictionary(data, "preflight"))
        )
    }

    private static func redactedMessage(_ message: String) -> String {
        String(message.unicodeScalars.filter {
            !CharacterSet.controlCharacters.contains($0)
        }.prefix(240))
    }

    private static func parseVersion(_ value: String) -> SemanticVersion? {
        let fields = value.split(separator: ".", omittingEmptySubsequences: false)
        guard fields.count == 2 || fields.count == 3,
              let major = Int(fields[0]), let minor = Int(fields[1]),
              major >= 0, minor >= 0 else { return nil }
        let patch = fields.count == 3 ? Int(fields[2]) : 0
        guard let patch, patch >= 0 else { return nil }
        return SemanticVersion(major: major, minor: minor, patch: patch)
    }

    private static func string(_ object: JSONObject, _ key: String) -> String? {
        object[key] as? String
    }
    private static func bool(_ object: JSONObject, _ key: String) -> Bool? {
        guard let number = object[key] as? NSNumber,
              CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
        return number.boolValue
    }
    private static func uint(_ object: JSONObject, _ key: String) -> UInt64? {
        guard let number = object[key] as? NSNumber,
              CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue >= 0,
              number.doubleValue.rounded() == number.doubleValue else { return nil }
        return number.uint64Value
    }
    private static func requiredString(_ object: JSONObject, _ key: String) throws -> String {
        guard let value = string(object, key), !value.isEmpty else {
            throw DecodeFailure.protocolViolation("missing/invalid string \(key)")
        }
        return value
    }
    private static func requiredBool(_ object: JSONObject, _ key: String) throws -> Bool {
        guard let value = bool(object, key) else {
            throw DecodeFailure.protocolViolation("missing/invalid boolean \(key)")
        }
        return value
    }
    private static func requiredUInt(_ object: JSONObject, _ key: String) throws -> UInt64 {
        guard let value = uint(object, key) else {
            throw DecodeFailure.protocolViolation("missing/invalid integer \(key)")
        }
        return value
    }
    private static func requiredDouble(_ object: JSONObject, _ key: String) throws -> Double {
        guard let number = object[key] as? NSNumber,
              CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.isFinite else {
            throw DecodeFailure.protocolViolation("missing/invalid number \(key)")
        }
        return number.doubleValue
    }
    private static func requiredEnum(_ object: JSONObject, _ key: String, _ allowed: Set<String>) throws -> String {
        let value = try requiredString(object, key)
        guard allowed.contains(value) else {
            throw DecodeFailure.protocolViolation("unknown enum \(key)=\(value)")
        }
        return value
    }
    private static func requiredID(_ object: JSONObject, _ key: String, allowCustom: Bool = false) throws -> String {
        let value = try requiredString(object, key)
        if allowCustom && value == "custom" { return value }
        let allowed = CharacterSet(charactersIn: "abcdefghijklmnopqrstuvwxyz0123456789-")
        guard value.count <= 64, value.unicodeScalars.allSatisfy(allowed.contains) else {
            throw DecodeFailure.protocolViolation("invalid identifier \(key)")
        }
        return value
    }
    private static func dictionary(_ object: JSONObject, _ key: String) throws -> JSONObject {
        guard let value = object[key] as? JSONObject else {
            throw DecodeFailure.protocolViolation("missing/invalid object \(key)")
        }
        return value
    }
    private static func stringArray(_ object: JSONObject, _ key: String) throws -> [String] {
        guard let values = object[key] as? [Any],
              values.allSatisfy({ $0 is String }) else {
            throw DecodeFailure.protocolViolation("missing/invalid string array \(key)")
        }
        return values.compactMap { $0 as? String }
    }
    private static func objectArray(_ object: JSONObject, _ key: String) throws -> [JSONObject] {
        guard let values = object[key] as? [Any],
              values.allSatisfy({ $0 is JSONObject }) else {
            throw DecodeFailure.protocolViolation("missing/invalid object array \(key)")
        }
        return values.compactMap { $0 as? JSONObject }
    }
    private static func stringMap(_ object: JSONObject, _ key: String, keys: Set<String>) throws -> [String: String] {
        let value = try dictionary(object, key)
        guard Set(value.keys) == keys, value.values.allSatisfy({ $0 is String }) else {
            throw DecodeFailure.protocolViolation("invalid map \(key)")
        }
        return value.mapValues { $0 as! String }
    }
    private static func uintMap(_ object: JSONObject) throws -> [String: UInt64] {
        var result: [String: UInt64] = [:]
        for key in object.keys {
            guard let value = uint(object, key) else {
                throw DecodeFailure.protocolViolation("invalid counter \(key)")
            }
            result[key] = value
        }
        return result
    }
    private static func boolMap(_ object: JSONObject) throws -> [String: Bool] {
        var result: [String: Bool] = [:]
        for key in object.keys {
            guard let value = bool(object, key) else {
                throw DecodeFailure.protocolViolation("invalid capability \(key)")
            }
            result[key] = value
        }
        return result
    }
    private static func stringifyMap(_ object: JSONObject) -> [String: String] {
        object.mapValues {
            if $0 is NSNull { return "UNKNOWN" }
            return String(describing: $0)
        }
    }
    private static func stringMapLoose(_ value: Any?) -> [String: String] {
        guard let object = value as? JSONObject else { return [:] }
        return stringifyMap(object)
    }
}

private extension Dictionary {
    func mapKeys<T: Hashable>(_ transform: (Key) -> T) -> [T: Value] {
        Dictionary<T, Value>(uniqueKeysWithValues: map { (transform($0.key), $0.value) })
    }
}
