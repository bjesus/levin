#include "piece_manager.hpp"
#include "config.hpp"
#include "session.hpp"
#include "disk_monitor.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <libtorrent/torrent_status.hpp>
#include <algorithm>
#include <filesystem>

namespace lt = libtorrent;

namespace levin {

PieceManager::PieceManager(const Config& config, Session& session, DiskMonitor& disk_monitor)
    : config_(config)
    , session_(session)
    , disk_monitor_(disk_monitor) {
    LOG_INFO("PieceManager initialized");
}

void PieceManager::update_metrics() {
    LOG_DEBUG("Updating torrent metrics");
    
    auto handles = session_.get_torrents();
    
    for (const auto& handle : handles) {
        if (!handle.is_valid()) continue;
        update_torrent_metrics(handle);
    }
    
    LOG_INFO("Updated metrics for {} torrents", torrents_.size());
}

void PieceManager::update_torrent_metrics(const lt::torrent_handle& handle) {
    if (!handle.is_valid()) return;
    
    lt::torrent_status status = handle.status();
    std::string info_hash = Session::info_hash_to_string(handle.info_hash());
    
    TorrentMetrics& metrics = torrents_[info_hash];
    metrics.info_hash = info_hash;
    metrics.name = status.name;
    metrics.handle = handle;
    
    // Get piece information
    auto torrent_info = handle.torrent_file();
    if (torrent_info) {
        metrics.total_pieces = torrent_info->num_pieces();
        metrics.total_size = torrent_info->total_size();
    }
    
    // Count pieces we have
    metrics.pieces_we_have = status.num_pieces;
    metrics.size_we_have = status.total_done;
    
    // Query seeder count
    metrics.total_seeders = query_seeder_count(handle);
    
    // Calculate torrent priority: 1 / (seeders + 1)
    metrics.torrent_priority = 1.0 / (metrics.total_seeders + 1);
    
    metrics.last_seeder_check = std::chrono::steady_clock::now();
    
    LOG_DEBUG("Torrent {}: {} seeders, {}/{} pieces, priority={:.6f}",
              metrics.name, metrics.total_seeders, 
              metrics.pieces_we_have, metrics.total_pieces,
              metrics.torrent_priority);
}

int PieceManager::query_seeder_count(const lt::torrent_handle& handle) {
    if (!handle.is_valid()) return 0;
    
    lt::torrent_status status = handle.status();
    
    // Get seeders from various sources
    int seeders = 0;
    
    // DHT provides distributed seeder estimates
    seeders = std::max(seeders, status.num_complete);
    
    // List seeds gives us connected seeds
    seeders = std::max(seeders, status.list_seeds);
    
    // If we can't get good data, estimate from peers
    if (seeders == 0 && status.num_peers > 0) {
        // Rough estimate: assume some peers are seeds
        seeders = status.num_peers / 4;
    }
    
    return seeders;
}

int PieceManager::get_piece_rarity(const lt::torrent_handle& handle, int piece_index) {
    if (!handle.is_valid()) return 0;
    
    // Get piece availability from libtorrent
    std::vector<int> piece_availability;
    handle.piece_availability(piece_availability);
    
    if (piece_index >= 0 && piece_index < static_cast<int>(piece_availability.size())) {
        return piece_availability[piece_index];
    }
    
    return 0;
}

double PieceManager::calculate_piece_priority(const TorrentMetrics& metrics, 
                                               int piece_index,
                                               int rarity_score) {
    // Torrent priority: fewer seeders = higher priority
    double torrent_priority = metrics.torrent_priority;
    
    // Piece rarity: fewer peers have it = higher priority
    // Normalize by total peers (if we know it)
    double piece_rarity_factor = 1.0;
    
    lt::torrent_status status = metrics.handle.status();
    int total_peers = status.num_peers;
    
    if (total_peers > 0 && rarity_score > 0) {
        // rarity_score is how many peers have it
        // We want: fewer peers with piece = higher priority
        piece_rarity_factor = 1.0 - (static_cast<double>(rarity_score) / total_peers);
        // Clamp to reasonable range
        piece_rarity_factor = std::max(0.1, std::min(1.0, piece_rarity_factor));
    }
    
    // Combined priority
    double priority = torrent_priority * piece_rarity_factor;
    
    return priority;
}

void PieceManager::rebuild_queues() {
    LOG_DEBUG("Rebuilding priority queues");
    
    // Clear old queues
    download_queue_ = std::priority_queue<PieceInfo>();
    deletion_queue_ = std::priority_queue<PieceInfo>();
    
    build_download_queue();
    build_deletion_queue();
    
    LOG_INFO("Queues rebuilt: {} pieces to download, {} pieces available for deletion",
             download_queue_.size(), deletion_queue_.size());
}

void PieceManager::build_download_queue() {
    for (const auto& [info_hash, metrics] : torrents_) {
        if (!metrics.handle.is_valid()) continue;
        
        auto torrent_info = metrics.handle.torrent_file();
        if (!torrent_info) continue;
        
        // Check each piece
        for (int i = 0; i < metrics.total_pieces; ++i) {
            if (metrics.handle.have_piece(i)) {
                continue;  // Skip pieces we already have
            }
            
            PieceInfo piece;
            piece.info_hash = info_hash;
            piece.piece_index = i;
            piece.size_bytes = torrent_info->piece_size(i);
            piece.have_piece = false;
            piece.rarity_score = get_piece_rarity(metrics.handle, i);
            piece.priority = calculate_piece_priority(metrics, i, piece.rarity_score);
            
            download_queue_.push(piece);
        }
    }
}

void PieceManager::build_deletion_queue() {
    for (const auto& [info_hash, metrics] : torrents_) {
        if (!metrics.handle.is_valid()) continue;
        
        auto torrent_info = metrics.handle.torrent_file();
        if (!torrent_info) continue;
        
        // Check each piece we have
        for (int i = 0; i < metrics.total_pieces; ++i) {
            if (!metrics.handle.have_piece(i)) {
                continue;  // Skip pieces we don't have
            }
            
            PieceInfo piece;
            piece.info_hash = info_hash;
            piece.piece_index = i;
            piece.size_bytes = torrent_info->piece_size(i);
            piece.have_piece = true;
            piece.rarity_score = get_piece_rarity(metrics.handle, i);
            piece.priority = calculate_piece_priority(metrics, i, piece.rarity_score);
            
            // Invert priority for deletion (we want to delete low-priority pieces first)
            piece.priority = -piece.priority;
            
            deletion_queue_.push(piece);
        }
    }
}

void PieceManager::rebalance_disk_usage() {
    auto space_status = disk_monitor_.check_space();
    
    if (space_status.over_budget) {
        LOG_WARN("Over disk budget by {}", utils::format_bytes(space_status.deficit_bytes));
        
        // Emergency mode: if severely over budget, pause all downloads first
        if (space_status.deficit_bytes > 1024 * 1024 * 100) {  // >100MB over
            LOG_ERROR("EMERGENCY: Severely over budget! Pausing all downloads");
            emergency_pause_downloads();
        }
        
        delete_pieces(space_status.deficit_bytes);
    } else if (space_status.budget_bytes > 0) {
        LOG_DEBUG("Under budget, {} available", utils::format_bytes(space_status.budget_bytes));
        download_pieces(space_status.budget_bytes);
    }
}

void PieceManager::emergency_pause_downloads(const std::string& reason) {
    LOG_WARN("Emergency mode: Pausing all active downloads");
    
    for (auto& [info_hash, metrics] : torrents_) {
        if (!metrics.handle.is_valid()) continue;
        
        // Set all pieces to low priority to stop downloads
        auto torrent_info = metrics.handle.torrent_file();
        if (!torrent_info) continue;
        
        for (int i = 0; i < metrics.total_pieces; ++i) {
            if (!metrics.handle.have_piece(i)) {
                metrics.handle.piece_priority(i, lt::dont_download);
            }
        }
    }
    
    LOG_WARN("All downloads paused due to {}", reason);
}

void PieceManager::download_pieces(uint64_t available_bytes) {
    if (download_queue_.empty()) {
        LOG_DEBUG("No pieces available to download");
        return;
    }
    
    uint64_t allocated = 0;
    int pieces_requested = 0;
    
    // Make a copy of the queue to iterate
    std::priority_queue<PieceInfo> temp_queue = download_queue_;
    
    while (!temp_queue.empty() && allocated < available_bytes) {
        PieceInfo piece = temp_queue.top();
        temp_queue.pop();
        
        // Check if this piece will fit
        if (piece.size_bytes > (available_bytes - allocated)) {
            continue;  // Skip pieces that won't fit
        }
        
        // Find the torrent
        auto it = torrents_.find(piece.info_hash);
        if (it == torrents_.end() || !it->second.handle.is_valid()) {
            continue;
        }
        
        // Set piece priority to high
        it->second.handle.piece_priority(piece.piece_index, lt::top_priority);
        
        allocated += piece.size_bytes;
        pieces_requested++;
        
        LOG_DEBUG("Prioritized piece {}/{} from {} (priority={:.6f}, size={})",
                  piece.piece_index, it->second.total_pieces, it->second.name,
                  piece.priority, utils::format_bytes(piece.size_bytes));
    }
    
    if (pieces_requested > 0) {
        LOG_INFO("Prioritized {} pieces for download ({} allocated)",
                 pieces_requested, utils::format_bytes(allocated));
    }
}

void PieceManager::delete_pieces(uint64_t bytes_to_free) {
    LOG_WARN("Need to free {} to meet storage requirements", 
             utils::format_bytes(bytes_to_free));
    
    // Get the data directory from config
    std::filesystem::path data_dir = config_.paths.data_directory;
    if (!std::filesystem::exists(data_dir)) {
        LOG_WARN("Data directory doesn't exist: {}", data_dir.string());
        return;
    }
    
    // Collect all files in the data directory with their sizes and modification times
    std::vector<std::tuple<std::filesystem::path, uint64_t, std::filesystem::file_time_type>> files;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(data_dir)) {
            if (entry.is_regular_file()) {
                files.push_back({
                    entry.path(),
                    entry.file_size(),
                    entry.last_write_time()
                });
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error scanning data directory {}: {}", data_dir.string(), e.what());
        return;
    }
    
    if (files.empty()) {
        LOG_WARN("No files to delete in data directory");
        return;
    }
    
    // Sort by modification time (oldest first)
    std::sort(files.begin(), files.end(), 
        [](const auto& a, const auto& b) {
            return std::get<2>(a) < std::get<2>(b);
        });
    
    uint64_t freed = 0;
    int files_deleted = 0;
    
    for (const auto& [file_path, file_size, mtime] : files) {
        if (freed >= bytes_to_free) break;
        
        try {
            std::filesystem::remove(file_path);
            freed += file_size;
            files_deleted++;
            LOG_DEBUG("Deleted file: {} ({})", file_path.string(), utils::format_bytes(file_size));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to delete {}: {}", file_path.string(), e.what());
        }
    }
    
    LOG_WARN("Deleted {} files, freed {} (target: {})",
             files_deleted, utils::format_bytes(freed), 
             utils::format_bytes(bytes_to_free));
}

} // namespace levin
