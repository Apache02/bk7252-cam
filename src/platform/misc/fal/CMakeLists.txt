add_library(platform_fal INTERFACE)

target_sources(
        platform_fal INTERFACE
        fal.c
)

target_include_directories(
        platform_fal INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        platform_fal INTERFACE
        hardware_flash
)
