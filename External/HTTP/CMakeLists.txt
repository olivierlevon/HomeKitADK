# HTTP Parser Library

add_library(HTTP STATIC util_http_reader.c)

target_include_directories(HTTP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(HTTP PROPERTIES
    OUTPUT_NAME "HTTP"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS HTTP
    ARCHIVE DESTINATION lib
)

install(FILES util_http_reader.h
    DESTINATION include/External/HTTP
)
