add_library(platform_boot INTERFACE)

target_sources(
        platform_boot INTERFACE
        boot_vectors.S
        boot_reset.S
        boot_init.c
)

target_include_directories(
        platform_boot INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_boot INTERFACE
        platform_cpu
)
