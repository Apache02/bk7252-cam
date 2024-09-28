add_executable(
        hello
        main.cpp
)

bk_firmware(hello)

target_link_libraries(
        hello PRIVATE
        platform_stdio
        utils
)
