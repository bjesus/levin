#include "torrent_session.h"
#include <functional>
#include <sstream>
#include <iomanip>

namespace levin {

void StubTorrentSession::configure(int /*port*/, const std::string& /*stun_server*/) {}

void StubTorrentSession::start(const std::string& /*data_directory*/) {
    running_ = true;
    paused_ = false;
}

void StubTorrentSession::stop() {
    running_ = false;
    paused_ = false;
}

bool StubTorrentSession::is_running() const { return running_; }

std::optional<std::string> StubTorrentSession::add_torrent(const std::string& torrent_path) {
    if (!running_) return std::nullopt;
    torrent_count_++;
    // Generate a fake info hash from the path
    std::hash<std::string> hasher;
    auto h = hasher(torrent_path);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << h;
    std::string hex = oss.str();
    // Pad to 40 hex chars
    while (hex.size() < 40) hex += "0";
    return hex.substr(0, 40);
}

void StubTorrentSession::remove_torrent(const std::string& /*info_hash*/) {
    if (torrent_count_ > 0) torrent_count_--;
}

int StubTorrentSession::torrent_count() const { return torrent_count_; }

void StubTorrentSession::pause_session() { paused_ = true; }
void StubTorrentSession::resume_session() { paused_ = false; }
bool StubTorrentSession::is_paused() const { return paused_; }

void StubTorrentSession::pause_downloads() { download_rate_limit_ = 1; }
void StubTorrentSession::resume_downloads() { download_rate_limit_ = 0; }
void StubTorrentSession::set_download_rate_limit(int bps) { download_rate_limit_ = bps; }
void StubTorrentSession::set_upload_rate_limit(int bps) { upload_rate_limit_ = bps; }
int StubTorrentSession::get_download_rate_limit() const { return download_rate_limit_; }

int StubTorrentSession::peer_count() const { return 0; }
int StubTorrentSession::download_rate() const { return 0; }
int StubTorrentSession::upload_rate() const { return 0; }
uint64_t StubTorrentSession::total_downloaded() const { return 0; }
uint64_t StubTorrentSession::total_uploaded() const { return 0; }

bool StubTorrentSession::is_webtorrent_enabled() const { return false; }
std::vector<std::string> StubTorrentSession::get_trackers(const std::string& /*info_hash*/) const {
    return {};
}

void StubTorrentSession::save_state(const std::string& /*path*/) {}
void StubTorrentSession::load_state(const std::string& /*path*/) {}

} // namespace levin
