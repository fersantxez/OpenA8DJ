import SwiftUI

@main
struct ControlCenterUIConstruction {
    @MainActor
    static func main() {
        guard CommandLine.arguments.count == 2 else {
            fatalError("fixture directory required")
        }
        let store = ControlCenterStore()
        do {
            try store.loadOfflineFixtures(
                from: URL(fileURLWithPath: CommandLine.arguments[1])
            )
        } catch {
            fatalError("fixture load failed: \(error)")
        }
        _ = ControlCenterRootView(store: store)
        _ = DashboardHeader(store: store)
        _ = OverviewView(store: store)
        _ = USBQualityView(store: store)
        _ = DriverModesView(store: store)
        _ = LoopbackView(store: store)
        _ = DiagnosticsView(store: store)
        print("control center UI construction: PASS")
    }
}
