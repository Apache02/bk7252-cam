#include <stdio.h>
#include <string.h>
#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include "hardware/i2c.h"
#include "hardware/icu.h"
#include "hardware/gpio.h"
#include "soc/jpeg.h"
#include "utils/busy_wait.h"

static constexpr int default_baud_hz = 100000;
static constexpr int max_baud_hz     = 1000000;
// Lowest rate whose clock divider still fits the 10-bit freq_div field.
static constexpr int min_baud_hz = 8448;

// The camera on I2C1 does not answer until the DVP/JPEG block is up: clock
// gate on, GPIO27-39 in DCMI mode and the encoder enabled.
static void dvp_jpeg_bring_up() {
    hw_write_fields(hw_jpeg->ctrl0,
        .div = 0, // 24 MHz MCLK
    );

    icu_jpeg_power_up();
    gpio_config_function(GPIO_FUNC_DCMI);

    hw_jpeg->ctrl1.enc_en = 1;
}

// Addresses 0x00-0x07 and 0x78-0x7F are reserved by the I2C specification.
static bool reserved_addr(uint8_t addr) { return (addr & 0x78) == 0 || (addr & 0x78) == 0x78; }

static void usage(const char *command) {
    printf("Usage: %s [--baud <hz>] [--no_dvp] scan i2c1\r\n", command);
    printf("  --baud <hz>  bus speed, %d..%d (default %d)\r\n", min_baud_hz, max_baud_hz, default_baud_hz);
    printf("  --no_dvp     skip the DVP/JPEG bring-up that the camera needs to answer\r\n");
}

static int scan_i2c1() {
    unsigned found = 0;

    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    for (unsigned addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) printf("%02x ", addr);

        if (reserved_addr(static_cast<uint8_t>(addr))) {
            printf("_");
        } else if (i2c1_write(static_cast<uint8_t>(addr), nullptr, 0) == 0) {
            printf("@");
            found++;
        } else {
            printf(".");
        }

        printf(addr % 16 == 15 ? "\r\n" : "  ");
        busy_wait_us(2000);
    }
    printf("Found addresses: %u\r\n", found);

    return 0;
}

int command_i2c(int argc, const char *argv[]) {
    int  baud_hz = default_baud_hz;
    bool no_dvp  = false;

    int i = 1;
    for (; i < argc && strncmp(argv[i], "--", 2) == 0; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--no_dvp") == 0) {
            no_dvp = true;
        } else if (strcmp(argv[i], "--baud") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
                return 1;
            }
            baud_hz = take_int(argv[i]).ok_or(0);
            if (baud_hz < min_baud_hz || baud_hz > max_baud_hz) {
                printf("Invalid baud: %s\r\n", argv[i]);
                return 1;
            }
        } else {
            printf("Unknown option: %s\r\n", argv[i]);
            return 1;
        }
    }

    if (argc - i != 2 || strcmp(argv[i], "scan") != 0) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[i + 1], "i2c1") != 0) {
        printf("Invalid bus: %s\r\n", argv[i + 1]);
        return 1;
    }

    if (!no_dvp) dvp_jpeg_bring_up();
    i2c1_init(static_cast<uint32_t>(baud_hz));

    return scan_i2c1();
}
