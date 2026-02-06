#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace levin::linux_shell {

// Simple JSON-ish message: a flat map of string key-value pairs
using Message = std::map<std::string, std::string>;

// Serialize/deserialize messages (simple JSON objects, flat key:value)
std::string serialize_message(const Message& msg);
Message deserialize_message(const std::string& data);

// IPC Server: listens on a Unix socket, calls handler for each message
class IpcServer {
public:
    using Handler = std::function<Message(const Message&)>;

    IpcServer();
    ~IpcServer();

    // Start listening. Returns 0 on success.
    int start(const std::string& socket_path, Handler handler);
    void stop();

    // Process pending connections (non-blocking). Call from tick loop.
    void poll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// IPC Client: connects to daemon socket, sends a message, gets reply
class IpcClient {
public:
    // Send a message and get reply. Returns empty map on error.
    static Message send(const std::string& socket_path, const Message& request);
};

} // namespace levin::linux_shell
