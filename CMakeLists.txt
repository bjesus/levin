cmake_minimum_required(VERSION 3.15...3.30)

project(levin VERSION 0.4.3 LANGUAGES CXX)

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

# Option to use system libtorrent (without WebTorrent) or build from source (with WebTorrent)
option(USE_SYSTEM_LIBTORRENT "Use system libtorrent instead of building from source" OFF)

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
find_package(PkgConfig REQUIRED)

# Fetch external dependencies
include(FetchContent)

# nlohmann_json - JSON library (fetch first to avoid conflicts with libtorrent's deps)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

if(USE_SYSTEM_LIBTORRENT)
    # Use system libtorrent (likely without WebTorrent support)
    pkg_check_modules(LIBTORRENT REQUIRED libtorrent-rasterbar>=2.0)
    set(LIBTORRENT_TARGETS ${LIBTORRENT_LIBRARIES})
    set(WEBTORRENT_ENABLED FALSE)
    message(STATUS "Using system libtorrent (WebTorrent likely NOT available)")
else()
    # Build libtorrent from source with WebTorrent support
    message(STATUS "Building libtorrent from source with WebTorrent support")
    
    # libtorrent build options - set before FetchContent
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(deprecated-functions OFF CACHE BOOL "" FORCE)
    set(webtorrent ON CACHE BOOL "" FORCE)  # Enable WebTorrent!
    set(build_examples OFF CACHE BOOL "" FORCE)
    set(build_tests OFF CACHE BOOL "" FORCE)
    set(build_tools OFF CACHE BOOL "" FORCE)
    set(python-bindings OFF CACHE BOOL "" FORCE)
    set(python-egg-info OFF CACHE BOOL "" FORCE)
    
    # libdatachannel options (bundled with libtorrent)
    set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(USE_SYSTEM_JSON ON CACHE BOOL "" FORCE)  # Use our nlohmann_json
    
    # libtorrent with WebTorrent enabled
    # Must use master branch - WebTorrent is not in any release yet
    # libtorrent bundles libdatachannel when webtorrent=ON
    FetchContent_Declare(
        libtorrent
        GIT_REPOSITORY https://github.com/arvidn/libtorrent.git
        GIT_TAG master
        GIT_SUBMODULES_RECURSE ON
    )
    
    # Fetch libtorrent using FetchContent_MakeAvailable which is simpler
    # EXCLUDE_FROM_ALL prevents building libtorrent's targets by default
    # We suppress the install/export errors by not using export features
    
    FetchContent_GetProperties(libtorrent)
    if(NOT libtorrent_POPULATED)
        FetchContent_Populate(libtorrent)
        
        # Completely remove install/export commands from libtorrent's CMakeLists.txt
        # This is necessary because libtorrent's export() calls reference datachannel-static
        # which causes CMake errors. We only need the library target, not install rules.
        file(READ "${libtorrent_SOURCE_DIR}/CMakeLists.txt" _lt_cmake_content)
        
        # Remove entire install() blocks by replacing them with empty strings
        # This regex matches install(...) including multi-line calls
        string(REGEX REPLACE "install[ \t]*\\([^)]*\\)" "" _lt_cmake_content "${_lt_cmake_content}")
        
        # Remove export() commands  
        string(REGEX REPLACE "export[ \t]*\\([^)]*\\)" "" _lt_cmake_content "${_lt_cmake_content}")
        
        file(WRITE "${libtorrent_SOURCE_DIR}/CMakeLists.txt" "${_lt_cmake_content}")
        
        add_subdirectory(${libtorrent_SOURCE_DIR} ${libtorrent_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
    
    set(LIBTORRENT_TARGETS torrent-rasterbar)
    set(WEBTORRENT_ENABLED TRUE)
    
    # Get libtorrent include directories
    get_target_property(LIBTORRENT_INCLUDE_DIRS torrent-rasterbar INTERFACE_INCLUDE_DIRECTORIES)
endif()

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

# nlohmann_json already fetched above

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${toml11_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
)

if(USE_SYSTEM_LIBTORRENT)
    include_directories(${LIBTORRENT_INCLUDE_DIRS})
    link_directories(${LIBTORRENT_LIBRARY_DIRS})
endif()

# Define WEBTORRENT_ENABLED for code
if(WEBTORRENT_ENABLED)
    add_compile_definitions(WEBTORRENT_ENABLED=1)
endif()

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
    ${LIBTORRENT_TARGETS}
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
    DESTINATION lib/systemd/user
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
if(USE_SYSTEM_LIBTORRENT)
    message(STATUS "  libtorrent-rasterbar: ${LIBTORRENT_VERSION} (system)")
else()
    message(STATUS "  libtorrent-rasterbar: master (bundled, WebTorrent enabled)")
endif()
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  toml11: v3.8.1")
message(STATUS "  spdlog: v1.12.0")
message(STATUS "  nlohmann_json: v3.11.3")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  WebTorrent: ${WEBTORRENT_ENABLED}")
message(STATUS "  Power Monitoring: ${POWER_MONITOR_AVAILABLE}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Use System libtorrent: ${USE_SYSTEM_LIBTORRENT}")
message(STATUS "==============================")
message(STATUS "")
