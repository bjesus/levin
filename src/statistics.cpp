#include "statistics.hpp"
#include "config.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace levin {

Statistics::Statistics(const Config& config)
    : config_(config)
    , stats_file_(config.statistics_file())
    , stats_{}
    , last_reported_download_(0)
    , last_reported_upload_(0) {
    
    // Initialize all stats to zero
    stats_.lifetime_downloaded_bytes = 0;
    stats_.lifetime_uploaded_bytes = 0;
    stats_.lifetime_uptime_seconds = 0;
    stats_.lifetime_session_count = 0;
    stats_.session_downloaded_bytes = 0;
    stats_.session_uploaded_bytes = 0;
    stats_.torrents_loaded = 0;
    stats_.pieces_have = 0;
    stats_.pieces_total = 0;
    stats_.peers_connected = 0;
    
    // Initialize session start time
    stats_.session_start_time = std::chrono::system_clock::now();
    stats_.first_start_time = stats_.session_start_time;
    
    LOG_INFO("Statistics initialized, file: {}", stats_file_);
}

Statistics::~Statistics() {
    save();
}

void Statistics::load() {
    std::ifstream file(stats_file_);
    if (!file.is_open()) {
        LOG_INFO("No existing statistics file, starting fresh");
        stats_.first_start_time = std::chrono::system_clock::now();
        stats_.lifetime_session_count = 1;
        return;
    }

    try {
        json j;
        file >> j;
        
        stats_.lifetime_downloaded_bytes = j.value("lifetime_downloaded", 0ULL);
        stats_.lifetime_uploaded_bytes = j.value("lifetime_uploaded", 0ULL);
        stats_.lifetime_uptime_seconds = j.value("lifetime_uptime", 0ULL);
        stats_.lifetime_session_count = j.value("session_count", 0);
        
        // Parse first start time
        if (j.contains("first_start")) {
            auto first_start_time_t = j["first_start"].get<std::time_t>();
            stats_.first_start_time = std::chrono::system_clock::from_time_t(first_start_time_t);
        }
        
        // Increment session count
        stats_.lifetime_session_count++;
        
        LOG_INFO("Loaded statistics: {} downloaded, {} uploaded, {} sessions",
                 stats_.lifetime_downloaded_bytes,
                 stats_.lifetime_uploaded_bytes,
                 stats_.lifetime_session_count);
                 
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load statistics: {}", e.what());
    }
}

void Statistics::save() {
    // Calculate current session uptime and add to lifetime
    auto session_uptime = get_session_uptime_seconds();
    uint64_t total_lifetime_uptime = stats_.lifetime_uptime_seconds + session_uptime;
    
    json j;
    j["lifetime_downloaded"] = stats_.lifetime_downloaded_bytes;
    j["lifetime_uploaded"] = stats_.lifetime_uploaded_bytes;
    j["lifetime_uptime"] = total_lifetime_uptime;
    j["session_count"] = stats_.lifetime_session_count;
    j["first_start"] = std::chrono::system_clock::to_time_t(stats_.first_start_time);
    
    try {
        std::ofstream file(stats_file_);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open statistics file for writing: {}", stats_file_);
            return;
        }
        
        file << j.dump(2) << std::endl;
        file.close();
        
        LOG_DEBUG("Statistics saved");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save statistics: {}", e.what());
    }
}

void Statistics::update(uint64_t downloaded, uint64_t uploaded) {
    // Update session stats
    stats_.session_downloaded_bytes = downloaded;
    stats_.session_uploaded_bytes = uploaded;
    
    // Calculate delta and add to lifetime
    // libtorrent resets counters on restart, so we track the delta
    if (downloaded >= last_reported_download_) {
        uint64_t delta = downloaded - last_reported_download_;
        stats_.lifetime_downloaded_bytes += delta;
    }
    
    if (uploaded >= last_reported_upload_) {
        uint64_t delta = uploaded - last_reported_upload_;
        stats_.lifetime_uploaded_bytes += delta;
    }
    
    last_reported_download_ = downloaded;
    last_reported_upload_ = uploaded;
}

void Statistics::update_state(int torrents, int pieces_have, int pieces_total, int peers) {
    stats_.torrents_loaded = torrents;
    stats_.pieces_have = pieces_have;
    stats_.pieces_total = pieces_total;
    stats_.peers_connected = peers;
}

uint64_t Statistics::get_session_uptime_seconds() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats_.session_start_time);
    return duration.count();
}

} // namespace levin
