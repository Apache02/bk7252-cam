#include "hardware/jpeg.h"
#include "soc/jpeg.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "hardware/gpio.h"


// static uint32_t        g_start_frame_count   = 0;
// static uint32_t        g_end_frame_count     = 0;
static int_handler_fn *volatile g_start_frame_handler = NULL;
static int_handler_fn *volatile g_end_frame_handler   = NULL;


static const uint32_t jpeg_quant_table[] = {
    0x07060608, 0x07080506, 0x09090707, 0x140c0a08, 0x0b0b0c0d, 0x1312190c, 0x1a1d140f, 0x1a1d1e1f,
    0x24201c1c, 0x2220272e, 0x1c1c232c, 0x2c293728, 0x34343130, 0x39271f34, 0x3c32383d, 0x3234332e,
    0x0c090909, 0x0d180c0b, 0x2132180d, 0x3232211c, 0x32323232, 0x32323232, 0x32323232, 0x32323232,
    0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232};


static void jpeg_init_quant_table() {
    const size_t count = sizeof(jpeg_quant_table) / sizeof(jpeg_quant_table[0]);
    for (size_t i = 0; i < count; i++) {
        hw_jpeg->quantization_table[i] = jpeg_quant_table[i];
    }
}

static void jpeg_isr() {
    const typeof(hw_jpeg->status) status = {.v = hw_jpeg->status.v};
    hw_jpeg->status.v                    = status.v;

    // if (status.start_frame) g_start_frame_count += 1;
    // if (status.end_frame) g_end_frame_count += 1;

    int_handler_fn *const start_handler = g_start_frame_handler;
    int_handler_fn *const end_handler   = g_end_frame_handler;

    if (status.start_frame && start_handler) start_handler();
    if (status.end_frame && end_handler) end_handler();
}

void jpeg_power_down() { icu_jpeg_power_down(); }
void jpeg_power_up() { icu_jpeg_power_up(); }

void jpeg_init() {
    icu_jpeg_power_down();
    hw_jpeg->ctrl0.v               = 0;
    hw_jpeg->ctrl1.v               = 0;
    hw_jpeg->status.v              = hw_jpeg->status.v;
    hw_jpeg->ctrl0.start_frame_int = 1;
    hw_jpeg->ctrl0.end_frame_int   = 1;

    intc_register_irq_handler(FIQ_SOURCE_JPEG_ENCODER, jpeg_isr);
    intc_enable_irq_source(FIQ_SOURCE_JPEG_ENCODER);

    jpeg_init_quant_table();

    gpio_config_function(GPIO_FUNC_DCMI);
}

void jpeg_configure(const jpeg_encoder_config_t *config) {
    const bool bitrate_control =
        config->frame_min_bytes > 0 && config->frame_max_bytes && config->frame_max_bytes > config->frame_min_bytes;

    hw_jpeg->ctrl0.v = 0;
    hw_jpeg->ctrl1.v = 0;

    hw_write_fields(hw_jpeg->ctrl1,
        .enc_en = 0,
        .x_pixel = config->image_width / 8,
        .y_pixel = config->image_height / 8,
        .enc_size = 1,
        .video_byte_rev = 1,
        .hsync_rev = config->hsync_invert,
        .vsync_rev = config->vsync_invert,
        .yuv_fmt_sel = config->yuv_format,
        .bit_rate_step = 7,
        .bit_rate_ctrl = bitrate_control,
    );

    hw_jpeg->target_byte_h = config->frame_max_bytes;
    hw_jpeg->target_byte_l = config->frame_min_bytes;

    hw_write_fields(hw_jpeg->ctrl0,
        .start_frame_int = 0,
        .end_frame_int = 1,
        .div = config->frequency,
    );
}

void jpeg_enable() {
    hw_jpeg->status.v     = hw_jpeg->status.v;
    hw_jpeg->ctrl1.enc_en = 1;
}

void jpeg_disable() {
    hw_jpeg->status.v     = hw_jpeg->status.v;
    hw_jpeg->ctrl1.enc_en = 0;
}

void jpeg_register_start_frame_handler(int_handler_fn *const handler) { g_start_frame_handler = handler; }
void jpeg_register_end_frame_handler(int_handler_fn *const handler) { g_end_frame_handler = handler; }
void jpeg_clear_start_frame_handler() { g_start_frame_handler = NULL; }
void jpeg_clear_end_frame_handler() { g_end_frame_handler = NULL; }

bool jpeg_is_fifo_empty() { return hw_jpeg->rx_state.empty_fifo; }

size_t jpeg_get_last_frame_size() { return hw_jpeg->byte_cnt_pfrm; }
