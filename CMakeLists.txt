cmake_minimum_required(VERSION 3.25)
project(CanaryEQ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

include(FetchContent)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(spdlog)

set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(webview
    GIT_REPOSITORY https://github.com/webview/webview.git
    GIT_TAG 0.12.0
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(webview)

add_library(canary-wav STATIC third_party/dr_wav.cpp)
target_include_directories(canary-wav SYSTEM PUBLIC third_party)
target_compile_options(canary-wav PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/wd4701>)

add_subdirectory(dsp)
add_subdirectory(engine)
add_subdirectory(app)
add_subdirectory(cli)
add_subdirectory(apo)

enable_testing()
add_subdirectory(tests)
