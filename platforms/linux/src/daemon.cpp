#include "daemon.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace levin::linux_shell {

// ---------------------------------------------------------------------------
// Signal handling
// ---------------------------------------------------------------------------

static volatile sig_atomic_t g_shutdown = 0;
static volatile sig_atomic_t g_reload = 0;

static void handle_term(int /*sig*/) {
    g_shutdown = 1;
}

static void handle_hup(int /*sig*/) {
    g_reload = 1;
}

void install_signal_handlers() {
    struct sigaction sa{};
    sa.sa_handler = handle_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    struct sigaction sa_hup{};
    sa_hup.sa_handler = handle_hup;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, nullptr);

    // Ignore SIGPIPE so writes to closed sockets don't kill us
    signal(SIGPIPE, SIG_IGN);
}

bool shutdown_requested() {
    return g_shutdown != 0;
}

bool reload_requested() {
    if (g_reload) {
        g_reload = 0;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// PID file management
// ---------------------------------------------------------------------------

int write_pid_file(const std::string& path) {
    std::ofstream f(path);
    if (!f) return -1;
    f << ::getpid() << "\n";
    return f.good() ? 0 : -1;
}

void remove_pid_file(const std::string& path) {
    ::unlink(path.c_str());
}

pid_t read_pid_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return -1;
    pid_t pid = -1;
    f >> pid;
    if (f.fail()) return -1;
    return pid;
}

bool is_process_running(pid_t pid) {
    if (pid <= 0) return false;
    // kill(pid, 0) checks if process exists without sending a signal
    return ::kill(pid, 0) == 0 || errno == EPERM;
}

// ---------------------------------------------------------------------------
// Daemonize
// ---------------------------------------------------------------------------

int daemonize(const std::string& pid_file) {
    // First fork: detach from controlling terminal
    pid_t pid = ::fork();
    if (pid < 0) return -1;
    if (pid > 0) {
        // Parent exits
        ::_exit(0);
    }

    // Child: become session leader
    if (::setsid() < 0) return -1;

    // Second fork: prevent reacquiring a controlling terminal
    pid = ::fork();
    if (pid < 0) return -1;
    if (pid > 0) {
        // First child exits
        ::_exit(0);
    }

    // Grandchild: the actual daemon process
    // Set file creation mask
    ::umask(0027);

    // Change working directory to root to avoid holding a mount busy
    if (::chdir("/") < 0) {
        // Non-fatal, but unusual
    }

    // Redirect stdin/stdout/stderr to /dev/null
    int devnull = ::open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        ::dup2(devnull, STDIN_FILENO);
        ::dup2(devnull, STDOUT_FILENO);
        ::dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) ::close(devnull);
    }

    // Write PID file
    if (write_pid_file(pid_file) < 0) return -1;

    return 0;
}

} // namespace levin::linux_shell
