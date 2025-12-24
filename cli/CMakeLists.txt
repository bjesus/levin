# CLI client
add_executable(levinctl cli_main.cpp)

target_link_libraries(levinctl
    nlohmann_json::nlohmann_json
    Threads::Threads
)

install(TARGETS levinctl
    RUNTIME DESTINATION bin
)
