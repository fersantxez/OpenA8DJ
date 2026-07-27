import Foundation
import CoreAudio
import IOKit
import Darwin

private let toolVersion = openA8DJHardwareProfilerVersion
private let reportSchema = "org.opena8dj.hardware-profiler.report.v1"
private let apiSchema = "org.opena8dj.public-api.response.v1"
private let catalogSchema = "org.opena8dj.hardware-known-issues.v1"
private let expectedVID = 0x17cc
private let expectedPID = 0x1978
private let expectedUID = "org.opena8dj.Audio8DJ"
private let unavailable = NSNull()

private enum Status: String {
    case pass = "PASS", warn = "WARN", fail = "FAIL", unknown = "UNKNOWN"
}

private struct Options {
    var json = false
    var catalog: String?
#if OPENA8DJ_HARDWARE_PROFILER_TESTING
    var fixture: String?
#endif
}

private func parseTestOption(_ argument: String, _ index: inout Int,
                             _ options: inout Options) -> Bool {
#if OPENA8DJ_HARDWARE_PROFILER_TESTING
    if argument == "--fixture" && options.fixture == nil &&
       index + 1 < CommandLine.arguments.count {
        index += 1
        options.fixture = CommandLine.arguments[index]
        return true
    }
#endif
    return false
}

private func parseOptions() throws -> Options {
    var options = Options()
    var index = 1
    while index < CommandLine.arguments.count {
        let argument = CommandLine.arguments[index]
        if argument == "--json" && !options.json {
            options.json = true
        } else if argument == "--catalog" && options.catalog == nil &&
                    index + 1 < CommandLine.arguments.count {
            index += 1
            options.catalog = CommandLine.arguments[index]
        } else if parseTestOption(argument, &index, &options) {
        } else if argument == "--help" || argument == "-h" {
            print("Usage: opena8dj-hardware-profiler [--json] [--catalog PATH]")
            exit(0)
        } else {
            throw NSError(domain: "arguments", code: 64,
                          userInfo: [NSLocalizedDescriptionKey: "invalid argument"])
        }
        index += 1
    }
    return options
}

private func dictionary(_ value: Any?) -> [String: Any]? {
    value as? [String: Any]
}

private func array(_ value: Any?) -> [[String: Any]] {
    value as? [[String: Any]] ?? []
}

private func bool(_ object: [String: Any], _ key: String) -> Bool? {
    guard let value = object[key] else { return nil }
    if value is NSNull { return nil }
    return value as? Bool
}

private func integer(_ object: [String: Any], _ key: String) -> Int64? {
    guard let value = object[key], !(value is NSNull) else { return nil }
    if let number = value as? NSNumber {
        if CFGetTypeID(number) == CFBooleanGetTypeID() { return nil }
        return number.int64Value
    }
    return nil
}

private func string(_ object: [String: Any], _ key: String) -> String? {
    object[key] as? String
}

private func evidence(_ source: String, _ key: String, _ value: Any?,
                      unit: String? = nil, confidence: String = "direct",
                      reason: String? = nil) -> [String: Any] {
    var result: [String: Any] = [
        "source": source, "key": key, "available": value != nil,
        "value": value ?? unavailable,
        "confidence": value == nil ? "unavailable" : confidence
    ]
    if let unit { result["unit"] = unit }
    if value == nil { result["reasonCode"] = reason ?? "not_available" }
    return result
}

private func check(_ id: String, _ status: Status, _ code: String, _ summary: String,
                   _ evidence: [[String: Any]] = [],
                   _ remediation: [String] = []) -> [String: Any] {
    [
        "id": id, "status": status.rawValue, "code": code, "summary": summary,
        "evidence": evidence, "remediation": remediation
    ]
}

private func readJSON(path: String, limit: Int = 2 * 1024 * 1024) throws -> Any {
    let url = URL(fileURLWithPath: path)
    let values = try url.resourceValues(forKeys: [.fileSizeKey])
    guard isRegularFile(path), (values.fileSize ?? limit + 1) <= limit else {
        throw NSError(domain: "input", code: 1,
                      userInfo: [NSLocalizedDescriptionKey: "input is not a bounded regular file"])
    }
    return try JSONSerialization.jsonObject(with: Data(contentsOf: url))
}

private func isRegularFile(_ path: String) -> Bool {
    var status = stat()
    return lstat(path, &status) == 0 && (status.st_mode & S_IFMT) == S_IFREG
}

// Small self-contained SHA-256 implementation avoids a network/package dependency.
private func sha256(_ data: Data) -> String {
    let k: [UInt32] = [
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    ]
    var bytes = [UInt8](data)
    let bitLength = UInt64(bytes.count) * 8
    bytes.append(0x80)
    while bytes.count % 64 != 56 { bytes.append(0) }
    bytes.append(contentsOf: (0..<8).reversed().map { UInt8((bitLength >> UInt64($0 * 8)) & 0xff) })
    var h: [UInt32] = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                       0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]
    @inline(__always) func rotate(_ x: UInt32, _ n: UInt32) -> UInt32 {
        (x >> n) | (x << (32 - n))
    }
    for offset in stride(from: 0, to: bytes.count, by: 64) {
        var w = [UInt32](repeating: 0, count: 64)
        for i in 0..<16 {
            let j = offset + i * 4
            w[i] = UInt32(bytes[j]) << 24 | UInt32(bytes[j+1]) << 16 |
                   UInt32(bytes[j+2]) << 8 | UInt32(bytes[j+3])
        }
        for i in 16..<64 {
            let s0 = rotate(w[i-15], 7) ^ rotate(w[i-15], 18) ^ (w[i-15] >> 3)
            let s1 = rotate(w[i-2], 17) ^ rotate(w[i-2], 19) ^ (w[i-2] >> 10)
            w[i] = w[i-16] &+ s0 &+ w[i-7] &+ s1
        }
        var a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7]
        for i in 0..<64 {
            let s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)
            let ch = (e & f) ^ ((~e) & g)
            let t1 = hh &+ s1 &+ ch &+ k[i] &+ w[i]
            let s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)
            let maj = (a & b) ^ (a & c) ^ (b & c)
            let t2 = s0 &+ maj
            hh=g; g=f; f=e; e=d &+ t1; d=c; c=b; b=a; a=t1 &+ t2
        }
        h[0] &+= a; h[1] &+= b; h[2] &+= c; h[3] &+= d
        h[4] &+= e; h[5] &+= f; h[6] &+= g; h[7] &+= hh
    }
    return h.map { String(format: "%08x", $0) }.joined()
}

private func numberProperty(_ properties: [String: Any], _ names: [String]) -> Int64? {
    for name in names {
        if let number = properties[name] as? NSNumber,
           CFGetTypeID(number) != CFBooleanGetTypeID() {
            return number.int64Value
        }
    }
    return nil
}

private func booleanProperty(_ properties: [String: Any], _ name: String) -> Bool? {
    guard let number = properties[name] as? NSNumber,
          CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
    return number.boolValue
}

private func connectionSpeedBitsPerSecond(_ value: Int64) -> Int64? {
    switch value {
    case 1: return 12_000_000
    case 2: return 1_500_000
    case 3: return 480_000_000
    case 4: return 5_000_000_000
    case 5: return 10_000_000_000
    case 6: return 20_000_000_000
    default: return nil
    }
}

private func normalizedUSBDevice(_ properties: [String: Any]) -> [String: Any]? {
    guard let vendor = numberProperty(properties, ["idVendor"]),
          let product = numberProperty(properties, ["idProduct"]),
          vendor == Int64(expectedVID), product == Int64(expectedPID) else { return nil }
    let deviceRelease = numberProperty(properties, ["bcdDevice"])
    let currentConfiguration = numberProperty(properties, ["kUSBCurrentConfiguration"])
    var device: [String: Any] = [
        "vendorId": vendor, "productId": product,
        "descriptorReadable": deviceRelease != nil,
        "currentConfiguration": currentConfiguration ?? unavailable,
        "usable": currentConfiguration.map { $0 > 0 } ?? unavailable,
        "bcdDevice": deviceRelease ?? unavailable
    ]
    if let bitrate = numberProperty(properties, ["UsbLinkSpeed"]), bitrate > 0 {
        device["linkSpeedBitsPerSecond"] = bitrate
        device["linkSpeedSource"] = "UsbLinkSpeed"
    } else if let speed = numberProperty(properties, ["USBSpeed"]),
              let bitrate = connectionSpeedBitsPerSecond(speed) {
        device["linkSpeedBitsPerSecond"] = bitrate
        device["linkSpeedSource"] = "USBSpeed-enum"
    } else {
        device["linkSpeedBitsPerSecond"] = unavailable
        device["linkSpeedSource"] = unavailable
    }

    // Only this explicit device-scoped mA pair is treated as comparable.
    if let available = numberProperty(properties, ["USB Current Available"]),
       let required = numberProperty(properties, ["USB Current Required"]),
       available >= 0, required >= 0 {
        device["availablePowerMilliAmps"] = available
        device["requiredPowerMilliAmps"] = required
        device["powerEvidenceScope"] = "device-current-pair"
        device["powerEvidenceUnit"] = "mA"
    } else {
        device["availablePowerMilliAmps"] = unavailable
        device["requiredPowerMilliAmps"] = unavailable
        device["powerEvidenceScope"] = unavailable
        device["powerEvidenceUnit"] = unavailable
    }
    device["failedRequestedPower"] =
        booleanProperty(properties, "kUSBFailedRequestedPower") ?? unavailable
    return device
}

private func liveUSB() -> (Bool, [[String: Any]], [[String: Any]]) {
    guard let matching = IOServiceMatching("IOUSBHostDevice") else {
        return (false, [], [["source": "ioregistry.usb", "reasonCode": "matching_unavailable",
                            "message": "USB enumeration unavailable."]])
    }
    var iterator: io_iterator_t = 0
    guard IOServiceGetMatchingServices(0, matching, &iterator) == KERN_SUCCESS else {
        return (false, [], [["source": "ioregistry.usb", "reasonCode": "query_failed",
                            "message": "USB enumeration failed."]])
    }
    defer { IOObjectRelease(iterator) }
    var devices: [[String: Any]] = []
    while true {
        let service = IOIteratorNext(iterator)
        if service == 0 { break }
        defer { IOObjectRelease(service) }
        var unmanaged: Unmanaged<CFMutableDictionary>?
        guard IORegistryEntryCreateCFProperties(service, &unmanaged, kCFAllocatorDefault, 0)
                == KERN_SUCCESS,
              let properties = unmanaged?.takeRetainedValue() as? [String: Any] else {
            continue
        }
        if let device = normalizedUSBDevice(properties) { devices.append(device) }
    }
    return (true, devices, [])
}

private func coreAudioString(_ device: AudioDeviceID, _ selector: AudioObjectPropertySelector) -> String? {
    var address = AudioObjectPropertyAddress(mSelector: selector,
        mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
    var value: Unmanaged<CFString>?
    var size = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
    guard AudioObjectGetPropertyData(device, &address, 0, nil, &size, &value) == noErr else {
        return nil
    }
    return value?.takeUnretainedValue() as String?
}

private func coreAudioUInt32(_ device: AudioDeviceID,
                             _ selector: AudioObjectPropertySelector) -> UInt32? {
    var address = AudioObjectPropertyAddress(mSelector: selector,
        mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
    var value: UInt32 = 0
    var size = UInt32(MemoryLayout<UInt32>.size)
    guard AudioObjectGetPropertyData(device, &address, 0, nil, &size, &value) == noErr else {
        return nil
    }
    return value
}

private func liveCoreAudio() -> (Bool, [[String: Any]], [[String: Any]]) {
    var address = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices,
        mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
    var size: UInt32 = 0
    guard AudioObjectGetPropertyDataSize(AudioObjectID(kAudioObjectSystemObject),
                                         &address, 0, nil, &size) == noErr else {
        return (false, [], [["source": "coreaudio.device", "reasonCode": "query_failed",
                            "message": "Core Audio enumeration failed."]])
    }
    var ids = [AudioDeviceID](repeating: 0, count: Int(size) / MemoryLayout<AudioDeviceID>.size)
    guard AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject),
                                     &address, 0, nil, &size, &ids) == noErr else {
        return (false, [], [["source": "coreaudio.device", "reasonCode": "query_failed",
                            "message": "Core Audio enumeration failed."]])
    }
    let devices = ids.compactMap { id -> [String: Any]? in
        guard let uid = coreAudioString(id, kAudioDevicePropertyDeviceUID) else { return nil }
        return ["uid": uid,
                "name": coreAudioString(id, kAudioObjectPropertyName) ?? unavailable,
                "manufacturer": coreAudioString(id, kAudioObjectPropertyManufacturer) ?? unavailable,
                "transportType": coreAudioUInt32(id, kAudioDevicePropertyTransportType) ??
                    unavailable]
    }
    return (true, devices, [])
}

private func controlExecutable() -> (URL?, String) {
    let own = URL(fileURLWithPath: CommandLine.arguments[0]).standardizedFileURL
        .deletingLastPathComponent().appendingPathComponent("opena8dj-control")
    let candidates = [(own, "sibling"), (URL(fileURLWithPath: "/usr/local/bin/opena8dj-control"),
                                         "fixed-installed")]
    for (url, provenance) in candidates {
        if isRegularFile(url.path) {
            return (url, provenance)
        }
    }
    return (nil, "unavailable")
}

private func runControl(_ executable: URL, _ arguments: [String]) -> [String: Any] {
    let process = Process()
    process.executableURL = executable
    process.arguments = arguments
    process.environment = ["PATH": "/usr/bin:/bin", "LANG": "C"]
    let output = Pipe(), errors = Pipe()
    process.standardOutput = output
    process.standardError = errors
    let semaphore = DispatchSemaphore(value: 0)
    let readers = DispatchGroup()
    let lock = NSLock()
    var stdout = Data(), stderrBytes = 0, overflow = false
    func consume(_ data: Data, keep: Bool) {
        guard !data.isEmpty else { return }
        var exceeded = false
        lock.lock()
        if keep {
            if stdout.count + data.count <= 1024 * 1024 {
                stdout.append(data)
            } else {
                overflow = true
                exceeded = true
            }
        } else {
            stderrBytes += data.count
            if stderrBytes > 1024 * 1024 {
                overflow = true
                exceeded = true
            }
        }
        lock.unlock()
        if exceeded && process.processIdentifier > 0 {
            _ = kill(process.processIdentifier, SIGTERM)
        }
    }
    func drain(_ handle: FileHandle, keep: Bool) {
        readers.enter()
        DispatchQueue.global(qos: .utility).async {
            while let data = try? handle.read(upToCount: 64 * 1024), !data.isEmpty {
                consume(data, keep: keep)
            }
            readers.leave()
        }
    }
    process.terminationHandler = { _ in semaphore.signal() }
    do { try process.run() } catch {
        return ["state": "unavailable", "reasonCode": "launch_failed"]
    }
    drain(output.fileHandleForReading, keep: true)
    drain(errors.fileHandleForReading, keep: false)
    var timedOut = false
    if semaphore.wait(timeout: .now() + .seconds(3)) == .timedOut {
        timedOut = true
        _ = kill(process.processIdentifier, SIGTERM)
        if semaphore.wait(timeout: .now() + .milliseconds(250)) == .timedOut {
            _ = kill(process.processIdentifier, SIGKILL)
            process.waitUntilExit()
        }
    }
    process.waitUntilExit()
    readers.wait()
    lock.lock()
    let didOverflow = overflow
    let capturedStdout = stdout
    lock.unlock()
    if timedOut {
        return ["state": "unavailable", "reasonCode": "timeout"]
    }
    guard !didOverflow,
          let object = try? JSONSerialization.jsonObject(with: capturedStdout),
          let document = object as? [String: Any] else {
        return ["state": "malformed",
                "reasonCode": didOverflow ? "output_limit_exceeded" : "invalid_json"]
    }
    return ["state": process.terminationStatus == 0 ? "ok" : "error", "document": document]
}

private func liveObservation() -> [String: Any] {
    let usb = liveUSB()
    let core = liveCoreAudio()
    let selected = controlExecutable()
    var errors = usb.2 + core.2
    var api: [String: Any] = ["provenance": selected.1]
    if let executable = selected.0 {
        api["version"] = runControl(executable, ["api", "version"])
        api["hardware"] = runControl(executable, ["api", "hardware"])
        api["stats"] = runControl(executable, ["api", "stats"])
    } else {
        let absent: [String: Any] = ["state": "unavailable", "reasonCode": "control_not_found"]
        api["version"] = absent; api["hardware"] = absent; api["stats"] = absent
        errors.append(["source": "opena8dj.api", "reasonCode": "control_not_found",
                       "message": "OpenA8DJ public API client is unavailable."])
    }
    let os = ProcessInfo.processInfo.operatingSystemVersion
    return [
        "generatedAt": ISO8601DateFormatter().string(from: Date()),
        "osVersion": "\(os.majorVersion).\(os.minorVersion).\(os.patchVersion)",
        "usbEnumerationAvailable": usb.0, "usbCandidates": usb.1,
        "coreAudioAvailable": core.0, "coreAudioDevices": core.1,
        "api": api, "collectorErrors": errors
    ]
}

#if OPENA8DJ_HARDWARE_PROFILER_TESTING
private func fixtureObservation(_ decoded: [String: Any]) -> [String: Any] {
    guard let raw = decoded["rawUSBRegistryProperties"] as? [[String: Any]] else {
        return decoded
    }
    var observation = decoded
    observation["usbEnumerationAvailable"] = true
    observation["usbCandidates"] = raw.compactMap(normalizedUSBDevice)
    observation.removeValue(forKey: "rawUSBRegistryProperties")
    return observation
}
#endif

private func apiDocument(_ api: [String: Any], _ name: String) -> [String: Any]? {
    guard let result = dictionary(api[name]), string(result, "state") == "ok" else { return nil }
    return dictionary(result["document"])
}

private func validEnvelope(_ document: [String: Any]?, operation: String) -> Bool {
    guard let document else { return false }
    guard let version = string(document, "apiVersion").flatMap(strictVersion),
          version.first == 1 else { return false }
    return string(document, "schema") == apiSchema &&
           bool(document, "ok") == true &&
           string(document, "operation") == operation &&
           dictionary(document["data"]) != nil
}

private func isJSONNumber(_ value: Any?) -> Bool {
    guard let number = value as? NSNumber else { return false }
    return CFGetTypeID(number) != CFBooleanGetTypeID()
}

private func nonnegativeInteger(_ object: [String: Any], _ key: String) -> Int64? {
    guard let value = integer(object, key), value >= 0 else { return nil }
    return value
}

private func validVersionDocument(_ document: [String: Any]?) -> Bool {
    guard validEnvelope(document, operation: "version.get"),
          let data = dictionary(document?["data"]),
          string(data, "apiVersion") != nil,
          string(data, "schema") == apiSchema,
          data["capabilities"] is [String] else { return false }
    return true
}

private func validHardwareDocument(_ document: [String: Any]?) -> Bool {
    guard validEnvelope(document, operation: "hardware.get"),
          let data = dictionary(document?["data"]),
          let marker = bool(data, "deviceInfoAvailable"),
          let capabilities = dictionary(data["capabilities"]) else { return false }
    let scalarKeys = ["firmwareVersion", "hardwareSubtype"]
    let capabilityKeys = ["analogAudioOutputs","analogAudioInputs","digitalAudioOutputs",
                          "digitalAudioInputs","midiOutputs","midiInputs","dataAlignment"]
    if marker {
        return scalarKeys.allSatisfy { nonnegativeInteger(data, $0) != nil } &&
               capabilityKeys.allSatisfy { nonnegativeInteger(capabilities, $0) != nil }
    }
    return scalarKeys.allSatisfy { data[$0] is NSNull } &&
           capabilityKeys.allSatisfy { capabilities[$0] is NSNull }
}

private let jitterBinKeys = ["le50","le100","le250","le500","le1000","gt1000"]
private let isoErrorKeys = ["queueFailures","completionStatusFailures",
                            "transactionStatusFailures","zeroLengthTransactions",
                            "shortTransactions"]

private func basicStatsData(_ document: [String: Any]?) -> [String: Any]? {
    guard validEnvelope(document, operation: "stats.get"),
          let data = dictionary(document?["data"]),
          let stream = dictionary(data["stream"]),
          bool(stream, "streaming") != nil,
          isJSONNumber(stream["sampleRate"]),
          let capture = dictionary(data["capture"]),
          let playback = dictionary(data["playback"]),
          integer(capture, "transfers") != nil,
          integer(playback, "transfers") != nil,
          let quality = dictionary(data["quality"]),
          bool(quality, "instrumentationAvailable") != nil else { return nil }
    return data
}

private func qualityStructureValid(_ data: [String: Any]) -> Bool {
    guard let quality = dictionary(data["quality"]),
          let jitter = dictionary(quality["completionJitter"]),
          let iso = dictionary(quality["isoErrors"]),
          let output = dictionary(data["output"]),
          let activeUnderruns = integer(output, "activeUnderruns"),
          let ringOverruns = integer(output, "ringOverruns"),
          activeUnderruns >= 0, ringOverruns >= 0 else { return false }
    for directionName in ["capture", "playback"] {
        guard let direction = dictionary(jitter[directionName]),
              let samples = integer(direction, "samples"),
              let invalid = integer(direction, "invalidIntervals"),
              let bins = dictionary(direction["bins"]),
              samples >= 0, invalid >= 0 else { return false }
        let values = jitterBinKeys.compactMap { integer(bins, $0) }
        guard values.count == jitterBinKeys.count,
              values.allSatisfy({ $0 >= 0 }),
              values.reduce(Int64(0), +) == samples,
              let errors = dictionary(iso[directionName]),
              isoErrorKeys.allSatisfy({
                  guard let value = integer(errors, $0) else { return false }
                  return value >= 0
              }) else { return false }
    }
    return true
}

private func validStatsDocument(_ document: [String: Any]?) -> Bool {
    guard let data = basicStatsData(document) else { return false }
    return qualityStructureValid(data)
}

private func percentileBin(_ direction: [String: Any], _ numerator: Int64) -> Int {
    guard let samples = integer(direction, "samples"), samples > 0,
          let bins = dictionary(direction["bins"]) else { return 6 }
    let target = (samples * numerator + 99) / 100
    var cumulative: Int64 = 0
    for (index, key) in ["le50","le100","le250","le500","le1000","gt1000"].enumerated() {
        cumulative += integer(bins, key) ?? 0
        if cumulative >= target { return index }
    }
    return 6
}

private func evaluateQuality(_ statsDocument: [String: Any]?) -> [String: Any] {
    guard let data = basicStatsData(statsDocument),
          let stream = dictionary(data["stream"]),
          let quality = dictionary(data["quality"]),
          let streaming = bool(stream, "streaming"),
          let instrumentation = bool(quality, "instrumentationAvailable") else {
        return check("usb.stream-quality", .unknown, "USB_QUALITY_UNAVAILABLE",
                     "Stream quality evidence is unavailable.",
                     [evidence("opena8dj.api.stats", "quality.instrumentationAvailable", nil,
                               reason: "not_streaming_or_unavailable")])
    }
    guard streaming, instrumentation else {
        return check("usb.stream-quality", .unknown, "USB_QUALITY_UNAVAILABLE",
                     "Stream quality instrumentation is inactive or unavailable.")
    }
    guard qualityStructureValid(data),
          let jitter = dictionary(quality["completionJitter"]),
          let capture = dictionary(jitter["capture"]),
          let playback = dictionary(jitter["playback"]),
          let iso = dictionary(quality["isoErrors"]),
          let output = dictionary(data["output"]) else {
        return check("usb.stream-quality", .fail, "USB_QUALITY_INVALID",
                     "Claimed quality instrumentation is structurally inconsistent.")
    }
    let transferObjects = [dictionary(data["capture"]), dictionary(data["playback"])]
    let directions = [capture, playback]
    var active = 0
    for index in 0..<2 {
        let transfers = transferObjects[index].flatMap { integer($0, "transfers") } ?? 0
        if transfers == 0 { continue }
        active += 1
        let samples = integer(directions[index], "samples")!
        if samples < 20 {
            return check("usb.stream-quality", .unknown, "USB_QUALITY_UNAVAILABLE",
                         "There are not enough quality samples.")
        }
    }
    if active == 0 {
        return check("usb.stream-quality", .unknown, "USB_QUALITY_UNAVAILABLE",
                     "No active stream direction has quality samples.")
    }
    var degraded = false
    for direction in directions {
        guard let bins = dictionary(direction["bins"]),
              let samples = integer(direction, "samples"), samples > 0 else { continue }
        if percentileBin(direction, 95) > 2 || percentileBin(direction, 99) > 3 ||
           (integer(bins, "gt1000") ?? 0) * 1000 > samples {
            degraded = true
        }
    }
    for directionName in ["capture", "playback"] {
        if let values = dictionary(iso[directionName]),
           isoErrorKeys.contains(where: { (integer(values, $0) ?? 0) > 0 }) {
            degraded = true
        }
    }
    if ["activeUnderruns","ringOverruns"].contains(where: {
           (integer(output, $0) ?? 0) > 0
       }) { degraded = true }
    return degraded ?
        check("usb.stream-quality", .warn, "USB_QUALITY_DEGRADED",
              "Concrete jitter, isochronous, or xrun degradation was observed.") :
        check("usb.stream-quality", .pass, "USB_QUALITY_HEALTHY",
              "Stream quality instrumentation is healthy for the sampled stream.")
}

private let allowedFacts: Set<String> = [
    "usb.vendorId","usb.productId","usb.bcdDevice","usb.linkSpeedBitsPerSecond",
    "usb.requiredPowerMilliAmps","usb.availablePowerMilliAmps",
    "device.firmwareVersion","device.hardwareSubtype","driver.version","api.version","os.version"
]
private let allowedOps: Set<String> = ["eq","ne","lt","lte","gt","gte","version-in-range"]
private let integerFacts: Set<String> = [
    "usb.vendorId","usb.productId","usb.bcdDevice","usb.linkSpeedBitsPerSecond",
    "usb.requiredPowerMilliAmps","usb.availablePowerMilliAmps",
    "device.firmwareVersion","device.hardwareSubtype"
]

private func isNonnegativeIntegerNumber(_ number: NSNumber) -> Bool {
    guard CFGetTypeID(number) != CFBooleanGetTypeID() else { return false }
    let value = number.doubleValue
    return value.isFinite && value >= 0 && value.rounded(.towardZero) == value &&
           value <= Double(Int64.max)
}

private func strictVersion(_ value: String) -> [Int]? {
    let parts = value.split(separator: ".", omittingEmptySubsequences: false)
    guard !parts.isEmpty else { return nil }
    var result: [Int] = []
    for part in parts {
        guard !part.isEmpty, part.allSatisfy(\.isNumber), let component = Int(part) else {
            return nil
        }
        result.append(component)
    }
    return result
}

private func compareVersion(_ lhs: [Int], _ rhs: [Int]) -> Int {
    let count = max(lhs.count, rhs.count)
    for index in 0..<count {
        let a = index < lhs.count ? lhs[index] : 0
        let b = index < rhs.count ? rhs[index] : 0
        if a != b { return a < b ? -1 : 1 }
    }
    return 0
}

private func catalogValidation(_ catalog: [String: Any]) -> String? {
    guard string(catalog, "schema") == catalogSchema,
          let version = string(catalog, "catalogVersion"), strictVersion(version) != nil,
          catalog["issues"] is [[String: Any]] else { return "schema_or_version" }
    if let recognized = catalog["recognizedFirmwareVersions"] {
        guard let values = recognized as? [NSNumber],
              values.allSatisfy(isNonnegativeIntegerNumber),
              Set(values.map(\.int64Value)).count == values.count else {
            return "invalid_recognized_firmware"
        }
    }
    var ids = Set<String>()
    for issue in array(catalog["issues"]) {
        guard let id = string(issue, "id"), !id.isEmpty, id.count <= 128,
              ids.insert(id).inserted,
              let status = string(issue, "status"), status == "WARN" || status == "FAIL",
              let summary = string(issue, "summary"), !summary.isEmpty,
              summary.count <= 1024,
              let source = dictionary(issue["source"]),
              let sourceTitle = string(source, "title"), !sourceTitle.isEmpty,
              sourceTitle.count <= 256,
              let sourceVersion = string(source, "version"), !sourceVersion.isEmpty,
              sourceVersion.count <= 128,
              (source["url"] == nil || (source["url"] as? String).map {
                  !$0.isEmpty && $0.count <= 2048
              } == true),
              let remediation = issue["remediation"] as? [String], !remediation.isEmpty,
              remediation.allSatisfy({ !$0.isEmpty && $0.count <= 2048 }),
              let predicates = issue["all"] as? [[String: Any]], !predicates.isEmpty else {
            return "invalid_rule"
        }
        if let group = issue["exclusiveGroup"], !(group is NSNull) {
            guard let text = group as? String, !text.isEmpty, text.count <= 128 else {
                return "invalid_exclusive_group"
            }
        }
        for predicate in predicates {
            guard let fact = string(predicate, "fact"), allowedFacts.contains(fact),
                  let op = string(predicate, "op"), allowedOps.contains(op) else {
                return "invalid_predicate"
            }
            if op == "version-in-range" {
                guard let minimum = string(predicate, "minimum"),
                      let maximum = string(predicate, "maximum"),
                      let parsedMinimum = strictVersion(minimum),
                      let parsedMaximum = strictVersion(maximum),
                      compareVersion(parsedMinimum, parsedMaximum) <= 0 else {
                    return "invalid_version_range"
                }
            } else {
                guard let value = predicate["value"], !(value is NSNull) else {
                    return "missing_value"
                }
                if integerFacts.contains(fact) {
                    guard let number = value as? NSNumber,
                          isNonnegativeIntegerNumber(number) else {
                        return "invalid_value_type"
                    }
                } else {
                    guard let text = value as? String, !text.isEmpty,
                          text.count <= 128 else { return "invalid_value_type" }
                }
            }
        }
    }
    return nil
}

private enum PredicateResult { case matched, unmatched, unresolved }

private func predicateResult(_ predicate: [String: Any], facts: [String: Any]) -> PredicateResult {
    guard let fact = string(predicate, "fact"), let op = string(predicate, "op"),
          let actual = facts[fact], !(actual is NSNull) else { return .unresolved }
    if op == "version-in-range" {
        guard let actualString = actual as? String, let parsed = strictVersion(actualString),
              let minimum = string(predicate, "minimum").flatMap(strictVersion),
              let maximum = string(predicate, "maximum").flatMap(strictVersion) else {
            return .unresolved
        }
        return compareVersion(parsed, minimum) >= 0 && compareVersion(parsed, maximum) <= 0
            ? .matched : .unmatched
    }
    guard let expected = predicate["value"] else { return .unresolved }
    if let lhs = actual as? NSNumber, let rhs = expected as? NSNumber,
       CFGetTypeID(lhs) != CFBooleanGetTypeID(), CFGetTypeID(rhs) != CFBooleanGetTypeID() {
        let a = lhs.int64Value, b = rhs.int64Value
        let result: Bool
        switch op {
        case "eq": result = a == b; case "ne": result = a != b
        case "lt": result = a < b; case "lte": result = a <= b
        case "gt": result = a > b; case "gte": result = a >= b
        default: result = false
        }
        return result ? .matched : .unmatched
    }
    if let a = actual as? String, let b = expected as? String {
        if op == "eq" { return a == b ? .matched : .unmatched }
        if op == "ne" { return a != b ? .matched : .unmatched }
    }
    return .unresolved
}

private func evaluateCatalog(_ catalog: [String: Any], facts: [String: Any],
                             metadata: [String: Any]) -> ([String: Any], [String: Any]) {
    if catalogValidation(catalog) != nil {
        return (check("known-issues.catalog", .unknown, "KNOWN_ISSUES_CATALOG_INVALID",
                      "The known-issues catalog is invalid."),
                ["catalog": metadata, "matches": [], "unresolved": [], "conflicts": []])
    }
    var matches: [[String: Any]] = [], unresolved: [String] = []
    for issue in array(catalog["issues"]) {
        let results = array(issue["all"]).map { predicateResult($0, facts: facts) }
        if results.contains(where: { if case .unmatched = $0 { return true }; return false }) {
            continue
        }
        if results.contains(where: { if case .unresolved = $0 { return true }; return false }) {
            unresolved.append(string(issue, "id")!)
            continue
        }
        matches.append(issue)
    }
    var conflicts: [String] = []
    let grouped = Dictionary(grouping: matches.compactMap { issue -> (String, [String: Any])? in
        guard let group = issue["exclusiveGroup"] as? String, !group.isEmpty else { return nil }
        return (group, issue)
    }, by: { $0.0 })
    for (group, members) in grouped where members.count > 1 {
        let signatures = Set(members.map {
            (string($0.1, "status") ?? "") + "|" +
            (($0.1["remediation"] as? [String]) ?? []).joined(separator: "\u{1f}")
        })
        if signatures.count > 1 { conflicts.append(group) }
    }
    var outputMatches: [[String: Any]] = []
    for issue in matches {
        outputMatches.append([
            "id": string(issue, "id")!, "status": string(issue, "status")!,
            "summary": string(issue, "summary")!,
            "source": issue["source"]!, "remediation": issue["remediation"]!
        ])
    }
    let known: [String: Any] = ["catalog": metadata, "matches": outputMatches,
                                "unresolved": unresolved, "conflicts": conflicts]
    if !conflicts.isEmpty {
        return (check("known-issues.catalog", .unknown, "KNOWN_ISSUES_CATALOG_CONFLICT",
                      "Conflicting exclusive known-issue rules matched."), known)
    }
    if !unresolved.isEmpty {
        return (check("known-issues.catalog", .unknown, "KNOWN_ISSUES_EVIDENCE_MISSING",
                      "A potentially applicable known issue lacks required evidence."), known)
    }
    if matches.contains(where: { string($0, "status") == "FAIL" }) {
        return (check("known-issues.catalog", .fail, "KNOWN_ISSUE_UNSUPPORTED",
                      "A locally documented unsupported issue matched."), known)
    }
    if !matches.isEmpty {
        return (check("known-issues.catalog", .warn, "KNOWN_ISSUE_MATCHED",
                      "One or more locally documented issues matched."), known)
    }
    return (check("known-issues.catalog", .pass, "KNOWN_ISSUES_NONE",
                  "No locally documented known issue matched."), known)
}

private func evaluate(_ observation: [String: Any], catalog: [String: Any],
                      catalogMetadata: [String: Any]) -> [String: Any] {
    let usbAvailable = bool(observation, "usbEnumerationAvailable") == true
    let candidates = array(observation["usbCandidates"])
    let exact = candidates.filter {
        integer($0, "vendorId") == Int64(expectedVID) &&
        integer($0, "productId") == Int64(expectedPID)
    }
    var checks: [[String: Any]] = []
    if !usbAvailable {
        checks.append(check("usb.identity", .unknown, "USB_IDENTITY_UNKNOWN",
                            "USB enumeration is unavailable.",
                            [evidence("ioregistry.usb", "enumeration", nil,
                                      reason: "query_unavailable")]))
    } else if exact.count == 1 {
        checks.append(check("usb.identity", .pass, "USB_IDENTITY_EXACT",
                            "One exact Audio 8 DJ USB identity is present.",
                            [evidence("ioregistry.usb", "vendorId", expectedVID),
                             evidence("ioregistry.usb", "productId", expectedPID)]))
    } else if exact.count > 1 {
        checks.append(check("usb.identity", .warn, "USB_DEVICE_MULTIPLE",
                            "Multiple exact Audio 8 DJ USB identities are present."))
    } else if !candidates.isEmpty {
        checks.append(check("usb.identity", .fail, "USB_IDENTITY_MISMATCH",
                            "An explicitly observed USB candidate has the wrong identity."))
    } else {
        checks.append(check("usb.identity", .fail, "USB_DEVICE_NOT_FOUND",
                            "No exact Audio 8 DJ USB device is present."))
    }

    let device = usbAvailable ? exact.first : nil
    if !usbAvailable {
        checks.append(check("usb.enumeration", .unknown, "USB_ENUMERATION_UNKNOWN",
                            "USB enumeration evidence is unavailable."))
    } else if let device {
        let descriptor = bool(device, "descriptorReadable")
        let configuration = integer(device, "currentConfiguration")
        let usable = bool(device, "usable")
        if descriptor == true && configuration != nil && usable != false {
            checks.append(check("usb.enumeration", .pass, "USB_ENUMERATION_OK",
                                "The exact USB device descriptor and configuration are readable."))
        } else if usable == false {
            checks.append(check("usb.enumeration", .fail, "USB_ENUMERATION_FAILED",
                                "The exact USB device is not usable."))
        } else {
            checks.append(check("usb.enumeration", .warn, "USB_DESCRIPTOR_INCOMPLETE",
                                "The exact USB identity is present but descriptors are incomplete."))
        }
    } else {
        checks.append(check("usb.enumeration", .unknown, "USB_ENUMERATION_UNKNOWN",
                            "USB enumeration cannot be evaluated without the exact device."))
    }

    if let device, bool(device, "linkUnusable") == true {
        checks.append(check("usb.link-speed", .fail, "USB_LINK_UNUSABLE",
                            "A reliable USB stack error reports an unusable link."))
    } else if let device, let speed = integer(device, "linkSpeedBitsPerSecond") {
        if speed >= 480_000_000 {
            checks.append(check("usb.link-speed", .pass, "USB_LINK_HIGH_SPEED",
                                "The direct USB link is high speed or faster.",
                                [evidence("ioregistry.usb", "linkSpeedBitsPerSecond", speed,
                                          unit: "bits-per-second")]))
        } else {
            checks.append(check("usb.link-speed", .warn, "USB_LINK_DEGRADED",
                                "The direct USB link is full/low speed.",
                                [evidence("ioregistry.usb", "linkSpeedBitsPerSecond", speed,
                                          unit: "bits-per-second")]))
        }
    } else {
        checks.append(check("usb.link-speed", .unknown, "USB_LINK_UNKNOWN",
                            "Direct USB link speed evidence is unavailable."))
    }

    if let device, bool(device, "failedRequestedPower") == true {
        checks.append(check("usb.power", .fail, "USB_POWER_INSUFFICIENT",
                            "The USB stack asserted failed requested power.",
                            [evidence("ioregistry.usb", "failedRequestedPower", true)]))
    } else if let device,
              string(device, "powerEvidenceScope") == "device-current-pair",
              string(device, "powerEvidenceUnit") == "mA",
              let available = integer(device, "availablePowerMilliAmps"),
              let required = integer(device, "requiredPowerMilliAmps") {
        let enough = available >= required
        checks.append(check("usb.power", enough ? .pass : .fail,
                            enough ? "USB_POWER_SUFFICIENT" : "USB_POWER_INSUFFICIENT",
                            enough ? "Comparable USB current evidence is sufficient." :
                                     "Comparable USB current evidence is insufficient.",
                            [evidence("ioregistry.usb", "availablePowerMilliAmps", available,
                                      unit: "mA"),
                             evidence("ioregistry.usb", "requiredPowerMilliAmps", required,
                                      unit: "mA")]))
    } else {
        checks.append(check("usb.power", .unknown, "USB_POWER_UNKNOWN",
                            "Comparable USB current evidence is unavailable."))
    }

    let api = dictionary(observation["api"]) ?? [:]
    let versionDoc = apiDocument(api, "version")
    let hardwareDoc = apiDocument(api, "hardware")
    let statsDoc = apiDocument(api, "stats")
    let hardwareValid = validHardwareDocument(hardwareDoc)
    let hardwareEnvelopeValid = validEnvelope(hardwareDoc, operation: "hardware.get")
    let hardwareData = hardwareEnvelopeValid ? dictionary(hardwareDoc?["data"]) : nil
    let infoAvailable = hardwareData.flatMap { bool($0, "deviceInfoAvailable") } == true
    let observedFirmware = infoAvailable ?
        hardwareData.flatMap { nonnegativeInteger($0, "firmwareVersion") } : nil
    let observedSubtype = infoAvailable ?
        hardwareData.flatMap { nonnegativeInteger($0, "hardwareSubtype") } : nil
    let capabilities = hardwareData.flatMap { dictionary($0["capabilities"]) }
    let completeDeviceInfo = observedFirmware != nil && observedSubtype != nil &&
        capabilities != nil &&
        ["analogAudioOutputs","analogAudioInputs","digitalAudioOutputs","digitalAudioInputs",
         "midiOutputs","midiInputs","dataAlignment"].allSatisfy { field in
            capabilities.flatMap { nonnegativeInteger($0, field) } != nil
        }
    let firmware = infoAvailable && completeDeviceInfo ? observedFirmware : nil
    let subtype = infoAvailable && completeDeviceInfo ? observedSubtype : nil
    let firmwareCheckIndex = checks.count
    if infoAvailable && !completeDeviceInfo {
        checks.append(check("device.firmware", .fail, "DEVICE_INFO_INVALID",
                            "Marker-qualified cached device information is structurally invalid."))
    } else if let firmware {
        let recognized = (catalog["recognizedFirmwareVersions"] as? [NSNumber])?
            .contains(where: { $0.int64Value == firmware }) == true
        checks.append(check("device.firmware", recognized ? .pass : .warn,
                            recognized ? "DEVICE_FIRMWARE_RECOGNIZED" : "DEVICE_FIRMWARE_UNKNOWN",
                            recognized ? "Cached firmware is recognized by the local catalog." :
                                         "Cached firmware is not cataloged.",
                            [evidence("opena8dj.api.hardware", "firmwareVersion", firmware)]))
    } else {
        checks.append(check("device.firmware", .unknown, "DEVICE_INFO_UNAVAILABLE",
                            "Marker-qualified cached device information is unavailable."))
    }

    let coreAvailable = bool(observation, "coreAudioAvailable") == true
    let coreDevices = array(observation["coreAudioDevices"])
    let uidDevices = coreDevices.filter { string($0, "uid") == expectedUID }
    if !coreAvailable {
        checks.append(check("coreaudio.device", .unknown, "COREAUDIO_QUERY_UNAVAILABLE",
                            "Core Audio enumeration is unavailable."))
    } else if uidDevices.count == 1 {
        let nameExpected = string(uidDevices[0], "name") == nil ||
            string(uidDevices[0], "name") == "Open Audio 8 DJ"
        let transportExpected = integer(uidDevices[0], "transportType") == nil ||
            integer(uidDevices[0], "transportType") == Int64(kAudioDeviceTransportTypeUSB)
        let metadataExpected = nameExpected && transportExpected
        checks.append(check("coreaudio.device", metadataExpected ? .pass : .warn,
                            metadataExpected ? "COREAUDIO_DEVICE_MATCHED" :
                                               "COREAUDIO_METADATA_UNEXPECTED",
                            metadataExpected ? "The exact Core Audio UID is present." :
                                               "The exact Core Audio UID has unexpected metadata."))
    } else if uidDevices.count > 1 {
        checks.append(check("coreaudio.device", .warn, "COREAUDIO_METADATA_UNEXPECTED",
                            "Multiple devices expose the exact Core Audio UID."))
    } else if device != nil {
        checks.append(check("coreaudio.device", .fail, "COREAUDIO_DEVICE_MISSING",
                            "USB is present but the expected Core Audio UID is absent."))
    } else {
        checks.append(check("coreaudio.device", .unknown, "COREAUDIO_QUERY_UNAVAILABLE",
                            "Core Audio pairing is unknown while USB is absent."))
    }

    let hardwareStructuresConsistent = hardwareValid && (!infoAvailable || completeDeviceInfo)
    let allAPIsValid = validVersionDocument(versionDoc) &&
                       hardwareStructuresConsistent &&
                       validStatsDocument(statsDoc)
    if uidDevices.count == 1 && allAPIsValid {
        let apiVersion = string(versionDoc.flatMap { dictionary($0["data"]) } ?? [:], "apiVersion")
            ?? string(versionDoc ?? [:], "apiVersion") ?? "1.0"
        let newer = strictVersion(apiVersion).map { compareVersion($0, [1,0]) > 0 } ?? false
        checks.append(check("driver.api-pairing", newer ? .warn : .pass,
                            newer ? "DRIVER_API_NEWER_COMPATIBLE" : "DRIVER_API_MATCHED",
                            newer ? "A newer compatible public API is paired." :
                                    "The expected public API and Core Audio UID are paired.",
                            [evidence("opena8dj.api", "controlExecutableProvenance",
                                      string(api, "provenance") ?? "unavailable")]))
    } else if uidDevices.count == 1 {
        checks.append(check("driver.api-pairing", .fail, "DRIVER_API_MISMATCH",
                            "The Core Audio UID exists but the public API is unavailable or incompatible."))
    } else {
        checks.append(check("driver.api-pairing", .unknown, "DRIVER_PAIRING_UNKNOWN",
                            "There is insufficient evidence to pair the driver surfaces."))
    }
    checks.append(evaluateQuality(statsDoc))

    var facts: [String: Any] = [
        "os.version": string(observation, "osVersion") ?? unavailable
    ]
    if let device {
        for (fact, key) in [
            ("usb.vendorId","vendorId"),("usb.productId","productId"),("usb.bcdDevice","bcdDevice"),
            ("usb.linkSpeedBitsPerSecond","linkSpeedBitsPerSecond")
        ] { facts[fact] = device[key] ?? unavailable }
        if string(device, "powerEvidenceScope") == "device-current-pair",
           string(device, "powerEvidenceUnit") == "mA" {
            facts["usb.requiredPowerMilliAmps"] =
                device["requiredPowerMilliAmps"] ?? unavailable
            facts["usb.availablePowerMilliAmps"] =
                device["availablePowerMilliAmps"] ?? unavailable
        }
    }
    facts["device.firmwareVersion"] = firmware ?? unavailable
    facts["device.hardwareSubtype"] = subtype ?? unavailable
    if let versionData = versionDoc.flatMap({ dictionary($0["data"]) }) {
        facts["api.version"] = string(versionData, "apiVersion") ?? unavailable
        facts["driver.version"] = string(versionData, "driverVersion") ?? unavailable
    }
    let catalogEvaluation = evaluateCatalog(catalog, facts: facts, metadata: catalogMetadata)
    let firmwareUnsupported = catalogValidation(catalog) == nil &&
        array(catalog["issues"]).contains { issue in
            string(issue, "status") == "FAIL" &&
            array(issue["all"]).contains {
                string($0, "fact") == "device.firmwareVersion"
            } &&
            array(issue["all"]).allSatisfy {
                if case .matched = predicateResult($0, facts: facts) { return true }
                return false
            }
        }
    if firmwareUnsupported {
        checks[firmwareCheckIndex] = check(
            "device.firmware", .fail, "DEVICE_INFO_INVALID",
            "The cached firmware is explicitly unsupported by the selected local catalog.",
            [evidence("opena8dj.api.hardware", "firmwareVersion", firmware!)])
    }
    checks.append(catalogEvaluation.0)

    let statuses = checks.compactMap { string($0, "status") }
    let overall: Status
    if statuses.contains(Status.fail.rawValue) { overall = .fail }
    else if statuses.contains(Status.warn.rawValue) { overall = .warn }
    else if statuses.contains(Status.unknown.rawValue) { overall = .unknown }
    else { overall = .pass }
    let exitStatus: Int = [.pass: 0, .warn: 1, .fail: 2, .unknown: 3][overall]!
    let summaryCodes = checks.filter { string($0, "status") != Status.pass.rawValue }
        .compactMap { string($0, "code") }
    return [
        "schema": reportSchema, "schemaVersion": 1, "toolVersion": toolVersion,
        "generatedAt": string(observation, "generatedAt").flatMap {
            ISO8601DateFormatter().date(from: $0) == nil ? nil : $0
        } ?? ISO8601DateFormatter().string(from: Date()),
        "overall": ["status": overall.rawValue, "exitStatus": exitStatus,
                    "summaryCodes": summaryCodes],
        "subject": ["expectedUsb": ["vendorId": expectedVID, "productId": expectedPID],
                    "expectedCoreAudioUid": expectedUID, "platform": "macOS"],
        "privacy": ["mode": "support-redacted",
                    "omitted": ["usbSerial","usbLocationId","registryPath","hostName",
                                "userName","socketPath"]],
        "checks": checks, "knownIssues": catalogEvaluation.1,
        "collectorErrors": array(observation["collectorErrors"]).map {
            ["source": string($0, "source") ?? "collector",
             "reasonCode": string($0, "reasonCode") ?? "unavailable",
             "message": "Collector reported unavailable evidence."]
        }
    ]
}

private func renderJSON(_ report: [String: Any]) throws {
    let data = try JSONSerialization.data(withJSONObject: report, options: [.sortedKeys])
    FileHandle.standardOutput.write(data)
    FileHandle.standardOutput.write(Data([0x0a]))
}

private func renderHuman(_ report: [String: Any]) {
    let overall = dictionary(report["overall"]) ?? [:]
    print("\(string(overall, "status") ?? "UNKNOWN") OpenA8DJ hardware profile")
    for item in array(report["checks"]) {
        print("[\(string(item, "status") ?? "UNKNOWN")] \(string(item, "id") ?? "-") " +
              "\(string(item, "code") ?? "-"): \(string(item, "summary") ?? "")")
        for record in array(item["evidence"]) {
            if bool(record, "available") == true {
                print("  evidence available: \(string(record, "key") ?? "-")")
            } else {
                print("  evidence unavailable: \(string(record, "key") ?? "-") " +
                      "(\(string(record, "reasonCode") ?? "not_available"))")
            }
        }
        for remediation in item["remediation"] as? [String] ?? [] {
            print("  remediation: \(remediation)")
        }
    }
    print("Privacy: support-redacted; serial, location, paths, host and user identity omitted.")
}

private func catalogPath(_ override: String?) throws -> (String, String) {
    if let override { return (override, "operator-supplied") }
    let own = URL(fileURLWithPath: CommandLine.arguments[0]).standardizedFileURL
        .deletingLastPathComponent().appendingPathComponent("hardware-profiler-known-issues-v1.json")
    if isRegularFile(own.path) {
        return (own.path, "bundled")
    }
    return ("/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json",
            "bundled")
}

do {
    let options = try parseOptions()
    let selectedCatalog = try catalogPath(options.catalog)
    let catalogData = isRegularFile(selectedCatalog.0) ?
        ((try? Data(contentsOf: URL(fileURLWithPath: selectedCatalog.0),
                    options: [.mappedIfSafe])) ?? Data()) : Data()
    let catalogObject: [String: Any]
    if catalogData.count <= 2 * 1024 * 1024,
       let decoded = try? JSONSerialization.jsonObject(with: catalogData)
         as? [String: Any] {
        catalogObject = decoded
    } else {
        catalogObject = [:]
    }
    let catalogMetadata: [String: Any] = [
        "schema": string(catalogObject, "schema") ?? unavailable,
        "version": string(catalogObject, "catalogVersion") ?? unavailable,
        "provenance": selectedCatalog.1, "sha256": sha256(catalogData)
    ]
    let observation: [String: Any]
#if OPENA8DJ_HARDWARE_PROFILER_TESTING
    if let fixture = options.fixture {
        guard let decoded = try readJSON(path: fixture) as? [String: Any] else {
            throw NSError(domain: "fixture", code: 70,
                          userInfo: [NSLocalizedDescriptionKey: "fixture is not an object"])
        }
        observation = fixtureObservation(decoded)
    } else {
        observation = liveObservation()
    }
#else
    observation = liveObservation()
#endif
    let report = evaluate(observation, catalog: catalogObject, catalogMetadata: catalogMetadata)
    if options.json { try renderJSON(report) } else { renderHuman(report) }
    exit(Int32(integer(dictionary(report["overall"]) ?? [:], "exitStatus") ?? 70))
} catch {
    let code = (error as NSError).code == 64 ? 64 : 70
    fputs("opena8dj-hardware-profiler: \(error.localizedDescription)\n", stderr)
    exit(Int32(code))
}
