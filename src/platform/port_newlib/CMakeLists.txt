add_library(platform_port_newlib INTERFACE)

target_sources(
        platform_port_newlib INTERFACE
        crt.c
        syscalls.c
)
