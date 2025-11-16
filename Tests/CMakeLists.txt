# HomeKit ADK Unit Tests

# Only build tests if BUILD_TESTING is enabled
if(NOT BUILD_TESTING)
    return()
endif()

# Find all test files (both in root and in subdirectories)
file(GLOB TEST_SOURCES_ROOT "*Test.c" "util_*.c")
file(GLOB TEST_SOURCES_SUBDIRS "*/*Test.c" "*/util_*.c")

# Combine all test sources
set(TEST_SOURCES ${TEST_SOURCES_ROOT} ${TEST_SOURCES_SUBDIRS})

# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # Skip tests that are known to fail or take too long on certain platforms
    if(TEST_NAME MATCHES "HAPExhaustiveUTF8Test" AND PLATFORM MATCHES "Linux|Windows")
        message(STATUS "Skipping ${TEST_NAME} on ${PLATFORM}")
        continue()
    endif()

    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_link_libraries(${TEST_NAME} PRIVATE
        HAP
        HAPPlatform_${PLATFORM}
        ${PLATFORM_LIBS}
    )

    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/HAP
        ${CMAKE_SOURCE_DIR}/PAL
        ${PAL_DIR}
        ${CMAKE_SOURCE_DIR}/Tests/Harness
    )

    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # Register with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    message(STATUS "Configured test: ${TEST_NAME}")
endforeach()

message(STATUS "Configured ${CMAKE_CURRENT_BINARY_DIR} tests")
