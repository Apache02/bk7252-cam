add_library(platform_telnet_server INTERFACE)

target_sources(
        platform_telnet_server INTERFACE
        telnet_server.c
        shell_command_exit.c
)

target_include_directories(
        platform_telnet_server INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# platform_stdio_* is deliberately absent, as in platform_http_server: the sources
# include debug.h from it, and the application links the variant it wants.
target_link_libraries(
        platform_telnet_server INTERFACE
        platform_freertos
        platform_port_lwip
        utils_ring_buffer
)
