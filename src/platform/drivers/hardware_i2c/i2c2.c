#include "hardware/i2c.h"

#include "soc/clock.h"
#include "soc/i2c.h"
#include "soc/icu.h"

#include "hardware/icu.h"
#include "hardware/gpio.h"
#include "hardware/intc.h"
#include "platform/cpu.h"
#include "platform/sched.h"
#include "platform/timeout.h"


#define I2C2_REF_CLK_HZ (XTAL_CLOCK_HZ) // selected below via icu_i2c2_clk()

// FIFO thresholds for the interrupt: transmit FIFO empty, receive FIFO holding one byte.
#define I2C2_INT_MODE_TX 0
#define I2C2_INT_MODE_RX 3


static uint32_t i2c2_clk_div(uint32_t baud_hz) {
    uint32_t a = (I2C2_REF_CLK_HZ + baud_hz - 1) / baud_hz; // ceil(ref / baud)
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
    hw_i2c2->data.data = *(g_state.data++); \
    g_state.remain     = g_state.remain - 1;

#define issue_stop(status)        \
    status.stop       = 1;        \
    hw_i2c2->status.v = status.v; \
    g_state.done      = 1;


// Ported from the reference build's i2c2_isr(), reduced to the master role. `start`
// and `stop` are one-shot commands that hardware does not clear; every start
// rewrites the whole status word, which also drops a `stop` left over from the
// previous transfer.
static void i2c2_isr() {
    typeof(hw_i2c2->status) status = {.v = hw_i2c2->status.v};

    if (!status.int_flag) {
        if (status.scl_timeout) {
            // the engine has to be switched off and on to recover from a stuck SCL
            hw_i2c2->config.enable = 0;
            hw_i2c2->config.enable = 1;
        }
        if (status.arb_lost) hw_i2c2->status.v = status.v;
        return;
    }

    if (status.start) {
        // address phase
        status.start = 0;

        if (!status.ack) {
            g_state.nack = 1;
            issue_stop(status);
        } else {
            g_state.ack = 1;
            if (!g_state.tx_mode) {
                status.ack = 1;
            } else if (g_state.remain > 0) {
                send_next_byte();
            } else {
                issue_stop(status);
            }
        }
    } else if (g_state.tx_mode) {
        // data write phase
        if (!status.ack) {
            g_state.nack = 1;
            issue_stop(status);
        } else if (g_state.remain == 0) {
            issue_stop(status);
        } else {
            send_next_byte();
        }
    } else {
        // data read phase
        *(g_state.data++) = (uint8_t)hw_i2c2->data.data;
        g_state.remain    = g_state.remain - 1;
        if (g_state.remain) {
            status.ack = 1;
        } else {
            status.ack = 0;
            issue_stop(status);
        }
    }

    status.int_flag   = 0;
    hw_i2c2->status.v = status.v;
}

void i2c2_deinit(void) {
    hw_i2c2->config.enable = 0;
    intc_disable_irq_source(IRQ_SOURCE_I2C2);
    intc_unregister_irq_handler(IRQ_SOURCE_I2C2, i2c2_isr);
    icu_i2c2_power_down();
    hw_i2c2->config.v = 0;
    hw_i2c2->status.v = 0;
}

void i2c2_init(uint32_t baud_hz) {
    icu_i2c2_power_up();
    icu_i2c2_clk(PERI_CLK_26M_XTAL);
    gpio_config_function(GPIO_FUNC_I2C2);

    intc_register_irq_handler(IRQ_SOURCE_I2C2, i2c2_isr);
    intc_enable_irq_source(IRQ_SOURCE_I2C2);

    hw_i2c2->config.v = 0;
    hw_i2c2->status.v = 0;

    typeof(hw_i2c2->config) cfg = {.v = 0};
    cfg.idle_cr                 = 3;
    cfg.scl_cr                  = 4;
    cfg.clk_div                 = i2c2_clk_div(baud_hz);
    cfg.clk_src                 = 3;
    cfg.timeout_en              = 1;
    cfg.fte                     = 1;
    cfg.enable                  = 1;
    hw_i2c2->config.v           = cfg.v;
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
static int i2c2_transfer(uint8_t addr7, volatile uint8_t *data, uint16_t len, bool is_tx) {
    if (!take_busy()) {
        return -EBUSY;
    }

    g_state.tx_mode = is_tx;
    g_state.remain  = len;
    g_state.data    = data;
    g_state.done    = 0;
    g_state.ack     = 0;
    g_state.nack    = 0;

    hw_i2c2->data.data = (uint32_t)((addr7 << 1) | (is_tx ? 0 : 1));

    typeof(hw_i2c2->status) status = {.v = 0};
    status.int_mode                = is_tx ? I2C2_INT_MODE_TX : I2C2_INT_MODE_RX;
    status.start                   = 1;
    hw_i2c2->status.v              = status.v;

    struct timeout_t t;
    if (!create_timeout(&t, 2 * (len + 2))) {
        release_busy();
        return -ENOMEM;
    }
    while (!g_state.done) {
        if (is_timeout_finished(&t)) {
            status.v          = hw_i2c2->status.v;
            status.start      = 0;
            status.stop       = 1;
            hw_i2c2->status.v = status.v;
            release_busy();
            return -ETIMEDOUT;
        }
        sched_yield();
    }
    free_timeout(&t);

    bool ack  = g_state.ack;
    bool nack = g_state.nack;

    release_busy();

    if (nack) return -EFAULT;
    if (ack) return 0;

    return -EIO; // done without ack or nack — shouldn't happen, but not a NACK either
}

int i2c2_write(uint8_t addr7, const uint8_t *data, uint16_t len) {
    return i2c2_transfer(addr7, (volatile uint8_t *)data, len, true);
}

int i2c2_read(uint8_t addr7, uint8_t *data, uint16_t len) { return i2c2_transfer(addr7, data, len, false); }
