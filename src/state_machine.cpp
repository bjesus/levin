#include "state_machine.hpp"
#include "logger.hpp"
#include <sstream>

namespace levin {

StateMachine::StateMachine(StateChangeCallback callback)
    : current_state_(LevinState::OFF)
    , callback_(std::move(callback))
    , user_enabled_(false)
    , battery_allows_(true)
    , network_allows_(true)
    , has_torrents_(false)
    , storage_allows_(true) {
}

LevinState StateMachine::get_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_;
}

void StateMachine::set_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (user_enabled_ != enabled) {
        LOG_INFO("User enabled: {} → {}", user_enabled_, enabled);
        user_enabled_ = enabled;
        recompute_state();
    }
}

void StateMachine::update_battery_condition(bool allows) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (battery_allows_ != allows) {
        LOG_DEBUG("Battery condition: {} → {}", battery_allows_, allows);
        battery_allows_ = allows;
        recompute_state();
    }
}

void StateMachine::update_network_condition(bool allows) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (network_allows_ != allows) {
        LOG_DEBUG("Network condition: {} → {}", network_allows_, allows);
        network_allows_ = allows;
        recompute_state();
    }
}

void StateMachine::update_has_torrents(bool has) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (has_torrents_ != has) {
        LOG_DEBUG("Has torrents: {} → {}", has_torrents_, has);
        has_torrents_ = has;
        recompute_state();
    }
}

void StateMachine::update_storage_condition(bool allows) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (storage_allows_ != allows) {
        LOG_DEBUG("Storage condition: {} → {}", storage_allows_, allows);
        storage_allows_ = allows;
        recompute_state();
    }
}

void StateMachine::recompute() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_DEBUG("Forcing state recomputation");
    recompute_state();
}

bool StateMachine::allows_network() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_ == LevinState::IDLE ||
           current_state_ == LevinState::SEEDING ||
           current_state_ == LevinState::DOWNLOADING;
}

bool StateMachine::requires_session() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_ == LevinState::IDLE ||
           current_state_ == LevinState::SEEDING ||
           current_state_ == LevinState::DOWNLOADING;
}

LevinState StateMachine::determine_state() const {
    // Must be called with mutex already locked
    
    // Priority 1: User disabled → OFF
    if (!user_enabled_) {
        return LevinState::OFF;
    }
    
    // Priority 2: Conditions not met → PAUSED
    if (!battery_allows_ || !network_allows_) {
        return LevinState::PAUSED;
    }
    
    // Priority 3: No torrents → IDLE
    if (!has_torrents_) {
        return LevinState::IDLE;
    }
    
    // Priority 4: Storage over limit → SEEDING
    if (!storage_allows_) {
        return LevinState::SEEDING;
    }
    
    // Default: Full operation → DOWNLOADING
    return LevinState::DOWNLOADING;
}

void StateMachine::recompute_state() {
    // Must be called with mutex already locked
    
    LevinState new_state = determine_state();
    
    if (new_state != current_state_) {
        LevinState old_state = current_state_;
        current_state_ = new_state;
        
        // Log transition with all conditions
        std::ostringstream oss;
        oss << "STATE TRANSITION: " << state_to_string(old_state)
            << " → " << state_to_string(new_state)
            << " [enabled=" << user_enabled_
            << ", battery=" << battery_allows_
            << ", network=" << network_allows_
            << ", torrents=" << has_torrents_
            << ", storage=" << storage_allows_ << "]";
        LOG_INFO(oss.str());
        
        // Trigger callback (with mutex still held to ensure atomicity)
        if (callback_) {
            callback_(old_state, new_state);
        }
    }
}

} // namespace levin
