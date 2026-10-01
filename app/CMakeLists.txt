add_library(canary-app-core STATIC
    looping_player.cpp
    control_bridge.cpp
)
target_include_directories(canary-app-core PUBLIC .)
target_link_libraries(canary-app-core PUBLIC canary-dsp canary-engine)

add_library(canary-ui STATIC
    control_window.cpp
)
target_link_libraries(canary-ui PUBLIC canary-app-core PRIVATE webview::core)

add_executable(canary
    main.cpp
)

target_link_libraries(canary PRIVATE canary-app-core canary-ui canary-wav spdlog::spdlog)
