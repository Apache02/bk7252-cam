add_library(platform_promise INTERFACE)

target_sources(
        platform_promise INTERFACE
        Promise.cpp
        Shared.cpp
)

target_include_directories(
        platform_promise INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_promise INTERFACE
        platform_freertos
        platform_cpu
)
