import SwiftUI

struct StatsView: View {
    @ObservedObject var engine: LevinEngine
    @State private var showPopulate = false
    @State private var populateProgress = ""
    @State private var isPopulating = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Enable toggle
            HStack {
                Text("Enable Seeding")
                    .font(.headline)
                Spacer()
                Toggle("", isOn: Binding(
                    get: { engine.isEnabled },
                    set: { engine.setEnabled($0) }
                ))
                .toggleStyle(.switch)
                .labelsHidden()
            }

            Divider()

            // State
            HStack {
                Text("State")
                    .foregroundStyle(.secondary)
                Spacer()
                Text(engine.stateName)
                    .fontWeight(.medium)
                    .foregroundStyle(stateColor)
            }

            Divider()

            // Transfer rates
            HStack(spacing: 16) {
                StatItem(label: "Download", value: formatRate(engine.downloadRate))
                StatItem(label: "Upload", value: formatRate(engine.uploadRate))
            }

            HStack(spacing: 16) {
                StatItem(label: "Total Down", value: formatBytes(engine.totalDownloaded))
                StatItem(label: "Total Up", value: formatBytes(engine.totalUploaded))
            }

            Divider()

            // Counts
            HStack(spacing: 16) {
                StatItem(label: "Torrents", value: "\(engine.torrentCount)")
                StatItem(label: "Books", value: formatNumber(engine.fileCount))
                StatItem(label: "Peers", value: "\(engine.peerCount)")
            }

            Divider()

            // Storage
            HStack(spacing: 16) {
                StatItem(
                    label: "Disk Usage",
                    value: formatBytes(engine.diskUsage) + (engine.overBudget ? " (!)" : "")
                )
                StatItem(label: "Disk Budget", value: formatBytes(engine.diskBudget))
            }

            Divider()

            // Bottom actions
            HStack {
                Button("Populate Torrents...") {
                    startPopulate()
                }
                .disabled(isPopulating)

                Spacer()

                SettingsLink {
                    Text("Settings...")
                }

                Button("Quit") {
                    engine.stop()
                    NSApplication.shared.terminate(nil)
                }
            }
        }
        .padding(16)
        .frame(width: 320)
        .onAppear {
            checkFirstRun()
        }
        .alert("Welcome to Levin", isPresented: $showPopulate) {
            Button("Populate") { startPopulate() }
            Button("Skip", role: .cancel) {
                UserDefaults.standard.set(true, forKey: "first_run_dismissed")
            }
        } message: {
            Text("Your torrent watch directory is empty. Would you like to download torrent files from Anna's Archive?")
        }
    }

    // MARK: - Helpers

    private var stateColor: Color {
        switch engine.stateName {
        case "Downloading": return .blue
        case "Seeding": return .green
        case "Paused": return .orange
        case "Idle": return .secondary
        default: return .primary
        }
    }

    private func checkFirstRun() {
        let dismissed = UserDefaults.standard.bool(forKey: "first_run_dismissed")
        if !dismissed && !engine.hasExistingTorrents {
            showPopulate = true
        }
    }

    private func startPopulate() {
        isPopulating = true
        populateProgress = "Fetching torrent list..."

        engine.populateTorrents(progress: { current, total, message in
            if total > 0 {
                populateProgress = "[\(current)/\(total)] \(message)"
            } else {
                populateProgress = message
            }
        }, completion: { result in
            isPopulating = false
            if result >= 0 {
                UserDefaults.standard.set(true, forKey: "first_run_dismissed")
            }
        })
    }
}

// MARK: - Subviews

struct StatItem: View {
    let label: String
    let value: String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.system(.body, design: .monospaced))
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

// MARK: - Formatting

func formatRate(_ bytesPerSec: Int) -> String {
    switch bytesPerSec {
    case 1_048_576...:
        return String(format: "%.1f MB/s", Double(bytesPerSec) / 1_048_576.0)
    case 1024...:
        return String(format: "%.1f KB/s", Double(bytesPerSec) / 1024.0)
    default:
        return "\(bytesPerSec) B/s"
    }
}

func formatBytes(_ bytes: UInt64) -> String {
    switch bytes {
    case 1_073_741_824...:
        return String(format: "%.2f GB", Double(bytes) / 1_073_741_824.0)
    case 1_048_576...:
        return String(format: "%.1f MB", Double(bytes) / 1_048_576.0)
    case 1024...:
        return String(format: "%.1f KB", Double(bytes) / 1024.0)
    default:
        return "\(bytes) B"
    }
}

func formatNumber(_ n: Int) -> String {
    let formatter = NumberFormatter()
    formatter.numberStyle = .decimal
    return formatter.string(from: NSNumber(value: n)) ?? "\(n)"
}
