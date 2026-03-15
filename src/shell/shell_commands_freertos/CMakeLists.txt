add_library(
        shell_commands_freertos INTERFACE
)

target_include_directories(
        shell_commands_freertos INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        shell_commands_freertos INTERFACE
        ${COMMANDS}
)

target_link_libraries(
        shell_commands_freertos INTERFACE
        shell_main
)
