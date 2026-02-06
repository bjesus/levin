#include <catch2/catch_test_macros.hpp>
#include "disk_manager.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

constexpr uint64_t MB = 1024ULL * 1024;

// --- Test Helpers ---

class TempDir {
public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("levin_test_" + std::to_string(counter_++));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    const fs::path& path() const { return path_; }
    operator const fs::path&() const { return path_; }

private:
    fs::path path_;
    static inline int counter_ = 0;
};

static void create_file(const fs::path& path, uint64_t size_bytes) {
    std::ofstream f(path, std::ios::binary);
    // Write in chunks to avoid huge memory allocation
    constexpr size_t chunk_size = 4096;
    std::string chunk(chunk_size, '\0');
    uint64_t remaining = size_bytes;
    while (remaining > 0) {
        size_t to_write = std::min(remaining, static_cast<uint64_t>(chunk_size));
        f.write(chunk.data(), static_cast<std::streamsize>(to_write));
        remaining -= to_write;
    }
}

static uint64_t dir_size(const fs::path& dir) {
    uint64_t total = 0;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            total += fs::file_size(entry.path(), ec);
        }
    }
    return total;
}

// --- Tests ---

TEST_CASE("delete_to_free removes enough data to meet deficit") {
    TempDir dir;
    for (int i = 0; i < 10; i++)
        create_file(dir.path() / ("f" + std::to_string(i)), 10*MB);
    REQUIRE(dir_size(dir) == 100*MB);

    levin::DiskManager dm;
    uint64_t freed = dm.delete_to_free(dir, 30*MB);
    REQUIRE(freed >= 30*MB);
    REQUIRE(dir_size(dir) <= 70*MB);
}

TEST_CASE("delete_to_free removes nothing when deficit is zero") {
    TempDir dir;
    create_file(dir.path() / "keep.dat", 10*MB);

    levin::DiskManager dm;
    uint64_t freed = dm.delete_to_free(dir, 0);
    REQUIRE(freed == 0);
    REQUIRE(fs::exists(dir.path() / "keep.dat"));
}

TEST_CASE("delete_to_free handles empty directory") {
    TempDir dir;
    levin::DiskManager dm;
    uint64_t freed = dm.delete_to_free(dir, 10*MB);
    REQUIRE(freed == 0);
}

TEST_CASE("delete_to_free does not delete more than necessary") {
    TempDir dir;
    for (int i = 0; i < 5; i++)
        create_file(dir.path() / ("f" + std::to_string(i)), 20*MB);

    levin::DiskManager dm;
    dm.delete_to_free(dir, 25*MB);

    int remaining = 0;
    for (auto& e : fs::directory_iterator(dir.path()))
        if (e.is_regular_file()) remaining++;
    REQUIRE(remaining >= 3);  // deleted at most 2 files (40MB >= 25MB)
}
