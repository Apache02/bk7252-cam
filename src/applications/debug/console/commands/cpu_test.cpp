#include "commands.h"
#include <stdio.h>

#include "typedef.h"
#include "utils/console_colors.h"


// utils
#define TRACE(var)                          printf(#var " = 0x%x\r\n", var)
#define TRACE_CHANGE(var, new_value)        printf(#var ": 0x%x -> 0x%x\r\n", var, new_value)
#define REG_READ(addr)                      (*((volatile UINT32 *)(addr)))
#define REG_WRITE(addr, _data)              (*((volatile UINT32 *)(addr)) = (_data))


#define SCTRL_BASE                          (0x00800000)
#define SCTRL_CONTROL                       (SCTRL_BASE + 02 * 4)

#define SYS_CTRL_CLK_SET_MCLK_DCO           0x00
#define SYS_CTRL_CLK_SET_MCLK_XTAL_26M      0x01
#define SYS_CTRL_CLK_SET_MCLK_DPLL          0x02
#define SYS_CTRL_CLK_SET_MCLK_LPO           0x03



//#define DCO_CALIB_26M           0x1
//#define DCO_CALIB_60M           0x2
//#define DCO_CALIB_80M           0x3
//#define DCO_CALIB_120M          0x4
//#define DCO_CALIB_180M          0x5
//
//#define DCO_CLK_SELECT          DCO_CALIB_180M


typedef struct {
    uint32_t mclk_source: 2;        // [0:1]
    uint32_t _pad01: 2;             // [2:3]
    uint32_t divider: 4;            // [4:7]
    uint32_t flash_26m: 1;          // [8]
    uint32_t ahb_div: 1;            // [9]
    uint32_t modem_pwd: 1;          // [10]
    uint32_t mac_pwd: 1;            // [11]
    uint32_t mpif_clk: 1;           // [12]
    uint32_t sdio_clk: 1;           // [13]
    uint32_t _pad02: 18;            // [14:31]
} __attribute__((packed)) hw_sys_ctrl_t;

#define hw_sys_ctrl      ((hw_sys_ctrl_t *)SCTRL_CONTROL)

void comand_sys_ctl_print(Console &c) {
    uint32_t reg = REG_READ(SCTRL_CONTROL);
    TRACE(reg);
    TRACE(hw_sys_ctrl->mclk_source);
    TRACE(hw_sys_ctrl->_pad01);
    TRACE(hw_sys_ctrl->divider);
    TRACE(hw_sys_ctrl->flash_26m);
    TRACE(hw_sys_ctrl->ahb_div);
    TRACE(hw_sys_ctrl->modem_pwd);
    TRACE(hw_sys_ctrl->mac_pwd);
    TRACE(hw_sys_ctrl->mpif_clk);
    TRACE(hw_sys_ctrl->sdio_clk);
    TRACE(hw_sys_ctrl->_pad02);
}

void command_sys_clk_source(Console &c) {
    static const char *const sys_clk_sources_names_tbl[] = {
            [0] = "DCO",
            [1] = "XTAL_26M",
            [2] = "DPLL",
            [3] = "LPO",
    };

    int set_value = c.packet.take_int().ok_or(-1);

    if (!(set_value == -1 || (set_value >= 0 && set_value <= 3))) {
        printf(COLOR_RED("Invalid value [%d]") "\r\n", set_value);
        return;
    }

    if (set_value != -1) {
        TRACE_CHANGE(hw_sys_ctrl->mclk_source, set_value);
        hw_sys_ctrl->mclk_source = set_value;
    }

    printf("hw_sys_ctrl->mclk_source = %s\r\n", sys_clk_sources_names_tbl[hw_sys_ctrl->mclk_source]);
}

void command_sys_clk_div(Console &c) {
    int set_value = c.packet.take_int().ok_or(-1);

    if (!(set_value == -1 || (set_value >= 0 && set_value <= 0b1111))) {
        printf(COLOR_RED("Invalid value [%d]") "\r\n", set_value);
        return;
    }

    if (set_value != -1) {
        TRACE_CHANGE(hw_sys_ctrl->divider, set_value);
        hw_sys_ctrl->divider = set_value;
    }

    TRACE(hw_sys_ctrl->divider);
}

