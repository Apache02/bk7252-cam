#pragma once

// A small DHCP server for one lwIP interface. DISCOVER and REQUEST are
// answered, RELEASE frees a lease, everything else is ignored.

#include <stdbool.h>

#include "lwip/netif.h"

// Listens on the interface's address; clients get the four addresses after it,
// starting at the interface's own plus one. Runs from any task.
bool dhcp_server_start(struct netif *netif);

// Stops listening and forgets every lease.
void dhcp_server_stop(void);
