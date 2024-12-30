simple_drivers_library(hardware_gpio)
target_link_libraries(
        hardware_gpio INTERFACE
        common_defs
)
target_sources(
        hardware_gpio INTERFACE
        gpio.c
)
