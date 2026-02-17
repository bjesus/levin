import SwiftUI
import ServiceManagement

struct SettingsView: View {
    @ObservedObject var engine: LevinEngine

    @AppStorage("run_on_battery") private var runOnBattery = false
    @AppStorage("max_download_kbps") private var maxDownloadKbps = 0
    @AppStorage("max_upload_kbps") private var maxUploadKbps = 0
    @AppStorage("min_free_gb") private var minFreeGB = 2.0
    @AppStorage("max_storage_gb") private var maxStorageGB = 0.0
    @AppStorage("launch_at_login") private var launchAtLogin = false

    var body: some View {
        Form {
            Section("General") {
                Toggle("Run on battery", isOn: $runOnBattery)
                    .onChange(of: runOnBattery) { _, value in
                        engine.setRunOnBattery(value)
                    }

                Toggle("Launch at login", isOn: $launchAtLogin)
                    .onChange(of: launchAtLogin) { _, value in
                        setLoginItem(enabled: value)
                    }
            }

            Section("Bandwidth") {
                HStack {
                    Text("Max download")
                    Spacer()
                    TextField("0", value: $maxDownloadKbps, format: .number)
                        .frame(width: 80)
                        .textFieldStyle(.roundedBorder)
                        .multilineTextAlignment(.trailing)
                    Text("KB/s")
                        .foregroundStyle(.secondary)
                }
                .onChange(of: maxDownloadKbps) { _, value in
                    engine.setDownloadLimit(value)
                }

                HStack {
                    Text("Max upload")
                    Spacer()
                    TextField("0", value: $maxUploadKbps, format: .number)
                        .frame(width: 80)
                        .textFieldStyle(.roundedBorder)
                        .multilineTextAlignment(.trailing)
                    Text("KB/s")
                        .foregroundStyle(.secondary)
                }
                .onChange(of: maxUploadKbps) { _, value in
                    engine.setUploadLimit(value)
                }

                Text("0 = unlimited")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section("Storage") {
                HStack {
                    Text("Min free space")
                    Spacer()
                    TextField("2", value: $minFreeGB, format: .number)
                        .frame(width: 80)
                        .textFieldStyle(.roundedBorder)
                        .multilineTextAlignment(.trailing)
                    Text("GB")
                        .foregroundStyle(.secondary)
                }
                .onChange(of: minFreeGB) { _, _ in
                    engine.setDiskLimits(minFreeGB: minFreeGB, maxStorageGB: maxStorageGB)
                }

                HStack {
                    Text("Max storage")
                    Spacer()
                    TextField("0", value: $maxStorageGB, format: .number)
                        .frame(width: 80)
                        .textFieldStyle(.roundedBorder)
                        .multilineTextAlignment(.trailing)
                    Text("GB")
                        .foregroundStyle(.secondary)
                }
                .onChange(of: maxStorageGB) { _, _ in
                    engine.setDiskLimits(minFreeGB: minFreeGB, maxStorageGB: maxStorageGB)
                }

                Text("0 = unlimited")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .frame(width: 380, height: 420)
        .navigationTitle("Levin Settings")
    }

    private func setLoginItem(enabled: Bool) {
        if #available(macOS 13.0, *) {
            do {
                if enabled {
                    try SMAppService.mainApp.register()
                } else {
                    try SMAppService.mainApp.unregister()
                }
            } catch {
                // Silently fail — login item registration may require approval
            }
        }
    }
}
