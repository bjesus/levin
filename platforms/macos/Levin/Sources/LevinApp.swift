import SwiftUI

@main
struct LevinApp: App {
    @StateObject private var engine = LevinEngine()

    var body: some Scene {
        MenuBarExtra {
            StatsView(engine: engine)
        } label: {
            // Load template image from bundle Resources.
            // macOS tints template images automatically for light/dark mode.
            let icon = loadMenuBarIcon()
            Image(nsImage: icon)
        }
        .menuBarExtraStyle(.window)

        Settings {
            SettingsView(engine: engine)
        }
    }

    private func loadMenuBarIcon() -> NSImage {
        // Try @2x first for Retina displays, fall back to @1x
        if let path = Bundle.main.path(forResource: "MenuBarIcon@2x", ofType: "png"),
           let img = NSImage(contentsOfFile: path) {
            img.isTemplate = true
            img.size = NSSize(width: 18, height: 18)  // Point size, not pixels
            return img
        }
        if let path = Bundle.main.path(forResource: "MenuBarIcon", ofType: "png"),
           let img = NSImage(contentsOfFile: path) {
            img.isTemplate = true
            img.size = NSSize(width: 18, height: 18)
            return img
        }
        // Fallback: SF Symbol
        return NSImage(systemSymbolName: "leaf.fill", accessibilityDescription: "Levin")!
    }
}
