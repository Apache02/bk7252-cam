add_executable(
        hello
        hello.cpp
)

bk_firmware(hello)

target_link_libraries(
        hello PRIVATE
        platform_nosys
        hardware_uart
        utils
)
