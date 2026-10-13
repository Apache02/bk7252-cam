#pragma once

// A small HTTP server over lwIP sockets. Several worker tasks wait on one
// listening socket, so at least two requests are handled at the same time.
//
// The server reads the request head and hands it to one handler; what to answer
// is the handler's business. Request bodies are not read. Every connection is
// closed after its response.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The strings live in the worker's buffer and are valid until the handler returns.
typedef struct {
    const char *method; // "GET", "POST", ...
    const char *path;   // "/stream", without the query
    const char *query;  // what follows '?', or "" when there is none
} http_request_t;

typedef struct http_response http_response_t;

// Runs on a worker's stack and holds that worker until it returns, so a handler
// that streams occupies one worker for as long as the stream lasts. When it
// returns without sending anything, the server answers 500.
typedef void (*http_handler_t)(const http_request_t *request, http_response_t *response, void *user);

typedef struct {
    const char    *method;  // "GET", "POST", ...
    const char    *path;    // "/stream"
    http_handler_t handler; // NULL ends the table
    void          *user;    // passed to handler instead of the server's user
} http_route_t;

// A handler that looks the request up in a route table. Its user is an array of
// http_route_t that ends with an entry whose handler is NULL:
//
//     static const http_route_t routes[] = {
//         { "GET", "/",       serve_page,   NULL },
//         { "GET", "/stream", serve_stream, &camera },
//         { NULL,  NULL,      NULL,         NULL },
//     };
//     config.handler = http_default_handler;
//     config.user    = (void *)routes;
//
// Method and path must match exactly, and the query is ignored. An unknown path
// gets 404, a known path with another method gets 405. The table is read on every
// request and has to outlive the server.
void http_default_handler(const http_request_t *request, http_response_t *response, void *user);

typedef struct {
    uint16_t       port;        // usually 80
    uint8_t        workers;     // requests handled at once, at least 2
    uint16_t       stack_words; // stack of one worker; 0 for the default
    http_handler_t handler;     // called for every request, usually http_default_handler
    void          *user;        // passed to handler as is
} http_server_config_t;

// Opens the listening socket and starts the workers. Fails, leaving nothing open,
// on a configuration without a handler or with fewer than 2 workers, when a
// server already runs, and when memory or sockets run out.
//
// The handler is not called for a request that cannot be served: a request line
// that is not "METHOD /path ..." gets 400, one longer than 256 bytes gets 414,
// and a head longer than 2 KiB gets 431. A client that does not finish its head
// within a few seconds is dropped without an answer.
bool http_server_start(const http_server_config_t *config);

// Closes the listening socket and waits for the workers to exit; responses still
// in progress start failing. The server can be started again afterwards. Not for
// a handler to call: it would wait for itself.
void http_server_stop(void);

// A response is built in two steps. The handler first sets the status and the
// headers, which are only remembered. The first http_respond_* call sends them
// together with the body, and after that every setter returns false.
//
// Only the status, Content-Type and Content-Length are supported. Connection:
// close is always sent. Defaults: status 200, no Content-Type, Content-Length
// taken from the body.

bool http_response_set_status(http_response_t *response, int status);

// The string is not copied and has to outlive the handler, as a literal does.
bool http_response_set_content_type(http_response_t *response, const char *content_type);

// Declares the body length. The body may then be sent in several calls, up to
// that many bytes in total.
bool http_response_set_content_length(http_response_t *response, uint32_t length);

// Sends no Content-Length; the body ends when the connection closes. This is what
// a stream of unknown length needs.
bool http_response_omit_content_length(http_response_t *response);

// While true, sends are marked as followed by more data, so lwIP holds a short
// piece back and packs it with the next one. Turn it on around a run of small
// sends (a part header, a frame, a line end) and off for the last. Whatever is
// still held back goes out when the handler returns.
void http_response_set_more(http_response_t *response, bool more);

// Sends a body. Both functions return false once the client is gone or has not
// taken data within the send timeout, and the handler should then return.
//
// With no length declared, the length is taken from the first body, and that
// call is the only one allowed. After set_content_length() or
// omit_content_length() any number of calls may follow; a stream is
// omit_content_length(), a multipart/x-mixed-replace Content-Type, and the part
// boundaries and headers written by the handler.
bool http_respond_string(http_response_t *response, const char *body);

// body may be NULL when length is 0.
bool http_respond_binary(http_response_t *response, const void *body, uint32_t length);

#ifdef __cplusplus
}
#endif
