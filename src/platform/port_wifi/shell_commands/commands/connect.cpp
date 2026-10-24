// Outgoing TCP, from the shell: connect, send one text, print what comes back.
// Numeric addresses only; nslookup turns a name into one.

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "shell/commands_wifi.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"


#define CONNECT_TIMEOUT_MS 10000
#define SEND_TIMEOUT_MS    5000
#define IDLE_TIMEOUT_MS    5000
#define TOTAL_TIMEOUT_MS   30000
// How often a wait looks for Ctrl+C and the limits.
#define POLL_MS 250

#define CTRL_C 0x03


static void usage(const char *command) {
    printf("usage: %s <ip>:<port> [text]\r\n", command);
    printf("\r\n  Connects, sends text if given, and prints what comes back until the peer\r\n");
    printf("  closes, nothing arrives for %u s, or %u s pass.\r\n", IDLE_TIMEOUT_MS / 1000, TOTAL_TIMEOUT_MS / 1000);
    printf("  text is the rest of the line; \\r \\n \\t \\\\ are escapes.\r\n");
    printf("  <ip> is a numeric address; see nslookup.\r\n");
}

static bool parse_target(const char *target, struct sockaddr_in *to) {
    const char *colon = strrchr(target, ':');
    if (!colon) return false;

    char   ip[16];
    size_t ip_len = static_cast<size_t>(colon - target);
    if (ip_len == 0 || ip_len >= sizeof(ip)) return false;
    memcpy(ip, target, ip_len);
    ip[ip_len] = '\0';

    char         *end  = nullptr;
    unsigned long port = strtoul(colon + 1, &end, 10);
    if (end == colon + 1 || *end || port < 1 || port > 0xFFFF) return false;

    to->sin_len    = sizeof(*to);
    to->sin_family = AF_INET;
    to->sin_port   = lwip_htons(static_cast<uint16_t>(port));
    return inet_aton(ip, &to->sin_addr) != 0;
}

// What the letter after a backslash stands for; 0 keeps the backslash as is.
static char unescape(char letter) {
    switch (letter) {
        case 'r':
            return '\r';
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case '\\':
            return '\\';
        default:
            return 0;
    }
}

// The shell has split the text on spaces; it is joined back with one space each.
// Returns nullptr when there is no text or no memory for it.
static char *join_text(int argc, const char *argv[], size_t *len) {
    size_t total = 0;
    for (int i = 0; i < argc; i++) total += strlen(argv[i]) + 1;
    if (total == 0) return nullptr;

    char *text = new (std::nothrow) char[total];
    if (!text) return nullptr;

    char *out = text;
    for (int i = 0; i < argc; i++) {
        if (i > 0) *out++ = ' ';
        for (const char *in = argv[i]; *in; in++) {
            char c = *in;
            if (c == '\\' && unescape(in[1])) c = unescape(*++in);
            *out++ = c;
        }
    }
    *len = static_cast<size_t>(out - text);
    return text;
}

// Text as is; any other byte as \xHH.
static void print_received(const uint8_t *data, int len) {
    char out[128];
    int  used = 0;

    for (int i = 0; i < len; i++) {
        const uint8_t c = data[i];
        if ((c >= 0x20 && c < 0x7F) || c == '\r' || c == '\n' || c == '\t') {
            out[used++] = static_cast<char>(c);
        } else {
            used += snprintf(out + used, sizeof(out) - used, "\\x%02x", c);
        }
        if (used > static_cast<int>(sizeof(out)) - 5) {
            printf("%.*s", used, out);
            used = 0;
        }
    }
    if (used) printf("%.*s", used, out);
}

static const char *connect_failure(int err) {
    switch (err) {
        case ECONNRESET:
        case ECONNREFUSED:
            return "refused";
        case EHOSTUNREACH:
            return "no route to the host";
        case ECONNABORTED:
            return "given up on, no answer";
        case ENOMEM:
        case ENOBUFS:
            return "out of memory";
        default:
            return "failed";
    }
}

static const char *receive_failure(int err) {
    switch (err) {
        case ECONNRESET:
            return "reset by the peer";
        case ECONNABORTED:
            return "aborted";
        default:
            return "receive failed";
    }
}

// Waits out a non-blocking connect. Returns 0 when connected, the errno it
// failed with, or ETIMEDOUT; Ctrl+C gives EINTR.
static int wait_connected(int s) {
    TickType_t start = xTaskGetTickCount();

    while (pdTICKS_TO_MS(xTaskGetTickCount() - start) < CONNECT_TIMEOUT_MS) {
        fd_set writable, failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(s, &writable);
        FD_SET(s, &failed);
        struct timeval wait = {.tv_sec = 0, .tv_usec = POLL_MS * 1000};

        int ready = lwip_select(s + 1, nullptr, &writable, &failed, &wait);
        if (ready < 0) return errno;
        if (ready > 0) {
            int       err     = 0;
            socklen_t err_len = sizeof(err);
            if (lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &err_len) < 0) return errno;
            return err;
        }

        if (getchar() == CTRL_C) return EINTR;
    }
    return ETIMEDOUT;
}

static bool send_all(int s, const char *data, size_t len, size_t *sent) {
    while (*sent < len) {
        int n = lwip_send(s, data + *sent, len - *sent, 0);
        if (n <= 0) return false;
        *sent += static_cast<size_t>(n);
    }
    return true;
}

// Prints until the connection ends one way or another; says which.
static const char *receive_all(int s, size_t *received) {
    uint8_t    buf[128];
    TickType_t start   = xTaskGetTickCount();
    TickType_t last_rx = start;

    for (;;) {
        int n = lwip_recv(s, buf, sizeof(buf), 0);
        if (n > 0) {
            print_received(buf, n);
            *received += static_cast<size_t>(n);
            last_rx = xTaskGetTickCount();
        } else if (n == 0) {
            return "closed by the peer";
        } else if (errno != EWOULDBLOCK) {
            return receive_failure(errno);
        }

        TickType_t now = xTaskGetTickCount();
        if (pdTICKS_TO_MS(now - last_rx) >= IDLE_TIMEOUT_MS) return "nothing more arrived";
        if (pdTICKS_TO_MS(now - start) >= TOTAL_TIMEOUT_MS) return "time limit reached";
        if (getchar() == CTRL_C) return "interrupted";
    }
}

int command_connect(int argc, const char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return argc < 2 ? -1 : 0;
    }

    struct sockaddr_in to = {};
    if (!parse_target(argv[1], &to)) {
        printf("wants <ip>:<port> with a numeric address, not %s\r\n", argv[1]);
        return -1;
    }

    size_t text_len = 0;
    char  *text     = join_text(argc - 2, argv + 2, &text_len);
    if (argc > 2 && !text) {
        printf("no memory for the text\r\n");
        return -1;
    }

    int s = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        printf("could not open a socket\r\n");
        delete[] text;
        return -1;
    }

    // Non-blocking only for the connect: a blocking one waits out every SYN retry.
    int on  = 1;
    int err = 0;
    lwip_ioctl(s, FIONBIO, &on);
    if (lwip_connect(s, reinterpret_cast<struct sockaddr *>(&to), sizeof(to)) < 0) {
        err = errno == EINPROGRESS ? wait_connected(s) : errno;
    }

    if (err) {
        if (err == ETIMEDOUT) {
            printf("connect to %s: timed out after %u s\r\n", argv[1], CONNECT_TIMEOUT_MS / 1000);
        } else if (err == EINTR) {
            printf("connect to %s: interrupted\r\n", argv[1]);
        } else {
            printf("connect to %s: %s\r\n", argv[1], connect_failure(err));
        }
        lwip_close(s);
        delete[] text;
        return -1;
    }

    int off = 0;
    lwip_ioctl(s, FIONBIO, &off);
    const struct timeval recv_timeout = {.tv_sec = 0, .tv_usec = POLL_MS * 1000};
    const struct timeval send_timeout = {.tv_sec = SEND_TIMEOUT_MS / 1000, .tv_usec = 0};
    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    lwip_setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

    printf("connected to %s\r\n", argv[1]);

    size_t      sent     = 0;
    size_t      received = 0;
    const char *ending   = nullptr;
    if (text && !send_all(s, text, text_len, &sent)) ending = "send failed";
    delete[] text;

    if (!ending) ending = receive_all(s, &received);
    lwip_close(s);

    printf("\r\n%lu bytes sent, %lu received, %s\r\n", static_cast<unsigned long>(sent),
           static_cast<unsigned long>(received), ending);
    return 0;
}
