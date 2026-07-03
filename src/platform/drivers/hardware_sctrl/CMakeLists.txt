simple_drivers_library(hardware_sctrl)
target_link_libraries(
        hardware_sctrl INTERFACE
        hardware_icu
        hardware_intc
)
target_sources(
        hardware_sctrl INTERFACE
        sctrl.c
        cpu_freq.c
)
