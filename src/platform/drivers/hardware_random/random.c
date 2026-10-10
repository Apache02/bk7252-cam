#include <string.h>
#include "soc/random.h"
#include "hardware/random.h"
#include "utils/busy_wait.h"

void trng_enable() {
    if (hw_trng->ctrl.enable) return;

    hw_trng->ctrl.enable = 1;

    // The first reads can repeat after a reset; with 16 us or more they differ every time.
    // 32 us leaves margin.
    busy_wait_us(32);
}

void trng_disable() { hw_trng->ctrl.enable = 0; }

// Back-to-back reads are linearly dependent, so XOR and shifts alone keep the dependency;
// the carries of the LCG multiply break it. Output of this loop passed NIST STS.
uint32_t get_random() {
    uint32_t word = hw_trng->data;
    for (int i = 0; i < 16; i++) {
        word = (word * 1664525 + 1013904223) ^ hw_trng->data;
    }
    return word;
}

void get_random_bytes(void *buf, size_t len) {
    uint8_t *dst = buf;

    while (len > 0) {
        const uint32_t word = get_random();
        const size_t   n    = len < sizeof(word) ? len : sizeof(word);
        memcpy(dst, &word, n);
        dst += n;
        len -= n;
    }
}
