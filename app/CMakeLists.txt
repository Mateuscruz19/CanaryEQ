add_executable(canary
    main.cpp
    looping_player.cpp
    control_window.cpp
    dr_wav.cpp
)

set_source_files_properties(dr_wav.cpp PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/wd4701>")

target_link_libraries(canary PRIVATE canary-dsp canary-engine spdlog::spdlog webview::core)
target_include_directories(canary SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
