#include "net.h"
#include "lwip/tcpip.h"
#include "hardware/random.h"

void net_init(void) {
    trng_enable();
    tcpip_init(NULL, NULL);
}
