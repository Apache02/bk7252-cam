simple_drivers_library(hardware_gdma)
target_link_libraries(
        hardware_gdma INTERFACE
        hardware_intc
)
target_sources(
        hardware_gdma INTERFACE
        gdma.c
)