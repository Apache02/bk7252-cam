#pragma once

#include <stdint.h>
#include <sys/cdefs.h>


#define GPIO_BASE_ADDR                      (0x00802800)
#define REG_GPIO_0_CONFIG                   (GPIO_BASE_ADDR + 0*4)
#define REG_GPIO_32_CONFIG                  (GPIO_BASE_ADDR + 48*4)

#define REG_GPIO_BANK0                      (REG_GPIO_0_CONFIG)
#define REG_GPIO_BANK1                      (REG_GPIO_32_CONFIG)

#define GPIO_INPUT_BIT                      (1 << 0)
#define GPIO_OUTPUT_BIT                     (1 << 1)
#define GPIO_INPUT_ENABLE_BIT               (1 << 2)
#define GPIO_OUTPUT_ENABLE_BIT              (1 << 3)
#define GPIO_PULL_MODE_BIT                  (1 << 4)
#define GPIO_PULL_ENABLE_BIT                (1 << 5)
#define GPIO_FUNCTION_ENABLE_BIT            (1 << 6)
#define GPIO_INPUT_MONITOR_BIT              (1 << 7)

#define GPIO_BANK0_COUNT                    (32)
#define GPIO_BANK0_START                    (0)
#define GPIO_BANK0_END                      (GPIO_BANK0_START + GPIO_BANK0_COUNT - 1)
#define GPIO_BANK1_COUNT                    (GPIO_NUM_MAX + 1 - GPIO_BANK0_COUNT)
#define GPIO_BANK1_START                    (GPIO_BANK0_COUNT)
#define GPIO_BANK1_END                      (GPIO_BANK1_START + GPIO_BANK1_COUNT - 1)

// for GPIO 0-15
#define REG_GPIO_FUNC_CFG_1                 (GPIO_BASE_ADDR + 32*4)
// for GPIO 16-31
#define REG_GPIO_FUNC_CFG_2                 (GPIO_BASE_ADDR + 46*4)
// for GPIO 32-39
#define REG_GPIO_FUNC_CFG_3                 (GPIO_BASE_ADDR + 47*4)

#define PERIAL_MODE_1                       (0)
#define PERIAL_MODE_2                       (1)
#define PERIAL_MODE_3                       (2)
#define PERIAL_MODE_4                       (3)


volatile typedef struct {
    union {
        struct __attribute__((aligned(4))) __packed {
            uint32_t input: 1;              // [0]
            uint32_t output: 1;             // [1]
            uint32_t input_enable: 1;       // [2]
            uint32_t output_enable: 1;      // [3]
            uint32_t pull_mode: 1;          // [4]
            uint32_t pull_enable: 1;        // [5]
            uint32_t function_enable: 1;    // [6]
            uint32_t input_monitor: 1;      // [7]
            uint32_t reserved_8_31: 24;     // [8:31]
        };

        uint32_t v;
    };
} hw_gpio_reg_t;

#define hw_gpio_bank0           ((volatile hw_gpio_reg_t *) REG_GPIO_BANK0)
#define hw_gpio_bank1           ((volatile hw_gpio_reg_t *) REG_GPIO_BANK1)

#define get_gpio_reg(gpio)      (gpio > GPIO_BANK0_END ? (hw_gpio_bank1 + (gpio - GPIO_BANK0_COUNT)) : (hw_gpio_bank0 + gpio))
