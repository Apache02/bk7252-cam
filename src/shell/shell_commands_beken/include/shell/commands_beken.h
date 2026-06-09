#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int command_reboot(int argc, const char *argv[]);

int command_partitions(int argc, const char *argv[]);

int command_stack(int argc, const char *argv[]);

int command_chip_id(int argc, const char *argv[]);

int command_efuse(int argc, const char *argv[]);

int command_random_test(int argc, const char *argv[]);

int command_time_delay(int argc, const char *argv[]);

int command_uptime(int argc, const char *argv[]);

int command_cpu_speed(int argc, const char *argv[]);

int command_mclk(int argc, const char *argv[]);

int command_uart_baudrate(int argc, const char *argv[]);

int command_uart1_baudrate(int argc, const char *argv[]);

int command_uart2_baudrate(int argc, const char *argv[]);

// Disable interrupts and jump to address.
int command_jump(int argc, const char *argv[]);

int command_jump_app(int argc, const char *argv[]);

int command_buffer(int argc, const char *argv[]);

int command_flash_crc32(int argc, const char *argv[]);

int command_flash_dump(int argc, const char *argv[]);

int command_flash_read_binary(int argc, const char *argv[]);

int command_flash_write(int argc, const char *argv[]);

int command_sha1(int argc, const char *argv[]);

int command_sha224(int argc, const char *argv[]);

int command_sha256(int argc, const char *argv[]);

int command_sha512(int argc, const char *argv[]);

#ifdef __cplusplus
}

#endif
