#ifndef CYBERIA_INPUT_COMMAND_H
#define CYBERIA_INPUT_COMMAND_H

#include <stdint.h>

/* The unit of client-to-server input. Carries what the server needs to apply
 * the action, plus the pair the client needs for reconciliation:
 * client_tick, stamped from session_server_tick_estimate(), and sequence,
 * from session_next_input_sequence(). The server echoes the highest applied
 * sequence in every snapshot header.
 *
 * This module builds and serialises commands only. It never renders and
 * never changes world state. */

/* Kind values mirror the binary uplink opcodes of the WS wire format, so
 * logs, traces, and encoders share one set of constants. */
typedef enum {
    INPUT_KIND_PLAYER_ACTION   = 0x11,
} input_kind_t;

typedef uint32_t cyberia_tick_t;
typedef uint32_t cyberia_input_seq_t;

typedef struct {
    input_kind_t           kind;
    cyberia_tick_t         client_tick;
    cyberia_input_seq_t    sequence;
    float                  target_x;
    float                  target_y;
} input_command_t;

/* Build a PLAYER_ACTION command at grid (x, y). Stamps the tick and the
 * sequence from the session module. */
input_command_t input_command_build_tap(float grid_x, float grid_y);

#endif /* CYBERIA_INPUT_COMMAND_H */
