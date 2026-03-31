/**
 * @file binary_aoi_decoder.c
 * @brief Decoder for the compact binary AOI protocol from the Go server.
 *
 * Parses little-endian binary messages and updates the global g_game_state,
 * mirroring the behavior of the JSON-based message_parser_parse_aoi_update().
 */

#include "binary_aoi_decoder.h"
#include "game_state.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

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
        layers[i].quantity = 1;
    }
    /* Skip any excess items we couldn't store */
    for (int i = n; i < (int)count; i++) {
        uint8_t slen = br_u8(r);
        r->pos += slen; /* skip itemId */
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
    p->base.last_update = g_game_state.last_update_time;

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
    b->base.last_update = g_game_state.last_update_time;

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

    if (gs->floor_count >= MAX_OBJECTS) return;
    int idx = gs->floor_count++;
    WorldObject* f = &gs->floors[idx];
    memset(f, 0, sizeof(WorldObject));
    strncpy(f->id, id, MAX_ID_LENGTH - 1);
    f->pos  = (Vector2){ px, py };
    f->dims = (Vector2){ dw, dh };
    strncpy(f->type, "floor", MAX_TYPE_LENGTH - 1);

    f->object_layer_count = read_item_ids(
        r, f->object_layers, MAX_OBJECT_LAYERS);
}

static void decode_obstacle_entity(BinReader* r, uint8_t flags) {
    (void)flags;
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    br_u8(r); /* direction */
    br_u8(r); /* mode */

    if (gs->obstacle_count >= MAX_OBJECTS) return;
    int idx = gs->obstacle_count++;
    WorldObject* o = &gs->obstacles[idx];
    memset(o, 0, sizeof(WorldObject));
    strncpy(o->id, id, MAX_ID_LENGTH - 1);
    o->pos  = (Vector2){ px, py };
    o->dims = (Vector2){ dw, dh };
    strncpy(o->type, "obstacle", MAX_TYPE_LENGTH - 1);
}

static void decode_portal_entity(BinReader* r, uint8_t flags) {
    (void)flags;
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

    if (gs->portal_count >= MAX_OBJECTS) return;
    int idx = gs->portal_count++;
    WorldObject* p = &gs->portals[idx];
    memset(p, 0, sizeof(WorldObject));
    strncpy(p->id, id, MAX_ID_LENGTH - 1);
    p->pos  = (Vector2){ px, py };
    p->dims = (Vector2){ dw, dh };
    strncpy(p->type, "portal", MAX_TYPE_LENGTH - 1);
    strncpy(p->portal_label, label, MAX_ID_LENGTH - 1);
}

static void decode_foreground_entity(BinReader* r, uint8_t flags) {
    (void)flags;
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH];
    br_id(r, id, sizeof(id));

    float px = br_f32(r);
    float py = br_f32(r);
    float dw = br_f32(r);
    float dh = br_f32(r);
    br_u8(r); /* direction */
    br_u8(r); /* mode */

    if (gs->foreground_count >= MAX_OBJECTS) return;
    int idx = gs->foreground_count++;
    WorldObject* fg = &gs->foregrounds[idx];
    memset(fg, 0, sizeof(WorldObject));
    strncpy(fg->id, id, MAX_ID_LENGTH - 1);
    fg->pos  = (Vector2){ px, py };
    fg->dims = (Vector2){ dw, dh };
    strncpy(fg->type, "foreground", MAX_TYPE_LENGTH - 1);
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
}

/* ── Main entry point ──────────────────────────────────────────── */

int binary_aoi_process(const uint8_t* data, size_t length) {
    if (!data || length < 5) {
        printf("[BINARY_AOI] Message too short (%zu bytes)\n", length);
        return -1;
    }

    BinReader r = { .data = data, .len = length, .pos = 0 };
    GameState* gs = &g_game_state;

    uint8_t msg_type = br_u8(&r);
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
