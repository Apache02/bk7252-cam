#include "hardware/gpio.h"
#include "soc/gpio.h"


#define PERIAL_MODE_1 (0)
#define PERIAL_MODE_2 (1)
#define PERIAL_MODE_3 (2)
#define PERIAL_MODE_4 (3)


void gpio_config(gpio_num_t gpio, gpio_dir_t dir) {
    if (gpio > GPIO_NUM_MAX) return;

    volatile hw_gpio_pin_t *reg = get_gpio_reg(gpio);

    switch (dir) {
        case GPIO_OUT:
            hw_write_fields(*reg);
            break;
        case GPIO_IN:
            hw_write_fields(*reg,
                .input_enable = 1,
                .output_enable = 1,
            );
            break;
        case GPIO_IN_PULLUP:
            // 0x3C — SDK GMODE_INPUT_PULLUP: pull_mode=1
            hw_write_fields(*reg,
                .input_enable = 1,
                .output_enable = 1,
                .pull_enable = 1,
                .pull_mode = 1,
            );
            break;
        case GPIO_IN_PULLDOWN:
            // 0x2C — SDK GMODE_INPUT_PULLDOWN: pull_mode=0
            hw_write_fields(*reg,
                .input_enable = 1,
                .output_enable = 1,
                .pull_enable = 1,
            );
            break;
        case GPIO_SECOND_FUNC:
            // 0x48
            hw_write_fields(*reg,
                .output_enable = 1,
                .function_enable = 1,
            );
            break;
        case GPIO_SECOND_FUNC_PULLUP:
            // 0x78
            hw_write_fields(*reg,
                .output_enable = 1,
                .function_enable = 1,
                .pull_enable = 1,
                .pull_mode = 1,
            );
            break;
        case GPIO_HIGH_IMPEDANCE:
            // 0x08
            hw_write_fields(*reg,
                .output_enable = 1,
            );
            break;
    }
}

uint8_t gpio_get(gpio_num_t gpio) {
    if (gpio > GPIO_NUM_MAX) return 0;
    volatile hw_gpio_pin_t *reg = get_gpio_reg(gpio);
    return reg->input != 0;
}

void gpio_put(gpio_num_t gpio, bool value) {
    if (gpio > GPIO_NUM_MAX) return;
    volatile hw_gpio_pin_t *reg = get_gpio_reg(gpio);
    reg->output                 = value;
}

void gpio_toggle(gpio_num_t gpio) {
    if (gpio > GPIO_NUM_MAX) return;
    volatile hw_gpio_pin_t *reg = get_gpio_reg(gpio);
    reg->output                 = !reg->output;
}

typedef struct {
    gpio_num_t gpio_from;
    gpio_num_t gpio_to;
    uint32_t   mode;
    gpio_dir_t gpio_dir;
} gpio_func_map_entry_t;

static const gpio_func_map_entry_t gpio_func_map[] = {
    [GPIO_FUNC_UART1]          = {10, 11, PERIAL_MODE_1, GPIO_SECOND_FUNC_PULLUP},
    [GPIO_FUNC_UART1_EXTENDED] = {10, 13, PERIAL_MODE_1, GPIO_SECOND_FUNC_PULLUP},
    [GPIO_FUNC_UART2]          = {0, 1, PERIAL_MODE_1, GPIO_SECOND_FUNC_PULLUP},
    [GPIO_FUNC_I2C1]           = {20, 21, PERIAL_MODE_1, GPIO_SECOND_FUNC_PULLUP},
    [GPIO_FUNC_I2C2]           = {0, 1, PERIAL_MODE_2, GPIO_SECOND_FUNC_PULLUP},
    [GPIO_FUNC_JTAG]           = {20, 23, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    [GPIO_FUNC_DCMI]           = {27, 39, PERIAL_MODE_1, GPIO_SECOND_FUNC},
    [GPIO_FUNC_CLK13M]         = {6, 6, PERIAL_MODE_1, GPIO_SECOND_FUNC},
    [GPIO_FUNC_CLK26M]         = {22, 22, PERIAL_MODE_1, GPIO_SECOND_FUNC},
    [GPIO_FUNC_PWM0]           = {6, 6, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    [GPIO_FUNC_PWM1]           = {7, 7, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    [GPIO_FUNC_PWM2]           = {8, 8, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    [GPIO_FUNC_PWM3]           = {9, 9, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    [GPIO_FUNC_PWM4]           = {24, 24, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    [GPIO_FUNC_PWM5]           = {26, 26, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    // [GPIO_FUNC_ADC1]           = {4, 4, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    // [GPIO_FUNC_ADC2]           = {5, 5, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    // [GPIO_FUNC_ADC3]           = {23, 23, PERIAL_MODE_1, GPIO_SECOND_FUNC},
    // [GPIO_FUNC_ADC4]           = {2, 2, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    // [GPIO_FUNC_ADC5]           = {3, 3, PERIAL_MODE_2, GPIO_SECOND_FUNC},
    // [GPIO_FUNC_ADC6]           = {12, 12, PERIAL_MODE_2, GPIO_SECOND_FUNC},
};

// Sets the 2-bit peripheral-mode field for one pin in its func_cfg_1/2/3 register.
static inline void gpio_set_func_mode(gpio_num_t gpio, uint32_t mode) {
    static volatile uint32_t *const func_cfg_regs[] = {
        &hw_gpio->func_mux_1,
        &hw_gpio->func_mux_2,
        &hw_gpio->func_mux_3,
    };

    static const unsigned BITS_PER_GPIO     = 2;
    static const unsigned GPIO_PER_REGISTER = 32 / BITS_PER_GPIO;

    volatile uint32_t *reg   = func_cfg_regs[gpio / GPIO_PER_REGISTER];
    uint32_t           shift = (gpio % GPIO_PER_REGISTER) * BITS_PER_GPIO;

    *reg = (*reg & ~(0x03u << shift)) | ((mode & 0x03u) << shift);
}

void gpio_config_function(gpio_func_t func) {
    if (func == GPIO_FUNC_INVALID || (unsigned)func >= sizeof(gpio_func_map) / sizeof(gpio_func_map[0])) return;

    const gpio_func_map_entry_t *map = &gpio_func_map[func];

    for (gpio_num_t gpio = map->gpio_from; gpio <= map->gpio_to; gpio++) {
        if (func == GPIO_FUNC_DCMI && gpio == 28) continue;

        gpio_config(gpio, map->gpio_dir);
        gpio_set_func_mode(gpio, map->mode);
    }
}
