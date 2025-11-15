# AccessorySetupGenerator - HomeKit Setup Code and Payload Generator

add_executable(AccessorySetupGenerator Main.c)

target_link_libraries(AccessorySetupGenerator PRIVATE
    HAP
    HAPPlatform_${PLATFORM}
    ${PLATFORM_LIBS}
)

target_include_directories(AccessorySetupGenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/HAP
    ${CMAKE_SOURCE_DIR}/PAL
    ${PAL_DIR}
)

set_target_properties(AccessorySetupGenerator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install
install(TARGETS AccessorySetupGenerator
    RUNTIME DESTINATION bin
)
