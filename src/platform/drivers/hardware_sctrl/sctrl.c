#include "hardware/sctrl.h"
#include "hardware/sctrl_regs.h"

typedef enum {
    MCLK_SOURCE_DCO = 0,
    MCLK_SOURCE_26M_XTAL,
    MCLK_SOURCE_DPLL,
    MCLK_SOURCE_LPO,
} mclk_source_t;


uint32_t chip_id() {
    return hw_sctrl->chip_id;
}

uint32_t device_id() {
    return hw_sctrl->device_id;
}

#define REG_RC_BASE_ADDR        (0x01050000)
#define RC_CNTL_STAT_ADDR       REG_RC_BASE_ADDR


void sctrl_rf_init() {
    hw_sctrl->modem_core_reset_phy_hclk.phy_hclk_enable = 1;
    hw_sctrl->control.modem_clk480m_pwd = 0;

    /*Enable BK7011:rc_en,ch0_en*/
//    rc_cntl_stat_set(0x09);
    (*(volatile uint32_t *)(RC_CNTL_STAT_ADDR)) = 0x09;
//    REG_PL_WR(RC_CNTL_STAT_ADDR, 0x09);

}