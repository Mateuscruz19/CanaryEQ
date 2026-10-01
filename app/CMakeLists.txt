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
    dr_wav.cpp
)

set_source_files_properties(dr_wav.cpp PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/wd4701>")

target_link_libraries(canary PRIVATE canary-app-core canary-ui spdlog::spdlog)
target_include_directories(canary SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
