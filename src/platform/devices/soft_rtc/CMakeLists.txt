add_library(device_soft_rtc INTERFACE)

target_sources(
        device_soft_rtc INTERFACE
        soft_rtc.c
)

target_include_directories(
        device_soft_rtc INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        device_soft_rtc INTERFACE
        hardware_timer
)
