simple_drivers_library(hardware_timer)
target_link_libraries(
        hardware_timer INTERFACE
        hardware_icu
        hardware_intc
)
target_sources(
        hardware_timer INTERFACE
        timer.c
        sys_counter.c
)

