# Windows Platform Abstraction Layer (PAL)

# Collect all Windows PAL source files
set(PAL_WINDOWS_SOURCES
    HAPPlatform.c
    HAPPlatformAbort.c
    HAPPlatformAccessorySetup.c
    HAPPlatformAccessorySetupDisplay.c
    HAPPlatformAccessorySetupNFC.c
    HAPPlatformClock.c
    HAPPlatformFileManager.c
    HAPPlatformKeyValueStore.c
    HAPPlatformLog.c
    HAPPlatformMFiHWAuth.c
    HAPPlatformMFiTokenAuth.c
    HAPPlatformRandomNumber.c
    HAPPlatformRunLoop.c
    HAPPlatformServiceDiscovery.c
    HAPPlatformSystemCommand.c
    HAPPlatformTCPStreamManager.c
)

# Optional: BLE support (stub for Windows)
if(HAVE_BLE)
    list(APPEND PAL_WINDOWS_SOURCES HAPPlatformBLEPeripheralManager.c)
endif()

# Create PAL library
add_library(HAPPlatform_Windows STATIC ${PAL_WINDOWS_SOURCES})

# Link with crypto implementation
if(USE_MBEDTLS)
    target_link_libraries(HAPPlatform_Windows PUBLIC MbedTLS::mbedtls MbedTLS::mbedcrypto MbedTLS::mbedx509)
else()
    target_link_libraries(HAPPlatform_Windows PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Link with platform libraries
target_link_libraries(HAPPlatform_Windows PUBLIC
    ${PLATFORM_LIBS}
)

# NFC support
if(HAVE_NFC)
    target_link_libraries(HAPPlatform_Windows PUBLIC nfc)
endif()

# BLE support using BTstack
if(HAVE_BLE)
    find_package(btstack CONFIG REQUIRED)
    target_link_libraries(HAPPlatform_Windows PUBLIC btstack::btstack)
    target_compile_definitions(HAPPlatform_Windows PUBLIC HAVE_BLE=1)
endif()

# QR Code generation
if(HAVE_QR_CODE)
    find_package(qrencode CONFIG)
    if(qrencode_FOUND)
        target_link_libraries(HAPPlatform_Windows PUBLIC qrencode)
        target_compile_definitions(HAPPlatform_Windows PUBLIC HAVE_QR_CODE=1)
    endif()
endif()

# Include directories
target_include_directories(HAPPlatform_Windows PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/PAL
    ${CMAKE_SOURCE_DIR}/PAL/Crypto/${CRYPTO_IMPL}
)

# Compile definitions
target_compile_definitions(HAPPlatform_Windows PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Windows-specific warnings
if(MSVC)
    target_compile_options(HAPPlatform_Windows PRIVATE
        /wd4996  # Disable deprecation warnings for certain Windows APIs
    )
endif()

# Set output name
set_target_properties(HAPPlatform_Windows PROPERTIES
    OUTPUT_NAME "HAPPlatform"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Install
install(TARGETS HAPPlatform_Windows
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/PAL/Windows
    FILES_MATCHING PATTERN "*.h"
)
