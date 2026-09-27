add_library(canary-engine STATIC
    src/audio_output.cpp
)

target_include_directories(canary-engine PUBLIC include)
target_compile_definitions(canary-engine PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(canary-engine PRIVATE ole32 avrt)
