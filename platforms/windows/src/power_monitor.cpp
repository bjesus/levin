#include "power_monitor.h"

PowerMonitor::PowerMonitor(Callback cb) : callback_(std::move(cb)) {}

bool PowerMonitor::isOnAC() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        // ACLineStatus: 0=offline, 1=online, 255=unknown
        return sps.ACLineStatus != 0;
    }
    return true; // assume AC if unknown (desktops)
}

void PowerMonitor::onPowerChange() {
    bool onAC = isOnAC();
    if (onAC != last_on_ac_) {
        last_on_ac_ = onAC;
        if (callback_) callback_(onAC);
    }
}

void PowerMonitor::start() {
    last_on_ac_ = isOnAC();
    if (callback_) callback_(last_on_ac_);
}
