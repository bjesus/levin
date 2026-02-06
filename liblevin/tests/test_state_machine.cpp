#include <catch2/catch_test_macros.hpp>
#include "state_machine.h"
#include <vector>
#include <utility>

using namespace levin;

TEST_CASE("Initial state is OFF") {
    StateMachine sm;
    REQUIRE(sm.state() == State::OFF);
}

TEST_CASE("Enabling with no conditions met goes to PAUSED") {
    StateMachine sm;
    sm.update_enabled(true);
    REQUIRE(sm.state() == State::PAUSED);
}

TEST_CASE("All conditions met but no torrents goes to IDLE") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    REQUIRE(sm.state() == State::IDLE);
}

TEST_CASE("All conditions met with torrents and storage goes to DOWNLOADING") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);
}

TEST_CASE("Storage full with torrents goes to SEEDING") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(false);
    REQUIRE(sm.state() == State::SEEDING);
}

TEST_CASE("Disabling always goes to OFF regardless of other conditions") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);

    sm.update_enabled(false);
    REQUIRE(sm.state() == State::OFF);
}

TEST_CASE("Battery loss overrides torrent and storage conditions") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);

    sm.update_battery(false);
    REQUIRE(sm.state() == State::PAUSED);
}

TEST_CASE("Network loss transitions to PAUSED") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);

    sm.update_network(false);
    REQUIRE(sm.state() == State::PAUSED);
}

TEST_CASE("State callback fires on transition") {
    StateMachine sm;
    std::vector<std::pair<State, State>> transitions;
    sm.set_callback([&](State o, State n) { transitions.push_back({o, n}); });

    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);

    REQUIRE(transitions.size() >= 1);
    REQUIRE(transitions.back().second == State::IDLE);
}

TEST_CASE("Redundant updates do not fire callback") {
    StateMachine sm;
    int count = 0;
    sm.set_callback([&](State, State) { count++; });

    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    int after_setup = count;

    sm.update_battery(true);  // same value
    sm.update_network(true);  // same value
    REQUIRE(count == after_setup);
}

TEST_CASE("SEEDING to DOWNLOADING when storage freed") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(false);
    REQUIRE(sm.state() == State::SEEDING);

    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);
}

TEST_CASE("Removing all torrents goes to IDLE even if storage ok") {
    StateMachine sm;
    sm.update_enabled(true);
    sm.update_battery(true);
    sm.update_network(true);
    sm.update_has_torrents(true);
    sm.update_storage(true);
    REQUIRE(sm.state() == State::DOWNLOADING);

    sm.update_has_torrents(false);
    REQUIRE(sm.state() == State::IDLE);
}
