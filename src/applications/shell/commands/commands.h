#pragma once

int command_reboot(int argc, const char *argv[]);

int command_watch_reg(int argc, const char *argv[]);

int command_sys_clk_source(int argc, const char *argv[]);

int command_sys_clk_div(int argc, const char *argv[]);

int comand_sys_ctl_print(int argc, const char *argv[]);

int comand_sys_ctl_test(int argc, const char *argv[]);

int command_pwm(int argc, const char *argv[]);

int command_pwm_stop(int argc, const char *argv[]);

int command_gpio_blink(int argc, const char *argv[]);

int command_sos(int argc, const char *argv[]);

int command_timers_test(int argc, const char *argv[]);

int command_timers_test2(int argc, const char *argv[]);

int command_write_regs(int argc, const char *argv[]);
