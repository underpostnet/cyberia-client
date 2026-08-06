#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>

typedef struct {
    size_t bytes_down;
    size_t bytes_up;
} conn_stats;

bool         connection_open(void);
void         connection_close(void);
bool         connection_is_open(void);
conn_stats   connection_get_stats(void);

/* Drive the connection state machine forward by one render frame.
 * Idempotent — call once per main_loop iteration. */
void game_client_on_tick(void);

/* Signal that the authoritative init_data payload has arrived; the FSM
 * graduates from AWAITING_INIT to RUNNING. */
void client_on_init_received(void);

/* The player tapped Start on the loading screen: release the server's
 * "loading" freeze. Reconnect joins re-release automatically. */
void client_confirm_loading_done(void);

/** Pack a message and send it. Takes ownership of msg. */
bool network_send(cJSON* msg);

/** Convenience: build and send a chat message. */
bool network_send_chat(const char* to_id, const char* text);

#endif // CLIENT_H
