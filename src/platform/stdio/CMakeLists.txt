add_library(
        platform_stdio INTERFACE
)

target_link_libraries(
        platform_stdio INTERFACE
        platform_drivers
)

target_sources(
        platform_stdio INTERFACE
        stdio.c
)
