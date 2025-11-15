# Lightbulb Example Application

add_executable(Lightbulb
    Main.c
    App.c
    DB.c
)

target_link_libraries(Lightbulb PRIVATE
    HAP
    HAPPlatform_${PLATFORM}
    ${PLATFORM_LIBS}
)

target_include_directories(Lightbulb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/HAP
    ${CMAKE_SOURCE_DIR}/PAL
    ${PAL_DIR}
)

set_target_properties(Lightbulb PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install
install(TARGETS Lightbulb
    RUNTIME DESTINATION bin
)

# Copy setup payload files if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/setup.json")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/setup.json" DESTINATION "${CMAKE_BINARY_DIR}/bin")
    install(FILES setup.json DESTINATION bin)
endif()
