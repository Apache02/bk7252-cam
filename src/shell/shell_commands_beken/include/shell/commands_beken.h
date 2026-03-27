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

int command_uart_baudrate(int argc, const char *argv[]);

#ifdef __cplusplus
}

template<typename T>
int command_flash_dump(int argc, const char *argv[]);

#endif
