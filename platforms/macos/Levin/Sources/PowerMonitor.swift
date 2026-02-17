import Foundation
import IOKit.ps

/// Monitors AC power state using IOKit power source notifications.
final class PowerMonitor {
    private let onChange: (Bool) -> Void
    private var runLoopSource: CFRunLoopSource?
    private var lastOnAC: Bool?

    init(onChange: @escaping (Bool) -> Void) {
        self.onChange = onChange
    }

    func start() {
        // Deliver initial state
        let onAC = Self.isOnACPower()
        lastOnAC = onAC
        onChange(onAC)

        // Register for power source change notifications
        let context = Unmanaged.passUnretained(self).toOpaque()
        guard let source = IOPSNotificationCreateRunLoopSource({ context in
            guard let context else { return }
            let monitor = Unmanaged<PowerMonitor>.fromOpaque(context).takeUnretainedValue()
            monitor.handleChange()
        }, context)?.takeRetainedValue() else {
            return
        }

        runLoopSource = source
        CFRunLoopAddSource(CFRunLoopGetMain(), source, .defaultMode)
    }

    func stop() {
        if let source = runLoopSource {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), source, .defaultMode)
            runLoopSource = nil
        }
    }

    private func handleChange() {
        let onAC = Self.isOnACPower()
        if onAC != lastOnAC {
            lastOnAC = onAC
            onChange(onAC)
        }
    }

    /// Returns true if the Mac is on AC power (plugged in).
    /// Desktops without battery always return true.
    static func isOnACPower() -> Bool {
        guard let info = IOPSCopyPowerSourcesInfo()?.takeRetainedValue(),
              let sources = IOPSCopyPowerSourcesList(info)?.takeRetainedValue() as? [Any] else {
            // No power source info — assume desktop on AC
            return true
        }

        if sources.isEmpty {
            return true
        }

        for source in sources {
            guard let desc = IOPSGetPowerSourceDescription(info, source as CFTypeRef)?
                .takeUnretainedValue() as? [String: Any] else {
                continue
            }

            if let powerSource = desc[kIOPSPowerSourceStateKey] as? String {
                if powerSource == kIOPSACPowerValue {
                    return true
                }
            }
        }

        return false
    }
}
