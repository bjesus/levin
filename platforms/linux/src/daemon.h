#pragma once
#include <string>
#include <sys/types.h>

namespace levin::linux_shell {

// Daemonize the process. Returns 0 in the daemon process, -1 on error.
// The parent process exits.
int daemonize(const std::string& pid_file);

// Write PID file. Returns 0 on success.
int write_pid_file(const std::string& path);

// Remove PID file.
void remove_pid_file(const std::string& path);

// Read PID from file. Returns -1 if not found or invalid.
pid_t read_pid_file(const std::string& path);

// Check if a process with the given PID is running.
bool is_process_running(pid_t pid);

// Install signal handlers for SIGTERM/SIGINT (clean shutdown) and SIGHUP (reload).
// Sets a global flag that should be checked in the main loop.
void install_signal_handlers();

// Check if shutdown was requested
bool shutdown_requested();

// Check if reload was requested (and clear the flag)
bool reload_requested();

} // namespace levin::linux_shell
