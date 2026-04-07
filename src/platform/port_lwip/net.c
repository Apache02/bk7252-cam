#include "net.h"
#include "lwip/tcpip.h"
#include "hardware/random.h"
#include <stdio.h>


static void done(__unused void *arg) {
    // printf("net init done\r\n");
}

void net_init(void) {
    trng_enable();
    tcpip_init(done, NULL);
}
