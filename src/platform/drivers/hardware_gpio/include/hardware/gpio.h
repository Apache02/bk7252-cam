#ifndef _HARDWARE_GPIO_H
#define _HARDWARE_GPIO_H

#include <stdint.h>
#include <stdbool.h>


typedef enum {
    GPIO_IN = 0,
    GPIO_OUT = 1,
    GPIO_IN_PULLUP,
    GPIO_IN_PULLDOWN,
    GPIO_SECOND_FUNC,
    GPIO_SECOND_FUNC_PULLUP, // Special for uart1
    GPIO_HIGH_IMPENDANCE,
} gpio_dir_t;

typedef enum {
    GPIO_FUNC_INVALID = 0,
    GPIO_FUNC_UART1,
    GPIO_FUNC_UART2,
    GPIO_FUNC_I2C1,
    GPIO_FUNC_I2C2,
    GPIO_FUNC_JTAG,
    GPIO_FUNC_DCMI,
} gpio_func_t;

typedef unsigned char gpio_num_t;


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
