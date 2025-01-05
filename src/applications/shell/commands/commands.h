#pragma once

#include "shell/Console.h"
#include "shell/commands_common.h"
#include "shell/commands_beken.h"


void command_reboot(Console &c);

void command_watch_reg(Console &c);

void command_sys_clk_source(Console &c);

void command_sys_clk_div(Console &c);

void comand_sys_ctl_print(Console &c);

void comand_sys_ctl_test(Console &c);

void command_pwm(Console &c);

void command_pwm_stop(Console &c);

void command_gpio_blink(Console &c);

void command_sos(Console &c);

void command_timers_test(Console &c);

void command_timers_test2(Console &c);

void command_write_regs(Console &c);