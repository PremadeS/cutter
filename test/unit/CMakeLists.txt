# helper function to register a single test
function(cutter_add_unit_test TEST_NAME)
    add_executable(${TEST_NAME} "${TEST_NAME}.cpp" ${TEST_COMMON_SOURCES})

    target_include_directories(${TEST_NAME} PRIVATE ${TEST_INCLUDES})

    set_target_properties(${TEST_NAME} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )

    target_link_libraries(${TEST_NAME} PRIVATE
	cutter_gui_lib # TODO: change this name
        Qt6::Test
        Qt6::Widgets
    )

    # test name will be shown in this format on output
    add_test(NAME unit.${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

function(cutter_add_unit_tests)
    foreach(TEST_NAME IN LISTS ARGN)
        cutter_add_unit_test(${TEST_NAME})
    endforeach()
endfunction()

# Add unit tests here (preferably alphabetically sorted)
cutter_add_unit_tests(
    tst_AbstractFilterView
    tst_AddressableItemModel
    tst_DisassemblyHelper
    tst_GraphLayouts
    tst_Json
    tst_PjHandle
    tst_ShortcutManager
    tst_SyntaxHighlighter
    tst_MdHighlighter
    tst_Helpers
    tst_BinaryTrees
    tst_LinkedListPool
    tst_RichTextPainter
    tst_SelectionHighlight
    tst_BasicInstructionHighlighter
)
