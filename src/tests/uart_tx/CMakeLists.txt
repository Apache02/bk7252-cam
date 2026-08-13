add_executable(
        test_uart_tx
        test_uart_tx.cpp
)

target_link_libraries(
        test_uart_tx PRIVATE
        platform_nosys
        platform_stdio_uart2
        platform_cpu
        hardware_uart
        hardware_intc
        hardware_time
        hardware_wdt
        utils_busy_wait
        utils_ring_buffer
)

bk_firmware_iram(test_uart_tx)
