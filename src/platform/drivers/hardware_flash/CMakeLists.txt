simple_drivers_library(hardware_flash)
target_link_libraries(
        hardware_flash INTERFACE
        hardware_sctrl
)
target_sources(
        hardware_flash INTERFACE
        flash.c
        flash_bypass.c
)
