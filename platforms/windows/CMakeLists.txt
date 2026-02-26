cmake_minimum_required(VERSION 3.20)

add_executable(levin-win WIN32
    src/main.cpp
    src/levin_engine.cpp
    src/power_monitor.cpp
    src/storage_monitor.cpp
    src/startup.cpp
    resources/levin.rc
)

target_link_libraries(levin-win PRIVATE
    levin
    shell32
    user32
    gdi32
    comctl32
    ole32
    iphlpapi
    advapi32
    ws2_32
)

target_include_directories(levin-win PRIVATE
    src
    resources
)

target_compile_definitions(levin-win PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0600
)

set_target_properties(levin-win PROPERTIES OUTPUT_NAME levin)
