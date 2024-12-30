add_library(common_defs INTERFACE)
target_include_directories(
        common_defs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
