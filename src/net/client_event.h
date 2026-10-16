#ifndef CYBERIA_NETWORK_CLIENT_EVENT_H
#define CYBERIA_NETWORK_CLIENT_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "content/stat_contract_generated.h"
#include "world/object_layer.h"
#include "world/world_types.h"

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
/* One chat line: the rune limit in UTF-8 bytes, plus the NUL. */
#define CHAT_LINE_BYTES  (CYBERIA_CHAT_TEXT_MAX_RUNES * 4 + 1)
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
    CLIENT_EVENT_ITEM_MOVE,
    CLIENT_EVENT_PARTY_INVITE,
    CLIENT_EVENT_PARTY_REPLY,
    CLIENT_EVENT_PARTY_LEAVE,
    CLIENT_EVENT_PARTY_KICK,
    CLIENT_EVENT_PARTY_DISBAND,
    CLIENT_EVENT_FRIENDS_LIST,
    CLIENT_EVENT_FRIEND_REQUEST,
    CLIENT_EVENT_FRIEND_REPLY,
    CLIENT_EVENT_FRIEND_REMOVE,
    CLIENT_EVENT_SESSION_LEAVE,
} client_event_kind_t;

/* Who one chat line reaches; packs to the wire word. */
typedef enum {
    CHAT_CHANNEL_WORLD,    /* every player on this server */
    CHAT_CHANNEL_PARTY,    /* the members of the party */
    CHAT_CHANNEL_DIRECT,   /* one player on the map, or a friend on this server */
    CHAT_CHANNEL_COUNT,
} chat_channel_t;

/* The container a slot of an item move belongs to; packs to the wire word. */
typedef enum {
    BOX_BAG,      /* the carried inventory */
    BOX_STORAGE,  /* the personal storage */
    BOX_VAULT,    /* the NPC vault at entity_id */
} Box;

/* One flat payload, the same shape as the server inputPayload. Each kind
 * reads only its own fields. */
typedef struct {
    float  target_x, target_y;                 /* player_action */
    EntityID entity_id;                        /* also chat to_id, party and friend player */
    char   item_id[CLIENT_EVENT_ID_BYTES];     /* also quest_code */
    char   code[CLIENT_EVENT_ID_BYTES];        /* dialog_code, friendship id */
    chat_channel_t channel;                    /* chat */
    char   text[CHAT_LINE_BYTES];              /* chat */
    int    quantity, recipe_index;             /* quantity: also the units of item_move */
    bool   active;                             /* also player_stasis, party and friend reply accept */
    int    slot;                               /* item_active: the bag slot of the stack */
    Box    from, to;                           /* item_move */
    int    from_slot, to_slot;
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
