#ifndef CYBERIA_NETWORK_CLIENT_EVENT_H
#define CYBERIA_NETWORK_CLIENT_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "object_layer.h"
#include "world_types.h"

/* Client events: the one uplink of input to the server.
 *
 * Any module pushes an action here and touches no stamp, no sequence and no
 * socket. push stamps the frame, the sequence and the time. The list keeps
 * each event until a snapshot says the server consumed it, so the reconcile
 * can replay it. flush sends each event once. The struct is the event; JSON
 * is only its transport form, built in flush.
 *
 * Raw input (input_event_t) never leaves the client. Its producer converts a
 * raw event into a client event, or into nothing. */

typedef uint32_t cyberia_frame_t;
typedef uint32_t cyberia_input_seq_t;

/* Matches the server queue cap, so one batch from this client always fits. */
#define CLIENT_EVENT_CAP 512
#define CHAT_LINE_BYTES  512
/* The server accepts an id of up to 128 bytes (maxItemIDLen), plus the NUL. */
#define CLIENT_EVENT_ID_BYTES 129

/* One kind per uplink input type. Each kind packs to its wire type word. */
typedef enum {
    CLIENT_EVENT_PLAYER_ACTION,
    CLIENT_EVENT_ITEM_ACTIVE,
    CLIENT_EVENT_PLAYER_STASIS,
    CLIENT_EVENT_CHAT,
    CLIENT_EVENT_TALK_DONE,
    CLIENT_EVENT_QUEST_ABANDON,
    CLIENT_EVENT_QUEST_ACCEPT,
    CLIENT_EVENT_SHOP_BUY,
    CLIENT_EVENT_CRAFT_ITEM,
    CLIENT_EVENT_CRAFT_CANCEL,
    CLIENT_EVENT_ITEM_OPS,
} client_event_kind_t;

/* Matches the server cap on one item_ops list (storageMaxSlots). */
#define CLIENT_EVENT_ITEM_OPS_MAX 64

/* One vault op: qty of item_id into the vault when to_vault, else out of it. */
typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    int  qty;
    bool to_vault;
} client_event_item_op_t;

/* One flat payload, the same shape as the server inputPayload. Each kind
 * reads only its own fields. */
typedef struct {
    float  target_x, target_y;                 /* player_action */
    EntityID entity_id;                        /* also chat to_id */
    char   item_id[CLIENT_EVENT_ID_BYTES];     /* also quest_code */
    char   code[CLIENT_EVENT_ID_BYTES];        /* dialog_code */
    char   text[CHAT_LINE_BYTES];              /* chat */
    int    quantity, recipe_index;
    bool   active;                             /* also player_stasis */
    /* ponytail: inline list, ~4.6 KB in every queue slot (~2.4 MB BSS).
     * Move it to a side pool keyed by sequence if memory matters. */
    int    op_count;                           /* item_ops */
    client_event_item_op_t ops[CLIENT_EVENT_ITEM_OPS_MAX];
} client_event_payload_t;

typedef struct {
    client_event_kind_t     kind;
    cyberia_frame_t         frame;     /* completed fixed steps at push */
    cyberia_input_seq_t     sequence;  /* also the order inside one frame */
    double                  timestamp; /* GetTime() at push */
    client_event_payload_t  payload;
} client_event_t;

/* Stamp and append one event. Push order is replay order. */
void client_event_push(client_event_kind_t kind, client_event_payload_t p);

/* Send the events not sent yet as one "events" message. A failed send keeps
 * them for the next flush. */
void client_event_flush(void);

/* Drop the events at the front with sequence <= consumed. */
void client_event_trim(cyberia_input_seq_t consumed);

/* The unconsumed events, oldest first. Read-only. */
const client_event_t* client_event_list(int* count);

/* Drop every event. Call on socket close. */
void client_event_reset(void);

#endif /* CYBERIA_NETWORK_CLIENT_EVENT_H */
