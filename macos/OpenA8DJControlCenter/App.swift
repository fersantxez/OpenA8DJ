import SwiftUI

@main
struct OpenA8DJControlCenterApp: App {
    @StateObject private var store = ControlCenterStore()

    var body: some Scene {
        WindowGroup {
            ControlCenterRootView(store: store)
        }
        .defaultSize(width: 940, height: 680)
        .commands {
            CommandGroup(replacing: .appInfo) {
                Button("About OpenA8DJ Control Center") {
                    NSApp.orderFrontStandardAboutPanel(options: [
                        .applicationName: "OpenA8DJ Control Center",
                        .applicationVersion: Bundle.main.object(
                            forInfoDictionaryKey: "CFBundleShortVersionString"
                        ) as? String ?? "UNKNOWN",
                        .credits: NSAttributedString(
                            string: "Independent OpenA8DJ software. No vendor endorsement."
                        )
                    ])
                }
            }
        }
    }
}
