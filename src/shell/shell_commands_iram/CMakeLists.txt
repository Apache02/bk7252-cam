add_library(
        shell_commands_iram INTERFACE
)

target_include_directories(
        shell_commands_iram INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        shell_commands_iram INTERFACE
        ${COMMANDS}
)

target_link_libraries(
        shell_commands_iram INTERFACE
        shell_main
        utils_crc
)