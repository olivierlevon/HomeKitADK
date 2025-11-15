# Base64 Encoding/Decoding Library

add_library(Base64 STATIC util_base64.c)

target_include_directories(Base64 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(Base64 PROPERTIES
    OUTPUT_NAME "Base64"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS Base64
    ARCHIVE DESTINATION lib
)

install(FILES util_base64.h
    DESTINATION include/External/Base64
)
