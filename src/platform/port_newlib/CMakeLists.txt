add_library(platform_port_newlib INTERFACE)

target_include_directories(
        platform_port_newlib INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        platform_port_newlib INTERFACE
        crt.c
        syscalls.c
        sbrk.c
)
