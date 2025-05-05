#pragma once

#include "shell/Console.h"

void command_partitions(Console &c);

void command_stack(Console &c);

void command_chip_id(Console &c);

void command_efuse(Console &c);

void command_random_test(Console &c);

template<typename T>
void command_flash_dump(__unused Console &c);

void command_time_delay(Console &c);

void command_uptime(Console &c);

void command_cpu_speed(Console &c);
