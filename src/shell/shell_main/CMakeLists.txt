add_library(
        shell_main INTERFACE
)

target_include_directories(
        shell_main INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        shell_main INTERFACE
        Shell.cpp
        Parser.cpp
        History.cpp
)
