#include "platform/http_server_defaults.h"

#include <string.h>


void http_default_handler(const http_request_t *request, http_response_t *response, void *user) {
    const http_route_t *routes       = user;
    bool                path_matched = false;

    for (const http_route_t *route = routes; route && route->handler; route++) {
        if (strcmp(route->path, request->path) != 0) continue;

        if (strcmp(route->method, request->method) == 0) {
            route->handler(request, response, route->user);
            return;
        }
        path_matched = true;
    }

    http_default_status(request, response, path_matched ? 405 : 404);
}

void http_default_static_page(__unused const http_request_t *request, http_response_t *response, void *user) {
    const char *page = user;
    http_response_set_status(response, 200);
    http_response_set_content_type(response, "text/html");
    http_respond_string(response, page);
}

void http_default_status(__unused const http_request_t *request, http_response_t *response, const int status) {
    const char *status_label = http_get_status_label(status);
    http_response_set_status(response, status);
    http_response_set_content_type(response, "text/plain");
    http_respond_string(response, status_label);
}

void http_default_404(__unused const http_request_t *request, http_response_t *response, void *user) {
    http_default_status(request, response, 404);
}

