
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

    add_test(NAME integration.${test_name} COMMAND ${test_name})
endfunction()

add_cutter_integration_test(tst_CoreLifeCycle tst_CoreLifeCycle.cpp)
