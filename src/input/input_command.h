/**
 * @file input/input_command.h
 * @brief Typed client-side InputCommand abstraction.
 *
 * An InputCommand is the canonical unit of client-→-server input. It carries
 * the data the server needs to apply the action, plus the tick + sequence
 * pair the client needs for prediction reconciliation:
 *
 *   - client_tick — the predicted server tick at which the client emitted
 *                   this command. Stamped from session_server_tick_estimate().
 *   - sequence    — monotonic per-session counter. Allocated via
 *                   session_next_input_sequence(). The server echoes the
 *                   highest applied sequence back in every snapshot header.
 *
 * Pipeline:
 *
 *   ui hit-test → input_command_build_*() → prediction_apply() (optimistic)
 *                                         → uplink_send_input_command()    (wire)
 *                                         → input_buffer_push()            (replay history)
 *
 * Rendering and world-state mutation must not happen here — this module
 * only constructs and serialises commands.
 */

#ifndef CYBERIA_INPUT_COMMAND_H
#define CYBERIA_INPUT_COMMAND_H

#include <stdint.h>

/** Kind values mirror the binary uplink opcodes used by the WS wire format,
 *  so log lines, traces, and uplink encoders can refer to one set of
 *  constants. */
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

/** Build a PLAYER_ACTION command at grid (x, y). Stamps tick+sequence from
 *  the session module. */
input_command_t input_command_build_tap(float grid_x, float grid_y);

#endif /* CYBERIA_INPUT_COMMAND_H */
