add_executable(canary-cli
    main.cpp
)

target_link_libraries(canary-cli PRIVATE canary-dsp canary-wav spdlog::spdlog)
