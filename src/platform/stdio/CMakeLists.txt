add_library(
        platform_stdio
        stdio.c
)

target_include_directories(
        platform_stdio INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
