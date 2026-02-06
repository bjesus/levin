cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# --- Catch2 (for tests) ---
if(LEVIN_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# --- liblevin sources ---
set(LIBLEVIN_SOURCES
    src/state_machine.cpp
    src/disk_manager.cpp
    src/levin.cpp
    src/stub_torrent_session.cpp
)

# Phase 5+: add more sources as they are implemented
# src/torrent_session.cpp    # real libtorrent session
# src/torrent_watcher.cpp
# src/annas_archive.cpp
# src/config.cpp
# src/statistics.cpp

add_library(levin STATIC ${LIBLEVIN_SOURCES})
target_include_directories(levin PUBLIC include)
target_compile_features(levin PUBLIC cxx_std_17)

# --- Tests ---
if(LEVIN_BUILD_TESTS)
    enable_testing()

    # Phase 1: State machine tests
    add_executable(test_state_machine tests/test_state_machine.cpp)
    target_link_libraries(test_state_machine PRIVATE levin Catch2::Catch2WithMain)
    add_test(NAME StateMachine COMMAND test_state_machine)

    # Phase 2: Disk budget tests
    add_executable(test_disk_manager tests/test_disk_manager.cpp)
    target_link_libraries(test_disk_manager PRIVATE levin Catch2::Catch2WithMain)
    add_test(NAME DiskManager COMMAND test_disk_manager)

    # Phase 3: Disk deletion tests
    add_executable(test_disk_deletion tests/test_disk_deletion.cpp)
    target_link_libraries(test_disk_deletion PRIVATE levin Catch2::Catch2WithMain)
    add_test(NAME DiskDeletion COMMAND test_disk_deletion)

    # Phase 4: C API tests
    add_executable(test_c_api tests/test_c_api.cpp)
    target_link_libraries(test_c_api PRIVATE levin Catch2::Catch2WithMain)
    add_test(NAME CAPI COMMAND test_c_api)
endif()
