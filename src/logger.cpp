#include "logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <stdexcept>

namespace levin {

// Static member initialization
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
std::mutex Logger::mutex_;
std::atomic<bool> Logger::initialized_{false};
std::atomic<bool> Logger::shutting_down_{false};

void Logger::init(const std::string& log_file, 
                 const std::string& log_level,
                 size_t max_size,
                 size_t max_files) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Don't re-initialize if already initialized
    if (initialized_.load()) {
        return;
    }
    
    try {
        // Create rotating file sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, max_size, max_files);

        // Create logger
        logger_ = std::make_shared<spdlog::logger>("levin", file_sink);

        // Set log level
        if (log_level == "trace") {
            logger_->set_level(spdlog::level::trace);
        } else if (log_level == "debug") {
            logger_->set_level(spdlog::level::debug);
        } else if (log_level == "info") {
            logger_->set_level(spdlog::level::info);
        } else if (log_level == "warn") {
            logger_->set_level(spdlog::level::warn);
        } else if (log_level == "error") {
            logger_->set_level(spdlog::level::err);
        } else if (log_level == "critical") {
            logger_->set_level(spdlog::level::critical);
        } else {
            logger_->set_level(spdlog::level::info); // Default
        }

        // Set pattern: [timestamp] [level] message
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        // Flush on info level and above
        logger_->flush_on(spdlog::level::info);

        initialized_.store(true);
        
        logger_->info("Logger initialized: file={}, level={}", log_file, log_level);

    } catch (const spdlog::spdlog_ex& ex) {
        throw std::runtime_error("Failed to initialize logger: " + std::string(ex.what()));
    }
}

std::shared_ptr<spdlog::logger> Logger::get() {
    // Quick check without lock - if we're shutting down or not initialized, return nullptr
    if (shutting_down_.load() || !initialized_.load()) {
        return nullptr;
    }
    
    // Return the logger - it's a shared_ptr so it's thread-safe to copy
    return logger_;
}

bool Logger::is_available() {
    return initialized_.load() && !shutting_down_.load() && logger_ != nullptr;
}

void Logger::shutdown() {
    // Mark as shutting down first to prevent new accesses
    shutting_down_.store(true);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logger_) {
        logger_->info("Logger shutting down");
        logger_->flush();
        spdlog::drop_all();
        logger_ = nullptr;
    }
    
    initialized_.store(false);
}

} // namespace levin
