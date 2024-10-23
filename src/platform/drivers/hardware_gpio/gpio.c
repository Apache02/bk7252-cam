#include "hardware/gpio.h"


#define GPIO_BASE_ADDR                      (0x0802800)
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

#define GPIO_TOTAL_COUNT                    40
#define GPIO_BANK0_COUNT                    32
#define GPIO_BANK0_START                    0
#define GPIO_BANK0_END                      (GPIO_BANK0_START + GPIO_BANK0_COUNT - 1)
#define GPIO_BANK1_COUNT                    (GPIO_TOTAL_COUNT - GPIO_BANK0_COUNT)
#define GPIO_BANK1_START                    GPIO_BANK0_COUNT
#define GPIO_BANK1_END                      (GPIO_BANK1_START + GPIO_BANK1_COUNT - 1)

// for GPIO 0-15
#define REG_GPIO_FUNC_CFG_1                   (GPIO_BASE_ADDR + 32*4)
// for GPIO 16-31
#define REG_GPIO_FUNC_CFG_2                  (GPIO_BASE_ADDR + 46*4)
// for GPIO 32-39
#define REG_GPIO_FUNC_CFG_3                  (GPIO_BASE_ADDR + 47*4)
#define PERIAL_MODE_1                       (0)
#define PERIAL_MODE_2                       (1)
#define PERIAL_MODE_3                       (2)
#define PERIAL_MODE_4                       (3)


volatile typedef struct {
    union {
        struct __attribute__((packed)) bits_t {
            uint32_t input: 1;
            uint32_t output: 1;
            uint32_t input_enable: 1;
            uint32_t output_enable: 1;
            uint32_t pull_mode: 1;
            uint32_t pull_enable: 1;
            uint32_t function_enable: 1;
            uint32_t input_monitor: 1;
        } bits;

        uint32_t reg;
    };
} gpio_register_t;

#define gpio_bank0          (volatile gpio_register_t *)REG_GPIO_BANK0
#define gpio_bank1          (volatile gpio_register_t *)REG_GPIO_BANK1

#define get_reg(gpio)       (gpio > GPIO_BANK0_END ? (gpio_bank1 + gpio - GPIO_BANK0_COUNT) : (gpio_bank0 + gpio))


void gpio_config(gpio_num_t gpio, gpio_dir_t dir) {
    if (gpio > GPIO_TOTAL_COUNT - 1) return;

    uint32_t reg_value = 0;

    if (dir == GPIO_OUT) {
        reg_value = 0;
    } else if (dir == GPIO_IN) {
        reg_value = GPIO_OUTPUT_ENABLE_BIT | GPIO_INPUT_ENABLE_BIT;
    } else if (dir == GPIO_IN_PULLUP) {
        reg_value = GPIO_OUTPUT_ENABLE_BIT | GPIO_INPUT_ENABLE_BIT | GPIO_PULL_ENABLE_BIT;
    } else if (dir == GPIO_IN_PULLDOWN) {
        reg_value = GPIO_OUTPUT_ENABLE_BIT | GPIO_INPUT_ENABLE_BIT | GPIO_PULL_ENABLE_BIT | GPIO_PULL_MODE_BIT;
    } else if (dir == GPIO_SECOND_FUNC) {
        reg_value = GPIO_FUNCTION_ENABLE_BIT | GPIO_OUTPUT_ENABLE_BIT;
    } else if (dir == GPIO_SECOND_FUNC_PULLUP) {
        reg_value = GPIO_FUNCTION_ENABLE_BIT | GPIO_OUTPUT_ENABLE_BIT | GPIO_PULL_ENABLE_BIT | GPIO_PULL_MODE_BIT;
    } else { // dir == GPIO_HIGH_IMPENDANCE
        reg_value = GPIO_OUTPUT_ENABLE_BIT;
    }

    gpio_register_t *reg = get_reg(gpio);
    reg->reg = reg_value;
}

uint8_t gpio_get(gpio_num_t gpio) {
    gpio_register_t *reg = get_reg(gpio);
    return reg->bits.input != 0;
}

void gpio_put(gpio_num_t gpio, bool value) {
    gpio_register_t *reg = get_reg(gpio);
    reg->bits.output = value;
}

void gpio_toggle(gpio_num_t gpio) {
    gpio_register_t *reg = get_reg(gpio);
    reg->bits.output = !reg->bits.output;
}

void gpio_config_function(gpio_func_t func) {
    gpio_num_t gpio_from = 0;
    gpio_num_t gpio_to = 0;
    uint8_t pull_up = 0;
    uint32_t pmode = PERIAL_MODE_1;

    switch (func) {
        case GPIO_FUNC_UART1:
            gpio_from = 10;
            gpio_to = 13;
            pmode = PERIAL_MODE_1;
            pull_up = 1;
            break;

        case GPIO_FUNC_UART2:
            gpio_from = 0;
            gpio_to = 1;
            pmode = PERIAL_MODE_1;
            pull_up = 1;
            break;

        case GPIO_FUNC_I2C1:
            gpio_from = 20;
            gpio_to = 21;
            pmode = PERIAL_MODE_1;
            break;

        case GPIO_FUNC_I2C2:
            gpio_from = 0;
            gpio_to = 1;
            pmode = PERIAL_MODE_2;
            break;
        case GPIO_FUNC_DCMI:
            gpio_from = 27;
            gpio_to = 39;
            pmode = PERIAL_MODE_1;
            break;

        case GPIO_FUNC_JTAG:
            gpio_from = 20;
            gpio_to = 23;
            pmode = PERIAL_MODE_2;
            break;

        case GPIO_FUNC_INVALID:
        default:
            return;
    }

    static uint32_t *const func_cfg_regs[] = {
            (uint32_t *const) REG_GPIO_FUNC_CFG_1,
            (uint32_t *const) REG_GPIO_FUNC_CFG_2,
            (uint32_t *const) REG_GPIO_FUNC_CFG_3,
    };

    for (int gpio = gpio_from; gpio <= gpio_to; gpio++) {
        if (pull_up == 0) gpio_config(gpio, GPIO_SECOND_FUNC);
        else gpio_config(gpio, GPIO_SECOND_FUNC_PULLUP);

        uint32_t shift = ((gpio & 0x0F) * 2);
        volatile uint32_t *reg = func_cfg_regs[gpio >> 4];
        *reg = (*reg) & (~(0x03 << shift)) | ((pmode & 0x3u) << shift);
    }
}
