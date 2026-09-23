simple_drivers_library(hardware_i2c)
target_link_libraries(
        hardware_i2c INTERFACE
        hardware_icu
        hardware_gpio
        hardware_intc
)
target_sources(
        hardware_i2c INTERFACE
        i2c1.c
        i2c2.c
)
