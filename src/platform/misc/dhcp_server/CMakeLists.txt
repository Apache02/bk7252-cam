add_library(platform_dhcp_server INTERFACE)

target_sources(
        platform_dhcp_server INTERFACE
        dhcp_leases.c
        dhcp_server.c
)

target_include_directories(
        platform_dhcp_server INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# platform_stdio_* is deliberately absent, as in port_wifi: the sources include
# debug.h from it, and the application links the variant it wants.
target_link_libraries(
        platform_dhcp_server INTERFACE
        platform_freertos
        platform_port_lwip
)
