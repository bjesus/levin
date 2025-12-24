cmake_minimum_required(VERSION 3.15)
project(levin VERSION 0.1.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define project version for code
add_compile_definitions(PROJECT_VERSION="${PROJECT_VERSION}")

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required system packages
find_package(Boost 1.70 REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Find libtorrent-rasterbar
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBTORRENT REQUIRED libtorrent-rasterbar>=2.0)

# Fetch external dependencies
include(FetchContent)

# toml11 - Header-only TOML parser
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG v3.8.1
)
FetchContent_MakeAvailable(toml11)

# spdlog - Fast logging library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann_json - JSON library
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${LIBTORRENT_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${toml11_SOURCE_DIR}
)

# Link directories
link_directories(
    ${LIBTORRENT_LIBRARY_DIRS}
)

# Unified binary source files (includes both daemon and CLI client)
set(LEVIN_SOURCES
    src/main.cpp
    src/cli_client.cpp
    src/config.cpp
    src/logger.cpp
    src/daemon.cpp
    src/session.cpp
    src/piece_manager.cpp
    src/disk_monitor.cpp
    src/torrent_watcher.cpp
    src/cli_server.cpp
    src/statistics.cpp
    src/utils.cpp
)

# Unified levin executable (daemon + CLI client)
add_executable(levin ${LEVIN_SOURCES})

target_link_libraries(levin
    ${Boost_LIBRARIES}
    ${LIBTORRENT_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# CLI client is now integrated into main binary
# add_subdirectory(cli)

# Tests (optional)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS levin
    RUNTIME DESTINATION bin
)

install(FILES scripts/levin.service
    DESTINATION /usr/lib/systemd/user
    OPTIONAL
)

# Print configuration summary
message(STATUS "")
message(STATUS "Levin Configuration Summary")
message(STATUS "===========================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  Boost: ${Boost_VERSION}")
message(STATUS "  libtorrent-rasterbar: ${LIBTORRENT_VERSION}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  toml11: v3.8.1")
message(STATUS "  spdlog: v1.12.0")
message(STATUS "  nlohmann_json: v3.11.3")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "==============================")
message(STATUS "")
