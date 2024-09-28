add_executable(
        blink
        blink.cpp
)

bk_firmware(blink)

target_link_libraries(
        blink PRIVATE
        platform_stdio
        utils
)
