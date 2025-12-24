# Fetch Catch2 for testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Test sources that need to be compiled
set(TEST_SOURCES
    test_config.cpp
    test_disk_monitor.cpp
    test_utils.cpp
)

# Create test executable
add_executable(levin_tests ${TEST_SOURCES})

target_link_libraries(levin_tests
    Catch2::Catch2WithMain
    ${Boost_LIBRARIES}
    ${LIBTORRENT_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Add the src directory to include path
target_include_directories(levin_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Compile the specific source files we need for testing
target_sources(levin_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/config.cpp
    ${CMAKE_SOURCE_DIR}/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/disk_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/utils.cpp
)

# Discover tests
include(CTest)
include(Catch)
catch_discover_tests(levin_tests)
