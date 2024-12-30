simple_drivers_library(hardware_flash)
target_link_libraries(
        hardware_flash INTERFACE
        common_defs
)
target_sources(
        hardware_flash INTERFACE
        flash.c
)
