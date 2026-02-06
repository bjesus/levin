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

# --- libtorrent (when not using stub) ---
if(NOT LEVIN_USE_STUB_SESSION)
    find_package(Boost REQUIRED COMPONENTS system)
    find_package(OpenSSL REQUIRED)

    FetchContent_Declare(
        libtorrent
        GIT_REPOSITORY https://github.com/arvidn/libtorrent.git
        GIT_TAG        RC_2_0
        GIT_SHALLOW    TRUE
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(webtorrent ON CACHE BOOL "Enable WebTorrent support" FORCE)
    set(deprecated-functions OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libtorrent)
endif()

# --- libcurl (for Anna's Archive client) ---
find_package(CURL REQUIRED)

# --- liblevin sources ---
set(LIBLEVIN_SOURCES
    src/state_machine.cpp
    src/disk_manager.cpp
    src/levin.cpp
    src/torrent_watcher.cpp
    src/annas_archive.cpp
    src/statistics.cpp
)

if(LEVIN_USE_STUB_SESSION)
    list(APPEND LIBLEVIN_SOURCES src/stub_torrent_session.cpp)
    message(STATUS "Levin: using stub torrent session (no libtorrent)")
else()
    list(APPEND LIBLEVIN_SOURCES
        src/stub_torrent_session.cpp   # still needed for potential runtime switching
        src/torrent_session.cpp
    )
    message(STATUS "Levin: using real libtorrent session with WebTorrent")
endif()

add_library(levin STATIC ${LIBLEVIN_SOURCES})
target_include_directories(levin PUBLIC include)
target_compile_features(levin PUBLIC cxx_std_17)

target_link_libraries(levin PUBLIC CURL::libcurl)

if(LEVIN_USE_STUB_SESSION)
    target_compile_definitions(levin PUBLIC LEVIN_USE_STUB_SESSION)
else()
    target_link_libraries(levin PUBLIC torrent-rasterbar)
endif()

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

    # Phase 5: Torrent session tests
    add_executable(test_torrent_session tests/test_torrent_session.cpp)
    target_link_libraries(test_torrent_session PRIVATE levin Catch2::Catch2WithMain)
    add_test(NAME TorrentSession COMMAND test_torrent_session)

    # Statistics tests
    add_executable(test_statistics tests/test_statistics.cpp)
    target_link_libraries(test_statistics PRIVATE levin Catch2::Catch2WithMain)
    add_test(NAME Statistics COMMAND test_statistics)
endif()
