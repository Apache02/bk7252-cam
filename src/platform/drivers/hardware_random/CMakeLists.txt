simple_drivers_library(hardware_random)
target_link_libraries(
        hardware_random INTERFACE
        common_defs
)
target_sources(
        hardware_random INTERFACE
        random.c
)
