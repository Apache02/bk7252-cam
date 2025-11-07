#include "hardware/i2c.h"
#include "hardware/i2c_regs.h"
#include "hardware/gpio.h"
#include "hardware/icu.h"
#include <stdio.h>
#include <assert.h>

#define OP_WRITE_BIT        0
#define OP_READ_BIT         1

#define I2C1_DEFAULT_CLK            26000000
//#define I2C1_DEFAULT_DIVIDER        0x16
#define I2C1_DEFAULT_DIVIDER        65



void i2c1_init() {
    icu_i2c1_power_down();
    i2c1_reset();

    i2c1_set_divider(I2C1_DEFAULT_DIVIDER);
    icu_peri_clk_i2c1(PERI_CLK_26M_XTAL);

    icu_i2c1_power_up();
    gpio_config_function(GPIO_FUNC_I2C1);
}

void i2c1_set_baudrate(uint32_t baudrate) {
    i2c1_set_divider(I2C1_DEFAULT_CLK / baudrate);
}

inline static bool await_busy(int timeout, uint32_t await_value) {
    for (int i = 0; i < timeout; i++) {
        for (int j=1000; j>0; j--) __asm__("nop;nop;nop;nop;");
        if (hw_i2c1->config.busy == await_value) return true;
    }
    return false;
}

inline static bool await_si(int timeout, uint32_t await_value) {
    for (int i = 0; i < timeout; i++) {
        for (int j=1000; j>0; j--) __asm__("nop;nop;nop;nop;");
        if (hw_i2c1->config.si == await_value) return true;
    }
    return false;
}

static void i2c1_send_addr_byte(uint8_t addr, int op) {
    // send addr
    hw_i2c1->data.byte = ((addr & 0x7F) << 1) + op;
    typeof(hw_i2c1->config) config = hw_i2c1->config;
    config.sto = 0;             // reset stop bit
    config.sta = 1;             // Start condition bit
    config.tx_mode = 1;         // ?
    config.ensmb = 1;           // ?
    hw_i2c1->config.v = config.v;
}

static void i2c1_abort_transfer() {
    typeof(hw_i2c1->config) config = hw_i2c1->config;

    if (config.tx_mode) {
        config.sto = 1;
        hw_i2c1->config = config;

        await_busy(10, 1);
        await_si(10, 1);
    }

    config.tx_mode = 0;
    config.ensmb = 0;
    config.sto = 0;
    config.si = 0;
    hw_i2c1->config = config;
}

int i2c1_write(uint8_t addr, const uint8_t *data, size_t length, int timeout_ms) {
    assert(length < 1024 * 256);

    i2c1_send_addr_byte(addr, OP_WRITE_BIT);

    if (!await_busy(timeout_ms, 1)) return -1;
    if (!await_si(timeout_ms, 1)) return -2;

    typeof(hw_i2c1->config) config = hw_i2c1->config;
    config.sta = 0;             // clear start condition
    config.si = 0;              // clear interrupt bit
    config.busy = 0;
    hw_i2c1->config = config;

    // Error: no ack bit after addr, means no device with this addr
    if (!config.ack_rx) {
        i2c1_abort_transfer();
        return -3;
    }

    int result = 0;
    for (size_t i = 0; i < length; i++) {
        hw_i2c1->data.byte = data[i];
        if (!await_busy(timeout_ms >> 8, 1)) {
            i2c1_abort_transfer();
            return -4;
        };
        if (!await_si(timeout_ms >> 8, 1)) {
            i2c1_abort_transfer();
            return -5;
        };

        config = hw_i2c1->config;
        config.si = 0;

        if (!config.ack_rx) break;

        result++;
    }

    i2c1_abort_transfer();

    return result;
}

int i2c1_read(uint8_t addr, uint8_t *buf, size_t length, int timeout) {
    return 0;
}

//void i2c1_xfer(uint8_t addr, uint8_t *data, size_t length, int timeout) {
//
//}

