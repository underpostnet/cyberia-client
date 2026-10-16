#include "chat_log.h"

#include "log.h"
#include "utils.h"

#include <raylib.h>
#include <string.h>

static ChatHistory s_histories[CHAT_LOG_CONVERSATIONS];
static int         s_count = 0;

static bool same(ChatConversation a, ChatConversation b) {
    return a.channel == b.channel && a.peer == b.peer;
}

static ChatHistory* find(ChatConversation c) {
    for (int i = 0; i < s_count; i++) {
        if (same(c, s_histories[i].conversation)) return &s_histories[i];
    }
    return NULL;
}

static ChatHistory* find_or_create(ChatConversation c) {
    ChatHistory* h = find(c);
    if (NULL != h) return h;
    if (CHAT_LOG_CONVERSATIONS <= s_count) {
        LOG_ERROR("[CHAT_LOG] conversation slots full, dropping a line");
        return NULL;
    }
    h = &s_histories[s_count++];
    memset(h, 0, sizeof *h);
    h->conversation = c;
    return h;
}

static void append(ChatConversation c, EntityID from_id, const char* from_name, const char* from_skin,
                   const char* text, bool mine, bool unread) {
    ChatHistory* h = find_or_create(c);
    if (NULL == h) return;
    /* A ring: the oldest line leaves when the history is full. */
    if (CHAT_LOG_LINES <= h->count) {
        memmove(h->lines, h->lines + 1, sizeof h->lines[0] * (CHAT_LOG_LINES - 1));
        h->count = CHAT_LOG_LINES - 1;
    }
    ChatLine* line = &h->lines[h->count++];
    memset(line, 0, sizeof *line);
    line->from_id = from_id;
    copy_str(line->from_name, sizeof line->from_name, from_name);
    copy_str(line->from_skin, sizeof line->from_skin, from_skin);
    copy_str(line->text, sizeof line->text, text);
    line->ts_ms = GetTime() * 1000.0;
    line->mine  = mine;
    if (unread) h->unread++;
    if (CHAT_CHANNEL_DIRECT == c.channel && 0 != from_id && !mine) {
        copy_str(h->peer_name, sizeof h->peer_name, from_name);
        copy_str(h->peer_skin, sizeof h->peer_skin, from_skin);
    }
}

void chat_log_open_direct(EntityID peer, const char* name, const char* skin) {
    ChatHistory* h = find_or_create(chat_direct(peer));
    if (NULL == h) return;
    copy_str(h->peer_name, sizeof h->peer_name, name);
    if (NULL != skin && '\0' != skin[0]) copy_str(h->peer_skin, sizeof h->peer_skin, skin);
}

void chat_log_push(ChatConversation c, EntityID from_id, const char* from_name,
                   const char* from_skin, const char* text, bool mine) {
    append(c, from_id, from_name, from_skin, text, mine, !mine);
}

void chat_log_push_system(ChatConversation c, const char* text) {
    append(c, 0, "", "", text, false, false);
}

const ChatHistory* chat_log_get(ChatConversation c) { return find(c); }

int chat_log_count(void) { return s_count; }

const ChatHistory* chat_log_at(int index) { return 0 <= index && s_count > index ? &s_histories[index] : NULL; }

int chat_log_unread(ChatConversation c) {
    const ChatHistory* h = find(c);
    return NULL != h ? h->unread : 0;
}

void chat_log_mark_read(ChatConversation c) {
    ChatHistory* h = find(c);
    if (NULL != h) h->unread = 0;
}

int chat_log_unread_direct(void) {
    int total = 0;
    for (int i = 0; i < s_count; i++) {
        if (CHAT_CHANNEL_DIRECT == s_histories[i].conversation.channel) total += s_histories[i].unread;
    }
    return total;
}

void chat_log_reset(void) { s_count = 0; }
