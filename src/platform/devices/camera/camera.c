#include "device/camera.h"
#include "device/camera_sensor.h"
#include <errno.h>
#include <string.h>
#include "board.h"
#include "soc/jpeg.h"
#include "hardware/gdma.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/jpeg.h"
#include "hardware/sctrl.h"
#include "platform/unistd.h"

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

#define I2C_BAUD_HZ         (100000)
#define I2C_SETTLE_US       (200000)
#define PROBE_ATTEMPTS      (10)
#define PROBE_RETRY_US      (20000)
#define RESET_PULSE_US      (100000)
#define SENSOR_SETTLE_US    (1000000)
#define FRAME_WAIT_SPINS    (30000000u)
#define DRAIN_STABLE_POLLS  (50)
#define DRAIN_MAX_POLLS     (400000)
#define END_MARKER_SCAN_LEN (16)
#define PREFERRED_DMA_CHANNEL (4)
#define TRAILER_BYTES       (5)   // 00 and the 32-bit frame size after FF D9
#define MAX_DMA_BYTES       (65536)

static int                      g_dma_channel = -1;
static const camera_sensor_t   *g_sensor;
static camera_config_t          g_config;
static volatile uint32_t        g_end_frame_count;
static volatile uint32_t        g_last_frame_size;

// The interrupt owns the slot states and the queue head; the caller owns the queue tail and
// clears a slot's busy flag on release.
static struct {
    camera_stream_config_t config;
    volatile bool          active;
    bool                   armed;
    unsigned               current;
    volatile uint8_t       busy[CAMERA_STREAM_MAX_SLOTS];
    volatile uint32_t      size[CAMERA_STREAM_MAX_SLOTS];
    volatile uint8_t       queue[CAMERA_STREAM_MAX_SLOTS];
    volatile unsigned      head;
    volatile unsigned      tail;
    volatile uint32_t      dropped;
} g_stream;

static void stream_on_end_frame(void);

static void end_frame_handler(void) {
    g_last_frame_size = (uint32_t)jpeg_get_last_frame_size();
    g_end_frame_count = g_end_frame_count + 1;
    if (g_stream.active) stream_on_end_frame();
}

int camera_bus_read_reg(const uint8_t i2c_addr, const uint8_t reg, uint8_t *value) {
    const int rc = i2c1_write(i2c_addr, &reg, 1);
    if (rc != 0) return rc;
    return i2c1_read(i2c_addr, value, 1);
}

int camera_bus_write_reg(const uint8_t i2c_addr, const uint8_t reg, const uint8_t value) {
    const uint8_t data[2] = {reg, value};
    return i2c1_write(i2c_addr, data, 2);
}

static void power_down(void) {
    i2c1_deinit();
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

int camera_start(const camera_config_t *config) {
    if (config == NULL || config->frame_buf == NULL || config->frame_buf_size == 0 ||
        ((uintptr_t)config->frame_buf & 3) != 0 || (config->frame_buf_size & 3) != 0) {
        return -EINVAL;
    }

    bool name_known = config->sensor == NULL;
    for (size_t i = 0; i < camera_sensor_count && !name_known; i++) {
        name_known = strcmp(config->sensor, camera_sensors[i]->name) == 0;
    }
    if (!name_known) return -EINVAL;

    camera_stream_stop();
    g_sensor = NULL;
    g_config = *config;

    const int dma_rc = dma_acquire();
    if (dma_rc != 0) return dma_rc;

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
            i2c1_init(I2C_BAUD_HZ);
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
    if (found == NULL) {
        camera_stop();
        return -ENODEV;
    }

    if (load_init_table(found) != 0) {
        camera_stop();
        return -EIO;
    }

    usleep(SENSOR_SETTLE_US);
    g_sensor = found;
    return 0;
}

void camera_stop(void) {
    camera_stream_stop();
    dma_release();
    power_down();
    camera_sensor_power_down();
    g_sensor = NULL;
}

const char *camera_sensor_name(void) { return g_sensor ? g_sensor->name : NULL; }

uint32_t camera_frames_seen(void) { return g_end_frame_count; }

uint32_t camera_last_frame_size(void) { return g_last_frame_size; }

int camera_sensor_read(const uint8_t reg, uint8_t *value) {
    if (g_sensor == NULL) return -ENXIO;
    return camera_bus_read_reg(g_sensor->i2c_addr, reg, value);
}

int camera_sensor_write(const uint8_t reg, const uint8_t value) {
    if (g_sensor == NULL) return -ENXIO;
    return camera_bus_write_reg(g_sensor->i2c_addr, reg, value);
}

// Spins until the frame counter moves; returns false on timeout. No sleeping: the DMA must be
// armed within the short gap between two frames.
static bool wait_next_end_frame(const uint32_t start_count) {
    for (uint32_t spins = 0; spins < FRAME_WAIT_SPINS; spins++) {
        if (g_end_frame_count != start_count) return true;
    }
    return false;
}

// Waits until the FIFO is empty and the DMA write position stopped moving.
static void wait_dma_drained(const int ch) {
    size_t   last_pos = gdma_transferred(ch);
    uint32_t stable   = 0;
    for (uint32_t polls = 0; stable < DRAIN_STABLE_POLLS && polls < DRAIN_MAX_POLLS && gdma_busy(ch); polls++) {
        const size_t pos = gdma_transferred(ch);
        stable           = (jpeg_is_fifo_empty() && pos == last_pos) ? stable + 1 : 0;
        last_pos         = pos;
    }
}

// The stream ends with FF D9, a zero byte and the 32-bit frame size. Returns the JPEG length, or 0.
static size_t find_jpeg_end(const uint8_t *buf, const size_t bytes) {
    size_t size = 0;
    for (size_t i = bytes >= END_MARKER_SCAN_LEN ? bytes - END_MARKER_SCAN_LEN : 0; i + 1 < bytes; i++) {
        if (buf[i] == 0xFF && buf[i + 1] == 0xD9) size = i + 2;
    }
    return size;
}

// One DMA run covers exactly one frame: armed right after an end_frame, stopped after the next one.
int camera_capture_frame(camera_frame_t *frame) {
    if (g_sensor == NULL) return -ENXIO;
    if (g_stream.active) return -EBUSY;

    const int ch = g_dma_channel;

    const gdma_config_t dma_config = {
        .src = {
            .mode = GDMA_MODE_JPEG,
            .addr = (uint32_t)&hw_jpeg->rx_fifo_data,
            .incr = false,
            .dw   = GDMA_DATA_WIDTH_32,
        },
        .dst = {
            .mode = GDMA_MODE_DTCM,
            .addr = (uint32_t)g_config.frame_buf,
            .incr = true,
            .dw   = GDMA_DATA_WIDTH_32,
        },
    };
    const int rc = gdma_configure(ch, &dma_config);
    if (rc != 0) return rc < 0 ? rc : -EIO;

    uint32_t count = g_end_frame_count;
    if (!wait_next_end_frame(count)) return -ETIMEDOUT;
    count = g_end_frame_count;

    // arm in the gap after end_frame
    while (!jpeg_is_fifo_empty()) (void)hw_jpeg->rx_fifo_data;
    gdma_restart(ch, (uint32_t)g_config.frame_buf, g_config.frame_buf_size);

    int result = 0;
    if (wait_next_end_frame(count)) {
        wait_dma_drained(ch);

        const bool buffer_full = !gdma_busy(ch);
        gdma_stop(ch);
        const size_t bytes = gdma_transferred(ch);

        const size_t size      = bytes <= g_config.frame_buf_size ? find_jpeg_end(g_config.frame_buf, bytes) : 0;
        const bool   header_ok = bytes >= 2 && g_config.frame_buf[0] == 0xFF && g_config.frame_buf[1] == 0xD8;

        if (buffer_full) {
            result = -EOVERFLOW;
        } else if (!header_ok || size == 0) {
            result = -EBADMSG;
        } else {
            frame->data = g_config.frame_buf;
            frame->size = size;
        }
    } else {
        gdma_stop(ch);
        result = -ETIMEDOUT;
    }

    return result;
}

static uint8_t *stream_slot_ptr(const unsigned slot) {
    return g_stream.config.slots + (size_t)slot * g_stream.config.slot_size;
}

static void stream_arm(const unsigned slot) {
    gdma_restart(g_dma_channel, (uint32_t)stream_slot_ptr(slot), g_stream.config.slot_size);
}

// Runs in the end_frame interrupt: close the finished slot and point the DMA at the next free one.
static void stream_on_end_frame(void) {
    const unsigned cur = g_stream.current;

    if (!g_stream.armed) {
        while (!jpeg_is_fifo_empty()) (void)hw_jpeg->rx_fifo_data;
        stream_arm(cur);
        g_stream.armed = true;
        return;
    }

    const uint8_t *slot  = stream_slot_ptr(cur);
    const size_t   bytes = gdma_transferred(g_dma_channel);
    const size_t   size  = g_last_frame_size;
    const bool     whole = size >= 4 && bytes == size + TRAILER_BYTES && bytes <= g_stream.config.slot_size &&
                       slot[0] == 0xFF && slot[1] == 0xD8 && slot[size - 2] == 0xFF && slot[size - 1] == 0xD9;

    int next = -1;
    for (unsigned i = 1; i < g_stream.config.slot_count && next < 0; i++) {
        const unsigned candidate = (cur + i) % g_stream.config.slot_count;
        if (!g_stream.busy[candidate]) next = (int)candidate;
    }

    const bool queued = whole && next >= 0;
    if (queued) {
        g_stream.size[cur] = (uint32_t)size;
        g_stream.queue[g_stream.head % g_stream.config.slot_count] = (uint8_t)cur;
        g_stream.head      = g_stream.head + 1;
        g_stream.busy[next] = 1;
        g_stream.current   = (unsigned)next;
    } else {
        g_stream.dropped = g_stream.dropped + 1;
    }

    stream_arm(g_stream.current);
    if (queued && g_stream.config.on_frame != NULL) g_stream.config.on_frame();
}

int camera_stream_start(const camera_stream_config_t *config) {
    if (g_sensor == NULL) return -ENXIO;
    if (g_stream.active) return -EBUSY;
    if (config == NULL || config->slots == NULL || ((uintptr_t)config->slots & 3) != 0 ||
        config->slot_size == 0 || (config->slot_size & 3) != 0 || config->slot_size > MAX_DMA_BYTES ||
        config->slot_count < 2 || config->slot_count > CAMERA_STREAM_MAX_SLOTS) {
        return -EINVAL;
    }
    if (g_config.frame_max_bytes != 0 && config->slot_size < g_config.frame_max_bytes + TRAILER_BYTES) {
        return -EINVAL;
    }

    const gdma_config_t dma_config = {
        .src = {
            .mode = GDMA_MODE_JPEG,
            .addr = (uint32_t)&hw_jpeg->rx_fifo_data,
            .incr = false,
            .dw   = GDMA_DATA_WIDTH_32,
        },
        .dst = {
            .mode = GDMA_MODE_DTCM,
            .addr = (uint32_t)config->slots,
            .incr = true,
            .dw   = GDMA_DATA_WIDTH_32,
        },
    };
    const int rc = gdma_configure(g_dma_channel, &dma_config);
    if (rc != 0) return rc < 0 ? rc : -EIO;

    memset((void *)&g_stream, 0, sizeof(g_stream));
    g_stream.config  = *config;
    g_stream.busy[0] = 1;
    g_stream.active  = true;
    return 0;
}

void camera_stream_stop(void) {
    if (!g_stream.active) return;

    g_stream.active = false;
    gdma_stop(g_dma_channel);
}

int camera_stream_get(camera_frame_t *frame) {
    if (!g_stream.active) return -ENXIO;
    if (g_stream.tail == g_stream.head) return -EAGAIN;

    const unsigned slot = g_stream.queue[g_stream.tail % g_stream.config.slot_count];
    frame->data         = stream_slot_ptr(slot);
    frame->size         = g_stream.size[slot];
    g_stream.tail       = g_stream.tail + 1;
    return 0;
}

void camera_stream_release(const camera_frame_t *frame) {
    if (!g_stream.active || frame == NULL || frame->data < g_stream.config.slots) return;

    const size_t slot = (size_t)(frame->data - g_stream.config.slots) / g_stream.config.slot_size;
    if (slot < g_stream.config.slot_count) g_stream.busy[slot] = 0;
}

uint32_t camera_stream_dropped(void) { return g_stream.dropped; }

const char *camera_strerror(const int rc) {
    switch (rc) {
    case 0:
        return "ok";
    case -EAGAIN:
        return "no frame is ready";
    case -EINVAL:
        return "bad configuration or unknown sensor";
    case -ENXIO:
        return "camera is not started";
    case -ENODEV:
        return "no sensor answered";
    case -EIO:
        return "sensor register write failed";
    case -EBUSY:
        return "DMA channel is not available, or a stream is running";
    case -ETIMEDOUT:
        return "no frames arrived";
    case -EOVERFLOW:
        return "frame does not fit in the buffer";
    case -EBADMSG:
        return "received data is not a complete JPEG";
    default:
        return "unknown error";
    }
}
