#pragma once

// A Telnet server that works as one more console. Whatever is written to it goes to
// the connected client, and whatever the client types can be read back, so the
// application can tie it to stdio and run its shell over the network.
//
// One client at a time. A second one connects at the TCP level but gets no answer until
// the first has left.
//
// The server speaks just enough Telnet for a client to stay in character-at-a-time
// mode: it announces that it echoes and suppresses go-ahead, and it strips the
// client's option negotiation from the input. There is no login.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t    port;        // usually 23
    uint16_t    stack_words; // stack of the server task; 0 for the default
    const char *greeting;    // sent to a client once it is set up, or NULL; it must outlive the server

    // Run on the server task, once a client is attached and once it has left, and must not
    // call telnet_server_stop(). Either may be NULL.
    void (*on_connect)(void *user);
    void (*on_disconnect)(void *user);
    void *user;
} telnet_server_config_t;

// Opens the listening socket and starts the server task. Fails, leaving nothing open,
// when a server already runs, and when memory or sockets run out.
bool telnet_server_start(const telnet_server_config_t *config);

// Drops the client, closes the listening socket and waits for the server task to exit;
// on_disconnect runs for a client that was attached. The server can be started again
// afterwards. Not for a callback to call: the task would wait for itself.
void telnet_server_stop(void);

// Drops the attached client once the output queued for it has gone out, and returns true.
// Returns false when no client is attached. The drop itself happens on the server task, so
// this is safe from any task, a callback excepted.
bool telnet_server_disconnect(void);

// Queues data for the client. When the queue is full it waits up to 100 ms for the
// client to take some, and what still does not fit is lost; the server task itself never
// waits. Does nothing when no client is attached, so it can be called unconditionally,
// and does nothing in an interrupt handler.
void telnet_server_write(const void *data, size_t len);

// One byte typed by the client, or -1 when there is none.
int telnet_server_read(void);

#ifdef __cplusplus
}
#endif
