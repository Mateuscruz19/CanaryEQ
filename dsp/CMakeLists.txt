add_library(canary-dsp STATIC
    src/gain.cpp
    src/balance.cpp
    src/biquad.cpp
    src/three_band_eq.cpp
)

target_include_directories(canary-dsp PUBLIC include)
