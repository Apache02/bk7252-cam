add_library(platform_nosys INTERFACE)

target_sources(
        platform_nosys INTERFACE
        arch_main.c
        boot_handlers.S
)

target_link_libraries(
        platform_nosys INTERFACE
        platform_boot
        utils
)
