#include <stdio.h>
#include <string.h>
#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include "hardware/gpio.h"
#include "utils/busy_wait.h"

static constexpr uint8_t busy_pins[] = {
// Skip stdio UART pins during scan — reconfiguring them kills the console.
#if defined(PLATFORM_STDIO_UART1_ENABLED)
    10, 11, 12, 13,
#endif
#if defined(PLATFORM_STDIO_UART2_ENABLED)
    0,  1,
#endif
};

static bool is_pin_busy(uint8_t pin) {
    for (auto i : busy_pins) {
        if (i == pin)
            return true;
    }
    return false;
}

static uint8_t resolve_pins(const uint8_t gpio, uint8_t *pins) {
    uint8_t count = 0;
    for (uint8_t pin = 0; pin <= GPIO_NUM_MAX; pin++) {
        if ((gpio == static_cast<uint8_t>(-1) || pin == gpio) && !is_pin_busy(pin)) {
            pins[count++] = pin;
        }
    }
    return count;
}

static void usage(const char *command) {
    printf("Usage: %s <pin number|all> out|in|in_pu|in_pd\r\n", command);
    printf("       %s <pin number|all> get\r\n", command);
    printf("       %s <pin number|all> set <value:0|1>\r\n", command);
    printf("       %s scan|dump\r\n", command);
}

static int dump(const uint8_t count, const uint8_t *pins) {
    for (int i = 0; i < count; i++) {
        printf("%2d:%d", pins[i], gpio_get(pins[i]));
        if ((i & 7) == 7 || i == count - 1) {
            printf("\r\n");
        } else {
            printf("  ");
        }
    }
    return 0;
}

static int scan(const uint8_t count, const uint8_t *pins) {
    uint8_t last[GPIO_NUM_MAX + 1];

    for (int i = 0; i < count; i++) {
        last[i] = gpio_get(pins[i]);
    }

    printf("Scanning GPIO 0..%d for edges. Press any key to stop.\r\n", GPIO_NUM_MAX);

    while (getchar() < 0) {
        for (int i = 0; i < count; i++) {
            uint8_t cur = gpio_get(pins[i]);
            if (cur != last[i]) {
                printf("gpio %d: %s\r\n", pins[i], cur ? "RISE" : "FALL");
                last[i] = cur;
            }
        }

        busy_wait_us(1'000);
    }

    printf("Done.\r\n");
    return 0;
}

// gpio scan|dump
// gpio <pin> out|in|in_pu|in_pd|get|set <0|1>
int command_gpio(int argc, const char *argv[]) {
    uint8_t pins[GPIO_NUM_MAX + 1] = {};
    uint8_t pins_count = 0;

    if (argc >= 2) {
        if (strcmp(argv[1], "scan") == 0) {
            pins_count = resolve_pins(-1, pins);
            return scan(pins_count, pins);
        }
        if (strcmp(argv[1], "dump") == 0) {
            pins_count = resolve_pins(-1, pins);
            return dump(pins_count, pins);
        }
    }

    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    gpio_num_t gpio = -1;
    if (strcmp(argv[1], "all") != 0) {
        int pin_num = take_int(argv[1]).ok_or(-1);
        if (!(pin_num >= 0 && pin_num <= GPIO_NUM_MAX)) {
            printf("Invalid pin number\r\n");
            return 1;
        }

        gpio = static_cast<gpio_num_t>(pin_num);

        if (is_pin_busy(gpio)) {
            printf("Pin #%d is busy\r\n", gpio);
            return 1;
        }
    }

    pins_count = resolve_pins(gpio, pins);

    if (strcmp(argv[2], "set") == 0) {
        if (argc < 4) {
            printf("Missing value argument\r\n");
            return 1;
        }

        const int value = take_int(argv[3]).ok_or(-1);
        if (value != 0 && value != 1) {
            printf("Value must be 0 or 1\r\n");
            return 1;
        }

        for (int i = 0; i < pins_count; i++) {
            gpio_put(pins[i], value);
            printf("gpio %d: set %d\r\n", pins[i], value);
        }

        return 0;
    }

    if (strcmp(argv[2], "get") == 0) {
        for (int i = 0; i < pins_count; i++) {
            printf("gpio %d: %d\r\n", pins[i], gpio_get(pins[i]));
        }
        return 0;
    }

    if (strcmp(argv[2], "out") == 0) {
        for (int i = 0; i < pins_count; i++) {
            gpio_config(pins[i], GPIO_OUT);
            printf("gpio %d: config output\r\n", pins[i]);
        }
        return 0;
    }

    if (strcmp(argv[2], "in") == 0) {
        for (int i = 0; i < pins_count; i++) {
            gpio_config(pins[i], GPIO_IN);
            printf("gpio %d: config input\r\n", pins[i]);
        }
        return 0;
    }

    if (strcmp(argv[2], "in_pu") == 0) {
        for (int i = 0; i < pins_count; i++) {
            gpio_config(pins[i], GPIO_IN_PULLUP);
            printf("gpio %d: config input pull-up\r\n", pins[i]);
        }
        return 0;
    }

    if (strcmp(argv[2], "in_pd") == 0) {
        for (int i = 0; i < pins_count; i++) {
            gpio_config(pins[i], GPIO_IN_PULLDOWN);
            printf("gpio %d: config input pull-down\r\n", pins[i]);
        }
        return 0;
    }

    printf("Unknown operation\r\n");
    usage(argv[0]);
    return 1;
}
