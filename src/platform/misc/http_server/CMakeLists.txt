add_library(platform_http_server INTERFACE)

target_sources(
        platform_http_server INTERFACE
        http_default_handler.c
        http_server.c
)

target_include_directories(
        platform_http_server INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# platform_stdio_* is deliberately absent, as in platform_dhcp_server: the sources
# include debug.h from it, and the application links the variant it wants.
target_link_libraries(
        platform_http_server INTERFACE
        platform_freertos
        platform_port_lwip
)
