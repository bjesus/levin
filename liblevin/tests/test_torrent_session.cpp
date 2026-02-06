// Phase 5 tests: require real libtorrent session
// Only compiled when LEVIN_USE_STUB_SESSION is OFF

#ifndef LEVIN_USE_STUB_SESSION

#include <catch2/catch_test_macros.hpp>
#include "torrent_session.h"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Forward declaration of factory (defined in torrent_session.cpp)
namespace levin {
    std::unique_ptr<ITorrentSession> create_real_torrent_session();
}

static const std::string TEST_TORRENT = "tests/fixtures/test.torrent";

// Helper to find the test torrent relative to the executable
static std::string find_test_torrent() {
    // Try relative to CWD first, then a few common paths
    if (fs::exists(TEST_TORRENT)) return TEST_TORRENT;
    if (fs::exists("../liblevin/" + TEST_TORRENT)) return "../liblevin/" + TEST_TORRENT;
    if (fs::exists("../../liblevin/" + TEST_TORRENT)) return "../../liblevin/" + TEST_TORRENT;
    return TEST_TORRENT; // fall through, test will fail with clear message
}

TEST_CASE("Session starts and stops cleanly") {
    auto session = levin::create_real_torrent_session();
    session->configure(16881, "stun.l.google.com:19302");

    std::string tmp_dir = (fs::temp_directory_path() / "levin_session_test").string();
    fs::create_directories(tmp_dir);

    session->start(tmp_dir);
    REQUIRE(session->is_running());
    session->stop();
    REQUIRE(!session->is_running());

    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

TEST_CASE("WebTorrent is enabled") {
    auto session = levin::create_real_torrent_session();
    session->configure(16882, "stun.l.google.com:19302");

    std::string tmp_dir = (fs::temp_directory_path() / "levin_wt_test").string();
    fs::create_directories(tmp_dir);

    session->start(tmp_dir);
    REQUIRE(session->is_webtorrent_enabled());
    session->stop();

    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

TEST_CASE("Added torrent has WebSocket trackers") {
    auto session = levin::create_real_torrent_session();
    session->configure(16883, "stun.l.google.com:19302");

    std::string tmp_dir = (fs::temp_directory_path() / "levin_tracker_test").string();
    fs::create_directories(tmp_dir);

    session->start(tmp_dir);

    auto torrent_path = find_test_torrent();
    REQUIRE(fs::exists(torrent_path));

    auto handle = session->add_torrent(torrent_path);
    REQUIRE(handle.has_value());

    auto trackers = session->get_trackers(*handle);
    bool has_wss = false;
    for (auto& t : trackers) {
        if (t.rfind("wss://", 0) == 0) { has_wss = true; break; }
    }
    REQUIRE(has_wss);

    session->stop();

    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

TEST_CASE("pause_downloads sets rate limit to 1") {
    auto session = levin::create_real_torrent_session();
    session->configure(16884, "stun.l.google.com:19302");

    std::string tmp_dir = (fs::temp_directory_path() / "levin_rate_test").string();
    fs::create_directories(tmp_dir);

    session->start(tmp_dir);
    session->pause_downloads();
    REQUIRE(session->get_download_rate_limit() == 1);
    session->resume_downloads();
    REQUIRE(session->get_download_rate_limit() == 0);
    session->stop();

    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

TEST_CASE("pause_session stops all activity") {
    auto session = levin::create_real_torrent_session();
    session->configure(16885, "stun.l.google.com:19302");

    std::string tmp_dir = (fs::temp_directory_path() / "levin_pause_test").string();
    fs::create_directories(tmp_dir);

    session->start(tmp_dir);
    session->pause_session();
    REQUIRE(session->is_paused());
    session->resume_session();
    REQUIRE(!session->is_paused());
    session->stop();

    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
}

#else
// When using stub session, provide a placeholder test
#include <catch2/catch_test_macros.hpp>
TEST_CASE("Torrent session tests skipped (stub mode)") {
    REQUIRE(true);  // placeholder
}
#endif
