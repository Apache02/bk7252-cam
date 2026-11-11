#include "soc/clock.h"
#include "soc/sctrl.h"
#include "hardware/sctrl.h"
#include "platform/cpu.h"


#define HCLK_MAX_FREQ (90000000)

typedef enum {
    MCLK_SOURCE_DCO = 0,
    MCLK_SOURCE_26M_XTAL,
    MCLK_SOURCE_DPLL,
    MCLK_SOURCE_LPO,
} mclk_source_t;


static const struct {
    unsigned int  freq;
    mclk_source_t src;
    unsigned int  divider;
} configs[] = {
    {CPU_FREQ_180_MHZ, MCLK_SOURCE_DCO, 0},
    {CPU_FREQ_160_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_160_MHZ) - 1},
    {CPU_FREQ_120_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_120_MHZ) - 1},
    {CPU_FREQ_96_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_96_MHZ) - 1},
    {CPU_FREQ_80_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_80_MHZ) - 1},
    {CPU_FREQ_60_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_60_MHZ) - 1},
    {CPU_FREQ_48_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_48_MHZ) - 1},
    {CPU_FREQ_40_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_40_MHZ) - 1},
    {CPU_FREQ_32_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_32_MHZ) - 1},
    {CPU_FREQ_30_MHZ, MCLK_SOURCE_DPLL, (DPLL_CLOCK_HZ / CPU_FREQ_30_MHZ) - 1},
    {CPU_FREQ_26_MHZ, MCLK_SOURCE_26M_XTAL, 0},
    {CPU_FREQ_13_MHZ, MCLK_SOURCE_26M_XTAL, 1},
};

#define count_of(x) (sizeof(x) / sizeof(x[0]))

// Vendor sctrl_dco_cali(DCO_CALIB_180M): a fixed count word, no measurement, then a reset and a
// trigger pulse. Every step is its own write: the register sits behind the analog SPI link.
static void dco_set_180mhz(void) {
    hw_sctrl_analog_ctrl1_t reg = {.v = sctrl_analog_get(&hw_sctrl->analog_ctrl1)};
    // The vendor's "180 MHz" word; measured 181.0-181.5 MHz with the divider bypassed. dco_cnti tunes the DCO
    // almost linearly, about 0.82 MHz per step up to 0xDD; it is not locked to the XTAL.
    reg.dco_cnti   = 0xDD;
    reg.dco_div    = 0;
    reg.div_bypass = 1;
    sctrl_analog_set(&hw_sctrl->analog_ctrl1, reg.v);

    reg.spi_rst = 0;
    sctrl_analog_set(&hw_sctrl->analog_ctrl1, reg.v);
    reg.spi_rst = 1;
    sctrl_analog_set(&hw_sctrl->analog_ctrl1, reg.v);
    reg.dco_trig = 1;
    sctrl_analog_set(&hw_sctrl->analog_ctrl1, reg.v);
    reg.dco_trig = 0;
    sctrl_analog_set(&hw_sctrl->analog_ctrl1, reg.v);

    // The vendor switches to the DCO much later in its init; give it time to settle (~0.5 ms at 120 MHz).
    for (volatile uint32_t n = 0; n < 20000; n++) {
    }
}

bool sctrl_set_cpu_freq_hz(uint32_t freq) {
    GLOBAL_INT_DECLARATION();

    for (unsigned int i = 0; i < count_of(configs); i++) {
        if (configs[i].freq == freq) {
            // Not while the core already runs on the DCO: retuning its own clock source.
            if (configs[i].src == MCLK_SOURCE_DCO && hw_sctrl->ctrl.mclk_source != MCLK_SOURCE_DCO) dco_set_180mhz();

            GLOBAL_INT_DISABLE();
            typeof(hw_sctrl->ctrl) reg;
            reg.v            = hw_sctrl->ctrl.v;
            reg.mclk_source  = configs[i].src;
            reg.mclk_div     = configs[i].divider;
            reg.hclk_div2_en = freq > HCLK_MAX_FREQ ? 1 : 0;
            hw_sctrl->ctrl.v = reg.v;
            GLOBAL_INT_RESTORE();

            return true;
        }
    }
    return false;
}

uint32_t sctrl_get_cpu_freq_hz() {
    uint32_t     source = hw_sctrl->ctrl.mclk_source;
    unsigned int clk_freq;

    if (source == MCLK_SOURCE_DPLL) {
        clk_freq = DPLL_CLOCK_HZ;
    } else if (source == MCLK_SOURCE_DCO) {
        clk_freq = CPU_FREQ_180_MHZ;
    } else {
        clk_freq = XTAL_CLOCK_HZ;
    }

    return clk_freq / (hw_sctrl->ctrl.mclk_div + 1);
}
