#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

namespace levin {

/**
 * Logger singleton wrapper around spdlog.
 * Provides easy access to logging throughout the application.
 * Thread-safe initialization and access.
 */
class Logger {
public:
    /**
     * Initialize the logger with file path and log level.
     * 
     * @param log_file Path to log file
     * @param log_level Log level string ("trace", "debug", "info", "warn", "error", "critical")
     * @param max_size Maximum log file size before rotation (default: 10MB)
     * @param max_files Maximum number of rotated log files to keep (default: 3)
     */
    static void init(const std::string& log_file, 
                    const std::string& log_level,
                    size_t max_size = 1024 * 1024 * 10,
                    size_t max_files = 3);

    /**
     * Get the logger instance.
     * Returns nullptr if logger is not initialized or has been shut down.
     * Callers should check for nullptr before using.
     */
    static std::shared_ptr<spdlog::logger> get();

    /**
     * Check if logger is initialized and available.
     */
    static bool is_available();

    /**
     * Shutdown the logger (flush and close files).
     */
    static void shutdown();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::mutex mutex_;
    static std::atomic<bool> initialized_;
    static std::atomic<bool> shutting_down_;
};

} // namespace levin

// Convenience macros for logging - now check availability first
#define LOG_TRACE(...) do { auto _l = levin::Logger::get(); if (_l) _l->trace(__VA_ARGS__); } while(0)
#define LOG_DEBUG(...) do { auto _l = levin::Logger::get(); if (_l) _l->debug(__VA_ARGS__); } while(0)
#define LOG_INFO(...) do { auto _l = levin::Logger::get(); if (_l) _l->info(__VA_ARGS__); } while(0)
#define LOG_WARN(...) do { auto _l = levin::Logger::get(); if (_l) _l->warn(__VA_ARGS__); } while(0)
#define LOG_ERROR(...) do { auto _l = levin::Logger::get(); if (_l) _l->error(__VA_ARGS__); } while(0)
#define LOG_CRITICAL(...) do { auto _l = levin::Logger::get(); if (_l) _l->critical(__VA_ARGS__); } while(0)
