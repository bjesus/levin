#pragma once

#include <functional>
#include <memory>

namespace levin {

/**
 * Monitors power source changes (AC/battery) and triggers callbacks.
 * Platform-specific implementation using:
 * - Linux: DBus/UPower signals
 * - macOS: IOKit IOPowerSources API
 */
class PowerMonitor {
public:
    using PowerCallback = std::function<void(bool on_ac_power)>;

    PowerMonitor();
    ~PowerMonitor();

    /**
     * Start monitoring power state changes.
     * @param callback Function to call when power state changes
     */
    void start(PowerCallback callback);

    /**
     * Stop monitoring.
     */
    void stop();

    /**
     * Check current power state.
     * @return true if on AC power, false if on battery
     */
    bool is_on_ac_power() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace levin
