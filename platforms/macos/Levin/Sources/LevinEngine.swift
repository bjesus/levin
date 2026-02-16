import Foundation
import Combine

/// Wraps the liblevin C API and runs the tick loop on a dedicated serial queue.
/// All levin_* calls happen on `queue` to satisfy the single-thread contract.
/// Published properties are updated on the main thread for SwiftUI binding.
final class LevinEngine: ObservableObject {

    // MARK: - Published state (main thread)

    @Published var isRunning = false
    @Published var isEnabled = false
    @Published var stateName = "--"
    @Published var torrentCount = 0
    @Published var fileCount = 0
    @Published var peerCount = 0
    @Published var downloadRate = 0
    @Published var uploadRate = 0
    @Published var totalDownloaded: UInt64 = 0
    @Published var totalUploaded: UInt64 = 0
    @Published var diskUsage: UInt64 = 0
    @Published var diskBudget: UInt64 = 0
    @Published var overBudget = false

    // MARK: - Private

    private var handle: OpaquePointer?
    private let queue = DispatchQueue(label: "com.yoavmoshe.levin.engine", qos: .utility)
    private var timer: DispatchSourceTimer?
    private var powerMonitor: PowerMonitor?
    private var storageMonitor: StorageMonitor?

    // MARK: - Directories

    private static var appSupportDir: String {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        return base.appendingPathComponent("Levin").path
    }

    private static var watchDir: String { appSupportDir + "/watch" }
    private static var dataDir: String { appSupportDir + "/data" }
    private static var stateDir: String { appSupportDir + "/state" }

    // MARK: - Lifecycle

    init() {
        start()
    }

    deinit {
        stop()
    }

    func start() {
        queue.async { [weak self] in
            self?.startOnQueue()
        }
    }

    func stop() {
        timer?.cancel()
        timer = nil
        queue.sync { [weak self] in
            self?.stopOnQueue()
        }
    }

    // MARK: - Controls

    func setEnabled(_ enabled: Bool) {
        queue.async { [weak self] in
            guard let self, let h = self.handle else { return }
            levin_set_enabled(h, enabled ? 1 : 0)
        }
    }

    func setRunOnBattery(_ value: Bool) {
        queue.async { [weak self] in
            guard let self, let h = self.handle else { return }
            levin_set_run_on_battery(h, value ? 1 : 0)
        }
        UserDefaults.standard.set(value, forKey: "run_on_battery")
    }

    func setDownloadLimit(_ kbps: Int) {
        queue.async { [weak self] in
            guard let self, let h = self.handle else { return }
            levin_set_download_limit(h, Int32(kbps))
        }
        UserDefaults.standard.set(kbps, forKey: "max_download_kbps")
    }

    func setUploadLimit(_ kbps: Int) {
        queue.async { [weak self] in
            guard let self, let h = self.handle else { return }
            levin_set_upload_limit(h, Int32(kbps))
        }
        UserDefaults.standard.set(kbps, forKey: "max_upload_kbps")
    }

    func setDiskLimits(minFreeGB: Double, maxStorageGB: Double) {
        let minFreeBytes = UInt64(minFreeGB * 1_073_741_824)
        let maxStorageBytes = maxStorageGB > 0 ? UInt64(maxStorageGB * 1_073_741_824) : 0
        queue.async { [weak self] in
            guard let self, let h = self.handle else { return }
            levin_set_disk_limits(h, minFreeBytes, 0.05, maxStorageBytes)
        }
        UserDefaults.standard.set(minFreeGB, forKey: "min_free_gb")
        UserDefaults.standard.set(maxStorageGB, forKey: "max_storage_gb")
    }

    /// Populate torrents from Anna's Archive. Calls completion on main thread.
    func populateTorrents(progress: @escaping (Int, Int, String) -> Void,
                          completion: @escaping (Int) -> Void) {
        queue.async { [weak self] in
            guard let self, let h = self.handle else {
                DispatchQueue.main.async { completion(-1) }
                return
            }

            // Bridge the progress callback through an opaque context.
            // We use Unmanaged to prevent ARC from releasing the closure box
            // while the C callback is still active.
            class Box {
                let fn: (Int, Int, String) -> Void
                init(_ fn: @escaping (Int, Int, String) -> Void) { self.fn = fn }
            }
            let box = Box(progress)
            let raw = Unmanaged.passRetained(box).toOpaque()

            let result = levin_populate_torrents(h, { current, total, message, userdata in
                guard let userdata else { return }
                let box = Unmanaged<Box>.fromOpaque(userdata).takeUnretainedValue()
                let msg = message.map { String(cString: $0) } ?? ""
                DispatchQueue.main.async {
                    box.fn(Int(current), Int(total), msg)
                }
            }, raw)

            // Release the box now that populate is done
            Unmanaged<Box>.fromOpaque(raw).release()

            DispatchQueue.main.async { completion(Int(result)) }
        }
    }

    /// Whether the watch directory has any .torrent files
    var hasExistingTorrents: Bool {
        let watchDir = Self.watchDir
        guard let files = try? FileManager.default.contentsOfDirectory(atPath: watchDir) else {
            return false
        }
        return files.contains { $0.hasSuffix(".torrent") }
    }

    // MARK: - Private: Engine lifecycle (runs on self.queue)

    private func startOnQueue() {
        guard handle == nil else { return }

        // Ensure directories exist
        let fm = FileManager.default
        try? fm.createDirectory(atPath: Self.watchDir, withIntermediateDirectories: true)
        try? fm.createDirectory(atPath: Self.dataDir, withIntermediateDirectories: true)
        try? fm.createDirectory(atPath: Self.stateDir, withIntermediateDirectories: true)

        let defaults = UserDefaults.standard
        let runOnBattery = defaults.bool(forKey: "run_on_battery")
        let maxDownKbps = defaults.integer(forKey: "max_download_kbps")
        let maxUpKbps = defaults.integer(forKey: "max_upload_kbps")
        let minFreeGB = defaults.object(forKey: "min_free_gb") as? Double ?? 2.0
        let maxStorageGB = defaults.object(forKey: "max_storage_gb") as? Double ?? 0.0

        var config = levin_config_t()
        // Use strdup so the strings survive past this scope
        let watchCStr = strdup(Self.watchDir)
        let dataCStr = strdup(Self.dataDir)
        let stateCStr = strdup(Self.stateDir)
        config.watch_directory = UnsafePointer(watchCStr)
        config.data_directory = UnsafePointer(dataCStr)
        config.state_directory = UnsafePointer(stateCStr)
        config.min_free_bytes = UInt64(minFreeGB * 1_073_741_824)
        config.min_free_percentage = 0.05
        config.max_storage_bytes = maxStorageGB > 0 ? UInt64(maxStorageGB * 1_073_741_824) : 0
        config.run_on_battery = runOnBattery ? 1 : 0
        config.run_on_cellular = 0
        config.disk_check_interval_secs = 60
        config.max_download_kbps = Int32(maxDownKbps)
        config.max_upload_kbps = Int32(maxUpKbps)
        config.stun_server = nil

        handle = levin_create(&config)

        // Free the strdup'd strings (levin_create copies what it needs)
        free(watchCStr)
        free(dataCStr)
        free(stateCStr)

        guard let h = handle else { return }

        let rc = levin_start(h)
        if rc != 0 {
            levin_destroy(h)
            handle = nil
            return
        }

        levin_set_enabled(h, 1)

        // Start monitors
        startMonitors()

        // Start tick timer (1 second interval)
        let t = DispatchSource.makeTimerSource(queue: queue)
        t.schedule(deadline: .now(), repeating: .seconds(1))
        t.setEventHandler { [weak self] in
            self?.tick()
        }
        t.resume()
        timer = t

        DispatchQueue.main.async { [weak self] in
            self?.isRunning = true
            self?.isEnabled = true
        }
    }

    private func stopOnQueue() {
        powerMonitor?.stop()
        powerMonitor = nil
        storageMonitor?.stop()
        storageMonitor = nil

        if let h = handle {
            levin_stop(h)
            levin_destroy(h)
            handle = nil
        }

        DispatchQueue.main.async { [weak self] in
            self?.isRunning = false
            self?.isEnabled = false
        }
    }

    private func tick() {
        guard let h = handle else { return }
        levin_tick(h)

        let status = levin_get_status(h)
        let name = stateString(status.state)

        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.stateName = name
            self.torrentCount = Int(status.torrent_count)
            self.fileCount = Int(status.file_count)
            self.peerCount = Int(status.peer_count)
            self.downloadRate = Int(status.download_rate)
            self.uploadRate = Int(status.upload_rate)
            self.totalDownloaded = status.total_downloaded
            self.totalUploaded = status.total_uploaded
            self.diskUsage = status.disk_usage
            self.diskBudget = status.disk_budget
            self.overBudget = status.over_budget != 0
        }
    }

    private func startMonitors() {
        guard let h = handle else { return }

        powerMonitor = PowerMonitor { [weak self] onAC in
            self?.queue.async {
                levin_update_battery(h, onAC ? 1 : 0)
            }
        }
        powerMonitor?.start()

        storageMonitor = StorageMonitor(path: Self.dataDir) { [weak self] total, free in
            self?.queue.async {
                levin_update_storage(h, total, free)
            }
        }
        storageMonitor?.start(on: queue)
    }

    private func stateString(_ state: levin_state_t) -> String {
        switch state {
        case LEVIN_STATE_OFF: return "Off"
        case LEVIN_STATE_PAUSED: return "Paused"
        case LEVIN_STATE_IDLE: return "Idle"
        case LEVIN_STATE_SEEDING: return "Seeding"
        case LEVIN_STATE_DOWNLOADING: return "Downloading"
        default: return "Unknown"
        }
    }
}
