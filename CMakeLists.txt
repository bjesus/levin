cmake_minimum_required(VERSION 3.15)
project(levin VERSION 0.4.0 LANGUAGES CXX)

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
# Boost: Use BoostConfig.cmake if available (Boost >= 1.70)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Try new-style Boost first (for Boost >= 1.90)
find_package(Boost 1.70 QUIET CONFIG COMPONENTS system filesystem)
if(NOT Boost_FOUND)
    # Fall back to FindBoost module (for older versions)
    find_package(Boost 1.70 REQUIRED COMPONENTS system filesystem)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Find libtorrent-rasterbar
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBTORRENT REQUIRED libtorrent-rasterbar>=2.0)

# Platform-specific dependencies
set(POWER_MONITOR_AVAILABLE FALSE)
if(UNIX AND NOT APPLE)
    # Linux: GLib for DBus/UPower (optional)
    pkg_check_modules(GLIB gio-2.0)
    if(GLIB_FOUND)
        set(POWER_MONITOR_AVAILABLE TRUE)
        add_compile_definitions(HAVE_POWER_MONITOR)
        message(STATUS "Power monitoring enabled (GLib found)")
    else()
        message(WARNING "GLib not found - power monitoring will be disabled. Install libglib2.0-dev to enable.")
    endif()
elseif(APPLE)
    # macOS: IOKit and CoreFoundation
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    if(IOKIT_LIBRARY AND COREFOUNDATION_LIBRARY)
        set(POWER_MONITOR_AVAILABLE TRUE)
        add_compile_definitions(HAVE_POWER_MONITOR)
        message(STATUS "Power monitoring enabled (IOKit found)")
    endif()
endif()

# Fetch external dependencies
include(FetchContent)

# toml11 - Header-only TOML parser
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG v3.8.1
    EXCLUDE_FROM_ALL
)
set(TOML11_INSTALL OFF CACHE BOOL "" FORCE)
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
    ${CURL_INCLUDE_DIRS}
    ${toml11_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
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
    src/power_monitor.cpp
    src/annas_archive.cpp
    src/state_machine.cpp
)

# Unified levin executable (daemon + CLI client)
add_executable(levin ${LEVIN_SOURCES})

target_link_libraries(levin
    ${Boost_LIBRARIES}
    ${LIBTORRENT_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    CURL::libcurl
    Threads::Threads
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Platform-specific linking
if(UNIX AND NOT APPLE)
    target_link_libraries(levin ${GLIB_LIBRARIES})
    target_link_directories(levin PRIVATE ${GLIB_LIBRARY_DIRS})
elseif(APPLE)
    target_link_libraries(levin ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
endif()

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
