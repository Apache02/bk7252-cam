// ICMP echo, from the shell. Numeric addresses only: DNS is off in this
// build's lwipopts.h, so there is no name to resolve.

#include <new>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "shell/commands_wifi.h"
#include "shell/Parser.h"

#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/sockets.h"


#define PING_ID          0xAFAF
#define PING_DATA_SIZE   32
#define PING_PACKET_SIZE (sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE)


static void usage(const char *command) {
    printf("usage: %s <ip> [-c N] [-W ms]\r\n", command);
    printf("  -c    how many echoes to send (default 4)\r\n");
    printf("  -W    how long to wait for each reply, in ms (default 1000)\r\n");
    printf("\r\n  <ip> is a numeric address; there is no DNS here.\r\n");
}


class EchoRequest {
    const uint16_t data_size;
    void          *ptr = nullptr;

  public:
    static inline uint16_t header_size() { return sizeof(struct icmp_echo_hdr); }

    uint16_t total_size() const { return header_size() + data_size; }

    void *get() const { return ptr; }

  private:
    void *get_additional() const { return static_cast<void *>(static_cast<char *>(ptr) + header_size()); }

  public:
    EchoRequest(const uint16_t size) : data_size(size) {
        ptr = new (std::nothrow) char[total_size()];
        assert(ptr != nullptr);
    }

    ~EchoRequest() {
        if (ptr) delete[] static_cast<char *>(ptr);
        ptr = nullptr;
    }

    void fill(uint16_t seq) {
        struct icmp_echo_hdr *echo = static_cast<struct icmp_echo_hdr *>(ptr);
        ICMPH_TYPE_SET(echo, ICMP_ECHO);
        ICMPH_CODE_SET(echo, 0);
        echo->chksum = 0;
        echo->id     = PING_ID;
        echo->seqno  = lwip_htons(seq);

        /* fill the additional data buffer with some data */
        uint8_t *additional = static_cast<uint8_t *>(get_additional());
        for (size_t i = 0; i < data_size; i++) {
            additional[i] = static_cast<char>(i);
        }

        echo->chksum = inet_chksum(echo, total_size());
    }
};


// Waits out one reply, or the timeout, and reports what came back.
// Relies on SO_RCVTIMEO (set on fd once, before the ping loop) to give up.
static bool wait_for_reply(int fd, uint16_t seq, uint32_t sent_at) {
    uint8_t buf[sizeof(struct ip_hdr) + PING_PACKET_SIZE];

    for (;;) {
        struct sockaddr_in from      = {};
        socklen_t          from_size = sizeof(from);
        int                len       = lwip_recvfrom(fd, buf, sizeof(buf), 0, reinterpret_cast<struct sockaddr *>(&from), &from_size);
        if (len <= 0) return false;
        if (len < static_cast<int>(sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) continue;

        auto *ip   = reinterpret_cast<struct ip_hdr *>(buf);
        auto *echo = reinterpret_cast<struct icmp_echo_hdr *>(buf + IPH_HL_BYTES(ip));
        if (echo->id != PING_ID || echo->seqno != lwip_htons(seq)) continue;

        uint32_t rtt_ms = pdTICKS_TO_MS(xTaskGetTickCount()) - sent_at;
        printf("%d bytes from %s: seq=%u time=%lu ms\r\n", len, inet_ntoa(from.sin_addr), static_cast<unsigned>(seq),
               static_cast<unsigned long>(rtt_ms));
        return true;
    }
}

int command_ping(int argc, const char *argv[]) {
    unsigned long count      = 4;
    unsigned long timeout_ms = 1000;
    const char   *target     = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "-c") == 0) {
            count = (++i < argc) ? take_int(argv[i]).ok_or(0) : 0;
            if (count < 1) {
                printf("error: Invalid value -c\r\n");
                return -1;
            }
        } else if (strcmp(argv[i], "-W") == 0) {
            timeout_ms = (++i < argc) ? take_int(argv[i]).ok_or(0) : 0;
            if (timeout_ms < 1) {
                printf("error: Invalid value -W\r\n");
                return -1;
            }
        } else if (!target) {
            target = argv[i];
        } else {
            printf("unknown option: %s\r\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }

    if (!target) {
        usage(argv[0]);
        return -1;
    }

    struct sockaddr_in to          = {};
    struct sockaddr   *to_sockaddr = reinterpret_cast<struct sockaddr *>(&to);
    to.sin_len                     = sizeof(to);
    to.sin_family                  = AF_INET;
    if (!inet_aton(target, &to.sin_addr)) {
        printf("not a numeric address: %s\r\n", target);
        return -1;
    }

    int s = lwip_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s < 0) {
        printf("could not open a raw socket\r\n");
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("setting receive timeout failed\r\n");
        lwip_close(s);
        return -1;
    }

    unsigned   sent      = 0;
    unsigned   received  = 0;
    TickType_t xStartTime = xTaskGetTickCount();
    TickType_t xSendTime = 0;
    for (unsigned seq = 1; seq <= count; seq++) {
        sent++;
        vTaskDelayUntil(&xSendTime, pdMS_TO_TICKS(timeout_ms));

        EchoRequest packet(PING_DATA_SIZE);
        packet.fill(seq);

        xSendTime = xTaskGetTickCount();
        if (lwip_sendto(s, packet.get(), packet.total_size(), 0, to_sockaddr, sizeof(to)) < 0) {
            printf("send failed\r\n");
            continue;
        }

        if (wait_for_reply(s, seq, pdTICKS_TO_MS(xSendTime))) {
            received++;
        } else {
            printf("seq=%u timed out\r\n", seq);
        }

        if (getchar() == 0x03) break; // Ctrl+C
    }

    lwip_close(s);

    uint32_t total_ms = pdTICKS_TO_MS(xTaskGetTickCount() - xStartTime);
    unsigned lost_pct = sent ? 100 * (sent - received) / sent : 0;
    printf("\r\n%u transmitted, %u received, %u%% lost, time %lums\r\n", sent, received, lost_pct, total_ms);
    return received ? 0 : -1;
}
