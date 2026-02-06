#pragma once

#include <functional>

namespace levin {

enum class State {
    OFF,
    PAUSED,
    IDLE,
    SEEDING,
    DOWNLOADING
};

// Callback: (old_state, new_state)
using StateCallback = std::function<void(State, State)>;

class StateMachine {
public:
    StateMachine();

    State state() const;

    void update_enabled(bool enabled);
    void update_battery(bool ok);
    void update_network(bool ok);
    void update_has_torrents(bool has);
    void update_storage(bool ok);

    void set_callback(StateCallback cb);

private:
    void evaluate();

    State current_state_;
    bool enabled_;
    bool battery_ok_;
    bool network_ok_;
    bool has_torrents_;
    bool storage_ok_;
    StateCallback callback_;
};

} // namespace levin
