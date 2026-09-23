// I2C1 bus scan, using the real interrupt-driven hardware_i2c driver.
//
// The HI704 camera on I2C1 does not answer at all until the DVP/JPEG block is
// brought up first (clock gate + GPIO27-39 in DCMI mode + JPEG_ENC_EN) —
// confirmed on hardware. That bring-up is this test's job, not the driver's.

#include <stdint.h>
#include <stdio.h>

#include "platform/stdio.h"
#include "platform/cpu.h"
#include "hardware/wdt.h"
#include "hardware/icu.h"
#include "hardware/gpio.h"
#include "hardware/sctrl.h"
#include "hardware/i2c.h"
#include "utils/busy_wait.h"

#include "soc/jpeg.h"


static unsigned g_total  = 0;
static unsigned g_passed = 0;

static void report(const char *name, bool ok) {
    g_total++;
    if (ok) g_passed++;
    printf("  [%s] %s\r\n", ok ? " OK " : "FAIL", name);
}

static void dvp_jpeg_bring_up() {
    hw_jpeg->ctrl0.v = 0; // div=0 -> 24 MHz MCLK

    icu_jpeg_power_up();
    gpio_config_function(GPIO_FUNC_DCMI);

    typeof(hw_jpeg->ctrl1) ctrl1 = {};
    ctrl1.v                      = hw_jpeg->ctrl1.v;
    ctrl1.enc_en                 = 1;
    hw_jpeg->ctrl1.v             = ctrl1.v;
}

static bool reserved_addr(uint8_t addr) { return (addr & 0x78) == 0 || (addr & 0x78) == 0x78; }

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);
    wdt_set(10000);
    wdt_up();

    sctrl_init();

    dvp_jpeg_bring_up();
    i2c1_init(100000);
    portENABLE_IRQ();

    printf("\r\n== I2C1 bus scan ==\r\n\r\n");

    unsigned found_count = 0;

    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    for (unsigned addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) printf("%02x ", addr);

        if (reserved_addr(static_cast<uint8_t>(addr))) {
            printf("_");
        } else {
            bool ok = i2c1_write(static_cast<uint8_t>(addr), nullptr, 0) == 0;
            if (ok) {
                printf("@");
                found_count++;
            } else {
                printf(".");
            }
        }

        printf(addr % 16 == 15 ? "\r\n" : "  ");
        busy_wait_us(2000); // between-address settle
    }
    printf("\r\n");

    report("found a device on I2C1", found_count > 0);

    printf("\r\n%u / %u passed\r\n", g_passed, g_total);
    printf("==END==\r\n");
    wdt_reboot(100);
    return 0;
}
