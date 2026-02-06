#pragma once
#include "liblevin.h"
#include <string>

namespace levin::linux_shell {

struct ShellConfig {
    levin_config_t lib_config;
    std::string log_level;
    // Owned string storage (levin_config_t has const char* pointers into these)
    std::string watch_dir;
    std::string data_dir;
    std::string state_dir;
    std::string stun;
};

// Load config from file. If path is empty, uses default XDG path.
ShellConfig load_config(const std::string& config_path = "");

}
