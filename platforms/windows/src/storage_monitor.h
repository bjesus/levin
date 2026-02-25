#pragma once

#include <windows.h>
#include <functional>
#include <string>

// Polls disk free/total space on a timer thread.
class StorageMonitor {
public:
    using Callback = std::function<void(uint64_t total, uint64_t free_bytes)>;

    StorageMonitor(const std::string& path, Callback cb);
    ~StorageMonitor();

    void start();
    void stop();

    // Call once to get immediate reading
    void poll();

private:
    std::string path_;
    Callback callback_;
    HANDLE timer_queue_ = nullptr;
    HANDLE timer_ = nullptr;

    static void CALLBACK timerProc(PVOID param, BOOLEAN timerOrWait);
};
