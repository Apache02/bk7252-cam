add_library(platform_nosys INTERFACE)

target_sources(
        platform_nosys INTERFACE
        entry.c
        boot_handlers.S
        sbrk.c
)

target_link_libraries(
        platform_nosys INTERFACE
        platform_boot
        platform_port_newlib
        hardware_intc
        utils
)
