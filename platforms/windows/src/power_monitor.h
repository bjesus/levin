#pragma once

#include <windows.h>
#include <functional>

// Monitors AC/battery power state via WM_POWERBROADCAST.
// Call checkPower() to get current state; register the hidden
// window's HWND and forward WM_POWERBROADCAST to onPowerChange().
class PowerMonitor {
public:
    using Callback = std::function<void(bool onAC)>;

    explicit PowerMonitor(Callback cb);

    // Get current power state
    static bool isOnAC();

    // Call from WndProc when WM_POWERBROADCAST is received
    void onPowerChange();

    // Initial state delivery
    void start();

private:
    Callback callback_;
    bool last_on_ac_ = true;
};
