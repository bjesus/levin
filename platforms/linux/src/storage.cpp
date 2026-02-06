#include "storage.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>

namespace levin::linux_shell {

StorageInfo get_storage_info(const std::string& path) {
    struct statvfs buf{};
    if (statvfs(path.c_str(), &buf) != 0) {
        return {0, 0};
    }

    uint64_t block_size = buf.f_frsize ? buf.f_frsize : buf.f_bsize;
    return {
        static_cast<uint64_t>(buf.f_blocks) * block_size,
        static_cast<uint64_t>(buf.f_bavail) * block_size
    };
}

namespace {

// Recursively accumulate disk usage (actual blocks, not apparent size).
// Returns 0 on any error for a subtree (best effort).
uint64_t accumulate_usage(const std::string& dir_path) {
    uint64_t total = 0;

    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (entry->d_name[0] == '.') {
            if (entry->d_name[1] == '\0') continue;
            if (entry->d_name[1] == '.' && entry->d_name[2] == '\0') continue;
        }

        std::string full_path = dir_path + "/" + entry->d_name;

        struct stat st{};
        if (lstat(full_path.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectories
            total += accumulate_usage(full_path);
        } else if (S_ISREG(st.st_mode)) {
            // st_blocks is in 512-byte units on Linux
            total += static_cast<uint64_t>(st.st_blocks) * 512;
        }
        // Symlinks, devices, etc. are intentionally skipped.
    }

    closedir(dir);
    return total;
}

} // anonymous namespace

uint64_t get_disk_usage(const std::string& path) {
    struct stat st{};
    if (lstat(path.c_str(), &st) != 0) return 0;

    if (S_ISDIR(st.st_mode)) {
        return accumulate_usage(path);
    }
    if (S_ISREG(st.st_mode)) {
        return static_cast<uint64_t>(st.st_blocks) * 512;
    }
    return 0;
}

} // namespace levin::linux_shell
