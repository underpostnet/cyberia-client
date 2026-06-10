#ifndef WORLD_TYPES_H
#define WORLD_TYPES_H

#include <raylib.h>
#include <stdint.h>

#include "object_layer.h"

/**
 * @file world_types.h
 * @brief Plain world-entity data structures shared by the serializer and the
 *        game-state mirror.
 *
 * These are pure structure definitions with no behaviour and no global state.
 * The serializer depends only on this header so it can stay structure-aware
 * without pulling in the GameState god object (g_game_state) or its accessors.
 */

#define MAX_OBJECT_LAYERS   20
#define MAX_PATH_POINTS     100
#define MAX_ID_LENGTH       64
#define MAX_BEHAVIOR_LENGTH 32

typedef struct EntityState EntityState;
typedef struct PlayerState PlayerState;
typedef struct BotState BotState;

struct EntityState {
    char id[MAX_ID_LENGTH];
    Vector2 pos_server;
    Vector2 pos_prev;
    Vector2 interp_pos;
    Vector2 dims;
    Direction direction;
    ObjectLayerMode mode;
    ObjectLayerState object_layers[MAX_OBJECT_LAYERS];
    int object_layer_count;
    float life;
    float max_life;
    float respawn_in;
    double last_update;     /* wall-clock time the entity was last touched */
    double snapshot_time;   /* wall-clock time of the snapshot that produced
                             * pos_server. Used by interpolation to compute
                             * a per-entity alpha instead of a global one. */
    int effective_level;
    uint8_t status_icon;
};

struct PlayerState {
    EntityState base;
    char map_code[MAX_ID_LENGTH];
    Vector2 path[MAX_PATH_POINTS]; /* debug only */
    int path_count;                /* debug only */
    Vector2 target_pos;            /* debug only */

    Vector2 tap_target;
    bool    has_tap_target;
};

struct BotState {
    EntityState base;
    char behavior[MAX_BEHAVIOR_LENGTH];
    char caster_id[MAX_ID_LENGTH];
    /* Authoritative quest binding from AOI — the code this action-provider
     * grants, "" for ordinary bots. Position-independent, so a wandering NPC
     * keeps its offer. */
    char grant_quest_code[MAX_ID_LENGTH];
};

typedef struct WorldObject {
    char             id[MAX_ID_LENGTH];
    Vector2          pos;
    Vector2          dims;
    ObjectLayerType  type_kind;
    char             type[MAX_TYPE_LENGTH];
    char             portal_label[MAX_ID_LENGTH];
    ObjectLayerState object_layers[MAX_OBJECT_LAYERS];
    int              object_layer_count;
} WorldObject;

#endif /* WORLD_TYPES_H */
