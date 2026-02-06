#include "state_machine.h"

namespace levin {

StateMachine::StateMachine()
    : current_state_(State::OFF)
    , enabled_(false)
    , battery_ok_(false)
    , network_ok_(false)
    , has_torrents_(false)
    , storage_ok_(false)
    , callback_(nullptr)
{
}

State StateMachine::state() const {
    return current_state_;
}

void StateMachine::update_enabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    evaluate();
}

void StateMachine::update_battery(bool ok) {
    if (battery_ok_ == ok) return;
    battery_ok_ = ok;
    evaluate();
}

void StateMachine::update_network(bool ok) {
    if (network_ok_ == ok) return;
    network_ok_ = ok;
    evaluate();
}

void StateMachine::update_has_torrents(bool has) {
    if (has_torrents_ == has) return;
    has_torrents_ = has;
    evaluate();
}

void StateMachine::update_storage(bool ok) {
    if (storage_ok_ == ok) return;
    storage_ok_ = ok;
    evaluate();
}

void StateMachine::set_callback(StateCallback cb) {
    callback_ = std::move(cb);
}

void StateMachine::evaluate() {
    // Priority-ordered evaluation per design doc
    State new_state;

    if (!enabled_) {
        new_state = State::OFF;
    } else if (!battery_ok_ || !network_ok_) {
        new_state = State::PAUSED;
    } else if (!has_torrents_) {
        new_state = State::IDLE;
    } else if (!storage_ok_) {
        new_state = State::SEEDING;
    } else {
        new_state = State::DOWNLOADING;
    }

    if (new_state != current_state_) {
        State old = current_state_;
        current_state_ = new_state;
        if (callback_) {
            callback_(old, new_state);
        }
    }
}

} // namespace levin
