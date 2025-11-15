# Lock Example Application

# Find all source files in the Lock directory
file(GLOB LOCK_SOURCES "*.c")

add_executable(Lock ${LOCK_SOURCES})

target_link_libraries(Lock PRIVATE
    HAP
    HAPPlatform_${PLATFORM}
    ${PLATFORM_LIBS}
)

target_include_directories(Lock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/HAP
    ${CMAKE_SOURCE_DIR}/PAL
    ${PAL_DIR}
)

set_target_properties(Lock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install
install(TARGETS Lock
    RUNTIME DESTINATION bin
)

# Copy setup payload files if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/setup.json")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/setup.json" DESTINATION "${CMAKE_BINARY_DIR}/bin")
    install(FILES setup.json DESTINATION bin)
endif()
