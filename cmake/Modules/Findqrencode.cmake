# Findqrencode.cmake - Locate libqrencode QR code library
# This module defines:
#  qrencode_FOUND - System has qrencode
#  qrencode_INCLUDE_DIRS - qrencode include directories
#  qrencode_LIBRARIES - qrencode libraries
#  qrencode::qrencode - Imported target

# Search in HOMEKIT_DEPS first, then vcpkg, then system
find_path(qrencode_INCLUDE_DIR
    NAMES qrencode.h
    PATHS
        $ENV{HOMEKIT_DEPS}/include
        $ENV{VCPKG_ROOT}/installed/x64-windows/include
        C:/HomeKitADK-deps/include
)

find_library(qrencode_LIBRARY
    NAMES qrencode libqrencode qrencode_static
    PATHS
        $ENV{HOMEKIT_DEPS}/lib
        $ENV{VCPKG_ROOT}/installed/x64-windows/lib
        C:/HomeKitADK-deps/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(qrencode
    REQUIRED_VARS qrencode_LIBRARY qrencode_INCLUDE_DIR
)

if(qrencode_FOUND AND NOT TARGET qrencode::qrencode)
    add_library(qrencode::qrencode UNKNOWN IMPORTED)
    set_target_properties(qrencode::qrencode PROPERTIES
        IMPORTED_LOCATION "${qrencode_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${qrencode_INCLUDE_DIR}"
    )
    set(qrencode_LIBRARIES ${qrencode_LIBRARY})
    set(qrencode_INCLUDE_DIRS ${qrencode_INCLUDE_DIR})
endif()

mark_as_advanced(qrencode_INCLUDE_DIR qrencode_LIBRARY)
