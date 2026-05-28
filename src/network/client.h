#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>

/* Network client — owns the WebSocket lifecycle, retries, and the
 * authoritative tap uplink. Implements a small reconnection state machine
 * driven entirely from `client_tick()` so the main loop never needs to
 * inspect socket internals. */

typedef enum {
    CLIENT_DISCONNECTED = 0,
    CLIENT_CONNECTING,
    CLIENT_CONNECTED,
    CLIENT_AWAITING_INIT,
    CLIENT_RUNNING,
} ClientStatus;

typedef struct {
    size_t bytes_down;
    size_t bytes_up;
} conn_stats;

bool         connection_open(void);
void         connection_close(void);
bool         connection_is_open(void);
conn_stats   connection_get_stats(void);
ClientStatus client_status(void);

/* Drive the connection state machine forward by one render frame.
 * Idempotent — call once per main_loop iteration. */
void game_client_tick(void);

/* Signal that the authoritative init_data payload has arrived; the FSM
 * graduates from AWAITING_INIT to RUNNING. */
void client_on_init_received(void);

/** Send a pre-built binary uplink frame (BinWriter output). */
bool network_send_binary(const uint8_t* data, uint16_t len);

/** Convenience: tap event — builds and sends UPLINK_PLAYER_ACTION.
 *  `client_tick` + `sequence` MUST match the InputCommand that was
 *  pushed into the prediction replay buffer; the server echoes back the
 *  highest applied `sequence` in every snapshot so the client can drain
 *  the buffer. */
bool network_send_event_tap(Vector2 grid, uint32_t client_tick, uint32_t sequence);

/** Convenience: chat — builds and sends UPLINK_CHAT. */
bool network_send_chat(const char* to_id, const char* text);

#endif // CLIENT_H
