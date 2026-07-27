import AppKit
import SwiftUI

struct ControlCenterRootView: View {
    @ObservedObject var store: ControlCenterStore
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        NavigationSplitView {
            List(DashboardSection.allCases, selection: $store.selectedSection) { section in
                Label(section.rawValue, systemImage: symbol(for: section))
                    .tag(section)
            }
            .navigationTitle("Control Center")
            .frame(minWidth: 190)
        } detail: {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    DashboardHeader(store: store)
                    actionBanner
                    sectionView
                }
                .padding(20)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .background(Color(nsColor: .windowBackgroundColor))
            .overlay(WindowVisibilityProbe { store.setWindowVisible($0) }.frame(width: 0, height: 0))
            .navigationTitle(store.selectedSection?.rawValue ?? "Dashboard paused")
            .toolbar {
                Button {
                    store.manualRefresh()
                } label: {
                    Label("Refresh", systemImage: "arrow.clockwise")
                }
                .keyboardShortcut("r", modifiers: .command)
                .disabled(store.isBusy)
                .help("Refresh all evidence, including the profiler")
            }
        }
        .frame(minWidth: 800, minHeight: 580)
        .onAppear { store.setSceneActive(scenePhase == .active) }
        .onChange(of: scenePhase) { store.setSceneActive($0 == .active) }
        .onChange(of: store.selectedSection) { _ in
            store.setWindowVisible(NSApp.keyWindow?.isVisible == true)
        }
        .alert(item: $store.pendingConfirmation) { confirmation in
            Alert(
                title: Text(confirmation.title),
                message: Text(confirmation.message),
                primaryButton: .default(Text("Confirm")) { store.confirmPendingAction() },
                secondaryButton: .cancel { store.cancelPendingAction() }
            )
        }
    }

    @ViewBuilder private var sectionView: some View {
        switch store.selectedSection ?? .overview {
        case .overview: OverviewView(store: store)
        case .usbQuality: USBQualityView(store: store)
        case .driverModes: DriverModesView(store: store)
        case .loopback: LoopbackView(store: store)
        case .diagnostics: DiagnosticsView(store: store)
        }
    }

    @ViewBuilder private var actionBanner: some View {
        switch store.actionOutcome {
        case .idle:
            EmptyView()
        case .applied(let message):
            BannerView(symbol: "checkmark.circle", title: "Applied", message: message)
        case .pending(let message):
            BannerView(symbol: "clock", title: "Pending", message: message)
        case .rolledBack(let error):
            BannerView(symbol: "arrow.uturn.backward.circle", title: "Rolled back", message: error.description)
        case .rollbackFailed(let error):
            BannerView(symbol: "exclamationmark.octagon", title: "Rollback failed", message: error.description)
        case .rollbackUnavailable(let error):
            BannerView(symbol: "questionmark.diamond", title: "Rollback unavailable", message: error.description)
        case .indeterminate(let error):
            BannerView(symbol: "exclamationmark.triangle", title: "Action failed / indeterminate", message: error.description)
        }
    }

    private func symbol(for section: DashboardSection) -> String {
        switch section {
        case .overview: return "rectangle.grid.2x2"
        case .usbQuality: return "waveform.path.ecg"
        case .driverModes: return "speedometer"
        case .loopback: return "arrow.triangle.2.circlepath"
        case .diagnostics: return "stethoscope"
        }
    }
}

struct DashboardHeader: View {
    @ObservedObject var store: ControlCenterStore

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 3) {
                Text("OpenA8DJ Control Center").font(.title2.weight(.semibold))
                Text(phaseLabel).font(.subheadline).foregroundStyle(.secondary)
            }
            Spacer()
            if store.isBusy {
                ProgressView().controlSize(.small)
                Text("Action in progress").font(.caption)
            }
            Label(apiLabel, systemImage: phaseSymbol)
                .font(.caption.weight(.medium))
                .padding(.horizontal, 9)
                .padding(.vertical, 5)
                .background(.quaternary, in: Capsule())
                .accessibilityLabel("Backend status")
                .accessibilityValue("\(phaseLabel), \(apiLabel)")
        }
    }

    private var apiLabel: String {
        switch store.backend {
        case .known(let backend):
            return "API \(backend.apiVersion.description)" +
                (backend.newerMinor ? " · newer backend — partially verified" : "")
        case .unavailable(let reason): return reason.description
        }
    }

    private var phaseLabel: String {
        switch store.phase {
        case .starting: return "Starting — no validated snapshot yet"
        case .online: return "Online — compatible current evidence"
        case .partial: return "Partial — some evidence is unavailable or unverified"
        case .offline: return "Offline — retrying with bounded backoff"
        case .permissionDenied: return "Permission denied — automatic retries are limited"
        case .mismatch: return "Protocol/backend mismatch"
        case .stale(_, let reason): return "Stale — \(reason)"
        }
    }

    private var phaseSymbol: String {
        switch store.phase {
        case .online: return "checkmark.circle"
        case .partial, .stale: return "exclamationmark.circle"
        case .starting: return "hourglass"
        case .offline: return "bolt.slash"
        case .permissionDenied: return "lock.trianglebadge.exclamationmark"
        case .mismatch: return "arrow.triangle.branch"
        }
    }
}

struct OverviewView: View {
    @ObservedObject var store: ControlCenterStore

    var body: some View {
        LazyVGrid(columns: [GridItem(.adaptive(minimum: 340), spacing: 14)], spacing: 14) {
            DashboardCard(title: "Device, firmware, and power", symbol: "externaldrive.connected.to.line.below") {
                switch store.profiler {
                case .known(let report):
                    MetricRow(label: "Overall", value: report.overallStatus, detail: store.ageLabel(for: "profiler"))
                    ForEach(requiredChecks(report)) { check in
                        MetricRow(
                            label: check.id,
                            value: check.status,
                            detail: "\(check.code) · \(check.summary)" +
                                evidenceDetail(check) +
                                (check.remediations.isEmpty ? "" : " · \(check.remediations.joined(separator: " · "))")
                        )
                    }
                case .unavailable(let reason):
                    UnknownView(reason: reason)
                }
            }

            DashboardCard(title: "Electrical profile", symbol: "slider.horizontal.3") {
                switch store.profile {
                case .known(let value):
                    MetricRow(label: "Active profile", value: value.activeProfile, detail: store.ageLabel(for: "profile"))
                    MetricRow(label: "Input mode", value: value.inputMode, detail: "decode \(yesNo(value.inputDecode)); software lock \(yesNo(value.softwareLock))")
                    MetricRow(label: "Ground lifts", value: "Vinyl \(yesNo(value.groundLiftVinyl)) · CD/Line \(yesNo(value.groundLiftCDLine)) · Phono \(yesNo(value.groundLiftPhono))")
                    Text("Sources: \(mapLabel(value.inputSources))")
                    Text("Transforms: \(mapLabel(value.inputTransforms))")
                    activeProfileGuidance(value.activeProfile)
                    profileMenu
                case .unavailable(let reason): UnknownView(reason: reason)
                }
            }

            DashboardCard(title: "Stream", symbol: "waveform") {
                switch store.stream {
                case .known(let value):
                    MetricRow(label: "Streaming", value: yesNo(value.streaming), detail: store.ageLabel(for: "stream"))
                    MetricRow(label: "Sample rate", value: value.streaming ? "\(value.sampleRateHz.formatted()) Hz" : "not streaming")
                    MetricRow(label: "Output ring", value: "\(value.outputRingFrames) / \(value.outputTargetLatencyFrames) frames")
                case .unavailable(let reason): UnknownView(reason: reason)
                }
            }
        }
    }

    private var profileMenu: some View {
        Group {
            if case .known(let choices) = store.profiles {
                Menu("Apply canonical profile…") {
                    ForEach(choices) { choice in
                        Button(choice.title) { store.requestProfile(choice.id) }
                            .disabled(!choice.canonical || store.isBusy)
                    }
                }
                .disabled(store.isBusy || !canWriteProfile)
                Text("Unknown additive profiles are displayed but cannot be applied by this panel build.")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder private func activeProfileGuidance(_ id: String) -> some View {
        if case .known(let choices) = store.profiles,
           let choice = choices.first(where: { $0.id == id }) {
            Text("\(choice.surface) · \(choice.summary)")
                .font(.caption).foregroundStyle(.secondary)
            Text(cablingGuidance(id))
                .font(.caption).foregroundStyle(.secondary)
        }
    }

    private var canWriteProfile: Bool {
        if case .known(let backend) = store.backend {
            return backend.capabilities.contains("profile.write")
        }
        return false
    }

    private func requiredChecks(_ report: ProfilerSnapshot) -> [ProfilerCheck] {
        let order = [
            "usb.identity", "usb.enumeration", "usb.link-speed", "usb.power",
            "device.firmware", "coreaudio.device", "driver.api-pairing", "usb.stream-quality"
        ]
        return order.compactMap { id in report.checks.first { $0.id == id } }
    }

    private func evidenceDetail(_ check: ProfilerCheck) -> String {
        guard !check.evidence.isEmpty else { return "" }
        return " · " + check.evidence.map {
            if $0.available {
                return "\($0.key)=\($0.value ?? "available")"
            }
            return "\($0.key)=UNKNOWN(\($0.reasonCode ?? "unavailable"))"
        }.joined(separator: ", ")
    }

    private func cablingGuidance(_ id: String) -> String {
        switch id {
        case "traktor-dvs-vinyl", "vinyl-recording":
            return "Use A/B for phono cartridges; C/D are line-level inputs."
        case "traktor-dvs-cd-line":
            return "Use A/B for timecode CD or line players."
        case "dj-set-recording":
            return "Feed a mixer recording or booth output to C/D."
        case "effects-loop":
            return "Start sends low and check for feedback."
        case "microphone":
            return "Use the front XLR path; phantom power is unavailable."
        default:
            return "Verify cabling before applying an electrical profile."
        }
    }
}

struct USBQualityView: View {
    @ObservedObject var store: ControlCenterStore

    var body: some View {
        DashboardCard(title: "USB quality window", symbol: "cable.connector") {
            switch store.quality {
            case .known(let value):
                MetricRow(label: "Stability", value: truthfulClassification(value), detail: store.ageLabel(for: "quality"))
                if value.reasons.isEmpty {
                    Text("Backend reasons: none")
                } else {
                    Text("Backend reasons: \(value.reasons.joined(separator: ", "))")
                }
                Divider()
                MetricRow(label: "Capture jitter", value: "p95 \(value.captureJitter.p95.label) · p99 \(value.captureJitter.p99.label)", detail: "\(value.captureJitter.samples) samples")
                MetricRow(label: "Playback jitter", value: "p95 \(value.playbackJitter.p95.label) · p99 \(value.playbackJitter.p99.label)", detail: "\(value.playbackJitter.samples) samples")
                MetricRow(label: "Isochronous errors", value: "capture \(value.isoErrors.captureTotal) · playback \(value.isoErrors.playbackTotal)")
                MetricRow(label: "Hard xruns", value: "\(value.xruns.totalHardXruns)", detail: "active underruns \(value.xruns.activeUnderruns); ring overruns \(value.xruns.ringOverruns)")
                MetricRow(label: "Hard-xrun delta", value: DashboardReducer.deltaLabel(store.qualityXrunDelta))
                MetricRow(label: "Late writes", value: "\(value.xruns.lateWriteBatches) batches · \(value.xruns.lateWriteFrames) frames")
                MetricRow(label: "Sampling context", value: "\(value.windowMilliseconds) ms", detail: "streaming \(yesNo(value.streaming)); \(value.sampleRateHz.formatted()) Hz; instrumentation \(yesNo(value.instrumentationAvailable))")
                DisclosureGroup("Isochronous component deltas") {
                    ForEach(value.isoErrors.components.keys.sorted(), id: \.self) { key in
                        MetricRow(label: key, value: "\(value.isoErrors.components[key] ?? 0)")
                    }
                }
            case .unavailable(let reason): UnknownView(reason: reason)
            }
        }
    }

    private func truthfulClassification(_ value: USBQualitySnapshot) -> String {
        if value.classification == "stable" && !value.instrumentationAvailable {
            return "backend mismatch"
        }
        return value.classification
    }
}

struct DriverModesView: View {
    @ObservedObject var store: ControlCenterStore

    var body: some View {
        VStack(spacing: 14) {
            DashboardCard(title: "Driver mode", symbol: "speedometer") {
                switch store.driverMode {
                case .known(let mode):
                    MetricRow(label: "Requested", value: mode.requestedMode)
                    MetricRow(label: "Effective", value: mode.effectiveMode)
                    MetricRow(label: "Pending", value: yesNo(mode.pending), detail: store.ageLabel(for: "driverMode"))
                    MetricRow(label: "Streaming", value: yesNo(mode.streaming))
                    DisclosureGroup("Policy and outcome") {
                        MetricRow(label: "Last result", value: mode.lastResult)
                        MetricRow(label: "Rejection reason", value: mode.rejectionReason)
                        MetricRow(label: "Target latency", value: "\(mode.effectivePolicy.outputTargetLatencyFrames) frames")
                        MetricRow(label: "Worker QoS", value: mode.effectivePolicy.workerQoS)
                        MetricRow(label: "Generation", value: "\(mode.generation)")
                    }
                    HStack {
                        Button("Balanced") { store.requestDriverMode(.balanced) }
                        Button("Performance") { store.requestDriverMode(.performance) }
                        Button("Vintage Compatible") { store.requestDriverMode(.vintageCompatible) }
                    }
                    .disabled(store.isBusy || !canWriteMode)
                case .unavailable(let reason): UnknownView(reason: reason)
                }
            }
            timecodeCard
            vintageCard
        }
    }

    private var timecodeCard: some View {
        DashboardCard(title: "Timecode Optimized", symbol: "record.circle") {
            if case .known(let mode) = store.driverMode {
                switch mode.timecode {
                case .known(let value):
                    MetricRow(label: "Armed", value: yesNo(value.armed), detail: "Armed is never the same as active")
                    MetricRow(label: "Active", value: yesNo(value.optimizedActive))
                    MetricRow(label: "Arm state", value: value.armState)
                    MetricRow(label: "Wait / fail-open reason", value: DashboardReducer.timecodeWaitReason(value, pending: mode.pending))
                    MetricRow(label: "Profile", value: value.electricalProfile, detail: "verified \(yesNo(value.profileVerified))")
                    MetricRow(label: "Evidence", value: value.evidenceKind, detail: "fresh \(yesNo(value.windowFresh)); qualified \(yesNo(value.qualified))")
                    MetricRow(label: "Eligible windows", value: "\(value.eligibleWindows) / \(value.requiredEligibleWindows)", detail: "dropouts \(value.dropoutWindows)")
                    MetricRow(label: "Input lead", value: "\(value.inputLeadFrames) / \(value.inputLeadCeilingFrames) frames")
                    ForEach(["A", "B", "C", "D"], id: \.self) { pair in
                        if let evidence = value.pairWindows[pair] {
                            switch evidence {
                            case .known(let window):
                                MetricRow(label: "Pair \(pair)", value: window.active ? "active" : "inactive", detail: "RMS \(window.leftACRMS.formatted()) / \(window.rightACRMS.formatted())")
                            case .unavailable(let reason):
                                MetricRow(label: "Pair \(pair)", value: reason.description)
                            }
                        }
                    }
                    DisclosureGroup("Timecode counters") {
                        ForEach(value.counters.keys.sorted(), id: \.self) { key in
                            MetricRow(label: key, value: "\(value.counters[key] ?? 0)")
                        }
                    }
                    Button(value.armed ? "Disarm" : "Arm for A,B") {
                        store.requestTimecode(armed: !value.armed)
                    }
                    .disabled(store.isBusy || !canArmTimecode)
                case .unavailable(let reason): UnknownView(reason: reason)
                }
            } else {
                UnknownView(reason: .notYetObserved)
            }
        }
    }

    private var vintageCard: some View {
        DashboardCard(title: "Vintage Compatible", symbol: "exclamationmark.shield") {
            Text("Experimental — Unverified").font(.headline)
            if case .known(let mode) = store.driverMode {
                switch mode.vintage {
                case .known(let value):
                    MetricRow(label: "Status", value: value.status)
                    MetricRow(label: "Claim", value: value.claim)
                    Text("Reasons: \(value.reasons.isEmpty ? "none reported" : value.reasons.joined(separator: ", "))")
                    DisclosureGroup("Preflight and capabilities") {
                        ForEach(value.capabilities.keys.sorted(), id: \.self) { key in
                            MetricRow(label: key, value: yesNo(value.capabilities[key] == true))
                        }
                        ForEach(value.preflight.keys.sorted(), id: \.self) { key in
                            MetricRow(label: key, value: value.preflight[key] ?? "UNKNOWN")
                        }
                    }
                case .unavailable(let reason): UnknownView(reason: reason)
                }
            }
        }
    }

    private var canWriteMode: Bool {
        if case .known(let backend) = store.backend {
            return backend.capabilities.contains("driver-mode.write")
        }
        return false
    }

    private var canArmTimecode: Bool {
        if case .known(let backend) = store.backend {
            return backend.capabilities.contains("timecode-optimized.arm")
        }
        return false
    }
}

struct LoopbackView: View {
    @ObservedObject var store: ControlCenterStore

    var body: some View {
        DashboardCard(title: "Virtual loopback", symbol: "arrow.triangle.2.circlepath") {
            Text("Disabled by default. Enabling exposes one physical output pair as an application-readable virtual input for this session.")
                .foregroundStyle(.secondary)
            switch store.loopback {
            case .known(let value):
                MetricRow(label: "Enabled", value: yesNo(value.enabled), detail: store.ageLabel(for: "loopback"))
                MetricRow(label: "Source pair", value: value.sourcePair)
                MetricRow(label: "Session only", value: yesNo(value.sessionOnly))
                MetricRow(label: "Physical playback publishing", value: yesNo(value.physicalPlaybackPublishing))
                MetricRow(label: "Readers", value: "\(value.registeredReaderCount)", detail: "generation \(value.generation)")
                MetricRow(label: "Frames", value: "published \(value.sourceFramesPublished) · delivered \(value.framesDelivered)", detail: "silence \(value.silenceFrames)")
                MetricRow(label: "Gaps / overruns", value: "\(value.gapFrames) gaps · \(value.overrunEvents) events", detail: "\(value.overrunFrames) overrun frames")
                MetricRow(
                    label: "Gap / overrun deltas",
                    value: "\(DashboardReducer.deltaLabel(store.loopbackDeltas.gaps)) gaps · \(DashboardReducer.deltaLabel(store.loopbackDeltas.overrunEvents)) events",
                    detail: "\(DashboardReducer.deltaLabel(store.loopbackDeltas.overrunFrames)) frames"
                )
                Picker("Output pair", selection: $store.selectedLoopbackPair) {
                    ForEach(OutputPair.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                HStack {
                    Button(value.enabled && value.sourcePair == store.selectedLoopbackPair.rawValue ? "Enabled" : "Enable selected pair") {
                        store.requestLoopbackEnable(store.selectedLoopbackPair)
                    }
                    .disabled(store.isBusy || !canWrite || (value.enabled && value.sourcePair == store.selectedLoopbackPair.rawValue))
                    Button("Disable") { store.requestLoopbackDisable() }
                        .disabled(store.isBusy || !value.enabled)
                }
            case .unavailable(let reason): UnknownView(reason: reason)
            }
        }
    }

    private var canWrite: Bool {
        if case .known(let backend) = store.backend {
            return backend.capabilities.contains("loopback.write")
        }
        return false
    }
}

struct DiagnosticsView: View {
    @ObservedObject var store: ControlCenterStore

    var body: some View {
        VStack(spacing: 14) {
            DashboardCard(title: "Backend consistency", symbol: "arrow.triangle.branch") {
                if store.mismatchReasons.isEmpty {
                    Text("No disagreement in the currently validated adjacent snapshots.")
                } else {
                    ForEach(store.mismatchReasons, id: \.self) {
                        Label($0, systemImage: "exclamationmark.triangle")
                    }
                }
            }
            DashboardCard(title: "Source errors", symbol: "exclamationmark.bubble") {
                if store.sourceErrors.isEmpty {
                    Text("No current source-specific error.")
                } else {
                    ForEach(store.sourceErrors.keys.sorted(), id: \.self) { source in
                        if let error = store.sourceErrors[source] {
                            MetricRow(
                                label: source,
                                value: error.code,
                                detail: "\(error.message) · retryable \(yesNo(error.retryable)) · phase \(error.phase)"
                            )
                        }
                    }
                }
                Text("Raw JSON, paths, environment values, and arbitrary stderr are intentionally omitted.")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

struct DashboardCard<Content: View>: View {
    let title: String
    let symbol: String
    @ViewBuilder let content: Content

    init(title: String, symbol: String, @ViewBuilder content: () -> Content) {
        self.title = title
        self.symbol = symbol
        self.content = content()
    }

    var body: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 9) { content }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.vertical, 3)
        } label: {
            Label(title, systemImage: symbol).font(.headline)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel(title)
    }
}

struct MetricRow: View {
    let label: String
    let value: String
    var detail: String = ""

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(label).foregroundStyle(.secondary)
            Spacer(minLength: 12)
            VStack(alignment: .trailing, spacing: 1) {
                Text(value).font(.body.monospacedDigit()).multilineTextAlignment(.trailing)
                if !detail.isEmpty {
                    Text(detail).font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.trailing)
                }
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(label)
        .accessibilityValue(detail.isEmpty ? value : "\(value), \(detail)")
    }
}

struct UnknownView: View {
    let reason: EvidenceReason
    var body: some View {
        Label(reason.description, systemImage: "questionmark.circle")
            .accessibilityLabel("Evidence unavailable")
            .accessibilityValue(reason.description)
    }
}

struct BannerView: View {
    let symbol: String
    let title: String
    let message: String

    var body: some View {
        Label {
            VStack(alignment: .leading) {
                Text(title).font(.headline)
                Text(message).font(.caption)
            }
        } icon: {
            Image(systemName: symbol)
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.quaternary, in: RoundedRectangle(cornerRadius: 8))
        .accessibilityElement(children: .combine)
    }
}

private struct WindowVisibilityProbe: NSViewRepresentable {
    let changed: (Bool) -> Void

    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        DispatchQueue.main.async { context.coordinator.attach(to: view.window) }
        return view
    }

    func updateNSView(_ nsView: NSView, context: Context) {
        DispatchQueue.main.async { context.coordinator.attach(to: nsView.window) }
    }

    func makeCoordinator() -> Coordinator { Coordinator(changed: changed) }

    final class Coordinator {
        let changed: (Bool) -> Void
        weak var window: NSWindow?
        var tokens: [NSObjectProtocol] = []

        init(changed: @escaping (Bool) -> Void) { self.changed = changed }
        deinit { tokens.forEach(NotificationCenter.default.removeObserver) }

        func attach(to window: NSWindow?) {
            guard self.window !== window else {
                report()
                return
            }
            tokens.forEach(NotificationCenter.default.removeObserver)
            tokens.removeAll()
            self.window = window
            guard let window else { changed(false); return }
            let names: [Notification.Name] = [
                NSWindow.didBecomeKeyNotification,
                NSWindow.didResignKeyNotification,
                NSWindow.didMiniaturizeNotification,
                NSWindow.didDeminiaturizeNotification,
                NSWindow.willCloseNotification
            ]
            tokens = names.map { name in
                if name == NSWindow.willCloseNotification {
                    return NotificationCenter.default.addObserver(
                        forName: name, object: window, queue: .main
                    ) { [weak self] _ in self?.changed(false) }
                }
                return NotificationCenter.default.addObserver(
                    forName: name, object: window, queue: .main
                ) { [weak self] _ in self?.report() }
            }
            report()
        }

        func report() {
            guard let window else { changed(false); return }
            changed(window.isVisible && !window.isMiniaturized && NSApp.isActive)
        }
    }
}

func yesNo(_ value: Bool) -> String { value ? "yes" : "no" }
private func mapLabel(_ value: [String: String]) -> String {
    value.keys.sorted().map { "\($0)=\(value[$0] ?? "UNKNOWN")" }.joined(separator: ", ")
}
