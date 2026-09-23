#include "hardware/i2c.h"

#include "soc/clock.h"
#include "soc/i2c.h"
#include "soc/icu.h"

#include "hardware/icu.h"
#include "hardware/gpio.h"
#include "hardware/intc.h"
#include "platform/cpu.h"
#include "platform/sched.h"

#include <errno.h>


#define I2C1_REF_CLK_HZ (XTAL_CLOCK_HZ) // selected below via icu_i2c1_clk()

#define I2C1_TIMEOUT_ITERS(len) (((len) + 1) * 4)

// Ported from the reference build's I2C_CLK_DIVID(rate) macro in i2c_pub.h.
static uint32_t i2c1_freq_div(uint32_t baud_hz) {
    uint32_t a = (I2C1_REF_CLK_HZ + baud_hz - 1) / baud_hz; // ceil(ref / baud)
    a -= 6;
    return (a + 2) / 3 - 1; // ceil(a / 3) - 1
}

static struct {
    volatile uint8_t  tx_mode;
    volatile uint8_t  ack;
    volatile uint8_t  nack;
    volatile uint8_t  busy;
    volatile uint16_t remain;
    volatile uint8_t  done;
    volatile uint8_t *data;
} g_state = {0};


#define send_next_byte()                    \
    hw_i2c1->data.data = *(g_state.data++); \
    g_state.remain     = g_state.remain - 1;

#define issue_stop(status)        \
    status.sto        = 1;        \
    hw_i2c1->config.v = status.v; \
    g_state.done      = 1;


// Ported from the reference build's drv_iic1.c i2c1_isr(). `sta`/`sto` are
// one-shot commands, not self-clearing flags — leaving `sta` set when `sto`
// is issued corrupts the STOP condition.
static void i2c1_isr() {
    typeof(hw_i2c1->config) status = {.v = hw_i2c1->config.v};

    if (!status.si) return;

    if (status.sta) {
        // address phase
        status.sta        = 0;
        hw_i2c1->config.v = status.v;

        if (!status.ack_rx) {
            g_state.nack = 1;
            issue_stop(status);
        } else {
            g_state.ack = 1;
            if (!g_state.tx_mode) {
                status.tx_mode    = 0;
                hw_i2c1->config.v = status.v;
            } else if (g_state.remain > 0) {
                send_next_byte();
            } else {
                issue_stop(status);
            }
        }
    } else if (g_state.tx_mode) {
        // data write phase
        if (!status.ack_rx || g_state.remain == 0) {
            issue_stop(status);
        } else {
            send_next_byte();
        }
    } else {
        // data read phase
        *(g_state.data++) = (uint8_t)hw_i2c1->data.data;
        g_state.remain    = g_state.remain - 1;
        if (g_state.remain) {
            status.ack_tx     = 1;
            hw_i2c1->config.v = status.v;
        } else {
            status.ack_tx     = 0;
            hw_i2c1->config.v = status.v;
            issue_stop(status);
        }
    }

    status.si         = 0;
    hw_i2c1->config.v = status.v;
}

static __unused void i2c1_power_down() {
    hw_i2c1->config.ensmb = 0;
    intc_disable_irq_source(IRQ_SOURCE_I2C1);
    intc_unregister_irq_handler(IRQ_SOURCE_I2C1, i2c1_isr);
    icu_i2c1_power_down();
    hw_i2c1->config.v = 0;
}

void i2c1_init(uint32_t baud_hz) {
    icu_i2c1_power_up();
    icu_i2c1_clk(PERI_CLK_26M_XTAL);
    gpio_config_function(GPIO_FUNC_I2C1);

    intc_register_irq_handler(IRQ_SOURCE_I2C1, i2c1_isr);
    intc_enable_irq_source(IRQ_SOURCE_I2C1);

    hw_i2c1->config.v        = 0;
    hw_i2c1->config.freq_div = i2c1_freq_div(baud_hz);
}

static bool take_busy() {
    bool success = false;
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    if (g_state.busy == 0) {
        g_state.busy = 1;
        success      = true;
    }
    GLOBAL_INT_RESTORE();
    return success;
}

static void release_busy() { g_state.busy = 0; }

// One message: the address byte plus up to `len` data bytes, one direction.
static int i2c1_transfer(uint8_t addr7, volatile uint8_t *data, uint16_t len, bool is_tx) {
    if (!take_busy()) {
        return EBUSY;
    }

    g_state.tx_mode = is_tx;
    g_state.remain  = len;
    g_state.data    = data;
    g_state.done    = 0;
    g_state.ack     = 0;
    g_state.nack    = 0;

    hw_i2c1->data.data = (uint32_t)((addr7 << 1) | (is_tx ? 0 : 1));

    typeof(hw_i2c1->config) cfg;
    cfg.v             = hw_i2c1->config.v;
    cfg.ensmb         = 1;
    cfg.sta           = 1;
    cfg.tx_mode       = 1;
    hw_i2c1->config.v = cfg.v;

    uint32_t iters_left = I2C1_TIMEOUT_ITERS(len);
    while (!g_state.done) {
        if (--iters_left == 0) {
            release_busy();
            return ETIMEDOUT;
        }
        sched_yield();
    }

    bool ack  = g_state.ack;
    bool nack = g_state.nack;

    release_busy();

    if (ack) return 0;
    if (nack) return EFAULT;

    return EIO; // done without ack or nack — shouldn't happen, but not a NACK either
}

int i2c1_write(uint8_t addr7, const uint8_t *data, uint16_t len) {
    return i2c1_transfer(addr7, (volatile uint8_t *)data, len, true);
}

int i2c1_read(uint8_t addr7, uint8_t *data, uint16_t len) { return i2c1_transfer(addr7, data, len, false); }
