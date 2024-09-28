#include "drivers/gpio.h"
#include "arch.h"


#define GPIO_BASE_ADDR                       (0x0802800)

#define REG_GPIO_0_CONFIG                    (GPIO_BASE_ADDR + 0*4)
#define REG_GPIO_1_CONFIG                    (GPIO_BASE_ADDR + 1*4)
#define REG_GPIO_2_CONFIG                    (GPIO_BASE_ADDR + 2*4)
#define REG_GPIO_3_CONFIG                    (GPIO_BASE_ADDR + 3*4)
#define REG_GPIO_4_CONFIG                    (GPIO_BASE_ADDR + 4*4)
#define REG_GPIO_5_CONFIG                    (GPIO_BASE_ADDR + 5*4)
#define REG_GPIO_6_CONFIG                    (GPIO_BASE_ADDR + 6*4)
#define REG_GPIO_7_CONFIG                    (GPIO_BASE_ADDR + 7*4)
#define REG_GPIO_8_CONFIG                    (GPIO_BASE_ADDR + 8*4)
#define REG_GPIO_9_CONFIG                    (GPIO_BASE_ADDR + 9*4)
#define REG_GPIO_10_CONFIG                   (GPIO_BASE_ADDR + 10*4)
#define REG_GPIO_11_CONFIG                   (GPIO_BASE_ADDR + 11*4)
#define REG_GPIO_12_CONFIG                   (GPIO_BASE_ADDR + 12*4)
#define REG_GPIO_13_CONFIG                   (GPIO_BASE_ADDR + 13*4)
#define REG_GPIO_14_CONFIG                   (GPIO_BASE_ADDR + 14*4)
#define REG_GPIO_15_CONFIG                   (GPIO_BASE_ADDR + 15*4)
#define REG_GPIO_16_CONFIG                   (GPIO_BASE_ADDR + 16*4)
#define REG_GPIO_17_CONFIG                   (GPIO_BASE_ADDR + 17*4)
#define REG_GPIO_18_CONFIG                   (GPIO_BASE_ADDR + 18*4)
#define REG_GPIO_19_CONFIG                   (GPIO_BASE_ADDR + 19*4)
#define REG_GPIO_20_CONFIG                   (GPIO_BASE_ADDR + 20*4)
#define REG_GPIO_21_CONFIG                   (GPIO_BASE_ADDR + 21*4)
#define REG_GPIO_22_CONFIG                   (GPIO_BASE_ADDR + 22*4)
#define REG_GPIO_23_CONFIG                   (GPIO_BASE_ADDR + 23*4)
#define REG_GPIO_24_CONFIG                   (GPIO_BASE_ADDR + 24*4)
#define REG_GPIO_25_CONFIG                   (GPIO_BASE_ADDR + 25*4)
#define REG_GPIO_26_CONFIG                   (GPIO_BASE_ADDR + 26*4)
#define REG_GPIO_27_CONFIG                   (GPIO_BASE_ADDR + 27*4)
#define REG_GPIO_28_CONFIG                   (GPIO_BASE_ADDR + 28*4)
#define REG_GPIO_29_CONFIG                   (GPIO_BASE_ADDR + 29*4)
#define REG_GPIO_30_CONFIG                   (GPIO_BASE_ADDR + 30*4)
#define REG_GPIO_31_CONFIG                   (GPIO_BASE_ADDR + 31*4)

#define REG_GPIO_CFG_BASE_ADDR               (REG_GPIO_0_CONFIG)


#define GCFG_INPUT_POS                       0
#define GCFG_OUTPUT_POS                      1
#define GCFG_INPUT_ENABLE_POS                2
#define GCFG_OUTPUT_ENABLE_POS               3
#define GCFG_PULL_MODE_POS                   4
#define GCFG_PULL_ENABLE_POS                 5
#define GCFG_FUNCTION_ENABLE_POS             6
#define GCFG_INPUT_MONITOR_POS               7


#define GCFG_INPUT_BIT                       (1 << 0)
#define GCFG_OUTPUT_BIT                      (1 << 1)
#define GCFG_INPUT_ENABLE_BIT                (1 << 2)
#define GCFG_OUTPUT_ENABLE_BIT               (1 << 3)
#define GCFG_PULL_MODE_BIT                   (1 << 4)
#define GCFG_PULL_ENABLE_BIT                 (1 << 5)
#define GCFG_FUNCTION_ENABLE_BIT             (1 << 6)
#define GCFG_INPUT_MONITOR_BIT               (1 << 7)


#define REG_GPIO_FUNC_CFG                    (GPIO_BASE_ADDR + 32*4)
#define PERIAL_MODE_1                         (0)
#define PERIAL_MODE_2                         (1)
#define PERIAL_MODE_3                         (2)
#define PERIAL_MODE_4                         (3)

#define REG_GPIO_INTEN                       (GPIO_BASE_ADDR + 33*4)
#define REG_GPIO_INTLV0                      (GPIO_BASE_ADDR + 34*4)
#define REG_GPIO_INTLV1                      (GPIO_BASE_ADDR + 35*4)
#define REG_GPIO_INTSTA                      (GPIO_BASE_ADDR + 36*4)

#define REG_GPIO_EXTRAL_INT_CFG              (GPIO_BASE_ADDR + 38*4)
#define DPLL_UNLOCK_INT                       (1 << 0)

#define REG_GPIO_DETECT                      (GPIO_BASE_ADDR + 39*4)
#define IS_OVER_TEMP_DECT_BIT                 (1 << 0)
#define IS_USB_PLUG_IN_BIT                    (1 << 1)

#define REG_GPIO_ENC_WORD                    (GPIO_BASE_ADDR + 40*4)
#define REG_GPIO_DBG_MSG                     (GPIO_BASE_ADDR + 41*4)
#define REG_GPIO_DBG_MUX                     (GPIO_BASE_ADDR + 42*4)
#define REG_GPIO_DBG_CFG                     (GPIO_BASE_ADDR + 43*4)
#define REG_GPIO_DBG_REPORT                  (GPIO_BASE_ADDR + 44*4)

#define REG_GPIO_MODULE_SELECT               (GPIO_BASE_ADDR + 45*4)
#define GPIO_MODUL_NONE                      0xff

#define GPIO_SD_DMA_MODULE                   (0 << 1)
#define GPIO_SD_HOST_MODULE                  (1 << 1)
#define GPIO_SD_MODULE_POS                    1
#define GPIO_SD_MODULE_MASK                   (1 << GPIO_SD_MODULE_POS)

#define GPIO_SPI_DMA_MODULE                  (0 << 0)
#define GPIO_SPI_MODULE                      (1 << 0)
#define GPIO_SPI_MODULE_POS                   0
#define GPIO_SPI_MODULE_MASK                  (1 << GPIO_SPI_MODULE_POS)

#define REG_A0_CONFIG                        (GPIO_BASE_ADDR + 48*4)
#define REG_A1_CONFIG                        (GPIO_BASE_ADDR + 49*4)
#define REG_A2_CONFIG                        (GPIO_BASE_ADDR + 50*4)
#define REG_A3_CONFIG                        (GPIO_BASE_ADDR + 51*4)
#define REG_A4_CONFIG                        (GPIO_BASE_ADDR + 52*4)
#define REG_A5_CONFIG                        (GPIO_BASE_ADDR + 53*4)
#define REG_A6_CONFIG                        (GPIO_BASE_ADDR + 54*4)
#define REG_A7_CONFIG                        (GPIO_BASE_ADDR + 55*4)


void gpio_config(UINT32 index, UINT32 mode) {
    UINT32 val;
    UINT32 overstep = 0;
    volatile UINT32 *gpio_cfg_addr;

    if (index >= GPIONUM) {
        // todo panic
        return;
    }

    gpio_cfg_addr = (volatile UINT32 *) (REG_GPIO_CFG_BASE_ADDR + index * 4);

    switch (mode) {
        case GMODE_INPUT_PULLDOWN:
            val = 0x2C;
            break;

        case GMODE_OUTPUT:
            val = 0x00;
            break;

        case GMODE_SECOND_FUNC:
            val = 0x48;
            break;

        case GMODE_INPUT_PULLUP:
            val = 0x3C;
            break;

        case GMODE_INPUT:
            val = 0x0C;
            break;

        case GMODE_SECOND_FUNC_PULL_UP:
            val = 0x78;
            break;

        case GMODE_SET_HIGH_IMPENDANCE:
            val = 0x08;
            break;

        default:
            overstep = 1;
            break;
    }

    if (0 == overstep) {
        REG_WRITE(gpio_cfg_addr, val);
    }

    return;
}


static void gpio_enable_second_function(UINT32 func_mode) {
    UINT32 i;
    UINT32 reg;
    UINT32 modul_select = GPIO_MODUL_NONE;
    UINT32 pmode = PERIAL_MODE_1;
    UINT32 pmask = 0;
    UINT32 end_index = 0;
    UINT32 start_index = 0;
    UINT32 config_pull_up = 0;

    switch (func_mode) {
        case GFUNC_MODE_UART2:
            start_index = 0;
            end_index = 1;
            pmode = PERIAL_MODE_1;
            config_pull_up = 1;
            break;

        case GFUNC_MODE_I2C2:
            start_index = 0;
            end_index = 1;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_I2S:
            start_index = 2;
            end_index = 5;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_ADC1:
            start_index = 4;
            end_index = 4;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_ADC2:
            start_index = 5;
            end_index = 5;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_CLK13M:
            start_index = 6;
            end_index = 6;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_PWM0:
            start_index = 6;
            end_index = 6;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_PWM1:
            start_index = 7;
            end_index = 7;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_PWM2:
            start_index = 8;
            end_index = 8;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_PWM3:
            start_index = 9;
            end_index = 9;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_BT_PRIORITY:
            start_index = 9;
            end_index = 9;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_WIFI_ACTIVE:
            start_index = 7;
            end_index = 7;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_BT_ACTIVE:
            start_index = 8;
            end_index = 8;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_UART1:
            start_index = 10;
            end_index = 13;
            pmode = PERIAL_MODE_1;
            config_pull_up = 1;
            break;

        case GFUNC_MODE_SD_DMA:
            start_index = 14;
            end_index = 19;
            pmode = PERIAL_MODE_1;
            modul_select = GPIO_SD_DMA_MODULE;
            pmask = GPIO_SD_MODULE_MASK;
            break;

        case GFUNC_MODE_SD_HOST:
            start_index = 14;
            end_index = 19;
            pmode = PERIAL_MODE_1;
            config_pull_up = 1;
            modul_select = GPIO_SD_HOST_MODULE;
            pmask = GPIO_SD_MODULE_MASK;
            break;

        case GFUNC_MODE_SPI_DMA:
            start_index = 14;
            end_index = 17;
            pmode = PERIAL_MODE_2;
            modul_select = GPIO_SPI_DMA_MODULE;
            pmask = GPIO_SPI_MODULE_MASK;
            break;

        case GFUNC_MODE_SPI:
            start_index = 14;
            end_index = 17;
            pmode = PERIAL_MODE_2;
            modul_select = GPIO_SPI_MODULE;
            pmask = GPIO_SPI_MODULE_MASK;
            break;

        case GFUNC_MODE_PWM4:
            start_index = 24;
            end_index = 24;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_PWM5:
            start_index = 26;
            end_index = 26;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_I2C1:
            start_index = 20;
            end_index = 21;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_JTAG:
            start_index = 20;
            end_index = 23;
            pmode = PERIAL_MODE_2;
            break;

        case GFUNC_MODE_CLK26M:
            start_index = 22;
            end_index = 22;
            pmode = PERIAL_MODE_1;
            break;

        case GFUNC_MODE_ADC3:
            start_index = 23;
            end_index = 23;
            pmode = PERIAL_MODE_1;
            break;

        default:
            break;
    }

    for (i = start_index; i <= end_index; i++) {
        if (config_pull_up == 0)
            gpio_config(i, GMODE_SECOND_FUNC);
        else
            gpio_config(i, GMODE_SECOND_FUNC_PULL_UP);

        reg = REG_READ(REG_GPIO_FUNC_CFG);
        if (PERIAL_MODE_1 == pmode) {
            reg &= ~(BIT(i));
        } else /*(PERIAL_MODE_2 == pmode)*/
        {
            reg |= BIT(i);
        }
        REG_WRITE(REG_GPIO_FUNC_CFG, reg);
    }

    if (modul_select != GPIO_MODUL_NONE) {
        reg = REG_READ(REG_GPIO_MODULE_SELECT);
        reg &= ~(pmask);
        reg |= modul_select;
        REG_WRITE(REG_GPIO_MODULE_SELECT, reg);
    }
    return;
}

UINT32 gpio_input(UINT32 id) {
    UINT32 val = 0;
    volatile UINT32 *gpio_cfg_addr;

    gpio_cfg_addr = (volatile UINT32 *) (REG_GPIO_CFG_BASE_ADDR + id * 4);

    val = REG_READ(gpio_cfg_addr);

    return (val & GCFG_INPUT_BIT);
}

void gpio_output(UINT32 id, UINT32 val) {
    UINT32 reg_val;
    volatile UINT32 *gpio_cfg_addr;

    gpio_cfg_addr = (volatile UINT32 *) (REG_GPIO_CFG_BASE_ADDR + id * 4);

    reg_val = REG_READ(gpio_cfg_addr);

    reg_val &= ~GCFG_OUTPUT_BIT;
    reg_val |= (val & 0x01) << GCFG_OUTPUT_POS;
    REG_WRITE(gpio_cfg_addr, reg_val);

    return;
}

static void gpio_output_reverse(UINT32 id) {
    UINT32 reg_val;
    volatile UINT32 *gpio_cfg_addr;


    gpio_cfg_addr = (volatile UINT32 *) (REG_GPIO_CFG_BASE_ADDR + id * 4);

    reg_val = REG_READ(gpio_cfg_addr);
    reg_val ^= GCFG_OUTPUT_BIT;
    REG_WRITE(gpio_cfg_addr, reg_val);

    reverse_exit:
    return;
}

// EOF
