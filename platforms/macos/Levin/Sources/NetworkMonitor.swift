import Foundation
import Network

/// Monitors network connectivity using NWPathMonitor.
/// Reports wifi (includes wired Ethernet) and cellular status.
final class NetworkMonitor {
    private let monitor = NWPathMonitor()
    private let onChange: (Bool, Bool) -> Void  // (hasWifi, hasCellular)

    init(onChange: @escaping (Bool, Bool) -> Void) {
        self.onChange = onChange
    }

    func start(on queue: DispatchQueue) {
        monitor.pathUpdateHandler = { [weak self] path in
            guard path.status == .satisfied else {
                self?.onChange(false, false)
                return
            }
            let hasWifi = path.usesInterfaceType(.wifi) || path.usesInterfaceType(.wiredEthernet)
            let hasCellular = path.usesInterfaceType(.cellular)
            // Desktops may not report a specific interface type — if connected, treat as wifi
            self?.onChange(hasWifi || (!hasWifi && !hasCellular), hasCellular)
        }
        monitor.start(queue: queue)
    }

    func stop() {
        monitor.cancel()
    }
}
