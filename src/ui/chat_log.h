#ifndef CYBERIA_UI_CHAT_LOG_H
#define CYBERIA_UI_CHAT_LOG_H

/* The chat history of the session: one conversation per channel, and one per
 * peer of direct chat. Every player line comes from the server, the own line
 * as the server accepted it. A system line is local feedback, such as a
 * refusal. The log counts unread lines per conversation. */

#include <stdbool.h>

#include "net/client_event.h"
#include "world/object_layer.h"
#include "world/world_types.h"

#define CHAT_LOG_CONVERSATIONS 32
#define CHAT_LOG_LINES         60

/* A channel, and the peer of a direct conversation (0 otherwise). */
typedef struct {
    chat_channel_t channel;
    EntityID       peer;
} ChatConversation;

typedef struct {
    EntityID from_id;                      /* 0 for a system line */
    char     from_name[PLAYER_NAME_BYTES];
    char     text[CHAT_LINE_BYTES];
    double   at_ms;                        /* Unix ms: server time of a player line, local time of a system line */
    bool     mine;
} ChatLine;

typedef struct {
    ChatConversation conversation;
    char             peer_name[PLAYER_NAME_BYTES];  /* a direct conversation: who it is with */
    ChatLine         lines[CHAT_LOG_LINES];
    int              count;
    int              unread;
} ChatHistory;

static inline ChatConversation chat_channel(chat_channel_t channel) {
    return (ChatConversation){ .channel = channel };
}

static inline ChatConversation chat_direct(EntityID peer) {
    return (ChatConversation){ .channel = CHAT_CHANNEL_DIRECT, .peer = peer };
}

/* One player line at the server time `at_ms`. A line that is not `mine` counts as unread. */
void chat_log_push(ChatConversation c, EntityID from_id, const char* from_name, const char* text, double at_ms,
                   bool mine);

/* One local feedback line. It counts as read. */
void chat_log_push_system(ChatConversation c, const char* text);

/* Opens the direct conversation with a peer, named by its player name. */
void chat_log_open_direct(EntityID peer, const char* name);

/* NULL when the conversation was never opened and has no line. */
const ChatHistory* chat_log_get(ChatConversation c);

/* Every conversation, in the order they began. */
int                chat_log_count(void);
const ChatHistory* chat_log_at(int index);

int  chat_log_unread(ChatConversation c);
void chat_log_mark_read(ChatConversation c);

/* Unread lines of every direct conversation. */
int  chat_log_unread_direct(void);

/* Drop every conversation. Call on socket close: the next session names its
 * players with new ids. */
void chat_log_reset(void);

#endif /* CYBERIA_UI_CHAT_LOG_H */
