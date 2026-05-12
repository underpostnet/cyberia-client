#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <stdbool.h>
#include <cJSON.h>
#include <raylib.h>

typedef struct {
    size_t bytes_down;
    size_t bytes_up;
} conn_stats;

bool connection_open(void);
void connection_close(void);
bool connection_is_open(void);
conn_stats connection_get_stats(void);

/** sends text to the server */
bool network_send_text(const char* text);

/** sends a valid json object to the server */
bool network_send(cJSON* json_obj);

bool network_send_event_tap(Vector2 grid);

#endif // CLIENT_H
