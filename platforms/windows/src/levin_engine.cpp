#include "levin_engine.h"
#include "liblevin.h"

#include <shlobj.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// --- Settings persistence (simple key=value file) ---

static std::string settingsPath() {
    return LevinEngine::appDataDir() + "\\settings.ini";
}

static void saveSettings(const char* key, const std::string& value) {
    auto path = settingsPath();
    // Read all lines, replace or append
    std::vector<std::string> lines;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
    }
    std::string prefix = std::string(key) + "=";
    bool found = false;
    for (auto& l : lines) {
        if (l.compare(0, prefix.size(), prefix) == 0) {
            l = prefix + value;
            found = true;
            break;
        }
    }
    if (!found) lines.push_back(prefix + value);
    std::ofstream out(path);
    for (const auto& l : lines) out << l << "\n";
}

static std::string loadSetting(const char* key, const std::string& def = "") {
    auto path = settingsPath();
    std::ifstream in(path);
    std::string line;
    std::string prefix = std::string(key) + "=";
    while (std::getline(in, line)) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            return line.substr(prefix.size());
        }
    }
    return def;
}

static int loadSettingInt(const char* key, int def = 0) {
    auto s = loadSetting(key);
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
}

static double loadSettingDbl(const char* key, double def = 0.0) {
    auto s = loadSetting(key);
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

// --- Directories ---

std::string LevinEngine::appDataDir() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\Levin";
    }
    return ".\\Levin";
}

std::string LevinEngine::watchDir()  { return appDataDir() + "\\watch"; }
std::string LevinEngine::dataDir()   { return appDataDir() + "\\data"; }
std::string LevinEngine::stateDir()  { return appDataDir() + "\\state"; }

// --- Engine ---

LevinEngine::LevinEngine(HWND hwnd, UINT msg)
    : notify_hwnd_(hwnd), notify_msg_(msg) {}

LevinEngine::~LevinEngine() {
    stop();
}

void LevinEngine::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&LevinEngine::threadFunc, this);
}

void LevinEngine::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void LevinEngine::enqueueCmd(PendingCmd cmd) {
    std::lock_guard<std::mutex> lock(cmd_mu_);
    pending_cmds_.push_back(cmd);
}

void LevinEngine::processCmds() {
    std::vector<PendingCmd> cmds;
    {
        std::lock_guard<std::mutex> lock(cmd_mu_);
        cmds.swap(pending_cmds_);
    }
    auto* h = static_cast<levin_t*>(handle_);
    if (!h) return;

    for (const auto& c : cmds) {
        switch (c.type) {
        case PendingCmd::SET_ENABLED:
            levin_set_enabled(h, c.bool_val ? 1 : 0);
            {
                std::lock_guard<std::mutex> lock(mu_);
                status_.enabled = c.bool_val;
            }
            break;
        case PendingCmd::SET_BATTERY:
            levin_update_battery(h, c.bool_val ? 1 : 0);
            break;
        case PendingCmd::SET_STORAGE:
            levin_update_storage(h, c.u64_a, c.u64_b);
            break;
        case PendingCmd::SET_NETWORK:
            levin_update_network(h, c.bool_val ? 1 : 0, 0);
            break;
        case PendingCmd::SET_RUN_ON_BATTERY:
            levin_set_run_on_battery(h, c.bool_val ? 1 : 0);
            break;
        case PendingCmd::SET_DL_LIMIT:
            levin_set_download_limit(h, c.int_val);
            break;
        case PendingCmd::SET_UL_LIMIT:
            levin_set_upload_limit(h, c.int_val);
            break;
        case PendingCmd::SET_DISK_LIMITS: {
            uint64_t minFree = static_cast<uint64_t>(c.dbl_a * 1073741824.0);
            uint64_t maxStore = c.dbl_b > 0 ? static_cast<uint64_t>(c.dbl_b * 1073741824.0) : 0;
            levin_set_disk_limits(h, minFree, 0.05, maxStore);
            break;
        }
        default: break;
        }
    }
}

void LevinEngine::threadFunc() {
    // Ensure directories exist
    std::error_code ec;
    fs::create_directories(watchDir(), ec);
    fs::create_directories(dataDir(), ec);
    fs::create_directories(stateDir(), ec);

    // Load settings
    bool runOnBattery = loadSettingInt("run_on_battery", 0) != 0;
    int dlLimit = loadSettingInt("max_download_kbps", 0);
    int ulLimit = loadSettingInt("max_upload_kbps", 0);
    double minFreeGB = loadSettingDbl("min_free_gb", 2.0);
    double maxStorageGB = loadSettingDbl("max_storage_gb", 0.0);

    levin_config_t cfg{};
    auto w = watchDir(), d = dataDir(), s = stateDir();
    cfg.watch_directory = w.c_str();
    cfg.data_directory = d.c_str();
    cfg.state_directory = s.c_str();
    cfg.min_free_bytes = static_cast<uint64_t>(minFreeGB * 1073741824.0);
    cfg.min_free_percentage = 0.05;
    cfg.max_storage_bytes = maxStorageGB > 0 ? static_cast<uint64_t>(maxStorageGB * 1073741824.0) : 0;
    cfg.run_on_battery = runOnBattery ? 1 : 0;
    cfg.run_on_cellular = 0;
    cfg.disk_check_interval_secs = 60;
    cfg.max_download_kbps = dlLimit;
    cfg.max_upload_kbps = ulLimit;
    cfg.stun_server = nullptr;

    auto* h = levin_create(&cfg);
    handle_ = h;
    if (!h) { running_ = false; return; }

    if (levin_start(h) != 0) {
        levin_destroy(h);
        handle_ = nullptr;
        running_ = false;
        return;
    }

    levin_set_enabled(h, 1);

    int tick_count = 0;
    while (running_) {
        processCmds();
        levin_tick(h);

        auto st = levin_get_status(h);
        {
            std::lock_guard<std::mutex> lock(mu_);
            status_.enabled = true; // we always start enabled
            status_.state = st.state;
            status_.torrent_count = st.torrent_count;
            status_.file_count = st.file_count;
            status_.peer_count = st.peer_count;
            status_.download_rate = st.download_rate;
            status_.upload_rate = st.upload_rate;
            status_.total_downloaded = st.total_downloaded;
            status_.total_uploaded = st.total_uploaded;
            status_.disk_usage = st.disk_usage;
            status_.disk_budget = st.disk_budget;
            status_.over_budget = st.over_budget != 0;
        }

        // Notify UI thread
        PostMessage(notify_hwnd_, notify_msg_, 0, 0);

        Sleep(1000);
        tick_count++;
    }

    levin_stop(h);
    levin_destroy(h);
    handle_ = nullptr;
}

void LevinEngine::setEnabled(bool enabled) {
    PendingCmd c; c.type = PendingCmd::SET_ENABLED; c.bool_val = enabled;
    enqueueCmd(c);
}

void LevinEngine::setRunOnBattery(bool value) {
    PendingCmd c; c.type = PendingCmd::SET_RUN_ON_BATTERY; c.bool_val = value;
    enqueueCmd(c);
    saveSettings("run_on_battery", value ? "1" : "0");
}

void LevinEngine::setDownloadLimit(int kbps) {
    PendingCmd c; c.type = PendingCmd::SET_DL_LIMIT; c.int_val = kbps;
    enqueueCmd(c);
    saveSettings("max_download_kbps", std::to_string(kbps));
}

void LevinEngine::setUploadLimit(int kbps) {
    PendingCmd c; c.type = PendingCmd::SET_UL_LIMIT; c.int_val = kbps;
    enqueueCmd(c);
    saveSettings("max_upload_kbps", std::to_string(kbps));
}

void LevinEngine::setDiskLimits(double minFreeGB, double maxStorageGB) {
    PendingCmd c; c.type = PendingCmd::SET_DISK_LIMITS;
    c.dbl_a = minFreeGB; c.dbl_b = maxStorageGB;
    enqueueCmd(c);
    saveSettings("min_free_gb", std::to_string(minFreeGB));
    saveSettings("max_storage_gb", std::to_string(maxStorageGB));
}

void LevinEngine::updateBattery(bool onAC) {
    PendingCmd c; c.type = PendingCmd::SET_BATTERY; c.bool_val = onAC;
    enqueueCmd(c);
}

void LevinEngine::updateStorage(uint64_t total, uint64_t free_bytes) {
    PendingCmd c; c.type = PendingCmd::SET_STORAGE; c.u64_a = total; c.u64_b = free_bytes;
    enqueueCmd(c);
}

void LevinEngine::updateNetwork(bool hasNet) {
    PendingCmd c; c.type = PendingCmd::SET_NETWORK; c.bool_val = hasNet;
    enqueueCmd(c);
}

int LevinEngine::populateTorrents(void(*progress)(int, int, const char*, void*), void* ud) {
    // This must run on the engine thread. We'll block the caller.
    // Since populate is long-running, call it directly if handle_ is valid.
    auto* h = static_cast<levin_t*>(handle_);
    if (!h) return -1;
    return levin_populate_torrents(h, progress, ud);
}

LevinStatus LevinEngine::getStatus() const {
    std::lock_guard<std::mutex> lock(mu_);
    return status_;
}

bool LevinEngine::hasExistingTorrents() const {
    std::error_code ec;
    auto dir = watchDir();
    if (!fs::exists(dir, ec)) return false;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".torrent")
            return true;
    }
    return false;
}
