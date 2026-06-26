#include "soc/sctrl.h"
#include "hardware/sctrl.h"


typedef enum {
    MCLK_SOURCE_DCO = 0,
    MCLK_SOURCE_26M_XTAL,
    MCLK_SOURCE_DPLL,
    MCLK_SOURCE_LPO,
} mclk_source_t;


#define DPLL_CLOCK_HZ     (480 * 1000 * 1000)
#define XTAL_26M_CLOCK_HZ (26 * 1000 * 1000)


static const struct {
    unsigned int  freq;
    mclk_source_t src;
    unsigned int  divider;
} configs[] = {
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

bool sctrl_set_cpu_freq_hz(uint32_t freq) {
    for (unsigned int i = 0; i < count_of(configs); i++) {
        if (configs[i].freq == freq) {
            typeof(hw_sctrl->control) reg;
            reg.v               = hw_sctrl->control.v;
            reg.mclk_source     = configs[i].src;
            reg.divider         = configs[i].divider;
            hw_sctrl->control.v = reg.v;

            return true;
        }
    }
    return false;
}

uint32_t sctrl_get_cpu_freq_hz() {
    uint32_t source = hw_sctrl->control.mclk_source;
    unsigned int clk_freq;

    if (source == MCLK_SOURCE_DPLL) {
        clk_freq = DPLL_CLOCK_HZ;
    } else {
        clk_freq = XTAL_26M_CLOCK_HZ;
    }

    return clk_freq / (hw_sctrl->control.divider + 1);
}
