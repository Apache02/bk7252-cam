#include "hardware/i2c.h"
#include "hardware/i2c_regs.h"
#include "hardware/gpio.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include <assert.h>

#include "FreeRTOS.h"
#include "queue.h"

#define OP_WRITE_BIT        0
#define OP_READ_BIT         1

#define AWAIT_NONE          0
#define AWAIT_ACK           (1 << 0)
#define AWAIT_FINISH        (1 << 2)
#define AWAIT_ABORT         (1 << 3)

static struct {
    TaskHandle_t task;
    uint32_t await;
    bool ack;
    bool nack;
} i2c1_state = {NULL, 0, false, false};


#include <stdio.h>

static void i2c1_isr(void) {
    typeof(hw_i2c1->config) config = hw_i2c1->config;

    if (!config.si) return;
    hw_i2c1->config.si = 0;
    putchar('i');

    if (!i2c1_state.await) {
        return;
    }

    if (i2c1_state.await & AWAIT_ACK) {
        i2c1_state.await = AWAIT_NONE;
        if (config.ack_rx) {
            i2c1_state.ack = true;
            putchar('+');
        } else {
            i2c1_state.nack = true;
            i2c1_state.await = AWAIT_FINISH;
            putchar('-');
        }
        if (i2c1_state.task) vTaskNotifyGiveFromISR(i2c1_state.task, NULL);
        return;
    }

    if (i2c1_state.await & AWAIT_FINISH) {
        putchar('F');
        config.tx_mode = 0;
        config.ensmb = 0;
        config.sta = 0;
        config.sto = 0;
        hw_i2c1->config = config;
        i2c1_state.await = AWAIT_NONE;

        if (i2c1_state.task) vTaskNotifyGiveFromISR(i2c1_state.task, NULL);
        return;
    }

    if (i2c1_state.await & AWAIT_ABORT) {
//        putchar('A');
        hw_i2c1->config.sto = 1;
        i2c1_state.await = AWAIT_FINISH;
        return;
    }
}

void i2c1_init() {
    intc_register_irq_handler(IRQ_SOURCE_I2C1, i2c1_isr);

    icu_i2c1_power_down();
    i2c1_reset();
    i2c1_state.await = AWAIT_NONE;

    i2c1_set_divider(I2C1_DEFAULT_DIVIDER);
    icu_peri_clk_i2c1(PERI_CLK_26M_XTAL);
//    hw_icu->peri_clk_mux.v = 0xffffffff;

    intc_enable_irq_source(IRQ_SOURCE_I2C1);
    icu_i2c1_power_up();
    gpio_config_function(GPIO_FUNC_I2C1);
//    hw_icu->peri_clk_pwd.v = 0;
}

void i2c1_set_baudrate(uint32_t baudrate) {
    i2c1_set_divider(I2C1_DEFAULT_CLK / baudrate);
}

static void i2c1_send_addr_byte_internal(uint8_t addr, int op) {
    portENTER_CRITICAL();

    // send addr
    hw_i2c1->data.byte = ((addr & 0x7F) << 1) | op;

    typeof(hw_i2c1->config) config = hw_i2c1->config;
    config.sto = 0;             // reset stop bit
    config.sta = 1;             // start condition bit
    config.tx_mode = 1;         // ?
    config.ensmb = 1;           // ?
    hw_i2c1->config = config;

    i2c1_state.await = AWAIT_ACK;
    i2c1_state.ack = false;
    i2c1_state.nack = false;

    portEXIT_CRITICAL();
}

static void i2c1_send_byte_internal(uint8_t byte) {
    portENTER_CRITICAL();

    hw_i2c1->data.byte = byte;

    typeof(hw_i2c1->config) config = hw_i2c1->config;
    config.sto = 0;             // reset stop bit
    config.sta = 0;             // clear start condition bit
    config.tx_mode = 1;         // ?
    config.ensmb = 1;           // ?
    hw_i2c1->config = config;

    i2c1_state.await = AWAIT_ACK;
    i2c1_state.ack = false;
    i2c1_state.nack = false;

    portEXIT_CRITICAL();
}

static void i2c1_abort() {
    portENTER_CRITICAL();

    typeof(hw_i2c1->config) config = hw_i2c1->config;

    if (config.tx_mode) {
        config.sto = 1;
        hw_i2c1->config = config;
        i2c1_state.await = AWAIT_FINISH;
    } else {
        config.ensmb = 0;
        config.sta = 0;
        config.sto = 0;
        hw_i2c1->config = config;
        i2c1_state.await = AWAIT_NONE;
    }

    i2c1_state.task = NULL;

    portEXIT_CRITICAL();
}

int i2c1_write(uint8_t addr, const uint8_t *data, size_t length, int timeout_ms) {
    assert(length < 1024 * 256);

    i2c1_state.task = xTaskGetCurrentTaskHandle();

    if (i2c1_state.await != AWAIT_NONE) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0) {
            i2c1_abort();
            return -1;
        }
    }

    i2c1_state.ack = false;
    i2c1_state.nack = false;

    i2c1_send_addr_byte_internal(addr, OP_WRITE_BIT);
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0) {
        i2c1_abort();
        return -2;
    }

    if (!i2c1_state.ack) {
        i2c1_abort();
        return -3;
    }

    int count = 0;
    for (size_t i = 0; i < length; i++) {
        i2c1_send_byte_internal(data[i]);
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0) {
            i2c1_abort();
            return -4;
        }

        // stop transfer if ack was missing
        if (!i2c1_state.ack) break;

        count++;
    }

    i2c1_abort();

    return count;
}

int i2c1_read(uint8_t addr, uint8_t *buf, size_t length, int timeout) {
    return 0;
}
