#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>

typedef struct {
    size_t bytes_down;
    size_t bytes_up;
} conn_stats;

bool connection_open(void);
void connection_close(void);
bool connection_is_open(void);
/** Returns true if a socket handle exists (CONNECTING or CONNECTED).
 *  Use this to avoid reconnecting while an initial attempt is in flight. */
bool connection_is_active(void);
conn_stats connection_get_stats(void);

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
