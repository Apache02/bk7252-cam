#pragma once

#include "http_server.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

void http_default_static_page(const http_request_t *request, http_response_t *response, void *user);

void http_default_status(const http_request_t *request, http_response_t *response, int status);

void http_default_404(const http_request_t *request, http_response_t *response, void *user);


#ifdef __cplusplus
}
#endif
