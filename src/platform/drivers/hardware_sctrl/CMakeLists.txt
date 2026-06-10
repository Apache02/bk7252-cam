simple_drivers_library(hardware_sctrl)
target_link_libraries(
        hardware_sctrl INTERFACE
        hardware_icu
)
target_sources(
        hardware_sctrl INTERFACE
        sctrl.c
)
