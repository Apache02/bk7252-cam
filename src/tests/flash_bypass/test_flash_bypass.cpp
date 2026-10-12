// Flash bypass: ask the internal flash everything it will tell about itself
// except the contents of its main array, and print it to the UART.
//
// The flash is taken away from the flash controller (SCTRL flash_spi_mux) and
// driven by the general SPI master on GPIO14-17, which allows opcodes the
// controller does not issue: JEDEC and legacy IDs, status registers, SFDP,
// unique ID and the security/OTP registers.
//
// All of it is read-only. The CPU must not fetch from flash while the mux is
// flipped and the exception vectors live in flash, so the test runs from IRAM
// with IRQ/FIQ masked and prints only after the flash controller is back.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/stdio.h"
#include "platform/cpu.h"
#include "hardware/wdt.h"
#include "hardware/gpio.h"
#include "hardware/icu.h"
#include "utils/busy_wait.h"

#include "soc/spi.h"
#include "soc/sctrl.h"


#define SPI_TIMEOUT 200000

#define SFDP_LEN 128
#define SEC_LEN  32
#define OTP_LEN  512
#define UID_LEN  16

#define VENDOR_EON 0x1C

// Cross-check region: nothing reads it through the flash controller before the bypass, so
// a match afterwards cannot come from a cache line filled earlier. The flash stores 32 data
// bytes per 34 physical bytes (CRC), hence the address conversion.
#define CMP_LEN       16
#define CMP_XIP_ADDR  0x00011000u
#define CMP_PHYS_ADDR ((CMP_XIP_ADDR / 32) * 34 + CMP_XIP_ADDR % 32)

static unsigned g_total  = 0;
static unsigned g_passed = 0;

static void report(const char *name, bool ok) {
    g_total++;
    if (ok) g_passed++;
    printf("  [%s] %s\r\n", ok ? " OK " : "FAIL", name);
}


// ---- SPI master -------------------------------------------------------------

static bool g_timeout = false;

static void spi_cs(bool take) { hw_spi->ctrl.nssmd = take ? 2 : 3; }

static uint8_t spi_byte(uint8_t out) {
    for (int i = 0; hw_spi->stat.txfifo_full; i++) {
        if (i > SPI_TIMEOUT) {
            g_timeout = true;
            return 0;
        }
    }
    hw_spi->dat.v = out;
    for (int i = 0; hw_spi->stat.rxfifo_empty; i++) {
        if (i > SPI_TIMEOUT) {
            g_timeout = true;
            return 0;
        }
    }
    return (uint8_t)hw_spi->dat.v;
}

// One CS-framed transaction: tx bytes, then rx_len bytes clocked out as 0xFF.
static void xfer(const uint8_t *tx, unsigned tx_len, uint8_t *rx, unsigned rx_len) {
    spi_cs(true);
    for (unsigned i = 0; i < tx_len; i++) spi_byte(tx[i]);
    for (unsigned i = 0; i < rx_len; i++) rx[i] = spi_byte(0xFF);
    spi_cs(false);
}

static void cmd(uint8_t op) { xfer(&op, 1, nullptr, 0); }

static void cmd_read(uint8_t op, uint8_t *rx, unsigned rx_len) { xfer(&op, 1, rx, rx_len); }

// opcode, 24-bit address, dummy bytes, then data
static void addr_read(uint8_t op, uint32_t addr, unsigned dummy, uint8_t *rx, unsigned rx_len) {
    uint8_t tx[8] = {op, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
    xfer(tx, 4 + dummy, rx, rx_len);
}

// 1 MHz, mode 0, 8-bit, CS released; the pins are handed to the flash mux afterwards.
static bool bypass_enter() {
    hw_icu->peri_clk_pwd.spi = 1;
    hw_spi->ctrl.v = 0;
    hw_spi->stat.v = hw_spi->stat.v;
    hw_spi->slave_ctrl.v = hw_spi->slave_ctrl.v;
    while (!hw_spi->stat.rxfifo_empty) (void)hw_spi->dat.v;

    icu_spi_clk(PERI_CLK_26M_XTAL);
    hw_icu->peri_clk_pwd.spi = 0;

    hw_spi->ctrl.v = 0;
    hw_spi->ctrl.ckr = 12; // 26 MHz / (2 * 13)
    hw_spi->ctrl.nssmd = 3;
    hw_spi->ctrl.msten = 1;
    hw_spi->ctrl.spien = 1;

    for (int g = 14; g <= 17; g++) gpio_config((gpio_num_t)g, GPIO_HIGH_IMPEDANCE);

    hw_sctrl->control.flash_spi_mux = 1;
    for (int i = 0; i < SPI_TIMEOUT; i++) {
        if (hw_sctrl->control.flash_spi_mux) return true;
    }
    return false;
}

static void bypass_leave() {
    hw_sctrl->control.flash_spi_mux = 0;
    hw_spi->ctrl.v = 0;
    hw_icu->peri_clk_pwd.spi = 1;
}


// ---- Collected data ---------------------------------------------------------

struct flash_info_t {
    uint8_t jedec[3];
    uint8_t mfr_dev[2];
    uint8_t rel_pd_id;
    uint8_t sr1, sr2, sr3;
    uint8_t sfdp[SFDP_LEN];
    uint8_t uid[UID_LEN];
    uint8_t sec[4][SEC_LEN];
    bool    otp_read;
    uint8_t otp_sr1;
    uint8_t otp[OTP_LEN];
    uint8_t cmp[CMP_LEN]; // main-array bytes read over SPI, only to cross-check the controller
};

static flash_info_t g_info;

static void collect(flash_info_t &f) {
    cmd_read(0x9F, f.jedec, sizeof(f.jedec));
    addr_read(0x90, 0, 0, f.mfr_dev, sizeof(f.mfr_dev));
    addr_read(0xAB, 0, 0, &f.rel_pd_id, 1);
    cmd_read(0x05, &f.sr1, 1);
    cmd_read(0x35, &f.sr2, 1);
    cmd_read(0x15, &f.sr3, 1);
    addr_read(0x5A, 0, 1, f.sfdp, sizeof(f.sfdp));
    addr_read(0x4B, 0, 4, f.uid, sizeof(f.uid));

    // Security registers 1..3 sit at 0x1000, 0x2000, 0x3000 on most vendors; 0x0000 is
    // read as well so a chip with a different map still shows something.
    for (unsigned i = 0; i < 4; i++) addr_read(0x48, i * 0x1000, 1, f.sec[i], SEC_LEN);

    addr_read(0x03, CMP_PHYS_ADDR, 0, f.cmp, sizeof(f.cmp));

    // EON maps its 512-byte OTP sector over sector 511 after "Enter OTP mode" (3Ah);
    // 04h leaves the mode again. The opcode is not known on other vendors.
    if (f.jedec[0] == VENDOR_EON) {
        cmd(0x3A);
        cmd_read(0x05, &f.otp_sr1, 1);
        addr_read(0x03, 0x1FF000, 0, f.otp, sizeof(f.otp));
        cmd(0x04);
        f.otp_read = true;
    }
}


// ---- Reporting --------------------------------------------------------------

static const char *vendor_name(uint8_t id) {
    switch (id) {
        case 0x1C: return "EON";
        case 0xC8: return "GigaDevice";
        case 0xEF: return "Winbond";
        case 0x85: return "Puya";
        case 0x0B: return "XTX";
        case 0xC2: return "Macronix";
        case 0x20: return "XMC / Micron";
        case 0xA1: return "Fudan";
        case 0x68: return "Boya";
        case 0x5E: return "Zbit";
        case 0x9D: return "ISSI";
        default:   return "unknown";
    }
}

static const char *classify(const uint8_t *b, unsigned n) {
    bool all00 = true, allff = true;
    for (unsigned i = 0; i < n; i++) {
        if (b[i] != 0x00) all00 = false;
        if (b[i] != 0xFF) allff = false;
    }
    return all00 ? "all 00" : allff ? "all FF" : "data";
}

static void print_hex(const uint8_t *b, unsigned n) {
    for (unsigned i = 0; i < n; i++) {
        if (i && i % 16 == 0) printf("\r\n                 ");
        printf(" %02X", b[i]);
    }
    printf("\r\n");
}

static void print_buf(const char *label, const uint8_t *b, unsigned n) {
    const char *kind = classify(b, n);
    printf("  %-14s :", label);
    if (strcmp(kind, "data") == 0) {
        print_hex(b, n);
    } else {
        printf(" %s\r\n", kind);
    }
}

static void print_sfdp(const uint8_t *s) {
    if (memcmp(s, "SFDP", 4) != 0) {
        printf("  SFDP           : no signature\r\n");
        return;
    }

    unsigned nph = s[6] + 1;
    printf("  SFDP           : rev %u.%u, %u parameter header(s), access protocol %02X\r\n", s[5], s[4], nph, s[7]);

    for (unsigned i = 0; i < nph && 8 + 8 * (i + 1) <= SFDP_LEN; i++) {
        const uint8_t *h = &s[8 + 8 * i];
        unsigned ptr = h[4] | (h[5] << 8) | (h[6] << 16);
        printf("    header %u     : id %02X rev %u.%u, %u dwords @ 0x%02X\r\n", i, h[0], h[2], h[1], h[3], ptr);

        if (h[0] != 0x00 || ptr + 8 > SFDP_LEN) continue;

        uint32_t dw1 = s[ptr] | (s[ptr + 1] << 8) | (s[ptr + 2] << 16) | ((uint32_t)s[ptr + 3] << 24);
        uint32_t dw2 = s[ptr + 4] | (s[ptr + 5] << 8) | (s[ptr + 6] << 16) | ((uint32_t)s[ptr + 7] << 24);
        unsigned kib = (dw2 & 0x80000000u) ? ((dw2 & 0x7FFFFFFFu) >= 13 ? 1u << ((dw2 & 0x7FFFFFFFu) - 13) : 0)
                                           : (dw2 + 1) / 8192;
        printf("    density      : %u KiB\r\n", kib);
        printf("    4 KiB erase  : opcode %02X\r\n", (unsigned)((dw1 >> 8) & 0xFF));
    }
}

static void print_info(const flash_info_t &f) {
    unsigned cap_log2 = f.jedec[2];

    printf("  JEDEC ID (9F)  : %02X %02X %02X  (%s", f.jedec[0], f.jedec[1], f.jedec[2], vendor_name(f.jedec[0]));
    if (cap_log2 >= 0x10 && cap_log2 <= 0x1F) printf(", %u KiB", (1u << cap_log2) / 1024);
    printf(")\r\n");
    printf("  Manuf/dev (90) : %02X %02X\r\n", f.mfr_dev[0], f.mfr_dev[1]);
    printf("  Dev ID (AB)    : %02X\r\n", f.rel_pd_id);
    printf("  SR1/SR2/SR3    : %02X %02X %02X\r\n", f.sr1, f.sr2, f.sr3);
    print_sfdp(f.sfdp);
    print_buf("Unique ID (4B)", f.uid, UID_LEN);

    for (unsigned i = 0; i < 4; i++) {
        char label[16];
        snprintf(label, sizeof(label), "Sec reg @%04X", i * 0x1000);
        print_buf(label, f.sec[i], SEC_LEN);
    }

    if (f.otp_read) {
        printf("  OTP mode SR1   : %02X\r\n", f.otp_sr1);
        print_buf("OTP sector", f.otp, OTP_LEN);
    } else {
        printf("  OTP sector     : not probed (vendor opcode unknown)\r\n");
    }
}

static void xip_read(uint8_t *dst) {
    const volatile uint8_t *src = (const volatile uint8_t *)CMP_XIP_ADDR;
    for (unsigned i = 0; i < CMP_LEN; i++) dst[i] = src[i];
}

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);
    wdt_set(10000);
    wdt_up();

    printf("\r\n== Flash bypass info ==\r\n\r\n");

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    bool mux_ok = bypass_enter();
    if (mux_ok) collect(g_info);
    bypass_leave();
    GLOBAL_INT_RESTORE();

    uint8_t xip_cmp[CMP_LEN];
    xip_read(xip_cmp);

    if (mux_ok) print_info(g_info);
    printf("\r\n");

    report("flash handed to the SPI master", mux_ok);
    report("no SPI timeout", !g_timeout);
    report("JEDEC manufacturer is valid", g_info.jedec[0] != 0x00 && g_info.jedec[0] != 0xFF);
    report("legacy ID (90) matches JEDEC manufacturer", g_info.mfr_dev[0] == g_info.jedec[0]);
    report("flash idle (WIP and WEL clear)", (g_info.sr1 & 0x03) == 0);
    report("controller reads what the SPI master read (first access after bypass)",
           memcmp(xip_cmp, g_info.cmp, CMP_LEN) == 0);
    report("compared region holds data (not all 00/FF)", strcmp(classify(g_info.cmp, CMP_LEN), "data") == 0);

    printf("\r\n%u / %u passed\r\n", g_passed, g_total);
    printf("==END==\r\n");
    wdt_reboot(100);
    return 0;
}
