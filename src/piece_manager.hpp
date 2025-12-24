#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <cstdint>
#include <libtorrent/torrent_handle.hpp>

namespace levin {

class Config;
class Session;
class DiskMonitor;

/**
 * Information about a single piece.
 */
struct PieceInfo {
    std::string info_hash;      // Torrent info hash
    int piece_index;            // Piece index within torrent
    uint64_t size_bytes;        // Size of this piece
    bool have_piece;            // Do we have this piece?
    int rarity_score;           // How many peers have it (lower = rarer)
    double priority;            // Combined priority score
    
    // For priority queue ordering
    bool operator<(const PieceInfo& other) const {
        // Higher priority should come first (max heap by default, so invert)
        return priority < other.priority;
    }
};

/**
 * Metrics for a single torrent.
 */
struct TorrentMetrics {
    std::string info_hash;
    std::string name;
    libtorrent::torrent_handle handle;
    
    int total_seeders;          // Total seeders from DHT/trackers
    int total_pieces;           // Total pieces in torrent
    int pieces_we_have;         // Number of pieces we have
    uint64_t total_size;        // Total size in bytes
    uint64_t size_we_have;      // Size we have in bytes
    
    double torrent_priority;    // Base priority: 1 / (seeders + 1)
    
    std::chrono::steady_clock::time_point last_seeder_check;
};

/**
 * Manages piece-level prioritization and space allocation.
 * This is the core intelligence of the archival system.
 */
class PieceManager {
public:
    PieceManager(const Config& config, Session& session, DiskMonitor& disk_monitor);
    
    /**
     * Update all torrent metrics (seeder counts, piece info).
     * Call this periodically (e.g., every 15 minutes).
     */
    void update_metrics();
    
    /**
     * Rebuild priority queues based on current metrics.
     */
    void rebuild_queues();
    
    /**
     * Rebalance disk usage based on current space status.
     * Downloads pieces if under budget, deletes if over budget.
     */
    void rebalance_disk_usage();
    
    /**
     * Emergency mode: pause all downloads immediately.
     * @param reason Reason for pausing (defaults to "disk space emergency")
     */
    void emergency_pause_downloads(const std::string& reason = "disk space emergency");
    
    /**
     * Get total size of data we currently have.
     */
    uint64_t get_total_data_size() const;
    
    /**
     * Get metrics for all torrents.
     */
    const std::unordered_map<std::string, TorrentMetrics>& get_metrics() const {
        return torrents_;
    }

private:
    const Config& config_;
    Session& session_;
    DiskMonitor& disk_monitor_;
    
    // Torrent tracking
    std::unordered_map<std::string, TorrentMetrics> torrents_;
    
    // Priority queues
    std::priority_queue<PieceInfo> download_queue_;  // Highest priority first
    std::priority_queue<PieceInfo> deletion_queue_;  // Lowest priority first (inverted)
    
    /**
     * Update metrics for a single torrent.
     */
    void update_torrent_metrics(const libtorrent::torrent_handle& handle);
    
    /**
     * Query seeder count from DHT/trackers.
     */
    int query_seeder_count(const libtorrent::torrent_handle& handle);
    
    /**
     * Get piece rarity score (how many peers have it).
     */
    int get_piece_rarity(const libtorrent::torrent_handle& handle, int piece_index);
    
    /**
     * Calculate priority for a piece.
     * Priority = torrent_priority * piece_rarity_factor
     * where torrent_priority = 1 / (seeders + 1)
     * and piece_rarity_factor = 1 - (peers_with_piece / total_peers)
     */
    double calculate_piece_priority(const TorrentMetrics& metrics, 
                                     int piece_index, 
                                     int rarity_score);
    
    /**
     * Build download queue (pieces we don't have, prioritized).
     */
    void build_download_queue();
    
    /**
     * Build deletion queue (pieces we have, inverse priority).
     */
    void build_deletion_queue();
    
    /**
     * Download pieces up to available budget.
     */
    void download_pieces(uint64_t available_bytes);
    
    /**
     * Delete pieces to free up space.
     */
    void delete_pieces(uint64_t bytes_to_free);
};

} // namespace levin
