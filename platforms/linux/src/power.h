#pragma once

namespace levin::linux_shell {

// Returns true if the system is on AC power (plugged in).
// On desktop systems without battery, returns true.
bool is_on_ac_power();

}
