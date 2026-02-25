#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// Status snapshot posted to the UI thread
struct LevinStatus {
    bool enabled;
    int state;
    int torrent_count;
    int file_count;
    int peer_count;
    int download_rate;
    int upload_rate;
    uint64_t total_downloaded;
    uint64_t total_uploaded;
    uint64_t disk_usage;
    uint64_t disk_budget;
    bool over_budget;
};

class LevinEngine {
public:
    LevinEngine(HWND notify_hwnd, UINT notify_msg);
    ~LevinEngine();

    void start();
    void stop();

    void setEnabled(bool enabled);
    void setRunOnBattery(bool value);
    void setDownloadLimit(int kbps);
    void setUploadLimit(int kbps);
    void setDiskLimits(double minFreeGB, double maxStorageGB);

    void updateBattery(bool onAC);
    void updateStorage(uint64_t total, uint64_t free_bytes);
    void updateNetwork(bool hasNet);

    // Populate torrents — blocks, calls progress on the engine thread.
    // Returns number of torrents added (or <0 on error).
    int populateTorrents(void(*progress)(int current, int total, const char* msg, void* ud), void* ud);

    LevinStatus getStatus() const;

    // Directories
    static std::string appDataDir();
    static std::string watchDir();
    static std::string dataDir();
    static std::string stateDir();

    bool hasExistingTorrents() const;

private:
    void threadFunc();

    HWND notify_hwnd_;
    UINT notify_msg_;

    std::thread thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex mu_;
    LevinStatus status_{};
    void* handle_ = nullptr;  // levin_t*

    // Pending commands from UI thread
    struct PendingCmd {
        enum Type { NONE, SET_ENABLED, SET_BATTERY, SET_STORAGE, SET_NETWORK,
                    SET_RUN_ON_BATTERY, SET_DL_LIMIT, SET_UL_LIMIT, SET_DISK_LIMITS };
        Type type = NONE;
        bool bool_val = false;
        int int_val = 0;
        uint64_t u64_a = 0, u64_b = 0;
        double dbl_a = 0, dbl_b = 0;
    };
    std::mutex cmd_mu_;
    std::vector<PendingCmd> pending_cmds_;

    void enqueueCmd(PendingCmd cmd);
    void processCmds();
};
