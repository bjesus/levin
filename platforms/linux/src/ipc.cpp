#include "ipc.h"

#include <cerrno>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>

namespace levin::linux_shell {

// ---------------------------------------------------------------------------
// Minimal JSON serialization for flat string maps
// ---------------------------------------------------------------------------

// Escape a string for JSON: handle \, ", \n, \r, \t
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Unescape a JSON string value (inverse of json_escape)
static std::string json_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '\\': out += '\\'; break;
                case '"':  out += '"';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += '\\'; out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string serialize_message(const Message& msg) {
    std::string out = "{";
    bool first = true;
    for (const auto& kv : msg) {
        if (!first) out += ",";
        first = false;
        out += "\"";
        out += json_escape(kv.first);
        out += "\":\"";
        out += json_escape(kv.second);
        out += "\"";
    }
    out += "}\n";
    return out;
}

// Skip whitespace, return current position
static size_t skip_ws(const std::string& data, size_t pos) {
    while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t' ||
                                  data[pos] == '\n' || data[pos] == '\r')) {
        ++pos;
    }
    return pos;
}

// Parse a JSON quoted string starting at pos (which must point to the opening ").
// Returns the unescaped string and advances pos past the closing ".
static std::string parse_string(const std::string& data, size_t& pos) {
    if (pos >= data.size() || data[pos] != '"') return "";
    ++pos; // skip opening "

    std::string raw;
    while (pos < data.size() && data[pos] != '"') {
        if (data[pos] == '\\' && pos + 1 < data.size()) {
            raw += data[pos];
            raw += data[pos + 1];
            pos += 2;
        } else {
            raw += data[pos];
            ++pos;
        }
    }
    if (pos < data.size()) ++pos; // skip closing "
    return json_unescape(raw);
}

Message deserialize_message(const std::string& data) {
    Message msg;
    size_t pos = skip_ws(data, 0);
    if (pos >= data.size() || data[pos] != '{') return msg;
    ++pos; // skip {

    while (true) {
        pos = skip_ws(data, pos);
        if (pos >= data.size() || data[pos] == '}') break;

        // Expect a key string
        std::string key = parse_string(data, pos);
        pos = skip_ws(data, pos);
        if (pos >= data.size() || data[pos] != ':') break;
        ++pos; // skip :
        pos = skip_ws(data, pos);

        // Expect a value string
        std::string value = parse_string(data, pos);
        msg[key] = value;

        pos = skip_ws(data, pos);
        if (pos < data.size() && data[pos] == ',') ++pos;
    }
    return msg;
}

// ---------------------------------------------------------------------------
// IPC Server implementation
// ---------------------------------------------------------------------------

static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

struct IpcServer::Impl {
    int listen_fd = -1;
    std::string socket_path;
    Handler handler;
};

IpcServer::IpcServer() : impl_(std::make_unique<Impl>()) {}

IpcServer::~IpcServer() {
    stop();
}

int IpcServer::start(const std::string& socket_path, Handler handler) {
    impl_->handler = std::move(handler);
    impl_->socket_path = socket_path;

    // Remove stale socket if it exists
    ::unlink(socket_path.c_str());

    impl_->listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (impl_->listen_fd < 0) return -1;

    if (!set_nonblocking(impl_->listen_fd)) {
        ::close(impl_->listen_fd);
        impl_->listen_fd = -1;
        return -1;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path.size() >= sizeof(addr.sun_path)) {
        ::close(impl_->listen_fd);
        impl_->listen_fd = -1;
        return -1;
    }
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(impl_->listen_fd, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        ::close(impl_->listen_fd);
        impl_->listen_fd = -1;
        return -1;
    }

    if (::listen(impl_->listen_fd, 5) < 0) {
        ::close(impl_->listen_fd);
        ::unlink(socket_path.c_str());
        impl_->listen_fd = -1;
        return -1;
    }

    return 0;
}

void IpcServer::stop() {
    if (impl_->listen_fd >= 0) {
        ::close(impl_->listen_fd);
        impl_->listen_fd = -1;
    }
    if (!impl_->socket_path.empty()) {
        ::unlink(impl_->socket_path.c_str());
        impl_->socket_path.clear();
    }
}

void IpcServer::poll() {
    if (impl_->listen_fd < 0) return;

    // Accept all pending connections (non-blocking)
    while (true) {
        struct pollfd pfd{};
        pfd.fd = impl_->listen_fd;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 0); // timeout 0 = non-blocking
        if (ret <= 0) break;

        int client_fd = ::accept(impl_->listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }

        // Read the full request line from the client.
        // Use a short timeout to avoid blocking the daemon forever.
        struct timeval tv{};
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        std::string request_data;
        char buf[4096];
        while (true) {
            ssize_t n = ::read(client_fd, buf, sizeof(buf));
            if (n <= 0) break;
            request_data.append(buf, static_cast<size_t>(n));
            // Messages are newline-terminated
            if (request_data.find('\n') != std::string::npos) break;
        }

        if (!request_data.empty() && impl_->handler) {
            Message request = deserialize_message(request_data);
            Message reply = impl_->handler(request);
            std::string reply_data = serialize_message(reply);
            // Write full reply; ignore partial write errors on local socket
            size_t written = 0;
            while (written < reply_data.size()) {
                ssize_t n = ::write(client_fd, reply_data.data() + written,
                                    reply_data.size() - written);
                if (n <= 0) break;
                written += static_cast<size_t>(n);
            }
        }

        ::close(client_fd);
    }
}

// ---------------------------------------------------------------------------
// IPC Client implementation
// ---------------------------------------------------------------------------

Message IpcClient::send(const std::string& socket_path, const Message& request) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return {};

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path.size() >= sizeof(addr.sun_path)) {
        ::close(fd);
        return {};
    }
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return {};
    }

    // Set a receive timeout
    struct timeval tv{};
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Send the request
    std::string data = serialize_message(request);
    size_t written = 0;
    while (written < data.size()) {
        ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        if (n <= 0) {
            ::close(fd);
            return {};
        }
        written += static_cast<size_t>(n);
    }

    // Read the reply
    std::string reply_data;
    char buf[4096];
    while (true) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        reply_data.append(buf, static_cast<size_t>(n));
        if (reply_data.find('\n') != std::string::npos) break;
    }

    ::close(fd);
    if (reply_data.empty()) return {};
    return deserialize_message(reply_data);
}

} // namespace levin::linux_shell
