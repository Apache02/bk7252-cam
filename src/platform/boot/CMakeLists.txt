add_library(platform_boot INTERFACE)

target_sources(
        platform_boot INTERFACE
        boot_vectors.S
        boot_reset.S
        boot_init.c
)
