add_library(canary-apo SHARED
    src/canary_apo.cpp
    canary_apo.def
)

set_target_properties(canary-apo PROPERTIES OUTPUT_NAME canary_apo)
target_compile_definitions(canary-apo PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(canary-apo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/wd4324>)
target_link_libraries(canary-apo PRIVATE canary-dsp ole32 runtimeobject)
