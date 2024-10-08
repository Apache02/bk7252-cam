add_library(
        utils_shell INTERFACE
)

target_include_directories(
        utils_shell INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        utils_shell INTERFACE
        Console.cpp
        Packet.cpp
        History.cpp
)
