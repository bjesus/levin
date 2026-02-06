cmake_minimum_required(VERSION 3.22.1)
project(levin-android LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options inherited from app/build.gradle.kts cmake arguments ---
# LEVIN_USE_STUB_SESSION=ON
# LEVIN_BUILD_TESTS=OFF

# --- Path to the root liblevin source tree ---
set(LEVIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
get_filename_component(LEVIN_ROOT "${LEVIN_ROOT}" ABSOLUTE)

# --- liblevin sources ---
# We use a stub for annas_archive.cpp on Android because the real
# implementation depends on libcurl, which is not available without
# cross-compilation. The stub returns -1/empty for all operations.
set(LIBLEVIN_SOURCES
    ${LEVIN_ROOT}/liblevin/src/state_machine.cpp
    ${LEVIN_ROOT}/liblevin/src/disk_manager.cpp
    ${LEVIN_ROOT}/liblevin/src/levin.cpp
    ${LEVIN_ROOT}/liblevin/src/torrent_watcher.cpp
    ${LEVIN_ROOT}/liblevin/src/statistics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/annas_archive_stub.cpp
)

if(LEVIN_USE_STUB_SESSION)
    list(APPEND LIBLEVIN_SOURCES ${LEVIN_ROOT}/liblevin/src/stub_torrent_session.cpp)
else()
    list(APPEND LIBLEVIN_SOURCES
        ${LEVIN_ROOT}/liblevin/src/stub_torrent_session.cpp
        ${LEVIN_ROOT}/liblevin/src/torrent_session.cpp
    )
endif()

# --- Build liblevin as a shared library for Android ---
# We build a single shared lib that bundles liblevin + JNI bridge.
add_library(levin SHARED
    ${LIBLEVIN_SOURCES}
    jni_bridge.cpp
)

target_include_directories(levin PUBLIC
    ${LEVIN_ROOT}/liblevin/include
)

target_compile_features(levin PUBLIC cxx_std_17)

if(LEVIN_USE_STUB_SESSION)
    target_compile_definitions(levin PUBLIC LEVIN_USE_STUB_SESSION)
endif()

# --- Android-specific dependencies ---
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(levin
    ${log-lib}
    ${android-lib}
)

# On Android with stub session, we don't need libcurl.
# The stub session provides dummy implementations.
# When building with real libtorrent + curl, cross-compiled libs would be added here.
if(NOT LEVIN_USE_STUB_SESSION)
    # TODO: Add cross-compiled libcurl and libtorrent for Android
    # find_package(CURL REQUIRED)
    # target_link_libraries(levin CURL::libcurl torrent-rasterbar)
endif()
