cmake_minimum_required(VERSION 3.20)
project(levin VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(LEVIN_BUILD_TESTS "Build tests" ON)
option(LEVIN_BUILD_DAEMON "Build Linux daemon/CLI" ON)
option(LEVIN_USE_STUB_SESSION "Use stub torrent session (no libtorrent)" ON)

if(LEVIN_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(liblevin)

if(LEVIN_BUILD_DAEMON AND NOT ANDROID)
    # add_subdirectory(platforms/linux)  # TODO: Phase 6
endif()
