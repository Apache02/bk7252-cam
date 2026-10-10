#ifndef _HARDWARE_JPEG_H
#define _HARDWARE_JPEG_H

#include <sys/cdefs.h>
#include "hardware/intc.h"


typedef enum {
    JPEG_ENCODER_YUYV = 0,
    JPEG_ENCODER_UYVY = 1,
    JPEG_ENCODER_YYUV = 2,
    JPEG_ENCODER_UVYY = 3,
} jpeg_encoder_yuv_format_t;

typedef enum {
    JPEG_ENCODER_FREQ_24MHZ   = 0,
    JPEG_ENCODER_FREQ_16MHZ   = 1,
    JPEG_ENCODER_FREQ_12MHZ   = 2,
    JPEG_ENCODER_FREQ_24MHZ_2 = 3,
} jpeg_encoder_freq_t;

typedef struct jpeg_encoder_config {
    uint16_t                  image_width;
    uint16_t                  image_height;
    // sensor specified options
    jpeg_encoder_freq_t       frequency;
    jpeg_encoder_yuv_format_t yuv_format;
    bool                      hsync_invert;
    bool                      vsync_invert;
    // bitrate control
    uint32_t                  frame_min_bytes;
    uint32_t                  frame_max_bytes;
} jpeg_encoder_config_t;

#ifdef __cplusplus
extern "C" {
#endif

void jpeg_power_up();
void jpeg_power_down();

void jpeg_init();

void jpeg_configure(const jpeg_encoder_config_t *config);

void jpeg_enable();
void jpeg_disable();

void jpeg_register_start_frame_handler(int_handler_fn *const handler);
void jpeg_register_end_frame_handler(int_handler_fn *const handler);
void jpeg_clear_start_frame_handler();
void jpeg_clear_end_frame_handler();

bool   jpeg_is_fifo_empty();
size_t jpeg_get_last_frame_size();

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_JPEG_H
