#include "utils.hpp"
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
#include <stdexcept>
#include <filesystem>

namespace levin {
namespace utils {

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

std::string format_duration(uint64_t seconds) {
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }

    uint64_t minutes = seconds / 60;
    if (minutes < 60) {
        uint64_t secs = seconds % 60;
        return std::to_string(minutes) + "m " + std::to_string(secs) + "s";
    }

    uint64_t hours = minutes / 60;
    minutes = minutes % 60;
    if (hours < 24) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }

    uint64_t days = hours / 24;
    hours = hours % 24;
    return std::to_string(days) + "d " + std::to_string(hours) + "h " + 
           std::to_string(minutes) + "m";
}

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
}

bool directory_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

bool create_directory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

std::string get_xdg_path(const char* xdg_var, 
                         const char* fallback, 
                         const char* subdir) {
    const char* xdg_path = std::getenv(xdg_var);
    const char* home = std::getenv("HOME");
    
    std::string base;
    if (xdg_path && xdg_path[0] != '\0') {
        base = xdg_path;
    } else if (home) {
        base = std::string(home) + "/" + fallback;
    } else {
        throw std::runtime_error("Cannot determine home directory");
    }
    
    if (subdir) {
        base += std::string("/") + subdir;
    }
    
    return base;
}

bool ensure_directory(const std::string& path) {
    std::filesystem::path dir(path);
    
    if (std::filesystem::exists(dir)) {
        return std::filesystem::is_directory(dir);
    }
    
    std::error_code ec;
    return std::filesystem::create_directories(dir, ec);
}

} // namespace utils
} // namespace levin
