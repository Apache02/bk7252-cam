add_library(platform_boot INTERFACE)

target_sources(
        platform_boot INTERFACE
        boot_vectors.s
        boot_reset.S
)
