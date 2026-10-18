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

/* Keeps the resume ticket the server issued for this account session: it
 * lives in memory only and rejoins the session after a dropped connection. */
void game_client_hold_resume(const char* ticket);

/* A session_end the server sent: a resume that must wait tries again, a
 * restart of an account session resumes, anything else goes to the gate. */
void game_client_on_session_end(const char* reason, const char* travel_ticket);

/* Opens the world config_enter_world named once the current socket has closed. */
void game_client_enter_world(void);

/** Pack a message and send it. Takes ownership of msg. Only the auth message
 * and client_event_flush call it; every input goes through client_event. */
bool network_send(cJSON* msg);

#endif // CLIENT_H
