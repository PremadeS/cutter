find_package(Qt6 COMPONENTS Test REQUIRED)

set(TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(TEST_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ProxyModelTestHelper.h
)

enable_testing()

add_subdirectory(unit)
add_subdirectory(integration)
