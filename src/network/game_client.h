#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>

#include "input.h"

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

/** Send a pre-built binary uplink frame (BinWriter output). */
bool network_send_binary(const uint8_t* data, uint16_t len);

/** Convenience: chat — builds and sends UPLINK_CHAT. */
bool network_send_chat(const char* to_id, const char* text);

// TODO: should this live in a different file?
void replication_prepare_input(input_queue_t in_queue);

#endif // CLIENT_H
