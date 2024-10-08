simple_drivers_library(hardware_wdt)
target_link_libraries(
        hardware_wdt INTERFACE
        hardware_icu
)
target_sources(
        hardware_wdt INTERFACE
        wdt.c
)
