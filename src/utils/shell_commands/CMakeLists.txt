add_library(
        utils_shell_commands INTERFACE
)

target_include_directories(
        utils_shell_commands INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        utils_shell_commands INTERFACE
        ${COMMANDS}
)

target_link_libraries(
        utils_shell_commands INTERFACE
        utils_shell
        hardware_efuse
)
