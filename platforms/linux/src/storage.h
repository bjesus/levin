#pragma once
#include <cstdint>
#include <string>

namespace levin::linux_shell {

struct StorageInfo {
    uint64_t fs_total;
    uint64_t fs_free;
};

// Get filesystem total/free for the filesystem containing the given path
StorageInfo get_storage_info(const std::string& path);

// Get actual disk usage of a directory (equivalent to `du -s`, measures blocks used)
uint64_t get_disk_usage(const std::string& path);

}
