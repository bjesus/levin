#include "power.h"

#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>

namespace levin::linux_shell {

namespace {

// Read the entire contents of a small sysfs file into a string, trimmed.
std::string read_sysfs(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string contents;
    std::getline(f, contents);
    // Trim trailing whitespace/newlines
    while (!contents.empty() &&
           (contents.back() == '\n' || contents.back() == '\r' ||
            contents.back() == ' '  || contents.back() == '\t')) {
        contents.pop_back();
    }
    return contents;
}

} // anonymous namespace

bool is_on_ac_power() {
    static const char* sysfs_dir = "/sys/class/power_supply";

    DIR* dir = opendir(sysfs_dir);
    if (!dir) {
        // No power supply sysfs -- assume desktop on AC.
        return true;
    }

    bool found_mains = false;
    bool mains_online = false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        std::string base = std::string(sysfs_dir) + "/" + entry->d_name;
        std::string type = read_sysfs(base + "/type");

        if (type == "Mains") {
            found_mains = true;
            std::string online = read_sysfs(base + "/online");
            if (online == "1") {
                mains_online = true;
                break; // At least one mains supply is online -- that's enough.
            }
        }
    }

    closedir(dir);

    if (!found_mains) {
        // Desktop without battery / power supply info -- assume AC.
        return true;
    }

    return mains_online;
}

} // namespace levin::linux_shell
