add_executable(
        hello
        hello.cpp
)

bk_firmware(hello)

target_link_libraries(
        hello PRIVATE
        hardware_uart
        utils
)
