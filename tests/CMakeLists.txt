add_executable(canary-tests
    main.cpp
    gain_test.cpp
    balance_test.cpp
    eq_test.cpp
    bridge_test.cpp
    player_test.cpp
)

target_link_libraries(canary-tests PRIVATE canary-dsp canary-app-core)
target_include_directories(canary-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

add_test(NAME canary-tests COMMAND canary-tests)

add_executable(canary-ui-test
    ui_test.cpp
)

target_link_libraries(canary-ui-test PRIVATE canary-ui)

add_test(NAME canary-ui-test COMMAND canary-ui-test)
set_tests_properties(canary-ui-test PROPERTIES LABELS ui TIMEOUT 30)

add_executable(canary-apo-test
    apo_test.cpp
)

target_compile_definitions(canary-apo-test PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(canary-apo-test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/wd4324>)
target_link_libraries(canary-apo-test PRIVATE ole32 runtimeobject)
add_dependencies(canary-apo-test canary-apo)

add_test(NAME canary-apo-test COMMAND canary-apo-test $<TARGET_FILE:canary-apo>)
