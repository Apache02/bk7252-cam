simple_drivers_library(hardware_uart)
target_link_libraries(
        hardware_uart INTERFACE
        hardware_icu
        hardware_gpio
)
target_sources(
        hardware_uart INTERFACE
        uart.c
)

