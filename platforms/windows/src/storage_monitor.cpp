#include "storage_monitor.h"

StorageMonitor::StorageMonitor(const std::string& path, Callback cb)
    : path_(path), callback_(std::move(cb)) {}

StorageMonitor::~StorageMonitor() {
    stop();
}

void StorageMonitor::poll() {
    ULARGE_INTEGER free_bytes, total_bytes;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path_.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path_.c_str(), -1, &wpath[0], wlen);
    if (GetDiskFreeSpaceExW(wpath.c_str(), &free_bytes, &total_bytes, nullptr)) {
        if (callback_) callback_(total_bytes.QuadPart, free_bytes.QuadPart);
    }
}

void CALLBACK StorageMonitor::timerProc(PVOID param, BOOLEAN /*timerOrWait*/) {
    auto* self = static_cast<StorageMonitor*>(param);
    self->poll();
}

void StorageMonitor::start() {
    poll(); // immediate reading
    timer_queue_ = CreateTimerQueue();
    if (timer_queue_) {
        CreateTimerQueueTimer(&timer_, timer_queue_, timerProc, this,
                              60000, 60000, WT_EXECUTEDEFAULT);
    }
}

void StorageMonitor::stop() {
    if (timer_queue_) {
        DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE);
        timer_queue_ = nullptr;
        timer_ = nullptr;
    }
}
