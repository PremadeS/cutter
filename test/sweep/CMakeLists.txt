# Add sweep tests here (alphabetically sorted)
cutter_add_layer_tests(sweep
    test_SignalSweep
)

target_sources(test_SignalSweep PRIVATE ${CMAKE_SOURCE_DIR}/test/common/DeadlockWatchdog.cpp)
