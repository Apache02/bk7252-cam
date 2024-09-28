#ifndef _GPIO_H_
#define _GPIO_H_

#include "typedef.h"


enum {
    GMODE_INPUT_PULLDOWN = 0,
    GMODE_OUTPUT,
    GMODE_SECOND_FUNC,
    GMODE_INPUT_PULLUP,
    GMODE_INPUT,
    GMODE_SECOND_FUNC_PULL_UP,//Special for uart1
    GMODE_OUTPUT_PULLUP,
    GMODE_SET_HIGH_IMPENDANCE,
    GMODE_HIGH_Z
};

typedef enum {
    GPIO0 = 0,
    GPIO1,
    GPIO2,
    GPIO3,
    GPIO4,
    GPIO5,
    GPIO6,
    GPIO7,
    GPIO8,
    GPIO9,
    GPIO10,
    GPIO11,
    GPIO12,
    GPIO13,
    GPIO14,
    GPIO15,
    GPIO16,
    GPIO17,
    GPIO18,
    GPIO19,
    GPIO20,
    GPIO21,
    GPIO22,
    GPIO23,
    GPIO24,
    GPIO25,
    GPIO26,
    GPIO27,
    GPIO28,
    GPIO29,
    GPIO30,
    GPIO31,
    GPIO32,
    GPIO33,
    GPIO34,
    GPIO35,
    GPIO36,
    GPIO37,
    GPIO38,
    GPIO39,
    GPIONUM
} GPIO_INDEX;

enum {
    GFUNC_MODE_UART2 = 0,
    GFUNC_MODE_I2C2,
    GFUNC_MODE_I2S,
    GFUNC_MODE_ADC1,
    GFUNC_MODE_ADC2,
    GFUNC_MODE_CLK13M,
    GFUNC_MODE_PWM0,
    GFUNC_MODE_PWM1,
    GFUNC_MODE_PWM2,
    GFUNC_MODE_PWM3,
    GFUNC_MODE_WIFI_ACTIVE,
    GFUNC_MODE_BT_ACTIVE,
    GFUNC_MODE_BT_PRIORITY,
    GFUNC_MODE_UART1,
    GFUNC_MODE_SD_DMA,
    GFUNC_MODE_SD_HOST,
    GFUNC_MODE_SPI_DMA,
    GFUNC_MODE_SPI,
    GFUNC_MODE_PWM4,
    GFUNC_MODE_PWM5,
    GFUNC_MODE_I2C1,
    GFUNC_MODE_JTAG,
    GFUNC_MODE_CLK26M,
    GFUNC_MODE_ADC3,
    GFUNC_MODE_DCMI,
    GFUNC_MODE_ADC4,
    GFUNC_MODE_ADC5,
    GFUNC_MODE_ADC6,
    GFUNC_MODE_ADC7,
    GFUNC_MODE_SD1_HOST,
    GFUNC_MODE_SPI1,
    GFUNC_MODE_SPI_DMA1,
    GFUNC_MODE_QSPI_1LINE,
    GFUNC_MODE_QSPI_4LINE,
    GFUNC_MODE_QSPI_CLK,
    GFUNC_MODE_QSPI_CSN,
};

enum {
    GPIO_INT_LEVEL_LOW = 0,
    GPIO_INT_LEVEL_HIGH = 1,
    GPIO_INT_LEVEL_RISING = 2,
    GPIO_INT_LEVEL_FALLING = 3
};


#ifdef __cplusplus
extern "C" {
#endif

extern UINT32 gpio_input(UINT32 id);

//extern void gpio_init(void);
//extern void gpio_exit(void);
//void gpio_int_disable(UINT32 index);
//void gpio_int_enable(UINT32 index, UINT32 mode, void (*p_Int_Handler)(unsigned char));
void gpio_config(UINT32 index, UINT32 mode);

void gpio_output(UINT32 id, UINT32 val);

#ifdef __cplusplus
}
#endif


#endif // _GPIO_H_
