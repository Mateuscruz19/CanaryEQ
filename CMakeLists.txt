cmake_minimum_required(VERSION 3.25)
project(CanaryEQ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

add_subdirectory(dsp)
add_subdirectory(engine)
add_subdirectory(app)

enable_testing()
add_subdirectory(tests)
