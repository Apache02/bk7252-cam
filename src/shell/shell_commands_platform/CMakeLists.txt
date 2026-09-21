add_library(
        shell_commands_platform INTERFACE
)

target_include_directories(
        shell_commands_platform INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        shell_commands_platform INTERFACE
        ${COMMANDS}
)

target_link_libraries(
        shell_commands_platform INTERFACE
        shell_main
        platform_freertos
        platform_port_newlib
)
