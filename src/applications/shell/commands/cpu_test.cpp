#include "commands.h"
#include <stdio.h>


// utils
#define TRACE(var)                          printf(#var " = 0x%x\r\n", var)
#define TRACE_CHANGE(var, new_value)        printf(#var ": 0x%x -> 0x%x\r\n", var, new_value)
#define REG_READ(addr)                      (*((volatile UINT32 *)(addr)))
#define REG_WRITE(addr, _data)              (*((volatile UINT32 *)(addr)) = (_data))


#define SCTRL_BASE                          (0x00800000)
#define SCTRL_CONTROL                       (SCTRL_BASE + 02 * 4)


#define MCLK_FIELD_DCO                      (0x0)
#define MCLK_FIELD_26M_XTAL                 (0x1)
#define MCLK_FIELD_DPLL                     (0x2)
#define MCLK_FIELD_LPO                      (0x3)

//#define DCO_CALIB_26M           0x1
//#define DCO_CALIB_60M           0x2
//#define DCO_CALIB_80M           0x3
//#define DCO_CALIB_120M          0x4
//#define DCO_CALIB_180M          0x5
//
//#define DCO_CLK_SELECT          DCO_CALIB_180M


typedef struct {
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t mclk_source: 2;            // [0:1]
            uint32_t hclk_div2: 2;              // [2:3]
            uint32_t divider: 4;                // [4:7]
            uint32_t flash_26m: 1;              // [8]
            uint32_t hclk_div2_en: 1;           // [9]
            uint32_t modem_clk480m_pwd: 1;      // [10]
            uint32_t mac_clk480m__pwd: 1;       // [11]
            uint32_t mpif_clk_invert_bit: 1;    // [12]
            uint32_t sdio_clk_invert_bit: 1;    // [13]
            uint32_t dpll_div_reset: 1;         // [14]
            uint32_t ble_rf_en: 1;              // [15]
            uint32_t qspi_io_volt: 2;           // [16:17]
            uint32_t flash_sck_io_cap: 2;       // [18:19]
            uint32_t psram_vddpad_volt: 2;      // [20:21]
            uint32_t flash_spi_mux_bit: 1;      // [22]
            uint32_t efuse_vdd25_en: 1;         // [23]
            uint32_t reserved_24_31: (31 - 24 + 1);
        } bits;
    };
} hw_sys_ctrl_t;

#define hw_sys_ctrl      ((volatile hw_sys_ctrl_t *)SCTRL_CONTROL)

void comand_sys_ctl_print(Console &c) {
    TRACE((unsigned int) hw_sys_ctrl->reg);
    TRACE(hw_sys_ctrl->bits.mclk_source);
    TRACE(hw_sys_ctrl->bits.hclk_div2);
    TRACE(hw_sys_ctrl->bits.divider);
    TRACE(hw_sys_ctrl->bits.flash_26m);
    TRACE(hw_sys_ctrl->bits.hclk_div2_en);
    TRACE(hw_sys_ctrl->bits.modem_clk480m_pwd);
    TRACE(hw_sys_ctrl->bits.mac_clk480m__pwd);
    TRACE(hw_sys_ctrl->bits.mpif_clk_invert_bit);
    TRACE(hw_sys_ctrl->bits.sdio_clk_invert_bit);
    TRACE(hw_sys_ctrl->bits.dpll_div_reset);
    TRACE(hw_sys_ctrl->bits.ble_rf_en);
    TRACE(hw_sys_ctrl->bits.qspi_io_volt);
    TRACE(hw_sys_ctrl->bits.flash_sck_io_cap);
    TRACE(hw_sys_ctrl->bits.psram_vddpad_volt);
    TRACE(hw_sys_ctrl->bits.flash_spi_mux_bit);
    TRACE(hw_sys_ctrl->bits.efuse_vdd25_en);
    TRACE(hw_sys_ctrl->bits.reserved_24_31);
}

void comand_sys_ctl_test(Console &c) {
    // its disable MAC and made impossible to read time
     hw_sys_ctrl->bits.hclk_div2 = 0b11;
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
        TRACE_CHANGE(hw_sys_ctrl->bits.mclk_source, set_value);
        hw_sys_ctrl->bits.mclk_source = set_value;
    }

    printf("hw_sys_ctrl->mclk_source = %s\r\n", sys_clk_sources_names_tbl[hw_sys_ctrl->bits.mclk_source]);
}

void command_sys_clk_div(Console &c) {
    int set_value = c.packet.take_int().ok_or(-1);

    if (!(set_value == -1 || (set_value >= 0 && set_value <= 0b1111))) {
        printf(COLOR_RED("Invalid value [%d]") "\r\n", set_value);
        return;
    }

    if (set_value != -1) {
        TRACE_CHANGE(hw_sys_ctrl->bits.divider, set_value);
        hw_sys_ctrl->bits.divider = set_value;
    }

    TRACE(hw_sys_ctrl->bits.divider);
}

