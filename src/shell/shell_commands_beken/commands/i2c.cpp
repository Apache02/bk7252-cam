#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include "hardware/i2c.h"
#include "hardware/icu.h"
#include "hardware/gpio.h"
#include "hardware/sctrl.h"
#include "soc/jpeg.h"
#include "utils/busy_wait.h"

static constexpr int default_baud_hz = 100000;
static constexpr int max_baud_hz     = 1000000;
// Lowest rate whose clock divider still fits the 10-bit freq_div field.
static constexpr int min_baud_hz = 8448;

// Longest write or read the command accepts, in bytes.
static constexpr size_t max_transfer = 256;

// The camera on I2C1 does not answer until it is powered and the DVP/JPEG
// block is up: clock gate on, GPIO27-39 in DCMI mode and the encoder enabled.
static void dvp_jpeg_bring_up() {
    sctrl_vddram_enable(SCTRL_VDDRAM_3V5);

    hw_write_fields(hw_jpeg->ctrl0,
        .div = 0, // 24 MHz MCLK
    );

    icu_jpeg_power_up();
    gpio_config_function(GPIO_FUNC_DCMI);

    hw_jpeg->ctrl1.enc_en = 1;

    busy_wait_ms(10);
}

// Addresses 0x00-0x07 and 0x78-0x7F are reserved by the I2C specification.
static bool reserved_addr(uint8_t addr) { return (addr & 0x78) == 0 || (addr & 0x78) == 0x78; }

struct i2c_port {
    const char *name;
    void (*init)(uint32_t baud_hz);
    int (*write)(uint8_t addr7, const uint8_t *data, uint16_t len);
    int (*read)(uint8_t addr7, uint8_t *data, uint16_t len);
};

static constexpr i2c_port g_ports[] = {
    {"i2c1", i2c1_init, i2c1_write, i2c1_read},
};

static const i2c_port *find_port(const char *name) {
    for (const auto &port : g_ports) {
        if (strcmp(port.name, name) == 0) return &port;
    }
    return nullptr;
}

static void usage(const char *command) {
    printf("Usage:\r\n");
    printf("  %s scan  <port> [options]\r\n", command);
    printf("  %s read  <port> <addr> <N> [options]\r\n", command);
    printf("  %s write <port> <addr> <HEX> [options]\r\n", command);
    printf("  %s xfer  <port> <addr> <HEX> <N> [options]\r\n", command);
    printf("  port     i2c1\r\n");
    printf("  addr     7-bit address in hex, e.g. 30 or 0x30\r\n");
    printf("  HEX      bytes to write as hex digits, e.g. 0a1b or 0x0a1b\r\n");
    printf("  N        number of bytes to read, decimal, 1..%u\r\n", static_cast<unsigned>(max_transfer));
    printf("  xfer     write HEX, STOP, then read N bytes (no repeated start)\r\n");
    printf("  options  --baud <hz>  bus speed, %d..%d (default %d)\r\n", min_baud_hz, max_baud_hz, default_baud_hz);
    printf("           --no_dvp     skip the DVP/JPEG bring-up that the camera needs to answer\r\n");
}

static void print_error(const char *what, int rc) {
    switch (rc) {
    case -EFAULT: printf("%s: NACK\r\n", what); break;
    case -ETIMEDOUT: printf("%s: timeout\r\n", what); break;
    case -EBUSY: printf("%s: bus busy\r\n", what); break;
    default: printf("%s: error %d\r\n", what, rc); break;
    }
}

// Parses a hex number with an optional 0x prefix. Returns false on junk.
static bool parse_hex(const char *s, unsigned &out) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (*s == '\0') return false;

    char *end;
    out = strtoul(s, &end, 16);
    return *end == '\0';
}

static bool parse_addr(const char *s, uint8_t &addr) {
    unsigned value;
    if (!parse_hex(s, value) || value > 0x7f || reserved_addr(static_cast<uint8_t>(value))) {
        printf("Invalid address: %s (7-bit, 0x08..0x77)\r\n", s);
        return false;
    }
    addr = static_cast<uint8_t>(value);
    return true;
}

// Parses hex digits (optional 0x prefix) into bytes. Returns the byte count, or -1 on junk.
static int parse_hex_bytes(const char *s, uint8_t *out, size_t max) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    const size_t digits = strlen(s);
    if (digits == 0 || digits % 2 != 0 || digits / 2 > max) return -1;

    for (size_t i = 0; i < digits; i++) {
        if (!isxdigit(static_cast<unsigned char>(s[i]))) return -1;
    }
    for (size_t i = 0; i < digits / 2; i++) {
        char pair[3] = {s[2 * i], s[2 * i + 1], '\0'};
        out[i]       = static_cast<uint8_t>(strtoul(pair, nullptr, 16));
    }
    return static_cast<int>(digits / 2);
}

static bool parse_count(const char *s, uint16_t &count) {
    const int n = take_int(s).ok_or(0);
    if (n < 1 || n > static_cast<int>(max_transfer)) {
        printf("Invalid byte count: %s (1..%u)\r\n", s, static_cast<unsigned>(max_transfer));
        return false;
    }
    count = static_cast<uint16_t>(n);
    return true;
}

static void print_bytes(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x%s", data[i], (i % 16 == 15 || i == len - 1) ? "\r\n" : " ");
    }
}

static int scan_bus(const i2c_port &port) {
    unsigned found = 0;

    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    for (unsigned addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) printf("%02x ", addr);

        if (reserved_addr(static_cast<uint8_t>(addr))) {
            printf("_");
        } else if (port.write(static_cast<uint8_t>(addr), nullptr, 0) == 0) {
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

static int read_bus(const i2c_port &port, uint8_t addr, uint16_t count) {
    static uint8_t data[max_transfer];

    const int rc = port.read(addr, data, count);
    if (rc != 0) {
        print_error("read", rc);
        return 1;
    }
    print_bytes(data, count);
    return 0;
}

static int write_bus(const i2c_port &port, uint8_t addr, const uint8_t *data, uint16_t len) {
    const int rc = port.write(addr, data, len);
    if (rc != 0) {
        print_error("write", rc);
        return 1;
    }
    printf("Wrote %u byte(s)\r\n", len);
    return 0;
}

int command_i2c(int argc, const char *argv[]) {
    int  baud_hz = default_baud_hz;
    bool no_dvp  = false;

    // Positional arguments in order; options may appear anywhere between them.
    const char *args[5];
    int         nargs = 0;

    for (int i = 1; i < argc; i++) {
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
        } else if (strncmp(argv[i], "--", 2) == 0) {
            printf("Unknown option: %s\r\n", argv[i]);
            return 1;
        } else if (nargs < 5) {
            args[nargs++] = argv[i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (nargs < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *sub = args[0];
    int         expected;
    if (strcmp(sub, "scan") == 0) {
        expected = 2;
    } else if (strcmp(sub, "read") == 0 || strcmp(sub, "write") == 0) {
        expected = 4;
    } else if (strcmp(sub, "xfer") == 0) {
        expected = 5;
    } else {
        printf("Unknown subcommand: %s\r\n", sub);
        usage(argv[0]);
        return 1;
    }
    if (nargs != expected) {
        usage(argv[0]);
        return 1;
    }

    const i2c_port *port = find_port(args[1]);
    if (port == nullptr) {
        printf("Invalid port: %s\r\n", args[1]);
        return 1;
    }

    // Validate every argument before touching the hardware.
    static uint8_t tx[max_transfer];
    uint8_t        addr   = 0;
    int            tx_len = 0;
    uint16_t       count  = 0;

    if (strcmp(sub, "scan") != 0) {
        if (!parse_addr(args[2], addr)) return 1;
    }
    if (strcmp(sub, "write") == 0 || strcmp(sub, "xfer") == 0) {
        tx_len = parse_hex_bytes(args[3], tx, sizeof tx);
        if (tx_len < 0) {
            printf("Invalid HEX: %s (even number of hex digits, up to %u bytes)\r\n", args[3],
                   static_cast<unsigned>(max_transfer));
            return 1;
        }
    }
    if (strcmp(sub, "read") == 0) {
        if (!parse_count(args[3], count)) return 1;
    } else if (strcmp(sub, "xfer") == 0) {
        if (!parse_count(args[4], count)) return 1;
    }

    if (!no_dvp) dvp_jpeg_bring_up();
    port->init(static_cast<uint32_t>(baud_hz));

    if (strcmp(sub, "scan") == 0) return scan_bus(*port);
    if (strcmp(sub, "read") == 0) return read_bus(*port, addr, count);
    if (strcmp(sub, "write") == 0) return write_bus(*port, addr, tx, static_cast<uint16_t>(tx_len));

    // xfer
    if (write_bus(*port, addr, tx, static_cast<uint16_t>(tx_len)) != 0) return 1;
    return read_bus(*port, addr, count);
}
