// Name lookup, from the shell, through lwIP's resolver and the servers the DHCP
// lease gave. lwIP keeps one address per name, so one address is what prints.

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "shell/commands_wifi.h"

#include "lwip/api.h"
#include "lwip/dns.h"
#include "lwip/inet.h"

// lwIP gives up on a server after retries 1 + 1 + 2 + 3 s apart. A failure sooner
// than that is the server's answer, not silence.
#define DNS_GIVE_UP_MS 7000

static void usage(const char *command) {
    printf("usage: %s <name>\r\n", command);
    printf("\r\n  Asks the DNS server from the DHCP lease for the name's IPv4 address.\r\n");
}

int command_nslookup(int argc, const char *argv[]) {
    if (argc != 2 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return argc == 2 ? 0 : -1;
    }

    const char *name = argv[1];

    bool has_server = false;
    for (u8_t i = 0; i < DNS_MAX_SERVERS; i++) {
        const ip_addr_t *server = dns_getserver(i);
        if (ip_addr_isany(server)) continue;
        printf("server  %s\r\n", ipaddr_ntoa(server));
        has_server = true;
    }
    if (!has_server) {
        printf("no DNS server: there is no DHCP lease yet, see wifi info\r\n");
        return -1;
    }

    // Blocks until the resolver answers or runs out of retries; it always does one.
    ip_addr_t  addr     = {};
    TickType_t start    = xTaskGetTickCount();
    err_t      err      = netconn_gethostbyname(name, &addr);
    uint32_t   spent_ms = pdTICKS_TO_MS(xTaskGetTickCount() - start);

    if (err == ERR_OK) {
        printf("%s  %s  (%lu ms)\r\n", name, ipaddr_ntoa(&addr), static_cast<unsigned long>(spent_ms));
        return 0;
    }

    if (err == ERR_ARG) {
        printf("not a valid name: %s\r\n", name);
    } else if (err != ERR_VAL) {
        printf("%s: lookup failed, lwIP error %d\r\n", name, static_cast<int>(err));
    } else if (spent_ms < DNS_GIVE_UP_MS) {
        printf("%s: no such name (%lu ms)\r\n", name, static_cast<unsigned long>(spent_ms));
    } else {
        printf("%s: no answer from the server (%lu ms)\r\n", name, static_cast<unsigned long>(spent_ms));
    }
    return -1;
}
