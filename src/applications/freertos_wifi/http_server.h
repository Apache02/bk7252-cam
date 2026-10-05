#pragma once

// Starts the HTTP server task. Safe to call before the interface has an
// address: the listening socket just sits in accept() until DHCP finishes.
void http_server_start();
