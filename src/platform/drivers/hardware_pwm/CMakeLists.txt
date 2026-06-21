simple_drivers_library(hardware_pwm)
target_link_libraries(
        hardware_pwm INTERFACE
        hardware_icu
        hardware_intc
)
target_sources(
        hardware_pwm INTERFACE
        pwm.c
)
