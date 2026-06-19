#include <stdio.h>
#include <string.h>
#include "platform/stdio.h"
#include "hardware/wdt.h"

extern "C" void test_sha();
extern "C" void test_aes();

int main() {
    wdt_down();

    platform_stdio_init();

    // disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    wdt_set(10000);

    test_sha();
    test_aes();

    return 0;
}
