#include "client_event.h"

#include "game_client.h"
#include "replication.h"
#include "util/serial.h"
#include "util/log.h"

#include <assert.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

static struct {
    client_event_t items[CLIENT_EVENT_CAP];
    int            count;
    int            sent;   /* the events at the front that are already sent */
} s_q;

void client_event_push(client_event_kind_t kind, client_event_payload_t p) {
    if (!(s_q.count < CLIENT_EVENT_CAP)) {
        assert(false && "client event list full");
        LOG_ERROR("client event list full");
        abort();
    }
    s_q.items[s_q.count++] = (client_event_t){
        .kind      = kind,
        .frame     = session_frame(),
        .sequence  = session_next_input_sequence(),
        .timestamp = GetTime(),
        .payload   = p,
    };
}

static cJSON* pack_event(const client_event_t* e) {
    const client_event_payload_t* p = &e->payload;
    switch (e->kind) {
    case CLIENT_EVENT_PLAYER_ACTION:   return json_pack_player_action(p->target_x, p->target_y);
    case CLIENT_EVENT_ITEM_ACTIVE:     return json_pack_item_active(p->item_id, p->active);
    case CLIENT_EVENT_PLAYER_STASIS:   return json_pack_player_stasis(p->active);
    case CLIENT_EVENT_CHAT:            return json_pack_chat(p->item_id, p->text);
    case CLIENT_EVENT_TALK_DONE:       return json_pack_talk_done(p->entity_id, p->code);
    case CLIENT_EVENT_QUEST_ABANDON:   return json_pack_quest_abandon(p->item_id);
    case CLIENT_EVENT_QUEST_ACCEPT:    return json_pack_quest_accept(p->entity_id, p->item_id);
    case CLIENT_EVENT_SHOP_BUY:        return json_pack_shop_buy(p->entity_id, p->item_id, p->quantity);
    case CLIENT_EVENT_CRAFT_ITEM:      return json_pack_craft_item(p->entity_id, p->recipe_index);
    case CLIENT_EVENT_CRAFT_CANCEL:    return json_pack_craft_cancel();
    case CLIENT_EVENT_STORAGE_OPEN:    return json_pack_storage_open(p->entity_id);
    case CLIENT_EVENT_ITEM_OPS: {
        cJSON* ops = cJSON_CreateArray();
        assert(ops);
        for (int i = 0; i < p->op_count; i++) {
            cJSON_AddItemToArray(ops, json_pack_item_op(p->ops[i].item_id, p->ops[i].qty,
                                                        p->ops[i].to_vault));
        }
        return json_pack_item_ops(p->entity_id, ops);
    }
    }
    assert(false && "unknown client event kind");
    return NULL;
}

void client_event_flush(void) {
    if (s_q.sent == s_q.count) return;
    cJSON* events = cJSON_CreateArray();
    assert(events);
    for (int i = s_q.sent; i < s_q.count; i++) {
        const client_event_t* e = &s_q.items[i];
        cJSON* msg = pack_event(e);
        cJSON* p   = cJSON_GetObjectItemCaseSensitive(msg, "payload");
        cJSON_AddNumberToObject(p, "seq", e->sequence);
        cJSON_AddNumberToObject(p, "frame", e->frame);
        cJSON_AddNumberToObject(p, "timestamp", e->timestamp);
        cJSON_AddItemToArray(events, msg);
    }
    /* The JSON is gone either way; only the structs persist. */
    if (network_send(json_pack_events(events))) s_q.sent = s_q.count;
}

void client_event_trim(cyberia_input_seq_t consumed) {
    int n = 0;
    while (n < s_q.count && consumed >= s_q.items[n].sequence) n++;
    if (0 == n) return;
    memmove(s_q.items, s_q.items + n, (size_t)(s_q.count - n) * sizeof s_q.items[0]);
    s_q.count -= n;
    s_q.sent  -= n < s_q.sent ? n : s_q.sent;
}

const client_event_t* client_event_list(int* count) {
    assert(count);
    *count = s_q.count;
    return s_q.items;
}

void client_event_reset(void) {
    s_q.count = 0;
    s_q.sent  = 0;
}
