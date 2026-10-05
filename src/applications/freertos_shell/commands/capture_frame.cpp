#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "soc/jpeg.h"
#include "soc/i2c.h"
#include "hardware/i2c.h"
#include "hardware/icu.h"
#include "hardware/jpeg.h"
#include "hardware/gpio.h"
#include "hardware/gdma.h"
#include "hardware/sctrl.h"
#include "hi704_tables.h"

#define CAMERA_ADDR      (0x30)
#define CAMERA_RESET_PIN (28)
#define SENSOR_ID_REG    (0x04)

#define CAPTURE_DMA_CHANNEL (4)
#define FRAME_BUF_BYTES     (48 * 1024)

static uint8_t g_frame_buf[FRAME_BUF_BYTES] __attribute__((aligned(4)));

static volatile uint32_t g_end_frame_count;
static volatile uint32_t g_last_frame_size;

static void end_frame_handler() {
    g_last_frame_size = static_cast<uint32_t>(jpeg_get_last_frame_size());
    g_end_frame_count = g_end_frame_count + 1;
}

static void delay_ms(const uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

// Powers the encoder and the I2C bus down, then pulses the sensor pin.
static void camera_reset() {
    hw_i2c1->config.ensmb = 0;
    icu_i2c1_power_down();
    hw_i2c1->config.v = 0;

    jpeg_disable();
    jpeg_power_down();

    gpio_config(CAMERA_RESET_PIN, GPIO_OUT);
    gpio_put(CAMERA_RESET_PIN, false);
    delay_ms(100);
    gpio_put(CAMERA_RESET_PIN, true);
    delay_ms(100);
    gpio_put(CAMERA_RESET_PIN, false);
    delay_ms(100);
}

// The encoder must run before the sensor is touched over I2C, or MCLK does not run and the bus jams.
static void dvp_jpeg_bring_up() {
    sctrl_vddram_enable(SCTRL_VDDRAM_3V5);

    jpeg_init();

    const jpeg_encoder_config_t config = {
        .image_width     = 640,
        .image_height    = 480,
        .frequency       = JPEG_ENCODER_FREQ_24MHZ,
        .yuv_format      = JPEG_ENCODER_YUYV,
        .hsync_invert    = false,
        .vsync_invert    = false,
        .frame_min_bytes = sizeof(g_frame_buf) / 2,
        .frame_max_bytes = sizeof(g_frame_buf) - 1024,
    };
    jpeg_configure(&config);
    jpeg_register_end_frame_handler(end_frame_handler);

    jpeg_power_up();
    jpeg_enable();
}

static bool sensor_read_reg(const uint8_t reg, uint8_t *value) {
    return i2c1_write(CAMERA_ADDR, &reg, 1) == 0 && i2c1_read(CAMERA_ADDR, value, 1) == 0;
}

// The sensor needs a moment after power-up before it answers; poll its ID register.
static bool sensor_wait_ready(const unsigned max_attempts) {
    for (unsigned attempt = 0; attempt < max_attempts; attempt++) {
        uint8_t id = 0;
        if (sensor_read_reg(SENSOR_ID_REG, &id)) {
            printf("sensor id = 0x%02x\r\n", id);
            return true;
        }
        delay_ms(20);
    }
    return false;
}

static bool sensor_init(const hi704_table_t &table) {
    i2c1_init(100000);
    delay_ms(200);

    if (!sensor_wait_ready(50)) {
        printf("sensor does not answer on I2C\r\n");
        return false;
    }

    unsigned ok = 0;
    for (unsigned i = 0; i < table.count; i++) {
        if (i2c1_write(CAMERA_ADDR, table.pairs[i], 2) == 0) {
            ok++;
        } else {
            printf("register write #%u (0x%02x = 0x%02x) failed\r\n", i, table.pairs[i][0], table.pairs[i][1]);
        }
    }
    printf("sensor init (%s): %u of %u writes succeeded\r\n", table.name, ok, static_cast<unsigned>(table.count));
    return ok == table.count;
}

static const hi704_table_t *find_table(const char *name) {
    for (const hi704_table_t &table : hi704_tables) {
        if (strcmp(table.name, name) == 0) return &table;
    }
    return nullptr;
}

// capture_start [table]: encoder, sensor, then a short check that frames really arrive.
int command_capture_start(int argc, const char *argv[]) {
    const hi704_table_t *table = &hi704_tables[sizeof(hi704_tables) / sizeof(hi704_tables[0]) - 1];
    if (argc > 1) {
        table = find_table(argv[1]);
        if (table == nullptr) {
            printf("Usage: %s [", argv[0]);
            for (unsigned i = 0; i < sizeof(hi704_tables) / sizeof(hi704_tables[0]); i++) {
                printf(i ? "|%s" : "%s", hi704_tables[i].name);
            }
            printf("]\r\n");
            return 1;
        }
    }

    camera_reset();
    dvp_jpeg_bring_up();
    delay_ms(100);

    if (!sensor_init(*table)) return 1;

    delay_ms(1000); // let the sensor settle and start streaming

    const uint32_t before = g_end_frame_count;
    delay_ms(500);
    const uint32_t frames = g_end_frame_count - before;

    printf("camera started: %lu frames in ~0.5 s, last frame %lu bytes\r\n", frames, g_last_frame_size);
    return frames > 0 ? 0 : 1;
}

// Spins until the frame counter moves; returns false on timeout. No sleeping: the DMA must be
// armed within the short gap between two frames.
static bool wait_next_end_frame(const uint32_t start_count) {
    for (uint32_t spins = 0; spins < 30000000u; spins++) {
        if (g_end_frame_count != start_count) return true;
    }
    return false;
}

// One DMA run covers exactly one frame: armed right after an end_frame, stopped after the next one.
int command_capture_frame(__unused int argc, __unused const char *argv[]) {
    const int ch = gdma_reserve_specific_channel(CAPTURE_DMA_CHANNEL);
    if (ch < 0) {
        printf("cannot reserve DMA channel: %d\r\n", ch);
        return 1;
    }

    const gdma_config_t dma_config = {
        .src = {
            .mode = GDMA_MODE_JPEG,
            .addr = reinterpret_cast<uint32_t>(&hw_jpeg->rx_fifo_data),
            .incr = false,
            .dw   = GDMA_DATA_WIDTH_32,
        },
        .dst = {
            .mode = GDMA_MODE_DTCM,
            .addr = reinterpret_cast<uint32_t>(&g_frame_buf[0]),
            .incr = true,
            .dw   = GDMA_DATA_WIDTH_32,
        },
    };
    const int rc = gdma_configure(ch, &dma_config);
    if (rc != 0) {
        printf("cannot configure DMA: %d\r\n", rc);
        gdma_release_channel(ch);
        return 1;
    }

    int      result = 1;
    uint32_t count  = g_end_frame_count;
    if (!wait_next_end_frame(count)) {
        printf("no frames, run capture_start first\r\n");
        gdma_release_channel(ch);
        return 1;
    }
    count = g_end_frame_count;

    // arm in the gap after end_frame
    while (!jpeg_is_fifo_empty()) (void)hw_jpeg->rx_fifo_data;
    gdma_restart(ch, reinterpret_cast<uint32_t>(&g_frame_buf[0]), FRAME_BUF_BYTES);

    if (wait_next_end_frame(count)) {
        // wait until the FIFO is empty and the write pointer stopped moving
        size_t   last_pos = gdma_transferred(ch);
        uint32_t stable   = 0;
        for (uint32_t spins = 0; stable < 50 && spins < 400000 && gdma_busy(ch); spins++) {
            const size_t pos = gdma_transferred(ch);
            stable           = (jpeg_is_fifo_empty() && pos == last_pos) ? stable + 1 : 0;
            last_pos         = pos;
        }

        const bool buffer_full = !gdma_busy(ch);
        gdma_stop(ch);
        const uint32_t bytes = static_cast<uint32_t>(gdma_transferred(ch));

        // the stream ends with FF D9, a zero byte and the 32-bit frame size
        uint32_t size = 0;
        for (uint32_t i = bytes >= 16 ? bytes - 16 : 0; i + 1 < bytes && bytes <= FRAME_BUF_BYTES; i++) {
            if (g_frame_buf[i] == 0xFF && g_frame_buf[i + 1] == 0xD9) size = i + 2;
        }
        const bool header_ok = bytes >= 2 && g_frame_buf[0] == 0xFF && g_frame_buf[1] == 0xD8;

        if (buffer_full) {
            printf("DMA filled all %u bytes, encoder reports frame of %lu bytes\r\n", FRAME_BUF_BYTES,
                   g_last_frame_size);
        } else if (!header_ok || size == 0) {
            printf("broken frame: %lu bytes received, header %s, no end marker\r\n", bytes, header_ok ? "ok" : "bad");
        } else {
            printf("addr 0x%08lx  size %lu\r\n", reinterpret_cast<unsigned long>(&g_frame_buf[0]), size);
            result = 0;
        }
    } else {
        gdma_stop(ch);
        printf("frame did not finish\r\n");
    }

    gdma_release_channel(ch);
    return result;
}
