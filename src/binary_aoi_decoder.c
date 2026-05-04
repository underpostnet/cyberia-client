/**
 * @file binary_aoi_decoder.c
 * @brief Decoder for the compact binary AOI protocol from the Go server.
 *
 * Parses little-endian binary messages and updates the global g_game_state,
 * mirroring the behavior of the JSON-based message_parser_parse_aoi_update().
 */

#include "binary_aoi_decoder.h"
#include "game_state.h"
#include "floating_combat_text.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

/* ── Little-endian readers ─────────────────────────────────────── */

typedef struct {
    const uint8_t* data;
    size_t         len;
    size_t         pos;
} BinReader;

static inline int br_remaining(const BinReader* r) {
    return (r->pos < r->len) ? (int)(r->len - r->pos) : 0;
}

static inline uint8_t br_u8(BinReader* r) {
    if (r->pos >= r->len) return 0;
    return r->data[r->pos++];
}

static inline uint16_t br_u16(BinReader* r) {
    if (r->pos + 2 > r->len) { r->pos = r->len; return 0; }
    uint16_t v = (uint16_t)r->data[r->pos]
               | ((uint16_t)r->data[r->pos + 1] << 8);
    r->pos += 2;
    return v;
}

static inline int16_t br_i16(BinReader* r) {
    return (int16_t)br_u16(r);
}

static inline uint32_t br_u32(BinReader* r) {
    if (r->pos + 4 > r->len) { r->pos = r->len; return 0; }
    uint32_t v = (uint32_t)r->data[r->pos]
               | ((uint32_t)r->data[r->pos + 1] << 8)
               | ((uint32_t)r->data[r->pos + 2] << 16)
               | ((uint32_t)r->data[r->pos + 3] << 24);
    r->pos += 4;
    return v;
}

static inline float br_f32(BinReader* r) {
    if (r->pos + 4 > r->len) { r->pos = r->len; return 0.0f; }
    uint32_t bits = (uint32_t)r->data[r->pos]
                  | ((uint32_t)r->data[r->pos + 1] << 8)
                  | ((uint32_t)r->data[r->pos + 2] << 16)
                  | ((uint32_t)r->data[r->pos + 3] << 24);
    r->pos += 4;
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

/* Read a fixed 36-byte ID field into dst (null-terminated). */
static inline void br_id(BinReader* r, char* dst, size_t dst_size) {
    size_t copy_len = 36;
    if (r->pos + copy_len > r->len) { r->pos = r->len; dst[0] = '\0'; return; }
    if (copy_len >= dst_size) copy_len = dst_size - 1;
    memcpy(dst, r->data + r->pos, copy_len);
    dst[copy_len] = '\0';
    /* Trim trailing zeros from UUID field */
    for (int i = (int)copy_len - 1; i >= 0 && dst[i] == '\0'; i--) {
        /* already zero */ ;
    }
    r->pos += 36;
}

/* Read a length-prefixed string (1-byte len). */
static inline void br_string(BinReader* r, char* dst, size_t dst_size) {
    uint8_t slen = br_u8(r);
    if (r->pos + slen > r->len) { r->pos = r->len; dst[0] = '\0'; return; }
    size_t copy_len = (slen < dst_size - 1) ? slen : dst_size - 1;
    memcpy(dst, r->data + r->pos, copy_len);
    dst[copy_len] = '\0';
    r->pos += slen;
}

/* ── Item ID list reader (IDs only — no active/quantity) ───────── */

static int read_item_ids(BinReader* r, ObjectLayerState* layers, int max_layers) {
    uint8_t count = br_u8(r);
    int n = (count < max_layers) ? count : max_layers;
    for (int i = 0; i < n; i++) {
        br_string(r, layers[i].item_id, MAX_ITEM_ID_LENGTH);
        layers[i].active = true;
        layers[i].quantity = (int)br_u16(r); /* quantity now carried on wire */
    }
    /* Skip any excess items we couldn't store */
    for (int i = n; i < (int)count; i++) {
        uint8_t slen = br_u8(r);
        r->pos += slen; /* skip itemId */
        r->pos += 2;    /* skip quantity u16 */
    }
    return n;
}

/* ── Entity block readers ──────────────────────────────────────── */

static void decode_player_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    uint8_t dir = br_u8(r);
    uint8_t mode = br_u8(r);

    /* Find or allocate slot */
    int idx = -1;
    for (int i = 0; i < gs->other_player_count; i++) {
        if (strcmp(gs->other_players[i].base.id, id) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (gs->other_player_count >= MAX_ENTITIES) return;
        idx = gs->other_player_count++;
        memset(&gs->other_players[idx], 0, sizeof(PlayerState));
        strncpy(gs->other_players[idx].base.id, id, MAX_ID_LENGTH - 1);
    }

    PlayerState* p = &gs->other_players[idx];
    p->base.pos_prev = p->base.pos_server;
    p->base.pos_server = (Vector2){ px, py };
    p->base.dims = (Vector2){ dw, dh };
    p->base.direction = (Direction)dir;
    p->base.mode = (ObjectLayerMode)mode;
    p->base.last_update = gs->last_update_time;

    if (flags & BIN_FLAG_HAS_LIFE) {
        p->base.life = br_f32(r);
        p->base.max_life = br_f32(r);
    }
    if (flags & BIN_FLAG_HAS_RESPAWN) {
        p->base.respawn_in = br_f32(r);
    } else {
        p->base.respawn_in = 0.0f;
    }
    if (flags & BIN_FLAG_HAS_COLOR) {
        p->base.color.r = br_u8(r);
        p->base.color.g = br_u8(r);
        p->base.color.b = br_u8(r);
        p->base.color.a = br_u8(r);
    }
    p->base.object_layer_count = read_item_ids(
        r, p->base.object_layers, MAX_OBJECT_LAYERS);
    p->base.effective_level = (int)br_u16(r);
    p->base.status_icon = br_u8(r);  /* Entity Status Indicator */
}

static void decode_bot_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    uint8_t dir = br_u8(r);
    uint8_t mode = br_u8(r);

    int idx = -1;
    for (int i = 0; i < gs->bot_count; i++) {
        if (strcmp(gs->bots[i].base.id, id) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (gs->bot_count >= MAX_ENTITIES) return;
        idx = gs->bot_count++;
        memset(&gs->bots[idx], 0, sizeof(BotState));
        strncpy(gs->bots[idx].base.id, id, MAX_ID_LENGTH - 1);
    }

    BotState* b = &gs->bots[idx];
    b->base.pos_prev = b->base.pos_server;
    b->base.pos_server = (Vector2){ px, py };
    b->base.dims = (Vector2){ dw, dh };
    b->base.direction = (Direction)dir;
    b->base.mode = (ObjectLayerMode)mode;
    b->base.last_update = gs->last_update_time;

    if (flags & BIN_FLAG_HAS_LIFE) {
        b->base.life = br_f32(r);
        b->base.max_life = br_f32(r);
    }
    if (flags & BIN_FLAG_HAS_RESPAWN) {
        b->base.respawn_in = br_f32(r);
    } else {
        b->base.respawn_in = 0.0f;
    }
    if (flags & BIN_FLAG_HAS_BEHAVIOR) {
        br_string(r, b->behavior, MAX_BEHAVIOR_LENGTH);
    }
    if (flags & BIN_FLAG_HAS_COLOR) {
        b->base.color.r = br_u8(r);
        b->base.color.g = br_u8(r);
        b->base.color.b = br_u8(r);
        b->base.color.a = br_u8(r);
    }
    b->base.object_layer_count = read_item_ids(
        r, b->base.object_layers, MAX_OBJECT_LAYERS);
    br_string(r, b->caster_id, MAX_ID_LENGTH);
    b->base.effective_level = (int)br_u16(r);
    b->base.status_icon = br_u8(r);  /* Entity Status Indicator */
}

static void decode_floor_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    br_u8(r); /* direction — unused for floors */
    br_u8(r); /* mode      — unused for floors */

    Color floor_color = {0, 0, 0, 0};
    if (flags & BIN_FLAG_HAS_COLOR) {
        floor_color.r = br_u8(r);
        floor_color.g = br_u8(r);
        floor_color.b = br_u8(r);
        floor_color.a = br_u8(r);
    }

    if (gs->floor_count >= MAX_OBJECTS) return;
    int idx = gs->floor_count++;
    WorldObject* f = &gs->floors[idx];
    memset(f, 0, sizeof(WorldObject));
    strncpy(f->id, id, MAX_ID_LENGTH - 1);
    f->pos  = (Vector2){ px, py };
    f->dims = (Vector2){ dw, dh };
    strncpy(f->type, "floor", MAX_TYPE_LENGTH - 1);
    f->color = floor_color;

    f->object_layer_count = read_item_ids(
        r, f->object_layers, MAX_OBJECT_LAYERS);
}

static void decode_obstacle_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    br_u8(r); /* direction */
    br_u8(r); /* mode */

    Color obs_color = {0, 0, 0, 0};
    if (flags & BIN_FLAG_HAS_COLOR) {
        obs_color.r = br_u8(r);
        obs_color.g = br_u8(r);
        obs_color.b = br_u8(r);
        obs_color.a = br_u8(r);
    }

    if (gs->obstacle_count >= MAX_OBJECTS) return;
    int idx = gs->obstacle_count++;
    WorldObject* o = &gs->obstacles[idx];
    memset(o, 0, sizeof(WorldObject));
    strncpy(o->id, id, MAX_ID_LENGTH - 1);
    o->pos  = (Vector2){ px, py };
    o->dims = (Vector2){ dw, dh };
    strncpy(o->type, "obstacle", MAX_TYPE_LENGTH - 1);
    o->color = obs_color;
    o->object_layer_count = read_item_ids(
        r, o->object_layers, MAX_OBJECT_LAYERS);
}

static void decode_portal_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    br_u8(r); /* direction */
    br_u8(r); /* mode */

    /* portal label */
    char label[MAX_ID_LENGTH];
    br_string(r, label, sizeof(label));

    /* per-portal color (RGBA) */
    Color portal_color = gs->colors.portal; /* fallback */
    if (flags & BIN_FLAG_HAS_COLOR) {
        portal_color.r = br_u8(r);
        portal_color.g = br_u8(r);
        portal_color.b = br_u8(r);
        portal_color.a = br_u8(r);
    }

    if (gs->portal_count >= MAX_OBJECTS) return;
    int idx = gs->portal_count++;
    WorldObject* p = &gs->portals[idx];
    memset(p, 0, sizeof(WorldObject));
    strncpy(p->id, id, MAX_ID_LENGTH - 1);
    p->pos  = (Vector2){ px, py };
    p->dims = (Vector2){ dw, dh };
    strncpy(p->type, "portal", MAX_TYPE_LENGTH - 1);
    strncpy(p->portal_label, label, MAX_ID_LENGTH - 1);
    p->color = portal_color;
    p->object_layer_count = read_item_ids(
        r, p->object_layers, MAX_OBJECT_LAYERS);
}

static void decode_foreground_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    br_u8(r); /* direction */
    br_u8(r); /* mode */

    Color fg_color = {0, 0, 0, 0};
    if (flags & BIN_FLAG_HAS_COLOR) {
        fg_color.r = br_u8(r);
        fg_color.g = br_u8(r);
        fg_color.b = br_u8(r);
        fg_color.a = br_u8(r);
    }

    if (gs->foreground_count >= MAX_OBJECTS) return;
    int idx = gs->foreground_count++;
    WorldObject* fg = &gs->foregrounds[idx];
    memset(fg, 0, sizeof(WorldObject));
    strncpy(fg->id, id, MAX_ID_LENGTH - 1);
    fg->pos  = (Vector2){ px, py };
    fg->dims = (Vector2){ dw, dh };
    strncpy(fg->type, "foreground", MAX_TYPE_LENGTH - 1);
    fg->color = fg_color;
    fg->object_layer_count = read_item_ids(
        r, fg->object_layers, MAX_OBJECT_LAYERS);
}

/* ── Resource entity decoder ───────────────────────────────────── */

static void decode_resource_entity(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    uint8_t dir = br_u8(r);
    uint8_t mode = br_u8(r);

    if (gs->resource_count >= MAX_ENTITIES) return;
    int idx = gs->resource_count++;
    BotState* res = &gs->resources[idx];
    memset(res, 0, sizeof(BotState));
    strncpy(res->base.id, id, MAX_ID_LENGTH - 1);

    res->base.pos_server = (Vector2){ px, py };
    res->base.pos_prev = res->base.pos_server;
    res->base.interp_pos = res->base.pos_server; /* static — no interpolation */
    res->base.dims = (Vector2){ dw, dh };
    res->base.direction = (Direction)dir;
    res->base.mode = (ObjectLayerMode)mode;
    res->base.last_update = gs->last_update_time;

    if (flags & BIN_FLAG_HAS_LIFE) {
        res->base.life = br_f32(r);
        res->base.max_life = br_f32(r);
    }
    if (flags & BIN_FLAG_HAS_RESPAWN) {
        res->base.respawn_in = br_f32(r);
    } else {
        res->base.respawn_in = 0.0f;
    }
    if (flags & BIN_FLAG_HAS_COLOR) {
        res->base.color.r = br_u8(r);
        res->base.color.g = br_u8(r);
        res->base.color.b = br_u8(r);
        res->base.color.a = br_u8(r);
    }
    res->base.object_layer_count = read_item_ids(
        r, res->base.object_layers, MAX_OBJECT_LAYERS);
    res->base.status_icon = br_u8(r);
    strncpy(res->behavior, "resource", MAX_BEHAVIOR_LENGTH - 1);
}

/* ── Self-player decoder ───────────────────────────────────────── */

static void decode_self_player(BinReader* r, uint8_t flags) {
    GameState* gs = &g_game_state;
    PlayerState* p = &gs->player;

    br_id(r, p->base.id, MAX_ID_LENGTH);
    strncpy(gs->player_id, p->base.id, MAX_ID_LENGTH - 1);

    p->base.pos_prev = p->base.pos_server;
    p->base.pos_server.x = br_f32(r);
    p->base.pos_server.y = br_f32(r);
    p->base.dims.x = br_f32(r);
    p->base.dims.y = br_f32(r);
    p->base.direction = (Direction)br_u8(r);
    p->base.mode = (ObjectLayerMode)br_u8(r);
    p->base.last_update = gs->last_update_time;

    if (flags & BIN_FLAG_HAS_LIFE) {
        p->base.life = br_f32(r);
        p->base.max_life = br_f32(r);
    }
    if (flags & BIN_FLAG_HAS_RESPAWN) {
        p->base.respawn_in = br_f32(r);
    } else {
        p->base.respawn_in = 0.0f;
    }
    p->base.object_layer_count = read_item_ids(
        r, p->base.object_layers, MAX_OBJECT_LAYERS);

    /* Extended self-player fields */
    /* AOI rect — for debug rendering */
    br_f32(r); /* aoiMinX — not stored in client state currently */
    br_f32(r); /* aoiMinY */
    br_f32(r); /* aoiMaxX */
    br_f32(r); /* aoiMaxY */

    /* onPortal */
    br_u8(r);

    /* sumStatsLimit */
    gs->sum_stats_limit = (int)br_u16(r);

    /* activeStatsSum */
    gs->active_stats_sum = (int)br_u16(r);
    /* Mirror to self-entity for uniform overhead UI access */
    p->base.effective_level = (gs->active_stats_sum < gs->sum_stats_limit)
        ? gs->active_stats_sum : gs->sum_stats_limit;

    /* mapCode — length-prefixed string */
    br_string(r, p->map_code, MAX_ID_LENGTH);

    /* path */
    uint8_t path_len = br_u8(r);
    p->path_count = (path_len > MAX_PATH_POINTS) ? MAX_PATH_POINTS : path_len;
    for (int i = 0; i < p->path_count; i++) {
        p->path[i].x = (float)br_i16(r);
        p->path[i].y = (float)br_i16(r);
    }
    /* Skip excess path points if any */
    for (int i = p->path_count; i < (int)path_len; i++) {
        br_i16(r);
        br_i16(r);
    }

    /* targetPos */
    p->target_pos.x = (float)br_i16(r);
    p->target_pos.y = (float)br_i16(r);

    /* activePortalID — skip (not used by client renderer) */
    char portal_id_buf[MAX_ID_LENGTH];
    br_string(r, portal_id_buf, sizeof(portal_id_buf));

    /* Coin balance — u32, always present after activePortalID */
    gs->player_coins = (int)br_u32(r);

    /* Full inventory — ALL ObjectLayers (active + inactive) with quantities.
     * Powered by writeFullInventory on the server; used by inventory_bar. */
    {
        uint8_t inv_count = br_u8(r);
        int ni = (inv_count < MAX_OBJECT_LAYERS) ? (int)inv_count : MAX_OBJECT_LAYERS;
        for (int i = 0; i < ni; i++) {
            br_string(r, gs->full_inventory[i].item_id, MAX_ITEM_ID_LENGTH);
            gs->full_inventory[i].active   = (br_u8(r) != 0);
            gs->full_inventory[i].quantity = (int)br_u16(r);
        }
        /* Skip excess slots we couldn't store */
        for (int i = ni; i < (int)inv_count; i++) {
            uint8_t slen = br_u8(r);
            r->pos += slen; /* skip itemId  */
            r->pos += 1;    /* skip active  */
            r->pos += 2;    /* skip quantity */
        }
        gs->full_inventory_count = ni;
    }

    /* FrozenInteractionState — u8 (0 = normal, 1 = frozen).
     * Authoritative flag from the Go server; drives visual feedback
     * and client-side action blocking. */
    gs->frozen = (br_u8(r) != 0);

    /* Entity Status Indicator — u8 overhead icon ID for self-player. */
    gs->self_status_icon = br_u8(r);
    gs->player.base.status_icon = gs->self_status_icon;
}

/* ── Main entry point ──────────────────────────────────────────── */

int binary_aoi_process(const uint8_t* data, size_t length) {
    if (!data || length < 2) {
        printf("[BINARY_AOI] Message too short (%zu bytes)\n", length);
        return -1;
    }

    BinReader r = { .data = data, .len = length, .pos = 0 };
    GameState* gs = &g_game_state;

    uint8_t msg_type = br_u8(&r);

    /* ── Floating Combat Text event — compact 14-byte message ──────────── */
    if (msg_type == BIN_MSG_FCT) {
        if (length < 14) {
            printf("[BINARY_AOI] FCT message too short (%zu bytes, need 14)\n", length);
            return -1;
        }
        uint8_t  fct_type = br_u8(&r);
        float    world_x  = br_f32(&r);
        float    world_y  = br_f32(&r);
        uint32_t value    = br_u32(&r);
        fct_spawn(world_x, world_y, value, fct_type);
        return 0;
    }
    /* ── Item FCT event — variable-length message (≥15 bytes) ─────────────── */
    if (msg_type == BIN_MSG_ITEM_FCT) {
        if (length < 15) {
            printf("[BINARY_AOI] ItemFCT message too short (%zu bytes)\n", length);
            return -1;
        }
        uint8_t  fct_type = br_u8(&r);
        float    world_x  = br_f32(&r);
        float    world_y  = br_f32(&r);
        uint32_t qty      = br_u32(&r);
        uint8_t  id_len   = br_u8(&r);
        char     item_id[MAX_ITEM_ID_LENGTH];
        memset(item_id, 0, sizeof(item_id));
        if (id_len > 0 && id_len < MAX_ITEM_ID_LENGTH && r.pos + id_len <= r.len) {
            memcpy(item_id, r.data + r.pos, id_len);
        }
        fct_spawn_item(world_x, world_y, qty, fct_type, item_id);
        return 0;
    }
    /* ── AOI update / full AOI — standard entity-loop format ─────────── */
    if (length < 5) {
        printf("[BINARY_AOI] AOI message too short (%zu bytes)\n", length);
        return -1;
    }

    /* u16 reserved field — always 0, ignore */
    br_u16(&r);
    uint16_t entity_count = br_u16(&r);

    if (msg_type != BIN_MSG_AOI_UPDATE && msg_type != BIN_MSG_FULL_AOI) {
        printf("[BINARY_AOI] Unknown message type 0x%02x\n", msg_type);
        return -1;
    }

    /* Clear world objects (same as JSON parser does each AOI frame) */
    gs->other_player_count = 0;
    gs->bot_count = 0;
    gs->resource_count = 0;
    gs->obstacle_count = 0;
    gs->foreground_count = 0;
    gs->portal_count = 0;
    gs->floor_count = 0;

    /* Decode entity blocks */
    for (uint16_t i = 0; i < entity_count && br_remaining(&r) > 0; i++) {
        uint8_t flags = br_u8(&r);
        uint8_t etype = flags & 0x07;

        if (flags & BIN_FLAG_REMOVED) {
            /* Entity left AOI — skip its data block.
               For removed entities the server only sends the ID. */
            char skip_id[MAX_ID_LENGTH];
            br_id(&r, skip_id, sizeof(skip_id));
            continue;
        }

        switch (etype) {
            case BIN_ENTITY_PLAYER:     decode_player_entity(&r, flags);     break;
            case BIN_ENTITY_BOT:        decode_bot_entity(&r, flags);        break;
            case BIN_ENTITY_FLOOR:      decode_floor_entity(&r, flags);      break;
            case BIN_ENTITY_OBSTACLE:   decode_obstacle_entity(&r, flags);   break;
            case BIN_ENTITY_PORTAL:     decode_portal_entity(&r, flags);     break;
            case BIN_ENTITY_FOREGROUND: decode_foreground_entity(&r, flags); break;
            case BIN_ENTITY_RESOURCE:   decode_resource_entity(&r, flags);   break;
            default:
                printf("[BINARY_AOI] Unknown entity type %d at offset %zu\n", etype, r.pos);
                return -1;
        }
    }

    /* Self-player block comes last */
    if (br_remaining(&r) > 0) {
        uint8_t self_flags = br_u8(&r);
        decode_self_player(&r, self_flags);
    }

    return 0;
}
