#include "hardware/gpio.h"
#include "gpio_regs.h"

void gpio_config(gpio_num_t gpio, gpio_dir_t dir) {
    if (gpio > GPIO_NUM_MAX) return;

    uint32_t reg_value = 0;

    switch (dir) {
        case GPIO_OUT:
            reg_value = 0;
            break;
        case GPIO_IN:
            // 0x0C
            reg_value = GPIO_OUTPUT_ENABLE_BIT | GPIO_INPUT_ENABLE_BIT;
            break;
        case GPIO_IN_PULLUP:
            // 0x2C
            reg_value = GPIO_OUTPUT_ENABLE_BIT | GPIO_INPUT_ENABLE_BIT | GPIO_PULL_ENABLE_BIT;
            break;
        case GPIO_IN_PULLDOWN:
            // 0x3C
            reg_value = GPIO_OUTPUT_ENABLE_BIT | GPIO_INPUT_ENABLE_BIT | GPIO_PULL_ENABLE_BIT | GPIO_PULL_MODE_BIT;
            break;
        case GPIO_SECOND_FUNC:
            // 0x48
            reg_value = GPIO_FUNCTION_ENABLE_BIT | GPIO_OUTPUT_ENABLE_BIT;
            break;
        case GPIO_SECOND_FUNC_PULLUP:
            // 0x78
            reg_value = GPIO_FUNCTION_ENABLE_BIT | GPIO_OUTPUT_ENABLE_BIT | GPIO_PULL_ENABLE_BIT | GPIO_PULL_MODE_BIT;
            break;
        case GPIO_HIGH_IMPENDANCE:
            // 0x08
            reg_value = GPIO_OUTPUT_ENABLE_BIT;
            break;
        default:
            return;
    }

    volatile hw_gpio_reg_t *reg = get_gpio_reg(gpio);
    reg->v = reg_value;
}

uint8_t gpio_get(gpio_num_t gpio) {
    volatile hw_gpio_reg_t *reg = get_gpio_reg(gpio);
    return reg->input != 0;
}

void gpio_put(gpio_num_t gpio, bool value) {
    volatile hw_gpio_reg_t *reg = get_gpio_reg(gpio);
    reg->output = value;
}

void gpio_toggle(gpio_num_t gpio) {
    volatile hw_gpio_reg_t *reg = get_gpio_reg(gpio);
    reg->output = !reg->output;
}

void gpio_config_function(gpio_func_t func) {
    gpio_num_t gpio_from = 0;
    gpio_num_t gpio_to = 0;
    gpio_dir_t gpio_dir = GPIO_SECOND_FUNC;
    uint32_t pmode = PERIAL_MODE_1;

    switch (func) {
        case GPIO_FUNC_UART1:
            gpio_from = 10;
            gpio_to = 13;
            pmode = PERIAL_MODE_1;
            gpio_dir = GPIO_SECOND_FUNC_PULLUP;
            break;

        case GPIO_FUNC_UART2:
            gpio_from = 0;
            gpio_to = 1;
            pmode = PERIAL_MODE_1;
            gpio_dir = GPIO_SECOND_FUNC_PULLUP;
            break;

        case GPIO_FUNC_I2C1:
            gpio_from = 20;
            gpio_to = 21;
            pmode = PERIAL_MODE_1;
            gpio_dir = GPIO_SECOND_FUNC_PULLUP;
            break;

        case GPIO_FUNC_I2C2:
            gpio_from = 0;
            gpio_to = 1;
            pmode = PERIAL_MODE_2;
            gpio_dir = GPIO_SECOND_FUNC_PULLUP;
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
            (uint32_t *) REG_GPIO_FUNC_CFG_1,
            (uint32_t *) REG_GPIO_FUNC_CFG_2,
            (uint32_t *) REG_GPIO_FUNC_CFG_3,
    };

    for (int gpio = gpio_from; gpio <= gpio_to; gpio++) {
        if (func == GPIO_FUNC_DCMI && gpio == 28) continue;

        gpio_config(gpio, gpio_dir);

        uint32_t shift = ((gpio & 0x0F) * 2);
        volatile uint32_t *reg = func_cfg_regs[gpio >> 4];
        uint32_t value = *reg;
        value &= ~(0x03 << shift);
        value |= (pmode & 0x03) << shift;
        *reg = value;
    }
}
