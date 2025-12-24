#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace levin {

/**
 * Logger singleton wrapper around spdlog.
 * Provides easy access to logging throughout the application.
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
     * Must call init() before first use.
     */
    static std::shared_ptr<spdlog::logger> get();

    /**
     * Shutdown the logger (flush and close files).
     */
    static void shutdown();

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace levin

// Convenience macros for logging
#define LOG_TRACE(...) levin::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) levin::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) levin::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...) levin::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) levin::Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) levin::Logger::get()->critical(__VA_ARGS__)
