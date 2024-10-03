add_library(console INTERFACE)

file(
        GLOB COMMANDS
        ${CMAKE_CURRENT_LIST_DIR}/commands/*.cpp
)

target_sources(
        console INTERFACE
        Console.cpp
        Packet.cpp
        handlers.cpp

        ${COMMANDS}
)

target_link_libraries(
        console INTERFACE
)
