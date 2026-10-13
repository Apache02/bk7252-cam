#include "platform/http_server.h"

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

    http_response_set_status(response, path_matched ? 405 : 404);
    http_respond_string(response, "");
}
