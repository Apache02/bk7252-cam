#pragma once

#include "platform/soc.h"


#define GPIO_BASE_ADDR (0x00802800)

#define GPIO_BANK0_COUNT (32)
#define GPIO_BANK0_START (0)
#define GPIO_BANK0_END   (GPIO_BANK0_START + GPIO_BANK0_COUNT - 1)
#define GPIO_BANK1_COUNT (GPIO_NUM_MAX + 1 - GPIO_BANK0_COUNT)
#define GPIO_BANK1_START (GPIO_BANK0_COUNT)
#define GPIO_BANK1_END   (GPIO_BANK1_START + GPIO_BANK1_COUNT - 1)

#define GPIO_MODULE_NONE (0xff) // SDK name: GPIO_MODUL_NONE


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t input: 1;           // [0]
            uint32_t output: 1;          // [1]
            uint32_t input_enable: 1;    // [2]
            uint32_t output_enable: 1;   // [3]
            uint32_t pull_mode: 1;       // [4]
            uint32_t pull_enable: 1;     // [5]
            uint32_t function_enable: 1; // [6]
            uint32_t input_monitor: 1;   // [7]
            uint32_t reserved_8_31: 24;  // [31:8]
        };
    };
} hw_gpio_pin_t;

typedef volatile struct {
    hw_gpio_pin_t pin_cfg[32]; // word 0x00..0x1F: GPIO 0-31 pin config

    uint32_t func_mux_1; // word 0x20: GPIO 0-15 function select, 2 bits/pin
    uint32_t inten;      // word 0x21: GPIO 0-31 interrupt enable, 1 bit/pin
    uint32_t intlv0;     // word 0x22: GPIO 0-15 interrupt edge/level select, 2 bits/pin
    uint32_t intlv1;     // word 0x23: GPIO 16-31 interrupt edge/level select, 2 bits/pin
    uint32_t intsta;     // word 0x24: GPIO 0-31 interrupt status, 1 bit/pin, W1C

    uint32_t reserved_0x25; // word 0x25

    union {
        uint32_t v;
        struct {
            uint32_t dpll_unlock_int_en: 1;       // [0]
            uint32_t audio_dpll_unlock_int_en: 1; // [1]
            uint32_t dpll_unlock_int: 1;          // [2] W1C
            uint32_t audio_dpll_unlock_int: 1;    // [3] W1C
            uint32_t usb_plug_in_int_en: 1;       // [4]
            uint32_t usb_plug_out_int_en: 1;      // [5]
            uint32_t usb_plug_in_int: 1;          // [6] W1C
            uint32_t usb_plug_out_int: 1;         // [7] W1C
            uint32_t reserved_8_31: 24;           // [31:8]
        };
    } extra_int_cfg; // word 0x26. SDK name: REG_GPIO_EXTRAL_INT_CFG

    union {
        uint32_t v;
        struct {
            uint32_t is_over_temp_detect: 1; // [0]
            uint32_t is_usb_plug_in: 1;      // [1]
            uint32_t reserved_2_31: 30;      // [31:2]
        };
    } detect; // word 0x27

    uint32_t enc_word;   // word 0x28 (NOT INVESTIGATED — no usage in SDK gpio.c)
    uint32_t dbg_msg;    // word 0x29 (NOT INVESTIGATED)
    uint32_t dbg_mux;    // word 0x2A (NOT INVESTIGATED)
    uint32_t dbg_cfg;    // word 0x2B (NOT INVESTIGATED)
    uint32_t dbg_report; // word 0x2C (NOT INVESTIGATED)

    union {
        uint32_t v;
        struct {
            uint32_t spi_module: 2;     // [1:0] 0=SPI_DMA 1=SPI 2=SPI1_DMA 3=SPI1
            uint32_t sd_module: 2;      // [3:2] 0=SD_DMA 1=SD_HOST 2=SD1_DMA 3=SD1_HOST
            uint32_t reserved_4_31: 28; // [31:4]
        };
    } module_select; // word 0x2D

    uint32_t func_mux_2; // word 0x2E: GPIO 16-31 function select, 2 bits/pin
    uint32_t func_mux_3; // word 0x2F: GPIO 32-39 function select, 2 bits/pin (upper 16 bits unused)

    hw_gpio_pin_t pin_cfg2[8]; // word 0x30..0x37: GPIO 32-39 pin config

    uint32_t inten2;  // word 0x38: GPIO 32-39 interrupt enable, 1 bit/pin (upper 24 bits unused)
    uint32_t intlv3;  // word 0x39: GPIO 32-39 interrupt edge/level select, 2 bits/pin (upper 16 bits unused)
    uint32_t intsta2; // word 0x3A: GPIO 32-39 interrupt status, 1 bit/pin, W1C (upper 24 bits unused)
} hw_gpio_t;

#define hw_gpio ((volatile hw_gpio_t *)GPIO_BASE_ADDR)

#define hw_gpio_bank0 (hw_gpio->pin_cfg)
#define hw_gpio_bank1 (hw_gpio->pin_cfg2)

__unused static inline volatile hw_gpio_pin_t *get_gpio_reg(unsigned gpio) {
    // clang-format off
    return (gpio > GPIO_BANK0_END)
        ? (hw_gpio_bank1 + (gpio - GPIO_BANK0_COUNT))
        : (hw_gpio_bank0 + gpio);
    // clang-format on
}

// Sets one enable bit in hw_gpio->extra_int_cfg. Zeros the register's W1C
// status bits before writing back so this can't accidentally acknowledge an
// unrelated pending interrupt in the same register.
// Usage: gpio_extra_int_set(dpll_unlock_int_en = 1)
#define gpio_extra_int_set(int_en)                                            \
    do {                                                                      \
        typeof(hw_gpio->extra_int_cfg) tmp = {.v = hw_gpio->extra_int_cfg.v}; \
        tmp.dpll_unlock_int                = 0;                               \
        tmp.audio_dpll_unlock_int          = 0;                               \
        tmp.usb_plug_in_int                = 0;                               \
        tmp.usb_plug_out_int               = 0;                               \
        tmp.int_en;                                                           \
        hw_gpio->extra_int_cfg.v = tmp.v;                                     \
    } while (0)

#define gpio_extra_int_clear(int_bit)                                         \
    do {                                                                      \
        typeof(hw_gpio->extra_int_cfg) tmp = {.v = hw_gpio->extra_int_cfg.v}; \
        tmp.dpll_unlock_int                = 0;                               \
        tmp.audio_dpll_unlock_int          = 0;                               \
        tmp.usb_plug_in_int                = 0;                               \
        tmp.usb_plug_out_int               = 0;                               \
        tmp.int_bit                        = 1;                               \
        hw_gpio->extra_int_cfg.v           = tmp.v;                           \
    } while (0)
