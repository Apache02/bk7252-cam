simple_drivers_library(hardware_jpeg)
target_link_libraries(
        hardware_jpeg INTERFACE
        hardware_icu
)
target_sources(
    hardware_jpeg INTERFACE
    jpeg.c
)
