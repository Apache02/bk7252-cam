add_executable(
        debug
        main.cpp
)

target_include_directories(
        debug INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/
)

add_subdirectory(console)

target_link_libraries(
        debug PRIVATE
        platform_stdio
        console
        utils
)

bk_firmware(debug)
