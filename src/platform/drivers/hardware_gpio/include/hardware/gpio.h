#ifndef _HARDWARE_GPIO_H
#define _HARDWARE_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GPIO_IN  = 0,
    GPIO_OUT = 1,
    GPIO_IN_PULLUP,
    GPIO_IN_PULLDOWN,
    GPIO_SECOND_FUNC,
    GPIO_SECOND_FUNC_PULLUP, // Special for uart1
    GPIO_HIGH_IMPEDANCE,
} gpio_dir_t;

typedef enum {
    GPIO_FUNC_INVALID = 0,
    GPIO_FUNC_UART1,
    GPIO_FUNC_UART1_EXTENDED,
    GPIO_FUNC_UART2,
    GPIO_FUNC_I2C1,
    GPIO_FUNC_I2C2,
    GPIO_FUNC_JTAG,
    GPIO_FUNC_DCMI,
    GPIO_FUNC_CLK13M,
    GPIO_FUNC_CLK26M,
    GPIO_FUNC_PWM0,
    GPIO_FUNC_PWM1,
    GPIO_FUNC_PWM2,
    GPIO_FUNC_PWM3,
    GPIO_FUNC_PWM4,
    GPIO_FUNC_PWM5,
    GPIO_FUNC_ADC1,
    GPIO_FUNC_ADC2,
    GPIO_FUNC_ADC3,
    GPIO_FUNC_ADC4,
    GPIO_FUNC_ADC5,
    GPIO_FUNC_ADC6,
} gpio_func_t;

typedef unsigned char gpio_num_t;

#define GPIO_NUM_MAX 39

#ifdef __cplusplus
extern "C" {
#endif

void gpio_config(gpio_num_t gpio, gpio_dir_t dir);

void gpio_config_function(gpio_func_t func);

uint8_t gpio_get(gpio_num_t gpio);

void gpio_put(gpio_num_t gpio, bool value);

void gpio_toggle(gpio_num_t gpio);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_GPIO_H
