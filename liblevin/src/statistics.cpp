#include "statistics.h"
#include <cstdio>
#include <cstring>

namespace levin {

// File format: simple binary header + two uint64_t values.
// Magic: "LVST" (4 bytes), version: 1 (4 bytes), then total_downloaded (8), total_uploaded (8).
static const char MAGIC[4] = {'L', 'V', 'S', 'T'};
static const uint32_t VERSION = 1;
static const size_t FILE_SIZE = 4 + 4 + 8 + 8; // 24 bytes

bool Statistics::load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    uint8_t buf[FILE_SIZE];
    size_t n = std::fread(buf, 1, FILE_SIZE, f);
    std::fclose(f);

    if (n != FILE_SIZE) return false;
    if (std::memcmp(buf, MAGIC, 4) != 0) return false;

    uint32_t ver;
    std::memcpy(&ver, buf + 4, 4);
    if (ver != VERSION) return false;

    std::memcpy(&total_downloaded, buf + 8, 8);
    std::memcpy(&total_uploaded, buf + 16, 8);

    return true;
}

bool Statistics::save(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    uint8_t buf[FILE_SIZE];
    std::memcpy(buf, MAGIC, 4);
    uint32_t ver = VERSION;
    std::memcpy(buf + 4, &ver, 4);
    std::memcpy(buf + 8, &total_downloaded, 8);
    std::memcpy(buf + 16, &total_uploaded, 8);

    size_t n = std::fwrite(buf, 1, FILE_SIZE, f);
    std::fclose(f);

    return n == FILE_SIZE;
}

void Statistics::update(uint64_t base_downloaded, uint64_t base_uploaded,
                        uint64_t current_session_downloaded, uint64_t current_session_uploaded) {
    session_downloaded = current_session_downloaded;
    session_uploaded = current_session_uploaded;
    total_downloaded = base_downloaded + current_session_downloaded;
    total_uploaded = base_uploaded + current_session_uploaded;
}

} // namespace levin
