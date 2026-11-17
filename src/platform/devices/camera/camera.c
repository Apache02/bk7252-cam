#include "device/camera.h"
#include "device/camera_sensor.h"
#include <errno.h>
#include <string.h>
#include "board.h"
#include "soc/jpeg.h"
#include "hardware/gdma.h"
#include "hardware/gpio.h"
#include "hardware/jpeg.h"
#include "hardware/sctrl.h"
#include "platform/cpu.h"
#include "platform/sched.h"
#include "platform/timeout.h"
#include "platform/unistd.h"
#include "ring.h"

// A board without CAMERA_I2C has no camera: nothing is built, and the code that uses the camera is
// left out of the applications.
#ifdef CAMERA_I2C

#include "i2c.h"


// The preprocessor cannot compare the board's CAMERA_POWER_METHOD string, so the compiler picks the
// branch: strcmp() of two literals folds to a constant and the other branches are dropped. A call to
// one of these functions that survives (bad method, missing pin or voltage) is a compile error;
// that relies on optimization being on.
extern void camera_power_bad_method(void)
    __attribute__((error("CAMERA_POWER_METHOD must be \"NONE\", \"VDDRAM\", \"GPIO_UP\" or \"GPIO_DOWN\"")));
extern int camera_power_no_pin(void)
    __attribute__((error("this CAMERA_POWER_METHOD needs CAMERA_POWER_GPIO_PIN in board.h")));
extern sctrl_vddram_volt_t camera_power_no_volt(void)
    __attribute__((error("CAMERA_POWER_METHOD \"VDDRAM\" needs CAMERA_POWER_VDDRAM_VOLT in board.h")));

#ifndef CAMERA_POWER_METHOD
#define CAMERA_POWER_METHOD "NONE"
#endif
#ifndef CAMERA_POWER_GPIO_PIN
#define CAMERA_POWER_GPIO_PIN camera_power_no_pin()
#endif
#ifndef CAMERA_POWER_VDDRAM_VOLT
#define CAMERA_POWER_VDDRAM_VOLT camera_power_no_volt()
#endif

#define POWER_METHOD_IS(name) (__builtin_strcmp(CAMERA_POWER_METHOD, name) == 0)

#define I2C_BAUD_HZ           (100000)
#define I2C_SETTLE_US         (200000)
#define PROBE_ATTEMPTS        (10)
#define PROBE_RETRY_US        (20000)
#define RESET_PULSE_US        (100000)
#define SENSOR_SETTLE_US      (1000000)
#define FRAME_WAIT_SPINS      (30000000u)
#define DRAIN_STABLE_POLLS    (50)
#define DRAIN_MAX_POLLS       (400000)
#define PREFERRED_DMA_CHANNEL (4)

typedef enum {
    STATE_STOPPED,
    STATE_STARTING,
    STATE_RUNNING,
} camera_state_t;

static volatile camera_state_t g_state = STATE_STOPPED;
static int                     g_dma_channel = -1;
static const camera_sensor_t  *g_sensor;
static camera_config_t         g_config;
static ring_core_t             g_core;
static bool                    g_core_ready;
static volatile bool           g_capturing; // the interrupts may feed the ring core
static volatile uint32_t       g_frames;
static volatile uint32_t       g_last_frame_size;

int camera_bus_read_reg(const uint8_t i2c_addr, const uint8_t reg, uint8_t *value) {
    const int rc = i2c_write(i2c_addr, &reg, 1);
    if (rc != 0) return rc;
    return i2c_read(i2c_addr, value, 1);
}

int camera_bus_write_reg(const uint8_t i2c_addr, const uint8_t reg, const uint8_t value) {
    const uint8_t data[2] = {reg, value};
    return i2c_write(i2c_addr, data, 2);
}

// ---------------------------------------------------------------------------
// Power, sensor detection
// ---------------------------------------------------------------------------

static void power_down(void) {
    i2c_deinit();
    jpeg_disable();
    jpeg_power_down();
    jpeg_clear_end_frame_handler();
}

static void dma_release(void) {
    if (g_dma_channel < 0) return;

    gdma_stop(g_dma_channel);
    gdma_release_channel(g_dma_channel);
    g_dma_channel = -1;
}

// Channel 4 if it is free, any other otherwise.
static int dma_acquire(void) {
    dma_release();

    int ch = gdma_reserve_specific_channel(PREFERRED_DMA_CHANNEL);
    if (ch < 0) ch = gdma_reserve_channel();
    if (ch < 0) return -EBUSY;

    g_dma_channel = ch;
    return 0;
}

static void camera_sensor_power_on(void) {
    if (POWER_METHOD_IS("NONE")) {
        // supplied permanently
    } else if (POWER_METHOD_IS("VDDRAM")) {
        sctrl_vddram_enable(CAMERA_POWER_VDDRAM_VOLT);
    } else if (POWER_METHOD_IS("GPIO_UP") || POWER_METHOD_IS("GPIO_DOWN")) {
        gpio_config(CAMERA_POWER_GPIO_PIN, GPIO_OUT);
        gpio_put(CAMERA_POWER_GPIO_PIN, POWER_METHOD_IS("GPIO_UP"));
    } else {
        camera_power_bad_method();
    }
}

static void camera_sensor_power_down(void) {
    if (POWER_METHOD_IS("NONE")) {
        // supplied permanently
    } else if (POWER_METHOD_IS("VDDRAM")) {
        sctrl_vddram_disable();
    } else if (POWER_METHOD_IS("GPIO_UP") || POWER_METHOD_IS("GPIO_DOWN")) {
        gpio_put(CAMERA_POWER_GPIO_PIN, !POWER_METHOD_IS("GPIO_UP"));
    } else {
        camera_power_bad_method();
    }
}

static void pulse_reset_pin(void) {
#ifdef CAMERA_RESET_PIN
    gpio_config(CAMERA_RESET_PIN, GPIO_OUT);
    gpio_put(CAMERA_RESET_PIN, false);
    usleep(RESET_PULSE_US);
    gpio_put(CAMERA_RESET_PIN, true);
    usleep(RESET_PULSE_US);
    gpio_put(CAMERA_RESET_PIN, false);
    usleep(RESET_PULSE_US);
#endif
}

// The encoder has to run before the sensor is touched over I2C, or MCLK does not run and the bus jams.
static void encoder_start(const camera_sensor_t *sensor) {
    const jpeg_encoder_config_t config = {
        .image_width     = sensor->width,
        .image_height    = sensor->height,
        .frequency       = sensor->mclk,
        .yuv_format      = sensor->yuv_format,
        .hsync_invert    = sensor->hsync_invert,
        .vsync_invert    = sensor->vsync_invert,
        .frame_min_bytes = g_config.frame_min_bytes,
        .frame_max_bytes = g_config.frame_max_bytes,
    };
    jpeg_configure(&config);

    jpeg_power_up();
    jpeg_enable();
}

// The sensor needs a moment after power-up before it answers.
static bool wait_for_sensor(const camera_sensor_t *sensor) {
    for (unsigned attempt = 0; attempt < PROBE_ATTEMPTS; attempt++) {
        if (sensor->probe(sensor->i2c_addr)) return true;
        usleep(PROBE_RETRY_US);
    }
    return false;
}

static int load_init_table(const camera_sensor_t *sensor) {
    int result = 0;
    for (size_t i = 0; i < sensor->init_count; i++) {
        if (camera_bus_write_reg(sensor->i2c_addr, sensor->init_pairs[i][0], sensor->init_pairs[i][1]) != 0) {
            result = -EIO;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Capture: DMA into the ring, chunk interrupt (IRQ), end of frame (FIQ)
// ---------------------------------------------------------------------------

// Bytes the DMA has written beyond rb->in. Call with FIQ masked.
static size_t dma_past_in(void) {
    const struct ringbuf *rb = &camera_ring;
    return (gdma_dst_write_addr(g_dma_channel) - (uint32_t)rb->data - (uint32_t)(rb->in & rb->mask)) & rb->mask;
}

// Waits until the FIFO is empty and the DMA write position stopped moving.
static void wait_dma_drained(const int ch) {
    uint32_t last   = gdma_dst_write_addr(ch);
    uint32_t stable = 0;
    for (uint32_t polls = 0; stable < DRAIN_STABLE_POLLS && polls < DRAIN_MAX_POLLS && gdma_busy(ch); polls++) {
        const uint32_t pos = gdma_dst_write_addr(ch);
        stable             = (jpeg_is_fifo_empty() && pos == last) ? stable + 1 : 0;
        last               = pos;
    }
}

// One chunk was written. The end-of-frame FIQ changes the same fields, so it is masked for the body.
static void chunk_handler(int ch) {
    (void)ch;
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    if (g_capturing) ring_core_on_chunk(&g_core, dma_past_in());
    GLOBAL_INT_RESTORE();
}

static void end_frame_handler(void) {
    const uint32_t size = (uint32_t)jpeg_get_last_frame_size();
    g_last_frame_size   = size;
    g_frames            = g_frames + 1;
    if (!g_capturing) return;

    const int ch = g_dma_channel;
    wait_dma_drained(ch);
    gdma_stop(ch);

    const bool   unsynced = g_core.cur_id == 0;
    const size_t next     = ring_core_on_frame_end(&g_core, dma_past_in(), size);

    // The first re-arm happens mid-stream: drop what is left in the FIFO so the next frame starts clean.
    if (unsynced) {
        while (!jpeg_is_fifo_empty()) (void)hw_jpeg->rx_fifo_data;
    }
    gdma_restart(ch, (uint32_t)camera_ring.data + (uint32_t)(next & camera_ring.mask), CAMERA_CHUNK_BYTES);
}

static int capture_start(void) {
    if (!g_core_ready) {
        if (ring_core_init(&g_core, &camera_ring, CAMERA_CHUNK_BYTES) != 0) return -EINVAL;
        g_core_ready = true;
    }
    ring_core_start(&g_core, 0);

    const gdma_config_t dma_config = {
        .src =
            {
                .mode = GDMA_MODE_JPEG,
                .addr = (uint32_t)&hw_jpeg->rx_fifo_data,
                .incr = false,
                .dw   = GDMA_DATA_WIDTH_32,
            },
        .dst =
            {
                .mode          = GDMA_MODE_DTCM,
                .addr          = (uint32_t)camera_ring.data,
                .loop_addr     = (uint32_t)camera_ring.data,
                .loop_end_addr = (uint32_t)camera_ring.data + (uint32_t)(camera_ring.mask + 1),
                .incr          = true,
                .dw            = GDMA_DATA_WIDTH_32,
            },
        .repeat = true,
        .finish = chunk_handler,
    };
    int rc = gdma_configure(g_dma_channel, &dma_config);
    if (rc != 0) return rc < 0 ? rc : -EIO;

    rc = gdma_start(g_dma_channel, CAMERA_CHUNK_BYTES);
    if (rc != 0) return rc < 0 ? rc : -EIO;

    g_capturing = true;
    return 0;
}

// Spins until the first end of frame has aligned the DMA to the ring. No sleeping: it must also work in an
// image without a scheduler.
static bool wait_synced(void) {
    for (uint32_t spins = 0; spins < FRAME_WAIT_SPINS; spins++) {
        if (*(volatile uint32_t *)&g_core.cur_id != 0) return true;
    }
    return false;
}

static void shutdown(void) {
    g_capturing = false;
    dma_release();
    power_down();
    camera_sensor_power_down();
    g_sensor = NULL;
}

int camera_start(const camera_config_t *config) {
    static const camera_config_t defaults = {0};
    if (config == NULL) config = &defaults;

    bool name_known = config->sensor == NULL;
    for (size_t i = 0; i < camera_sensor_count && !name_known; i++) {
        name_known = strcmp(config->sensor, camera_sensors[i]->name) == 0;
    }
    if (!name_known) return -EINVAL;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    const camera_state_t seen = g_state;
    if (seen == STATE_STOPPED) g_state = STATE_STARTING;
    GLOBAL_INT_RESTORE();

    if (seen == STATE_RUNNING) return 0;
    if (seen == STATE_STARTING) return -EBUSY;

    g_sensor            = NULL;
    g_config            = *config;
    g_frames            = 0;
    g_last_frame_size   = 0;

    int rc = dma_acquire();
    if (rc != 0) {
        g_state = STATE_STOPPED;
        return rc;
    }

    power_down();
    pulse_reset_pin();
    camera_sensor_power_on();

    jpeg_init();
    jpeg_register_end_frame_handler(end_frame_handler);

    const camera_sensor_t *found     = NULL;
    bool                   i2c_ready = false;
    for (size_t i = 0; i < camera_sensor_count && found == NULL; i++) {
        const camera_sensor_t *candidate = camera_sensors[i];
        if (config->sensor != NULL && strcmp(config->sensor, candidate->name) != 0) continue;

        encoder_start(candidate);
        if (!i2c_ready) {
            i2c_init(I2C_BAUD_HZ);
            usleep(I2C_SETTLE_US);
            i2c_ready = true;
        }

        if (wait_for_sensor(candidate)) {
            found = candidate;
        } else {
            jpeg_disable();
            jpeg_power_down();
        }
    }

    rc = 0;
    if (found == NULL) {
        rc = -ENODEV;
    } else if (load_init_table(found) != 0) {
        rc = -EIO;
    }
    if (rc == 0) {
        usleep(SENSOR_SETTLE_US);
        g_sensor = found;
        rc       = capture_start();
        if (rc == 0 && !wait_synced()) rc = -ETIMEDOUT;
    }

    if (rc != 0) {
        shutdown();
        g_state = STATE_STOPPED;
        return rc;
    }

    g_state = STATE_RUNNING;
    return 0;
}

void camera_stop(void) {
    g_state = STATE_STOPPED;
    shutdown();
}

int camera_get_info(camera_info_t *info) {
    const camera_sensor_t *sensor = g_sensor;
    if (g_state != STATE_RUNNING || sensor == NULL) return -ENXIO;

    info->sensor = sensor->name;
    info->width  = sensor->width;
    info->height = sensor->height;
    return 0;
}

int camera_sensor_read(const uint8_t reg, uint8_t *value) {
    const camera_sensor_t *sensor = g_sensor;
    if (g_state != STATE_RUNNING || sensor == NULL) return -ENXIO;
    return camera_bus_read_reg(sensor->i2c_addr, reg, value);
}

int camera_sensor_write(const uint8_t reg, const uint8_t value) {
    const camera_sensor_t *sensor = g_sensor;
    if (g_state != STATE_RUNNING || sensor == NULL) return -ENXIO;
    return camera_bus_write_reg(sensor->i2c_addr, reg, value);
}

void camera_get_stats(camera_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    if (g_state != STATE_RUNNING) return;

    stats->frames           = g_frames;
    stats->frames_lost      = g_core.lost;
    stats->last_frame_bytes = g_last_frame_size;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

// CAMERA_WAIT_FOREVER never expires, 0 is already expired after the first attempt.
typedef struct {
    bool             forever;
    bool             no_wait;
    bool             armed;
    struct timeout_t timer;
} deadline_t;

static int deadline_begin(deadline_t *d, const uint32_t timeout_ms) {
    d->forever = timeout_ms == CAMERA_WAIT_FOREVER;
    d->no_wait = timeout_ms == 0;
    d->armed   = false;
    if (d->forever || d->no_wait) return 0;

    if (!create_timeout(&d->timer, timeout_ms)) return -ENOMEM;
    d->armed = true;
    return 0;
}

static bool deadline_expired(const deadline_t *d) {
    if (d->forever) return false;
    if (d->no_wait) return true;
    return is_timeout_finished(&d->timer);
}

static void deadline_end(deadline_t *d) {
    if (d->armed && !is_timeout_finished(&d->timer)) free_timeout(&d->timer);
    d->armed = false;
}

static int open_until(camera_stream_t *stream, const deadline_t *d) {
    for (;;) {
        if (g_state != STATE_RUNNING) return -ENXIO;
        if (ring_core_open(&g_core, stream) == 0) return 0;
        if (deadline_expired(d)) return -ETIMEDOUT;
        sched_yield();
    }
}

static int read_until(camera_stream_t *stream, void *dst, const size_t max, const deadline_t *d) {
    for (;;) {
        if (g_state != STATE_RUNNING) return -ENXIO;
        const int rc = ring_core_read(&g_core, stream, dst, max);
        if (rc != 0) return rc;
        if (deadline_expired(d)) return -ETIMEDOUT;
        sched_yield();
    }
}

int camera_stream_open(camera_stream_t *stream, const uint32_t timeout_ms) {
    if (stream == NULL) return -EINVAL;

    deadline_t d;
    int        rc = deadline_begin(&d, timeout_ms);
    if (rc != 0) return rc;

    rc = open_until(stream, &d);
    deadline_end(&d);
    return rc;
}

int camera_stream_read(camera_stream_t *stream, void *dst, const size_t max, const uint32_t timeout_ms) {
    if (stream == NULL || dst == NULL || max == 0) return -EINVAL;

    deadline_t d;
    int        rc = deadline_begin(&d, timeout_ms);
    if (rc != 0) return rc;

    rc = read_until(stream, dst, max, &d);
    deadline_end(&d);
    return rc;
}

bool camera_stream_at_end(const camera_stream_t *stream) { return stream->at_end; }

int camera_read_frame(void *dst, const size_t dst_size, const uint32_t timeout_ms) {
    if (dst == NULL || dst_size == 0) return -EINVAL;

    deadline_t d;
    int        rc = deadline_begin(&d, timeout_ms);
    if (rc != 0) return rc;

    for (;;) {
        camera_stream_t stream;
        rc = open_until(&stream, &d);
        if (rc != 0) break;

        size_t total = 0;
        for (;;) {
            if (total == dst_size) {
                rc = -ENOSPC; // not at the end yet: read() would have said so
                break;
            }
            rc = read_until(&stream, (uint8_t *)dst + total, dst_size - total, &d);
            if (rc > 0) {
                total += (size_t)rc;
                if (stream.at_end) {
                    rc = (int)total;
                    break;
                }
                continue;
            }
            break;
        }

        if (rc == -ESTALE || rc == -EBADMSG) {
            if (deadline_expired(&d)) rc = -ETIMEDOUT;
            if (rc != -ETIMEDOUT) continue; // the frame is lost: try the next one
        }
        break;
    }

    deadline_end(&d);
    return rc;
}

// clang-format off
const char *camera_strerror(const int rc) {
    switch (rc) {
        case 0:             return "ok";
        case -EINVAL:       return "bad argument or unknown sensor";
        case -ENXIO:        return "camera is not started";
        case -ENODEV:       return "no sensor answered";
        case -EIO:          return "sensor register write failed";
        case -EBUSY:        return "DMA channel is not available, or the camera is being started";
        case -ETIMEDOUT:    return "timed out";
        case -ESTALE:       return "data was overwritten before it was read";
        case -EBADMSG:      return "the frame is damaged";
        case -ENOSPC:       return "buffer is too small for the frame";
        case -ENOMEM:       return "no timer for the timeout";
        default:            return "unknown error";
    }
}
// clang-format on

#endif // CAMERA_I2C
