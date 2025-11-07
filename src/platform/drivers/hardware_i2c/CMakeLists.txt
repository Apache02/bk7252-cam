simple_drivers_library(hardware_i2c)
target_link_libraries(
        hardware_timer INTERFACE
        hardware_icu
        hardware_intc
        hardware_gpio
)
target_sources(
        hardware_i2c INTERFACE
#        i2c1_dummy.c
        i2c1_sync.c
#        i2c1_async.c
)
