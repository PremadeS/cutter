# Add sweep tests here (alphabetically sorted)
cutter_add_layer_tests(sweep
    test_SignalSweep
    test_UiSweep
)

target_sources(test_SignalSweep PRIVATE ${CMAKE_SOURCE_DIR}/test/common/DeadlockWatchdog.cpp)
target_sources(test_UiSweep PRIVATE ${CMAKE_SOURCE_DIR}/test/common/DeadlockWatchdog.cpp)
if(WIN32)
    # DeadlockWatchdog walks and symbolizes stacks with DbgHelp.
    target_link_libraries(test_SignalSweep PRIVATE dbghelp)
    target_link_libraries(test_UiSweep PRIVATE dbghelp)
endif()

# The UI sweep clicks through the whole main window; slower CI runners need more than the default.
set_tests_properties(sweep.test_UiSweep PROPERTIES TIMEOUT 900)
