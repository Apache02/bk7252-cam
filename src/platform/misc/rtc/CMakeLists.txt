add_library(platform_rtc INTERFACE)

target_sources(
        platform_rtc INTERFACE
        rtc.c
)

target_include_directories(
        platform_rtc INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_rtc INTERFACE
        hardware_timer
)
