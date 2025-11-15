# JSON Parser Library

add_library(JSON STATIC util_json_reader.c)

target_include_directories(JSON PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(JSON PROPERTIES
    OUTPUT_NAME "JSON"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS JSON
    ARCHIVE DESTINATION lib
)

install(FILES util_json_reader.h
    DESTINATION include/External/JSON
)
