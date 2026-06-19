import AppKit
import SwiftUI

struct Preset: Identifiable, Hashable {
    let id: String
    let title: String
    let surface: String
    let summary: String
    let cabling: String
}

struct ControlConfig: Codable {
    var schema: String?
    var preset: String?
    var inputMode: String?
    var inputModeValue: Int?
    var inputDecode: Bool?
    var softwareLock: Bool?
    var groundLiftVinyl: Bool?
    var groundLiftCDLine: Bool?
    var groundLiftPhono: Bool?
    var inputSourceA: String?
    var inputSourceB: String?
    var inputSourceC: String?
    var inputSourceD: String?
    var inputTransformA: String?
    var inputTransformB: String?
    var inputTransformC: String?
    var inputTransformD: String?
}

struct CommandResult {
    let status: Int32
    let output: String
}

enum ControlCommand {
    static func toolURL() -> URL {
        if let resource = Bundle.main.url(forResource: "opena8dj-control", withExtension: nil) {
            return resource
        }
        let dev = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
            .appendingPathComponent("build/opena8dj-control")
        if FileManager.default.isExecutableFile(atPath: dev.path) {
            return dev
        }
        return URL(fileURLWithPath: "/usr/local/bin/opena8dj-control")
    }

    static func run(_ args: [String], noWake: Bool = false) throws -> CommandResult {
        let process = Process()
        process.executableURL = toolURL()
        process.arguments = args
        var environment = ProcessInfo.processInfo.environment
        if noWake {
            environment["OPENA8DJ_CONTROL_NO_WAKE"] = "1"
        }
        process.environment = environment

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        try process.run()
        process.waitUntilExit()

        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        let output = String(data: data, encoding: .utf8) ?? ""
        return CommandResult(status: process.terminationStatus, output: output)
    }
}

@MainActor
final class ControlModel: ObservableObject {
    @Published var config: ControlConfig?
    @Published var selectedPreset: Preset
    @Published var statusText = "Not connected"
    @Published var detailText = ""
    @Published var isBusy = false
    @Published var lastError: String?

    let presets: [Preset] = [
        Preset(
            id: "traktor-dvs-vinyl",
            title: "DVS Vinyl",
            surface: "Traktor A/B",
            summary: "Default state: A/B timecode vinyl, input decode on, software lock.",
            cabling: "Turntables on A/B. The driver keeps vinyl input ready without Terminal commands."
        ),
        Preset(
            id: "playback-4out",
            title: "Output Only",
            surface: "4 stereo outputs",
            summary: "Advanced: A/B/C/D outputs with input decode off.",
            cabling: "Use only when you deliberately do not need vinyl or input capture."
        ),
        Preset(
            id: "traktor-dvs-cd-line",
            title: "DVS CD-Line",
            surface: "CDJ / line",
            summary: "A/B timecode CD or line players, CD-line ground lift.",
            cabling: "Use A/B for timecode CD or line-level players."
        ),
        Preset(
            id: "vinyl-recording",
            title: "Vinyl Recording",
            surface: "A/B phono",
            summary: "A/B phono capture with phono ground lift.",
            cabling: "Use only A/B for phono cartridges. The Audio 8 DJ has no phono on C/D."
        ),
        Preset(
            id: "dj-set-recording",
            title: "DJ Set Recording",
            surface: "C/D line input",
            summary: "C/D line capture while preserving A/B mode.",
            cabling: "Feed mixer REC OUT, BOOTH OUT, or second master to C/D line input."
        ),
        Preset(
            id: "effects-loop",
            title: "Effects Loop",
            surface: "C/D duplex",
            summary: "C/D input and output for external or software effects.",
            cabling: "Watch for feedback. Start with mixer sends low."
        ),
        Preset(
            id: "microphone",
            title: "Microphone",
            surface: "XLR mic",
            summary: "Front XLR mic path with input decode on.",
            cabling: "Set physical MIC/LINE to MIC. No phantom power is available."
        ),
        Preset(
            id: "midi-only",
            title: "MIDI Only",
            surface: "DIN MIDI",
            summary: "Playback-safe state for using the MIDI bridge.",
            cabling: "Connect controller MIDI OUT to Audio 8 MIDI IN and vice versa."
        ),
        Preset(
            id: "ground-diagnostics",
            title: "Ground Diagnostics",
            surface: "noise lab",
            summary: "Input/noise measurement state with software lock.",
            cabling: "Compare mixer ground, Audio 8 ground, and lift states one at a time."
        ),
        Preset(
            id: "engineering-diagnostics",
            title: "Engineering",
            surface: "full matrix",
            summary: "Input/output diagnostics with restorable state.",
            cabling: "Use only with known cabling and saved before/after config."
        )
    ]

    init() {
        selectedPreset = presets[0]
    }

    func refresh() {
        runBusy {
            let result = try ControlCommand.run(["export-config", "-"], noWake: true)
            if result.status != 0 {
                self.config = nil
                self.statusText = "Driver bridge unavailable"
                self.detailText = result.output.trimmingCharacters(in: .whitespacesAndNewlines)
                return
            }
            let data = Data(result.output.utf8)
            self.config = try JSONDecoder().decode(ControlConfig.self, from: data)
            self.statusText = "Connected"
            self.detailText = result.output
            if let presetID = self.config?.preset,
               let preset = self.presets.first(where: { $0.id == presetID }) {
                self.selectedPreset = preset
            }
        }
    }

    func applySelectedPreset() {
        runBusy {
            let result = try ControlCommand.run(["apply-preset", self.selectedPreset.id])
            if result.status != 0 {
                throw NSError(domain: "OpenA8DJ", code: Int(result.status), userInfo: [
                    NSLocalizedDescriptionKey: result.output
                ])
            }
            self.refresh()
        }
    }

    func exportConfig() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.json]
        panel.nameFieldStringValue = "opena8dj-config.json"
        guard panel.runModal() == .OK, let url = panel.url else {
            return
        }
        runBusy {
            let result = try ControlCommand.run(["export-config", url.path])
            if result.status != 0 {
                throw NSError(domain: "OpenA8DJ", code: Int(result.status), userInfo: [
                    NSLocalizedDescriptionKey: result.output
                ])
            }
            self.refresh()
        }
    }

    func importConfig() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.json]
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let url = panel.url else {
            return
        }
        runBusy {
            let result = try ControlCommand.run(["import-config", url.path])
            if result.status != 0 {
                throw NSError(domain: "OpenA8DJ", code: Int(result.status), userInfo: [
                    NSLocalizedDescriptionKey: result.output
                ])
            }
            self.refresh()
        }
    }

    private func runBusy(_ work: () throws -> Void) {
        isBusy = true
        lastError = nil
        defer { isBusy = false }
        do {
            try work()
        } catch {
            lastError = error.localizedDescription
            statusText = "Action failed"
        }
    }
}

struct ContentView: View {
    @StateObject private var model = ControlModel()

    var body: some View {
        NavigationSplitView {
            List(model.presets, selection: $model.selectedPreset) { preset in
                VStack(alignment: .leading, spacing: 4) {
                    Text(preset.title)
                        .font(.headline)
                    Text(preset.surface)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(.vertical, 4)
                .tag(preset)
            }
            .navigationTitle("Presets")
            .frame(minWidth: 220)
        } detail: {
            VStack(alignment: .leading, spacing: 18) {
                header
                Divider()
                presetPanel
                statePanel
                diagnosticsPanel
                Spacer()
            }
            .padding(24)
            .frame(minWidth: 660, minHeight: 520)
        }
        .onAppear {
            model.refresh()
        }
    }

    private var header: some View {
        HStack(alignment: .center) {
            VStack(alignment: .leading, spacing: 6) {
                Text("OpenA8DJ Control Center")
                    .font(.system(size: 26, weight: .semibold))
                Text(model.statusText)
                    .foregroundStyle(model.statusText == "Connected" ? .green : .secondary)
            }
            Spacer()
            Button("Refresh") {
                model.refresh()
            }
            .keyboardShortcut("r")
            .disabled(model.isBusy)
        }
    }

    private var presetPanel: some View {
        GroupBox("Selected Configuration") {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(model.selectedPreset.title)
                            .font(.title2.weight(.semibold))
                        Text(model.selectedPreset.summary)
                            .foregroundStyle(.secondary)
                    }
                    Spacer()
                    Button("Apply") {
                        model.applySelectedPreset()
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(model.isBusy)
                }
                Text(model.selectedPreset.cabling)
                    .font(.callout)
            }
            .padding(8)
        }
    }

    private var statePanel: some View {
        GroupBox("Hardware State") {
            Grid(alignment: .leading, horizontalSpacing: 24, verticalSpacing: 10) {
                stateRow("Preset", model.config?.preset ?? "unknown")
                stateRow("Input Mode", model.config?.inputMode ?? "unknown")
                stateRow("Input Decode", boolText(model.config?.inputDecode))
                stateRow("Software Lock", boolText(model.config?.softwareLock))
                stateRow("Ground Vinyl", boolText(model.config?.groundLiftVinyl))
                stateRow("Ground CD-Line", boolText(model.config?.groundLiftCDLine))
                stateRow("Ground Phono", boolText(model.config?.groundLiftPhono))
                stateRow("Input Sources", sourceSummary)
                stateRow("Input Transforms", transformSummary)
            }
            .padding(8)
        }
    }

    private var diagnosticsPanel: some View {
        GroupBox("Configuration Files") {
            HStack {
                Button("Export") {
                    model.exportConfig()
                }
                Button("Import") {
                    model.importConfig()
                }
                Spacer()
                if let error = model.lastError {
                    Text(error)
                        .foregroundStyle(.red)
                        .lineLimit(2)
                }
            }
            .padding(8)
        }
    }

    private var sourceSummary: String {
        guard let config = model.config else { return "unknown" }
        return "A=\(config.inputSourceA ?? "?") B=\(config.inputSourceB ?? "?") C=\(config.inputSourceC ?? "?") D=\(config.inputSourceD ?? "?")"
    }

    private var transformSummary: String {
        guard let config = model.config else { return "unknown" }
        return "A=\(config.inputTransformA ?? "?") B=\(config.inputTransformB ?? "?") C=\(config.inputTransformC ?? "?") D=\(config.inputTransformD ?? "?")"
    }

    private func stateRow(_ key: String, _ value: String) -> some View {
        GridRow {
            Text(key)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.system(.body, design: .monospaced))
        }
    }

    private func boolText(_ value: Bool?) -> String {
        guard let value else { return "unknown" }
        return value ? "on" : "off"
    }
}

@main
struct OpenA8DJControlCenterApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .windowStyle(.titleBar)
    }
}
