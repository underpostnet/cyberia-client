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

#include <stdbool.h>
#include <stdint.h>
#include "domain/tick.h"

/** Kind values mirror the binary uplink opcodes used by the WS wire format,
 *  so log lines, traces, and uplink encoders can refer to one set of
 *  constants. */
typedef enum {
    INPUT_KIND_UNKNOWN         = 0x00,
    INPUT_KIND_HANDSHAKE       = 0x10,
    INPUT_KIND_PLAYER_ACTION   = 0x11,
    INPUT_KIND_ITEM_ACTIVATION = 0x12,
    INPUT_KIND_FREEZE_START    = 0x13,
    INPUT_KIND_FREEZE_END      = 0x14,
    INPUT_KIND_CHAT            = 0x15,
    INPUT_KIND_GET_ITEMS_IDS   = 0x16,
} input_kind_t;

#define INPUT_MAX_ITEM_ID_LEN 64
#define INPUT_MAX_REASON_LEN  32
#define INPUT_MAX_CHAT_LEN    256

typedef struct {
    input_kind_t           kind;
    cyberia_tick_t         client_tick;
    cyberia_input_seq_t    sequence;
    /* Payload union (denormalised flat fields for ABI stability across the
     * JS bridge). Only fields relevant to `kind` are populated. */
    float    target_x;   /* PLAYER_ACTION */
    float    target_y;   /* PLAYER_ACTION */
    char     item_id[INPUT_MAX_ITEM_ID_LEN]; /* ITEM_ACTIVATION, GET_ITEMS_IDS, CHAT.to */
    bool     active;     /* ITEM_ACTIVATION */
    char     reason[INPUT_MAX_REASON_LEN];   /* FREEZE_START/END */
    char     chat_text[INPUT_MAX_CHAT_LEN];  /* CHAT */
} input_command_t;

/** Build a PLAYER_ACTION command at grid (x, y). Stamps tick+sequence from
 *  the session module. */
input_command_t input_command_build_tap(float grid_x, float grid_y);

/** Build an ITEM_ACTIVATION command. */
input_command_t input_command_build_item_activation(const char* item_id, bool active);

/** Build a CHAT command. */
input_command_t input_command_build_chat(const char* to_id, const char* text);

/** Build a FREEZE_START/FREEZE_END command. */
input_command_t input_command_build_freeze(bool start, const char* reason);

/** Build a GET_ITEMS_IDS command. */
input_command_t input_command_build_get_items_ids(const char* item_id);

#endif /* CYBERIA_INPUT_COMMAND_H */
