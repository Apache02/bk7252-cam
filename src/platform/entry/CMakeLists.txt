add_library(platform_entry INTERFACE)

target_sources(
        platform_entry INTERFACE
        boot_vectors.s
        arm.s
        arch_main.c
        boot_handlers.S
)

target_link_libraries(
        platform_entry INTERFACE
        hardware_intc
)

target_include_directories(
        platform_entry INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
