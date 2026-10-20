find_package(Qt6 COMPONENTS Test REQUIRED)

# helper function to register unit tests
function(cutter_add_unit_test TEST_NAME SRC_FILE)
    add_executable(${TEST_NAME} ${SRC_FILE} ${TEST_COMMON_SOURCES})

    target_include_directories(${TEST_NAME} PRIVATE ${TEST_INCLUDES})

    set_target_properties(${TEST_NAME} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )

    target_link_libraries(${TEST_NAME} PRIVATE
        cutter_gui_lib
        Qt6::Test
        Qt6::Widgets
    )

    # test name will be displayed in this format on output
    add_test(NAME unit.${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# register unit tests (preferably alphabetically sorted)
cutter_add_unit_test(tst_AbstractFilterView tst_AbstractFilterView.cpp)
cutter_add_unit_test(tst_AddressableItemModel tst_AddressableItemModel.cpp)
cutter_add_unit_test(tst_GraphLayouts tst_GraphLayouts.cpp)
