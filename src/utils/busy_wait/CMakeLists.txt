add_library(
        utils_busy_wait INTERFACE
)

target_sources(
        utils_busy_wait INTERFACE
        busy_wait.c
)

target_include_directories(
        utils_busy_wait INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(
        utils_busy_wait INTERFACE
)
