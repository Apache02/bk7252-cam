// Destroys the contents of the flash sector at 0x1FF000.
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/flash.h"
#include "hardware/wdt.h"
#include "platform/flash_kv.h"
#include "platform/stdio.h"
#include "utils/busy_wait.h"

#define KV_BASE   FLASH_KV_ADDR
#define PAUSE_MS  100

// The SVC stack is about 1 KB, so the test buffers live in BSS.
static uint8_t out[FLASH_KV_MAX_VALUE_LEN];
static uint8_t value[FLASH_KV_MAX_VALUE_LEN];
static uint8_t big[FLASH_KV_MAX_VALUE_LEN + 1];

static int g_failures;

static void say(const char *msg) {
    printf("%s\r\n", msg);
    busy_wait_ms(PAUSE_MS);
}

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\r\n", __LINE__, #cond);              \
            busy_wait_ms(PAUSE_MS);                                       \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

static const char *state_name(flash_kv_state_t s) {
    switch (s) {
    case FLASH_KV_BLANK: return "blank";
    case FLASH_KV_FOREIGN: return "foreign";
    case FLASH_KV_VALID: return "valid";
    case FLASH_KV_TORN: return "torn";
    }
    return "?";
}

static void fill(uint8_t *buf, size_t len, uint8_t seed) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)(seed + i);
}

static void test_basic() {
    say("basic");

    CHECK(flash_kv_set(1, "hello", 5) == 0);
    CHECK(flash_kv_get(1, out, sizeof(out), 0) == 5);
    CHECK(memcmp(out, "hello", 5) == 0);
    CHECK(flash_kv_get(1, nullptr, 0, 0) == 5);

    CHECK(flash_kv_set(1, "bye", 3) == 0);
    CHECK(flash_kv_get(1, out, sizeof(out), 0) == 3);
    CHECK(memcmp(out, "bye", 3) == 0);

    CHECK(flash_kv_set(2, "two", 3) == 0);
    CHECK(flash_kv_erase_key(2) == 0);
    CHECK(flash_kv_get(2, out, sizeof(out), 0) == -ENOENT);
    CHECK(flash_kv_get(1, out, sizeof(out), 0) == 3);

    CHECK(flash_kv_get(1, out, 2, 0) == -EMSGSIZE);
    CHECK(flash_kv_erase_key(99) == 0);
    CHECK(flash_kv_get(99, out, sizeof(out), 0) == -ENOENT);
}

static void test_limits() {
    say("limits");
    memset(big, 0, sizeof(big));

    CHECK(flash_kv_set(3, big, sizeof(big)) == -EINVAL);
    CHECK(flash_kv_set(FLASH_KV_KEY_NONE, "x", 1) == -EINVAL);
    CHECK(flash_kv_set(3, nullptr, 1) == -EINVAL);

    fill(big, FLASH_KV_MAX_VALUE_LEN, 7);
    CHECK(flash_kv_set(3, big, FLASH_KV_MAX_VALUE_LEN) == 0);
    CHECK(flash_kv_get(3, out, sizeof(out), 0) == FLASH_KV_MAX_VALUE_LEN);
    CHECK(memcmp(out, big, FLASH_KV_MAX_VALUE_LEN) == 0);
}

// Enough writes to fill the sector several times, so compaction runs and must keep the other keys.
static void test_compaction() {
    say("compaction");

    CHECK(flash_kv_set(10, "keep10", 6) == 0);
    CHECK(flash_kv_set(11, "keep11", 6) == 0);

    for (int i = 0; i < 60; i++) {
        fill(value, sizeof(value), (uint8_t)i);
        CHECK(flash_kv_set(12, value, sizeof(value)) == 0);
        CHECK(flash_kv_get(12, out, sizeof(out), 0) == (int)sizeof(value));
        CHECK(memcmp(out, value, sizeof(value)) == 0);
    }

    CHECK(flash_kv_get(10, out, sizeof(out), 0) == 6);
    CHECK(memcmp(out, "keep10", 6) == 0);
    CHECK(flash_kv_get(11, out, sizeof(out), 0) == 6);
    CHECK(flash_kv_state() == FLASH_KV_VALID);
}

// Index of the first byte after the last programmed byte of the sector.
static uint32_t log_end() {
    uint8_t *sector = static_cast<uint8_t *>(malloc(FLASH_SECTOR_SIZE));
    uint32_t end    = 0;

    flash_read(KV_BASE, sector, FLASH_SECTOR_SIZE);
    for (uint32_t i = 0; i < FLASH_SECTOR_SIZE; i++) {
        if (sector[i] != 0xFF) end = i + 1;
    }
    free(sector);
    return (end + 3u) & ~3u;
}

static void test_torn() {
    say("torn");

    // Start from a short log so the raw writes below stay inside the sector.
    flash_erase_sector(KV_BASE);
    CHECK(flash_kv_set(20, "before", 6) == 0);

    // A record head that claims 8 bytes of value, with no value and no CRC behind it.
    const uint8_t torn_head[] = {0x21, 0x00, 0x08, 0x00};
    flash_write(KV_BASE + log_end(), torn_head, sizeof(torn_head));

    CHECK(flash_kv_state() == FLASH_KV_TORN);
    CHECK(flash_kv_get(20, out, sizeof(out), 0) == 6);
    CHECK(memcmp(out, "before", 6) == 0);

    CHECK(flash_kv_set(21, "after", 5) == 0);
    CHECK(flash_kv_state() == FLASH_KV_VALID);
    CHECK(flash_kv_get(20, out, sizeof(out), 0) == 6);
    CHECK(flash_kv_get(21, out, sizeof(out), 0) == 5);
    CHECK(memcmp(out, "after", 5) == 0);

    // Leftover bytes behind a clean end: the head looks blank but the tail is not.
    const uint8_t junk[] = {0x00, 0x00, 0x00, 0x00};
    flash_write(KV_BASE + log_end() + 64, junk, sizeof(junk));
    CHECK(flash_kv_state() == FLASH_KV_TORN);
    CHECK(flash_kv_set(22, "x", 1) == 0);
    CHECK(flash_kv_state() == FLASH_KV_VALID);
    CHECK(flash_kv_get(20, out, sizeof(out), 0) == 6);
}

static void test_foreign() {
    say("foreign");

    flash_erase_sector(KV_BASE);
    CHECK(flash_kv_state() == FLASH_KV_BLANK);
    CHECK(flash_kv_get(1, out, sizeof(out), 0) == -ENOENT);

    const uint8_t other[] = {'T', 'L', 'V', 0, 0x10, 0, 0, 0, 1, 2, 3, 4};
    flash_write(KV_BASE, other, sizeof(other));
    CHECK(flash_kv_state() == FLASH_KV_FOREIGN);
    CHECK(flash_kv_get(1, out, sizeof(out), 0) == -ENOENT);
    CHECK(flash_kv_erase_key(1) == 0);
    CHECK(flash_kv_state() == FLASH_KV_FOREIGN);

    CHECK(flash_kv_set(1, "mine", 4) == 0);
    CHECK(flash_kv_state() == FLASH_KV_VALID);
    CHECK(flash_kv_get(1, out, sizeof(out), 0) == 4);
    CHECK(memcmp(out, "mine", 4) == 0);
}

static void ignore_record(void *, const flash_kv_record_t *) {}

static uint32_t used_bytes() {
    uint32_t used;
    flash_kv_foreach(ignore_record, nullptr, &used);
    return used;
}

static void test_same_value() {
    say("same value");

    CHECK(flash_kv_set(30, "same", 4) == 0);
    const uint32_t used = used_bytes();

    CHECK(flash_kv_set(30, "same", 4) == 0);
    CHECK(used_bytes() == used);

    CHECK(flash_kv_set(30, "sama", 4) == 0);
    CHECK(used_bytes() > used);
    CHECK(flash_kv_get(30, out, sizeof(out), 0) == 4);
    CHECK(memcmp(out, "sama", 4) == 0);

    // The same bytes with another length are another value.
    const uint32_t before = used_bytes();
    CHECK(flash_kv_set(30, "sam", 3) == 0);
    CHECK(used_bytes() > before);
    CHECK(flash_kv_get(30, out, sizeof(out), 0) == 3);

    const uint32_t erased = used_bytes();
    CHECK(flash_kv_erase_key(31) == 0);
    CHECK(used_bytes() == erased);
}

static void test_exact_size() {
    say("exact size");

    const uint32_t number = 0x12345678;
    CHECK(flash_kv_set(40, &number, sizeof(number)) == 0);

    uint32_t read = 0;
    CHECK(flash_kv_get(40, &read, sizeof(read), sizeof(read)) == sizeof(read));
    CHECK(read == number);

    // A value of another length is refused and the buffer stays untouched.
    CHECK(flash_kv_set(41, "ab", 2) == 0);
    read = 0xAAAAAAAA;
    CHECK(flash_kv_get(41, &read, sizeof(read), sizeof(read)) == -EBADMSG);
    CHECK(read == 0xAAAAAAAA);

    // Without exact_size the same value reads fine.
    CHECK(flash_kv_get(41, out, sizeof(out), 0) == 2);

    // The length query is checked as well.
    CHECK(flash_kv_get(40, nullptr, 0, 2) == -EBADMSG);
    CHECK(flash_kv_get(40, nullptr, 0, sizeof(number)) == sizeof(number));

    CHECK(flash_kv_get(99, &read, sizeof(read), sizeof(read)) == -ENOENT);
}

static void test_typed() {
    say("typed");

    CHECK(flash_kv_set_u8(50, 200) == 0);
    uint8_t number = 0;
    CHECK(flash_kv_get_u8(50, &number) == 0);
    CHECK(number == 200);

    CHECK(flash_kv_set_bool(51, true) == 0);
    bool flag = false;
    CHECK(flash_kv_get_bool(51, &flag) == 0);
    CHECK(flag);
    CHECK(flash_kv_set_bool(51, false) == 0);
    CHECK(flash_kv_get_bool(51, &flag) == 0);
    CHECK(!flag);

    // A value of another size is refused and the destination keeps what it had.
    CHECK(flash_kv_set(52, "ab", 2) == 0);
    number = 7;
    CHECK(flash_kv_get_u8(52, &number) == -EBADMSG);
    CHECK(number == 7);
    flag = true;
    CHECK(flash_kv_get_bool(52, &flag) == -EBADMSG);
    CHECK(flag);
    CHECK(flash_kv_get_u8(99, &number) == -ENOENT);
    CHECK(number == 7);

    char text[8];
    CHECK(flash_kv_set_string(53, "wifi") == 0);
    CHECK(flash_kv_get_string(53, text, sizeof(text)) == 4);
    CHECK(strcmp(text, "wifi") == 0);

    // Too long for the buffer: refused, and the default in it stays.
    CHECK(flash_kv_set_string(54, "a-very-long-name") == 0);
    strcpy(text, "default");
    CHECK(flash_kv_get_string(54, text, sizeof(text)) == -EMSGSIZE);
    CHECK(strcmp(text, "default") == 0);

    // An empty string removes the key.
    CHECK(flash_kv_set_string(53, "") == 0);
    CHECK(flash_kv_get_string(53, text, sizeof(text)) == -ENOENT);
    CHECK(flash_kv_get_string(53, text, 0) == -EINVAL);
}

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);
    wdt_set(10000);
    wdt_up();

    flash_unprotect();
    printf("state at start: %s\r\n", state_name(flash_kv_state()));
    busy_wait_ms(PAUSE_MS);

    test_foreign();
    test_basic();
    test_limits();
    test_same_value();
    test_exact_size();
    test_typed();
    test_compaction();
    test_torn();

    printf("%s (%d failures)\r\n", g_failures ? "FAILED" : "PASSED", g_failures);

    busy_wait_ms(2000);
    wdt_reboot(100);
    return 0;
}
