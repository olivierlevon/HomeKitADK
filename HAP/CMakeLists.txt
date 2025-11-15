# HomeKit Accessory Protocol (HAP) Core Library

# Collect all HAP source files
file(GLOB HAP_SOURCES "*.c")

# Create HAP core library
add_library(HAP STATIC ${HAP_SOURCES})

# Link dependencies
target_link_libraries(HAP PUBLIC
    HAPPlatform_${PLATFORM}
    Base64
    JSON
)

# Include directories
target_include_directories(HAP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/PAL
    ${PAL_DIR}
)

# Compile definitions based on enabled features
if(HAVE_NFC)
    target_compile_definitions(HAP PUBLIC HAVE_NFC=1)
endif()

if(HAVE_DISPLAY)
    target_compile_definitions(HAP PUBLIC HAVE_DISPLAY=1)
endif()

if(HAVE_MFI_HW_AUTH)
    target_compile_definitions(HAP PUBLIC HAVE_MFI_HW_AUTH=1)
endif()

# Set output properties
set_target_properties(HAP PROPERTIES
    OUTPUT_NAME "HAP"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    POSITION_INDEPENDENT_CODE ON
)

# Install
install(TARGETS HAP
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/HAP
    FILES_MATCHING PATTERN "*.h"
)
