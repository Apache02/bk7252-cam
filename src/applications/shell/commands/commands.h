#pragma once

#include "shell/Console.h"

void command_echo(Console &c);

void command_reboot(Console &c);

void command_dump(Console &c);

void command_dump32(Console &c);

void command_chip_id(Console &c);

void command_partitions(Console &c);

void command_watch_reg(Console &c);

void command_timer(Console &c);

void command_cpu_speed(Console &c);

void command_sys_clk_source(Console &c);

void command_sys_clk_div(Console &c);

void comand_sys_ctl_print(Console &c);

void comand_sys_ctl_test(Console &c);

void command_pwm(Console &c);

void command_pwm_stop(Console &c);

void command_timers_test(Console &c);

void command_timers_test2(Console &c);
