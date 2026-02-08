cmake_minimum_required(VERSION 3.22.1)
project(levin-android LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Options inherited from app/build.gradle.kts cmake arguments ---
# LEVIN_USE_STUB_SESSION=ON/OFF
# LEVIN_BUILD_TESTS=OFF

# --- Path to the root liblevin source tree ---
set(LEVIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
get_filename_component(LEVIN_ROOT "${LEVIN_ROOT}" ABSOLUTE)

# --- Prebuilt OpenSSL for this ABI ---
# Built by platforms/android/build-openssl.sh prior to Gradle build.
set(OPENSSL_PREBUILT_DIR "${LEVIN_ROOT}/platforms/android/prebuilt/openssl/${ANDROID_ABI}")

# --- libtorrent (when not using stub) ---
if(NOT LEVIN_USE_STUB_SESSION)
    # Boost headers (header-only). Downloaded by build-deps.sh.
    # libtorrent only needs Boost headers, not compiled libraries.
    set(BOOST_INCLUDE_DIR "${LEVIN_ROOT}/platforms/android/prebuilt/boost/include" CACHE PATH "Path to Boost headers")

    # Set up OpenSSL for find_package to discover prebuilt libs
    set(OPENSSL_ROOT_DIR "${OPENSSL_PREBUILT_DIR}" CACHE PATH "" FORCE)
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_PREBUILT_DIR}/include" CACHE PATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_PREBUILT_DIR}/lib/libcrypto.a" CACHE FILEPATH "" FORCE)
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_PREBUILT_DIR}/lib/libssl.a" CACHE FILEPATH "" FORCE)
    set(OPENSSL_USE_STATIC_LIBS TRUE CACHE BOOL "" FORCE)
    find_package(OpenSSL REQUIRED)

    # Fetch libtorrent from master (same as Linux build)
    FetchContent_Declare(
        libtorrent
        GIT_REPOSITORY https://github.com/arvidn/libtorrent.git
        GIT_TAG        master
        GIT_SUBMODULES_RECURSE TRUE
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(webtorrent ON CACHE BOOL "Enable WebTorrent/WebRTC via libdatachannel" FORCE)
    set(deprecated-functions OFF CACHE BOOL "" FORCE)

    # Disable libdatachannel tests/examples/benchmarks (they cause
    # "multiple rules" errors with ninja and are not needed on Android)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(NO_EXAMPLES ON CACHE BOOL "" FORCE)

    # Provide Boost headers to libtorrent.  When cross-compiling with NDK,
    # FindBoost can't locate the host system Boost.  We pre-set all the
    # variables that FindBoost would produce so libtorrent's
    # find_public_dependency(Boost REQUIRED) succeeds.
    set(BOOST_ROOT "/usr" CACHE PATH "" FORCE)
    set(Boost_INCLUDE_DIR "${BOOST_INCLUDE_DIR}" CACHE PATH "" FORCE)
    set(Boost_INCLUDE_DIRS "${BOOST_INCLUDE_DIR}" CACHE PATH "" FORCE)
    set(Boost_NO_SYSTEM_PATHS OFF CACHE BOOL "" FORCE)
    set(Boost_NO_BOOST_CMAKE ON CACHE BOOL "" FORCE)

    # Force Boost include path into compiler flags.  NDK's sysroot prevents
    # include_directories() from adding host paths, so we inject it directly.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${BOOST_INCLUDE_DIR}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${BOOST_INCLUDE_DIR}")

    # Android NDK has pthreads built into libc (bionic).  FindThreads fails
    # because the cmake try_compile can't link a test program when cross-
    # compiling.  Pre-set the result so find_package(Threads) succeeds.
    set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
    set(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE BOOL "" FORCE)
    set(Threads_FOUND TRUE CACHE BOOL "" FORCE)
    set(CMAKE_USE_PTHREADS_INIT 1 CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(libtorrent)
endif()

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

if(NOT LEVIN_USE_STUB_SESSION)
    target_link_libraries(levin torrent-rasterbar)
endif()
