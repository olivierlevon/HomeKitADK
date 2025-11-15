# FindBTstack.cmake - Locate BTstack Bluetooth library
# This module defines:
#  BTstack_FOUND - System has BTstack
#  BTstack_INCLUDE_DIRS - BTstack include directories
#  BTstack_LIBRARIES - BTstack libraries
#  BTstack::btstack - Imported target

# Search in HOMEKIT_DEPS first, then vcpkg, then system
find_path(BTstack_INCLUDE_DIR
    NAMES btstack.h
    PATHS
        $ENV{HOMEKIT_DEPS}/include
        $ENV{HOMEKIT_DEPS}/include/btstack
        $ENV{VCPKG_ROOT}/installed/x64-windows/include
        $ENV{VCPKG_ROOT}/installed/x64-windows/include/btstack
        C:/HomeKitADK-deps/include
        C:/HomeKitADK-deps/include/btstack
    PATH_SUFFIXES btstack
)

find_library(BTstack_LIBRARY
    NAMES btstack libbtstack btstack_static
    PATHS
        $ENV{HOMEKIT_DEPS}/lib
        $ENV{VCPKG_ROOT}/installed/x64-windows/lib
        C:/HomeKitADK-deps/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BTstack
    REQUIRED_VARS BTstack_LIBRARY BTstack_INCLUDE_DIR
)

if(BTstack_FOUND AND NOT TARGET BTstack::btstack)
    add_library(BTstack::btstack UNKNOWN IMPORTED)
    set_target_properties(BTstack::btstack PROPERTIES
        IMPORTED_LOCATION "${BTstack_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BTstack_INCLUDE_DIR}"
    )
    set(BTstack_LIBRARIES ${BTstack_LIBRARY})
    set(BTstack_INCLUDE_DIRS ${BTstack_INCLUDE_DIR})
endif()

mark_as_advanced(BTstack_INCLUDE_DIR BTstack_LIBRARY)
