add_executable(canary-tests
    main.cpp
    gain_test.cpp
    balance_test.cpp
    eq_test.cpp
)

target_link_libraries(canary-tests PRIVATE canary-dsp)
target_include_directories(canary-tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

add_test(NAME canary-tests COMMAND canary-tests)
