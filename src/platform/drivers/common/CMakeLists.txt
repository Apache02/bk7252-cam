add_library(hardware_common INTERFACE)
target_include_directories(
        hardware_common INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
