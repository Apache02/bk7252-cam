simple_drivers_library(hardware_efuse)
target_link_libraries(
        hardware_efuse INTERFACE
        hardware_sctrl
)
target_sources(
        hardware_efuse INTERFACE
        efuse.c
)
