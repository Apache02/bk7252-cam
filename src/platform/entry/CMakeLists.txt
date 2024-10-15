add_library(platform_entry INTERFACE)

target_sources(
        platform_entry INTERFACE
        boot_vectors.s
        arm.s
        boot_reset.S
        arch_main.c
        boot_handlers.S
)

target_include_directories(
        platform_entry INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
