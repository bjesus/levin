import Foundation

/// Periodically checks filesystem storage and reports total/free bytes.
/// Uses statvfs (same POSIX call as the Linux shell).
final class StorageMonitor {
    private let path: String
    private let onChange: (UInt64, UInt64) -> Void
    private var timer: DispatchSourceTimer?

    init(path: String, onChange: @escaping (UInt64, UInt64) -> Void) {
        self.path = path
        self.onChange = onChange
    }

    func start(on queue: DispatchQueue, interval: TimeInterval = 60) {
        // Deliver initial reading
        report()

        let t = DispatchSource.makeTimerSource(queue: queue)
        t.schedule(deadline: .now() + interval, repeating: interval)
        t.setEventHandler { [weak self] in
            self?.report()
        }
        t.resume()
        timer = t
    }

    func stop() {
        timer?.cancel()
        timer = nil
    }

    private func report() {
        var buf = statvfs()
        guard statvfs(path, &buf) == 0 else { return }

        let blockSize = buf.f_frsize != 0 ? UInt64(buf.f_frsize) : UInt64(buf.f_bsize)
        let total = UInt64(buf.f_blocks) * blockSize
        let free = UInt64(buf.f_bavail) * blockSize

        onChange(total, free)
    }
}
