import Darwin
import Foundation
import SwiftUI

private func helperMode() -> Bool {
    let executable = URL(fileURLWithPath: CommandLine.arguments[0])
    let name = executable.lastPathComponent
    guard name == "cc-store-cancel-helper" ||
            name == "cc-store-cap-helper" else { return false }
    let directory = executable.deletingLastPathComponent()
    let arguments = Array(CommandLine.arguments.dropFirst())
    let key = arguments.joined(separator: " ")
    let log = directory.appendingPathComponent("operations.log")
    if let handle = try? FileHandle(forWritingTo: log) {
        _ = try? handle.seekToEnd()
        try? handle.write(contentsOf: Data((key + "\n").utf8))
        try? handle.close()
    }
    if name == "cc-store-cancel-helper", key == "api profiles" {
        Thread.sleep(forTimeInterval: 2)
    }
    let fixture: String
    switch key {
    case "api version": fixture = "version.json"
    case "api profiles": fixture = "good_profiles.json"
    case "api driver-modes": fixture = "good_modes.json"
    case "api stats": fixture = "good_stats.json"
    case "api profile": fixture = "good_profile.json"
    case "api driver-mode": fixture = "good_driver_mode.json"
    case "api loopback get": fixture = "good_loopback.json"
    case "usb-quality --json --interval-ms 1000 --count 2":
        fixture = "good_quality.ndjson"
    case "--json": fixture = "good_profiler.json"
    default:
        FileHandle.standardError.write(Data("unexpected helper operation\n".utf8))
        exit(4)
    }
    do {
        let data = try Data(contentsOf: directory.appendingPathComponent(fixture))
        FileHandle.standardOutput.write(data)
        exit(0)
    } catch {
        FileHandle.standardError.write(Data("fixture unavailable\n".utf8))
        exit(4)
    }
}

private func prepareHelper(
    named name: String,
    fixtures: URL,
    versionCapabilities: [String]? = nil
) throws -> (directory: URL, executable: URL) {
    let directory = FileManager.default.temporaryDirectory
        .appendingPathComponent("opena8dj-ui-test-\(UUID().uuidString)")
    try FileManager.default.createDirectory(
        at: directory, withIntermediateDirectories: false
    )
    let executable = directory.appendingPathComponent(name)
    try FileManager.default.copyItem(
        at: URL(fileURLWithPath: CommandLine.arguments[0]),
        to: executable
    )
    try FileManager.default.setAttributes(
        [.posixPermissions: 0o755], ofItemAtPath: executable.path
    )
    for fixture in [
        "good_profiles.json", "good_modes.json", "good_stats.json",
        "good_profile.json", "good_driver_mode.json", "good_loopback.json",
        "good_quality.ndjson", "good_profiler.json"
    ] {
        try FileManager.default.copyItem(
            at: fixtures.appendingPathComponent(fixture),
            to: directory.appendingPathComponent(fixture)
        )
    }
    let sourceVersion = try Data(
        contentsOf: fixtures.appendingPathComponent("good_version.json")
    )
    if let versionCapabilities {
        var root = try JSONSerialization.jsonObject(with: sourceVersion) as! [String: Any]
        var data = root["data"] as! [String: Any]
        data["capabilities"] = versionCapabilities
        root["data"] = data
        var encoded = try JSONSerialization.data(
            withJSONObject: root, options: [.sortedKeys]
        )
        encoded.append(0x0a)
        try encoded.write(to: directory.appendingPathComponent("version.json"))
    } else {
        try sourceVersion.write(to: directory.appendingPathComponent("version.json"))
    }
    FileManager.default.createFile(
        atPath: directory.appendingPathComponent("operations.log").path,
        contents: Data()
    )
    return (directory, executable)
}

@main
struct ControlCenterUIConstruction {
    @MainActor
    static func main() async {
        if helperMode() { return }
        guard CommandLine.arguments.count == 2 else {
            fatalError("fixture directory required")
        }
        let fixtures = URL(fileURLWithPath: CommandLine.arguments[1])
        do {
            let cancelHelper = try prepareHelper(
                named: "cc-store-cancel-helper", fixtures: fixtures
            )
            defer { try? FileManager.default.removeItem(at: cancelHelper.directory) }
            let cancelRunner = BoundedProcessRunner(resolver: { _ in
                cancelHelper.executable
            })
            let cancelStore = ControlCenterStore(runner: cancelRunner)
            let refresh = Task { await cancelStore.runRefreshCycleForTesting() }
            try await Task.sleep(for: .milliseconds(150))
            refresh.cancel()
            await refresh.value
            let cancelStats = await cancelStore.processStatisticsForTesting()
            let cancelLog = try String(
                contentsOf: cancelHelper.directory.appendingPathComponent("operations.log"),
                encoding: .utf8
            )
            guard cancelStats.launches <= 2,
                  !cancelLog.contains("api driver-modes") else {
                fatalError("bootstrap launched after foreground cancellation")
            }

            let capHelper = try prepareHelper(
                named: "cc-store-cap-helper",
                fixtures: fixtures,
                versionCapabilities: ["stats.read"]
            )
            defer { try? FileManager.default.removeItem(at: capHelper.directory) }
            let capRunner = BoundedProcessRunner(resolver: { _ in capHelper.executable })
            let capStore = ControlCenterStore(runner: capRunner)
            await capStore.runRefreshCycleForTesting()
            let capLog = try String(
                contentsOf: capHelper.directory.appendingPathComponent("operations.log"),
                encoding: .utf8
            )
            for prohibited in [
                "api profiles", "api driver-modes", "usb-quality",
                "api profile", "api driver-mode", "api loopback"
            ] where capLog.contains(prohibited) {
                fatalError("unsupported read was invoked: \(prohibited)")
            }
            guard capStore.phase == .partial else {
                fatalError("unsupported capabilities did not yield partial phase")
            }

            let store = ControlCenterStore()
            try store.loadOfflineFixtures(from: fixtures)
            _ = ControlCenterRootView(store: store)
            _ = DashboardHeader(store: store)
            _ = OverviewView(store: store)
            _ = USBQualityView(store: store)
            _ = DriverModesView(store: store)
            _ = LoopbackView(store: store)
            _ = DiagnosticsView(store: store)
            print("control center UI construction and store lifecycle: PASS")
        } catch {
            fatalError("control center UI test failed: \(error)")
        }
    }
}
