cmake_minimum_required(VERSION 3.20)

add_executable(levin-daemon
    src/main.cpp
    src/daemon.cpp
    src/ipc.cpp
    src/config.cpp
    src/storage.cpp
    src/power.cpp
)

target_link_libraries(levin-daemon PRIVATE levin)
target_include_directories(levin-daemon PRIVATE src)
set_target_properties(levin-daemon PROPERTIES OUTPUT_NAME levin)
