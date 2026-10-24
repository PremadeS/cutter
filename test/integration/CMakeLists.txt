
function(add_cutter_integration_test test_name test_source)
    add_executable(${test_name} ${test_source} ${TEST_COMMON_SOURCES})

    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
    )

    target_include_directories(${test_name} PRIVATE 
        ${TEST_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name} PRIVATE
        cutter_gui_lib
        Qt6::Test
        Qt6::Widgets
    )

    add_test(NAME integration.${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endfunction()

# Apply shared properties (labels + headless platform + timeout) to a set of
# integration tests. QT_QPA_PLATFORM=offscreen lets the widget (L3) tests run
# without a display server, which is required for CI runners.
function(cutter_integration_test_props)
    foreach(test_name IN LISTS ARGN)
        set_tests_properties(integration.${test_name} PROPERTIES
            LABELS "integration"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 300
        )
    endforeach()
endfunction()

add_cutter_integration_test(tst_Docking tst_Docking.cpp)

cutter_integration_test_props(
	tst_Docking
)
