# CLI client
add_executable(levin cli_main.cpp)

target_link_libraries(levin
    nlohmann_json::nlohmann_json
    Threads::Threads
)

install(TARGETS levin
    RUNTIME DESTINATION bin
)
