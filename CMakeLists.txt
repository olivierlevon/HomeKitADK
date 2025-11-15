# HomeKit Accessory Development Kit (ADK)
# CMake build system for cross-platform compilation
# Supports: Windows, Linux, macOS, Raspberry Pi

cmake_minimum_required(VERSION 3.20)

project(HomeKitADK
    VERSION 1.0.0
    DESCRIPTION "Apple HomeKit Accessory Protocol Implementation"
    LANGUAGES C CXX
)

# CMake options
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTING "Build unit tests" ON)
option(HAVE_NFC "Enable NFC support with libnfc" OFF)
option(HAVE_BLE "Enable FULL Bluetooth LE support with BTstack" OFF)
option(HAVE_DISPLAY "Enable display support for QR codes" ON)
option(HAVE_QR_CODE "Enable QR code generation with libqrencode" ON)
option(HAVE_MFI_HW_AUTH "Enable MFi hardware authentication" OFF)
option(USE_MBEDTLS "Use MbedTLS 4.x (TLS 1.3) instead of OpenSSL" OFF)
option(BUILD_APPLICATIONS "Build example applications (Lightbulb, Lock, etc.)" ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# Platform detection
if(WIN32)
    set(PLATFORM "Windows")
    set(PAL_DIR "${CMAKE_SOURCE_DIR}/PAL/Windows")
elseif(APPLE)
    set(PLATFORM "Darwin")
    set(PAL_DIR "${CMAKE_SOURCE_DIR}/PAL/Darwin")
elseif(UNIX AND NOT APPLE)
    # Check for Raspberry Pi
    if(EXISTS "/sys/firmware/devicetree/base/model")
        file(READ "/sys/firmware/devicetree/base/model" DEVICE_MODEL)
        if(DEVICE_MODEL MATCHES "Raspberry Pi")
            set(PLATFORM "Raspi")
            set(PAL_DIR "${CMAKE_SOURCE_DIR}/PAL/Raspi")
        else()
            set(PLATFORM "Linux")
            set(PAL_DIR "${CMAKE_SOURCE_DIR}/PAL/Linux")
        endif()
    else()
        set(PLATFORM "Linux")
        set(PAL_DIR "${CMAKE_SOURCE_DIR}/PAL/Linux")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Building for platform: ${PLATFORM}")
message(STATUS "PAL directory: ${PAL_DIR}")

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Find required packages
if(USE_MBEDTLS)
    find_package(MbedTLS 4.0 REQUIRED)
    set(CRYPTO_IMPL "MbedTLS")
    set(CRYPTO_DIR "${CMAKE_SOURCE_DIR}/PAL/Crypto/MbedTLS")
else()
    find_package(OpenSSL 3.0 REQUIRED)
    set(CRYPTO_IMPL "OpenSSL")
    set(CRYPTO_DIR "${CMAKE_SOURCE_DIR}/PAL/Crypto/OpenSSL")
endif()

message(STATUS "Using crypto implementation: ${CRYPTO_IMPL}")

# Platform-specific dependencies
if(WIN32)
    # Windows requires Winsock2, BCrypt, Bonjour SDK
    set(PLATFORM_LIBS ws2_32 bcrypt advapi32)

    # Try to find Bonjour SDK (dnssd)
    find_library(DNSSD_LIBRARY NAMES dnssd PATHS
        "C:/Program Files/Bonjour SDK/Lib/x64"
        "C:/Program Files (x86)/Bonjour SDK/Lib/x64"
    )
    find_path(DNSSD_INCLUDE_DIR dns_sd.h PATHS
        "C:/Program Files/Bonjour SDK/Include"
        "C:/Program Files (x86)/Bonjour SDK/Include"
    )

    if(DNSSD_LIBRARY AND DNSSD_INCLUDE_DIR)
        message(STATUS "Found Bonjour SDK: ${DNSSD_LIBRARY}")
        list(APPEND PLATFORM_LIBS ${DNSSD_LIBRARY})
        include_directories(${DNSSD_INCLUDE_DIR})
    else()
        message(WARNING "Bonjour SDK not found. Service discovery will not work. Download from: https://developer.apple.com/bonjour/")
    endif()

    if(HAVE_NFC)
        find_package(libnfc CONFIG REQUIRED)
        list(APPEND PLATFORM_LIBS nfc)
    endif()

    # BLE support using BTstack
    if(HAVE_BLE)
        find_package(btstack CONFIG REQUIRED)
        list(APPEND PLATFORM_LIBS btstack)
        message(STATUS "BLE support enabled with BTstack")
    endif()

    # QR Code generation
    if(HAVE_QR_CODE)
        find_package(qrencode CONFIG)
        if(qrencode_FOUND)
            list(APPEND PLATFORM_LIBS qrencode)
            message(STATUS "QR code generation enabled with libqrencode")
        else()
            message(WARNING "libqrencode not found. QR codes will be text-only. Install: vcpkg install qrencode:x64-windows")
        endif()
    endif()

elseif(UNIX)
    set(PLATFORM_LIBS pthread m)

    # Find DNS-SD (Avahi on Linux, Bonjour on macOS)
    find_library(DNSSD_LIBRARY NAMES dns_sd)
    if(DNSSD_LIBRARY)
        list(APPEND PLATFORM_LIBS ${DNSSD_LIBRARY})
    else()
        message(FATAL_ERROR "DNS-SD library not found. Install: libavahi-compat-libdnssd-dev (Linux) or Bonjour (macOS)")
    endif()

    if(HAVE_NFC)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBNFC REQUIRED libnfc)
        include_directories(${LIBNFC_INCLUDE_DIRS})
        list(APPEND PLATFORM_LIBS ${LIBNFC_LIBRARIES})
    endif()
endif()

# Compiler flags
if(MSVC)
    add_compile_options(
        /W4
        /WX-
        /MP
        /permissive-
        /Zc:__cplusplus
        /D_CRT_SECURE_NO_WARNINGS
        /D_WIN32_WINNT=0x0601
        /DWIN32_LEAN_AND_MEAN
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Zi /Od /RTC1)
    else()
        add_compile_options(/O2 /GL)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif()
else()
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        -ffunction-sections
        -fdata-sections
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
        add_link_options(-Wl,--gc-sections -Wl,--as-needed)
    endif()
endif()

# Feature definitions
if(HAVE_NFC)
    add_compile_definitions(HAVE_NFC=1)
endif()

if(HAVE_BLE)
    add_compile_definitions(HAVE_BLE=1)
endif()

if(HAVE_DISPLAY)
    add_compile_definitions(HAVE_DISPLAY=1)
endif()

if(HAVE_QR_CODE)
    add_compile_definitions(HAVE_QR_CODE=1)
endif()

if(HAVE_MFI_HW_AUTH)
    add_compile_definitions(HAVE_MFI_HW_AUTH=1)
endif()

# Protocol support (IP is always enabled, BLE is optional)
add_compile_definitions(HAVE_DRIVER_IP_STACK=1)

if(HAVE_BLE)
    add_compile_definitions(HAVE_DRIVER_BLE_STACK=1)
endif()

# Log level (0=None, 1=Default, 2=Info, 3=Debug)
set(HAP_LOG_LEVEL "2" CACHE STRING "Logging level")
add_compile_definitions(HAP_LOG_LEVEL=${HAP_LOG_LEVEL})

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/HAP
    ${CMAKE_SOURCE_DIR}/PAL
    ${PAL_DIR}
    ${CRYPTO_DIR}
    ${CMAKE_SOURCE_DIR}/External/Base64
    ${CMAKE_SOURCE_DIR}/External/JSON
    ${CMAKE_SOURCE_DIR}/External/HTTP
)

# Subdirectories
add_subdirectory(External/Base64)
add_subdirectory(External/JSON)
add_subdirectory(HAP)
add_subdirectory(PAL/${PLATFORM})

if(BUILD_APPLICATIONS)
    add_subdirectory(Applications)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(Tests)
endif()

# Install targets
install(DIRECTORY HAP/ DESTINATION include/HAP FILES_MATCHING PATTERN "*.h")
install(DIRECTORY PAL/ DESTINATION include/PAL FILES_MATCHING PATTERN "*.h")

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HomeKitADKConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HomeKitADKConfigVersion.cmake"
    DESTINATION lib/cmake/HomeKitADK
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== HomeKit ADK Build Configuration ===")
message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Crypto: ${CRYPTO_IMPL}")
message(STATUS "NFC Support: ${HAVE_NFC}")
message(STATUS "Display Support: ${HAVE_DISPLAY}")
message(STATUS "MFi HW Auth: ${HAVE_MFI_HW_AUTH}")
message(STATUS "Build Applications: ${BUILD_APPLICATIONS}")
message(STATUS "Build Tests: ${BUILD_TESTING}")
message(STATUS "Log Level: ${HAP_LOG_LEVEL}")
message(STATUS "=======================================")
message(STATUS "")
