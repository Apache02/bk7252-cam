#include "soc/jpeg.h"
#include "hardware/jpeg.h"
#include "hardware/intc.h"


#define count_of(x) (sizeof(x) / sizeof(x[0]))


const uint32_t jpeg_quant_table[] = {0x07060608, 0x07080506, 0x09090707, 0x140c0a08, 0x0b0b0c0d, 0x1312190c, 0x1a1d140f,
                                     0x1a1d1e1f, 0x24201c1c, 0x2220272e, 0x1c1c232c, 0x2c293728, 0x34343130, 0x39271f34,
                                     0x3c32383d, 0x3234332e, 0x0c090909, 0x0d180c0b, 0x2132180d, 0x3232211c, 0x32323232,
                                     0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232, 0x32323232,
                                     0x32323232, 0x32323232, 0x32323232, 0x32323232};


void jpeg_init_quant_table(void) {
    for (int i = 0; i < count_of(jpeg_quant_table); i++) {
        hw_jpeg->quantization_table[i] = jpeg_quant_table[i];
    }
}

static void jpeg_isr(void) {
    typeof(hw_jpeg->status) status = {.v = hw_jpeg->status.v};
    hw_jpeg->status.v              = status.v;

    if (status.start_frame) {
        // start frame
    }
    if (status.end_frame) {
        // end frame
    }
}


void jpeg_init(void) {
    intc_register_irq_handler(FIQ_SOURCE_JPEG_ENCODER, jpeg_isr);
    intc_enable_irq_source(FIQ_SOURCE_JPEG_ENCODER);

    hw_jpeg->ctrl0.v  = 0;
    hw_jpeg->status.v = hw_jpeg->status.v;
    hw_jpeg->ctrl0.start_frame_int = 1;
    hw_jpeg->ctrl0.end_frame_int = 1;
}
