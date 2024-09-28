add_executable(
        console
        console.cpp
)

bk_firmware(console)

target_link_libraries(
        console PRIVATE
        platform_stdio
        utils
)
