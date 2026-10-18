#include "platform/http_server.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "lwip/sockets.h"

// #define DEBUG_NAME "httpd"
#include "debug.h"

#define MIN_WORKERS         2
#define DEFAULT_STACK_WORDS (configMINIMAL_STACK_SIZE * 8)

// A stream handler has to keep up with the camera, so the workers sit well above
// the idle task.
#define WORKER_PRIORITY (configMAX_PRIORITIES - 3)

// Connections that are accepted by lwIP but not yet taken by a worker. Each one
// holds a netconn, see MEMP_NUM_NETCONN.
#define LISTEN_BACKLOG 3

// How long a worker waits in accept() before looking at whether it was stopped.
#define POLL_MS 200

// Longest wait for a client to send or to take data.
#define IO_TIMEOUT_S 5

// The start of the request is kept for parsing; the rest of the head is read and
// dropped. Closing a socket with unread data makes lwIP send a reset, and
// browsers drop the response on it.
#define REQUEST_BUF_SIZE 256
#define HEAD_LIMIT       2048

#define HEADER_BUF_SIZE 192

struct http_server {
    http_server_config_t config;
    int                  listen_fd;
    volatile bool        stopping;

    // Every worker gives it once as its last act.
    SemaphoreHandle_t exited;
    uint8_t           started; // workers created and not yet collected
};

// One server at a time.
static struct http_server *s_server;

// What a handler's http_request_t points into. The request has to stay the first member.
struct http_conn {
    http_request_t request;
    int            fd;
    uint32_t       body_left; // body bytes not yet handed out

    // Body bytes that arrived in the same packet as the end of the head.
    uint8_t pending[128];
    uint8_t pending_len;
    uint8_t pending_pos;
};

typedef enum {
    LENGTH_AUTO,   // the first body decides it
    LENGTH_FIXED,  // set by the handler
    LENGTH_NONE,   // no header; the body ends when the connection closes
    LENGTH_SEALED, // AUTO after its one body
} length_mode_t;

struct http_response {
    const struct http_server *server;
    int                       fd;
    int                       status;
    const char               *content_type;
    const char               *content_encoding;
    length_mode_t             length_mode;
    uint32_t                  length;    // what Content-Length says
    uint32_t                  remaining; // what the handler may still send
    bool                      headers_sent;
    bool                      more;
    bool                      failed;
};

// ---- Response --------------------------------------------------------------

// clang-format off
static const char *reason(const int status) {
    switch (status) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Status";
    }
}
// clang-format on

__attribute__((format(printf, 4, 5))) static bool append(char *buf, size_t size, size_t *used, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + *used, size - *used, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= size - *used) return false;
    *used += (size_t)n;
    return true;
}

static bool send_raw(struct http_response *r, const void *data, size_t len, bool more) {
    if (r->failed || r->server->stopping) return false;
    if (len == 0) return true;

    if (lwip_send(r->fd, data, len, more ? MSG_MORE : 0) != (ssize_t)len) {
        r->failed = true;
        return false;
    }
    return true;
}

// `more` says that body bytes follow at once, so the head can share a packet with them.
static bool send_headers(struct http_response *r, bool more) {
    char   buf[HEADER_BUF_SIZE];
    size_t used = 0;

    bool ok = append(buf, sizeof(buf), &used, "HTTP/1.1 %d %s\r\n", r->status, reason(r->status));
    if (ok && r->content_type) ok = append(buf, sizeof(buf), &used, "Content-Type: %s\r\n", r->content_type);
    if (ok && r->content_encoding) ok = append(buf, sizeof(buf), &used, "Content-Encoding: %s\r\n", r->content_encoding);
    if (ok && (r->length_mode == LENGTH_AUTO || r->length_mode == LENGTH_FIXED)) {
        ok = append(buf, sizeof(buf), &used, "Content-Length: %lu\r\n", (unsigned long)r->length);
    }
    if (ok) ok = append(buf, sizeof(buf), &used, "Connection: close\r\n\r\n");

    r->headers_sent = true;
    if (!ok) {
        r->failed = true;
        return false;
    }
    return send_raw(r, buf, used, more);
}

bool http_response_set_status(http_response_t *r, int status) {
    if (r->headers_sent || status < 100 || status > 599) return false;
    r->status = status;
    return true;
}

bool http_response_set_content_type(http_response_t *r, const char *content_type) {
    if (r->headers_sent) return false;
    r->content_type = content_type;
    return true;
}

bool http_response_set_content_encoding(http_response_t *r, const char *content_encoding) {
    if (r->headers_sent) return false;
    r->content_encoding = content_encoding;
    return true;
}

bool http_response_set_content_length(http_response_t *r, uint32_t length) {
    if (r->headers_sent) return false;
    r->length_mode = LENGTH_FIXED;
    r->length      = length;
    r->remaining   = length;
    return true;
}

bool http_response_omit_content_length(http_response_t *r) {
    if (r->headers_sent) return false;
    r->length_mode = LENGTH_NONE;
    return true;
}

void http_response_set_more(http_response_t *r, bool more) { r->more = more; }

bool http_respond_binary(http_response_t *r, const void *body, uint32_t length) {
    if (r->failed || r->server->stopping || r->length_mode == LENGTH_SEALED) return false;
    if (!body && length > 0) return false;

    if (r->length_mode == LENGTH_AUTO) {
        r->length    = length;
        r->remaining = length;
    }
    if (r->length_mode != LENGTH_NONE && length > r->remaining) return false;

    if (!r->headers_sent && !send_headers(r, length > 0 || r->more)) return false;
    if (r->length_mode != LENGTH_NONE) r->remaining -= length;

    const bool ok = send_raw(r, body, length, r->more);
    if (r->length_mode == LENGTH_AUTO) r->length_mode = LENGTH_SEALED;
    return ok;
}

bool http_respond_string(http_response_t *r, const char *body) {
    return http_respond_binary(r, body, body ? (uint32_t)strlen(body) : 0);
}

// An empty reply with just a status, over whatever the handler had set up.
static void reject(struct http_response *r, int status) {
    if (r->headers_sent) return;

    r->status           = status;
    r->content_type     = NULL;
    r->content_encoding = NULL;
    r->length_mode      = LENGTH_AUTO;
    r->more             = false;
    http_respond_binary(r, NULL, 0);
}

// ---- Requests --------------------------------------------------------------

typedef enum { HEAD_OK, HEAD_LOST, HEAD_TOO_LARGE } head_result_t;

// Takes the one header the server needs. `line` is a header line without its '\n', possibly cut short.
static void scan_header(struct http_conn *conn, const char *line) {
    static const char NAME[] = "content-length:";

    if (strncasecmp(line, NAME, sizeof(NAME) - 1) != 0) return;
    conn->request.content_length = (uint32_t)strtoul(line + sizeof(NAME) - 1, NULL, 10);
}

// Reads up to the blank line that ends the head and keeps the start of it in buf,
// always NUL-terminated. Bytes after the blank line go to conn->pending.
static head_result_t read_head(struct http_conn *conn, char *buf, size_t size, size_t *kept_out, bool *truncated) {
    size_t   kept  = 0;
    size_t   total = 0;
    unsigned seen  = 0; // how much of "\r\n\r\n" has matched so far
    char     line[32];
    size_t   line_len = 0;

    *truncated = false;

    for (;;) {
        char      chunk[128];
        const int n = lwip_recv(conn->fd, chunk, sizeof(chunk), 0);
        if (n <= 0) return HEAD_LOST;

        for (int i = 0; i < n; i++) {
            if (kept < size - 1) {
                buf[kept++] = chunk[i];
            } else {
                *truncated = true;
            }

            if (chunk[i] == '\n') {
                line[line_len] = '\0';
                scan_header(conn, line);
                line_len = 0;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = chunk[i];
            }

            const char expected = (seen % 2 == 0) ? '\r' : '\n';
            if (chunk[i] == expected) {
                if (++seen == 4) {
                    buf[kept] = '\0';
                    *kept_out = kept;

                    conn->pending_len = (uint8_t)(n - i - 1);
                    memcpy(conn->pending, chunk + i + 1, conn->pending_len);
                    return HEAD_OK;
                }
            } else {
                seen = (chunk[i] == '\r') ? 1 : 0;
            }
        }

        total += (size_t)n;
        if (total > HEAD_LIMIT) return HEAD_TOO_LARGE;
    }
}

// Splits "METHOD /path?query HTTP/1.1" in place. Returns 0, or the status to answer with.
static int parse_request_line(char *buf, size_t kept, bool truncated, http_request_t *request) {
    size_t end = 0;
    while (end < kept && buf[end] != '\r' && buf[end] != '\n') end++;
    if (end == kept) return truncated ? 414 : 400;
    buf[end] = '\0';

    char *space = strchr(buf, ' ');
    if (!space || space == buf) return 400;
    *space = '\0';

    char *path = space + 1;
    if (path[0] != '/') return 400;

    // The version after the path is not looked at.
    space = strchr(path, ' ');
    if (space) *space = '\0';

    static const char NO_QUERY[] = "";
    char             *query      = strchr(path, '?');
    if (query) *query++ = '\0';

    request->method = buf;
    request->path   = path;
    request->query  = query ? query : NO_QUERY;
    return 0;
}

int http_request_read_body(const http_request_t *request, void *buf, size_t size) {
    struct http_conn *conn = (struct http_conn *)request;
    uint8_t          *out  = buf;
    size_t            done = 0;

    while (done < size && conn->body_left > 0) {
        size_t want = size - done;
        if (want > conn->body_left) want = conn->body_left;

        size_t got;
        if (conn->pending_pos < conn->pending_len) {
            got = conn->pending_len - conn->pending_pos;
            if (got > want) got = want;
            memcpy(out + done, conn->pending + conn->pending_pos, got);
            conn->pending_pos += (uint8_t)got;
        } else {
            const int n = lwip_recv(conn->fd, out + done, want, 0);
            if (n <= 0) return -1;
            got = (size_t)n;
        }

        done += got;
        conn->body_left -= (uint32_t)got;
    }

    return (int)done;
}

static void serve(const struct http_server *server, int fd) {
    const struct timeval timeout = {.tv_sec = IO_TIMEOUT_S, .tv_usec = 0};
    lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct http_response response = {
        .server      = server,
        .fd          = fd,
        .status      = 200,
        .length_mode = LENGTH_AUTO,
    };

    char             head[REQUEST_BUF_SIZE];
    size_t           kept      = 0;
    bool             truncated = false;
    struct http_conn conn      = {.fd = fd};

    switch (read_head(&conn, head, sizeof(head), &kept, &truncated)) {
        case HEAD_LOST:
            break;

        case HEAD_TOO_LARGE:
            reject(&response, 431);
            break;

        case HEAD_OK: {
            const int error = parse_request_line(head, kept, truncated, &conn.request);
            if (error) {
                reject(&response, error);
                break;
            }

            conn.body_left = conn.request.content_length;
            server->config.handler(&conn.request, &response, server->config.user);
            reject(&response, 500); // no effect once anything was sent
            break;
        }
    }

    // Closing also pushes out whatever a trailing MSG_MORE still holds back.
    lwip_close(fd);
}

static void worker_task(void *arg) {
    struct http_server *server = arg;

    while (!server->stopping) {
        const int fd = lwip_accept(server->listen_fd, NULL, NULL);
        if (fd < 0) {
            // Mostly the poll timeout. The pause keeps a dead socket from
            // becoming a busy loop; connections wait in lwIP meanwhile.
            vTaskDelay(1);
            continue;
        }
        serve(server, fd);
    }

    // Last use of the server: the stopper frees it as soon as it has this.
    xSemaphoreGive(server->exited);
    vTaskDelete(NULL);
}

// ---- Start and stop --------------------------------------------------------

static int open_listen_socket(uint16_t port) {
    const int fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;

    const int            reuse   = 1;
    const struct timeval timeout = {.tv_sec = 0, .tv_usec = POLL_MS * 1000};
    struct sockaddr_in   any     = {
              .sin_len    = sizeof(any),
              .sin_family = AF_INET,
              .sin_port   = lwip_htons(port),
              .sin_addr   = {.s_addr = PP_HTONL(INADDR_ANY)},
              .sin_zero   = {0},
    };

    if (lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        lwip_bind(fd, (struct sockaddr *)&any, sizeof(any)) < 0 || lwip_listen(fd, LISTEN_BACKLOG) < 0) {
        lwip_close(fd);
        return -1;
    }
    return fd;
}

static void destroy_server(struct http_server *server) {
    if (server->listen_fd >= 0) lwip_close(server->listen_fd);
    if (server->exited) vSemaphoreDelete(server->exited);
    vPortFree(server);
}

const char *http_get_status_label(const int status) { return reason(status); }

bool http_server_start(const http_server_config_t *config) {
    if (!config || !config->handler || config->workers < MIN_WORKERS || s_server) return false;

    struct http_server *server = pvPortMalloc(sizeof(*server));
    if (!server) {
        LOG_W("no memory for the HTTP server");
        return false;
    }

    memset(server, 0, sizeof(*server));
    server->config = *config;
    if (server->config.stack_words == 0) server->config.stack_words = DEFAULT_STACK_WORDS;
    server->exited    = xSemaphoreCreateCounting(config->workers, 0);
    server->listen_fd = open_listen_socket(config->port);

    if (!server->exited || server->listen_fd < 0) {
        LOG_W("could not listen on port %u", (unsigned)config->port);
        destroy_server(server);
        return false;
    }

    s_server = server;
    for (uint8_t i = 0; i < config->workers; i++) {
        if (xTaskCreate(worker_task, "http", server->config.stack_words, server, WORKER_PRIORITY, NULL) != pdPASS) {
            LOG_W("no memory for HTTP worker %u", (unsigned)i);
            http_server_stop();
            return false;
        }
        server->started++;
    }

    return true;
}

void http_server_stop(void) {
    struct http_server *server = s_server;
    if (!server) return;

    // A worker sees the flag within one poll interval, or when its current send
    // or receive times out. A worker that never reports leaves the server
    // allocated rather than freed under it.
    server->stopping = true;
    while (server->started > 0) {
        if (xSemaphoreTake(server->exited, pdMS_TO_TICKS((IO_TIMEOUT_S + 2) * 1000)) != pdTRUE) {
            LOG_W("%u HTTP workers did not stop", (unsigned)server->started);
            return;
        }
        server->started--;
    }

    destroy_server(server);
    s_server = NULL;
}
