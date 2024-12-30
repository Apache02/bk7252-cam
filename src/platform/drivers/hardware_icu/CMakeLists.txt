simple_drivers_library(hardware_icu)
target_link_libraries(
        hardware_icu INTERFACE
        common_defs
)
target_sources(
        hardware_icu INTERFACE
        icu.c
)
