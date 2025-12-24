#pragma once

#include <string>
#include <cstdint>

namespace levin {
namespace utils {

/**
 * Format bytes to human-readable string (e.g., "1.5 GB", "256 MB")
 */
std::string format_bytes(uint64_t bytes);

/**
 * Format duration in seconds to human-readable string (e.g., "2d 5h 30m")
 */
std::string format_duration(uint64_t seconds);

/**
 * Check if a file exists
 */
bool file_exists(const std::string& path);

/**
 * Check if a directory exists
 */
bool directory_exists(const std::string& path);

/**
 * Create directory (and parents if needed)
 */
bool create_directory(const std::string& path);

/**
 * Get XDG-compliant directory path with fallback.
 * @param xdg_var XDG environment variable name (e.g., "XDG_CONFIG_HOME")
 * @param fallback Fallback path relative to HOME (e.g., ".config")
 * @param subdir Optional subdirectory (e.g., "levin")
 * @return Full path to directory
 */
std::string get_xdg_path(const char* xdg_var, 
                         const char* fallback, 
                         const char* subdir = nullptr);

/**
 * Ensure directory exists, creating it if necessary.
 * @param path Directory path to create
 * @return true if directory exists or was created successfully
 */
bool ensure_directory(const std::string& path);

} // namespace utils
} // namespace levin
