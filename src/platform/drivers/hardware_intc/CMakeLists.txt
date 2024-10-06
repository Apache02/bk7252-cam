simple_drivers_library(hardware_intc)
target_sources(
        hardware_intc INTERFACE
        intc.c
        int_handlers.c
)

