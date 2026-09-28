add_library(canary-dsp STATIC
    src/gain.cpp
    src/balance.cpp
)

target_include_directories(canary-dsp PUBLIC include)
