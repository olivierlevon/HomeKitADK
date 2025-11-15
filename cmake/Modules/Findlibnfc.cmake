# Findlibnfc.cmake - Locate libnfc NFC library
# This module defines:
#  libnfc_FOUND - System has libnfc
#  libnfc_INCLUDE_DIRS - libnfc include directories
#  libnfc_LIBRARIES - libnfc libraries
#  libnfc::nfc - Imported target

# Search in HOMEKIT_DEPS first, then vcpkg, then system
find_path(libnfc_INCLUDE_DIR
    NAMES nfc/nfc.h
    PATHS
        $ENV{HOMEKIT_DEPS}/include
        $ENV{VCPKG_ROOT}/installed/x64-windows/include
        C:/HomeKitADK-deps/include
)

find_library(libnfc_LIBRARY
    NAMES nfc libnfc nfc_static
    PATHS
        $ENV{HOMEKIT_DEPS}/lib
        $ENV{VCPKG_ROOT}/installed/x64-windows/lib
        C:/HomeKitADK-deps/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libnfc
    REQUIRED_VARS libnfc_LIBRARY libnfc_INCLUDE_DIR
)

if(libnfc_FOUND AND NOT TARGET libnfc::nfc)
    add_library(libnfc::nfc UNKNOWN IMPORTED)
    set_target_properties(libnfc::nfc PROPERTIES
        IMPORTED_LOCATION "${libnfc_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${libnfc_INCLUDE_DIR}"
    )
    set(libnfc_LIBRARIES ${libnfc_LIBRARY})
    set(libnfc_INCLUDE_DIRS ${libnfc_INCLUDE_DIR})
endif()

mark_as_advanced(libnfc_INCLUDE_DIR libnfc_LIBRARY)
