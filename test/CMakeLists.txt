# TODO: what about qt5??
find_package(Qt6 COMPONENTS Test REQUIRED)

set(TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(TEST_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CutterTest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CutterTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ProxyModelTestHelper.h # TODO: maybe move this inside helpers dir?
)

# helper func to register unit tests
function(cutter_add_unit_test TEST_NAME SRC_FILE)    
    add_executable(${TEST_NAME} ${SRC_FILE} ${TEST_COMMON_SOURCES})

    target_include_directories(${TEST_NAME} PRIVATE ${TEST_INCLUDES})
    
    set_target_properties(${TEST_NAME} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )
    
    target_link_libraries(${TEST_NAME} PRIVATE
        cutter_gui_lib    # Replace with Cutter's target library name
        Qt6::Test
        Qt6::Widgets
    )
   
    # this name will be displayed when running tests
    add_test(NAME unit.widgets.${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# add unit tests here, preferably sorted alphabetically
# TODO: maybe move these insit test/unit??
cutter_add_unit_test(tst_FunctionsWidget unit/widgets/tst_FunctionsWidget.cpp)
