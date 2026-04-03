#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "object_layer.h"

/**
 * @file game_state.h
 * @brief Core game state management structures
 *
 * This module defines all the data structures needed to maintain
 * the game state, including entities, world objects, and configuration.
 * Migrated from Python GameState and related classes.
 */

#define MAX_ENTITIES 1000
#define MAX_OBJECTS 5000
#define MAX_OBJECT_LAYERS 20
#define MAX_PATH_POINTS 100
#define MAX_MESSAGE_SIZE 65536
#define MAX_ID_LENGTH 64
#define MAX_SKILL_ENTRIES 64
#define MAX_LOGIC_EVENT_IDS 8



#define MAX_BEHAVIOR_LENGTH 32
#define MAX_ENTITY_TYPES     16
#define MAX_DEFAULT_ITEM_IDS  8

// Forward declarations
typedef struct GameState GameState;
typedef struct EntityState EntityState;
typedef struct PlayerState PlayerState;
typedef struct BotState BotState;


// Color structure (equivalent to Python ColorRGBA)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} ColorRGBA;

// Base entity state structure
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
    double last_update;
    ColorRGBA color; /* entity-specific color sent by server (0 = use palette default) */
    int effective_level; /* clamped sum of all stat fields, sent by server for all entities */
};

// Player state structure
struct PlayerState {
    EntityState base; // Inheritance simulation
    char map_code[MAX_ID_LENGTH]; /* map code string, e.g. "fallback-map-0" */
    Vector2 path[MAX_PATH_POINTS];
    int path_count;
    Vector2 target_pos;
};

// Bot state structure
struct BotState {
    EntityState base; // Inheritance simulation
    char behavior[MAX_BEHAVIOR_LENGTH];
    char caster_id[MAX_ID_LENGTH];
};

// World object structure (for obstacles, portals, etc.)
typedef struct {
    char id[MAX_ID_LENGTH];
    Vector2 pos;
    Vector2 dims;
    char type[MAX_TYPE_LENGTH];
    char portal_label[MAX_ID_LENGTH]; // For portals
    ObjectLayerState object_layers[MAX_OBJECT_LAYERS];
    int object_layer_count;
    Color color; // Per-entity color (portals use subtype-specific colors)
} WorldObject;

// Color palette for the game
typedef struct {
    Color background;
    Color grid_background;
    Color floor_background;
    Color obstacle;
    Color foreground;
    Color player;
    Color other_player;
    Color path;
    Color target;
    Color aoi;
    Color debug_text;
    Color error_text;
    Color portal;
    Color portal_inter_portal;
    Color portal_inter_random;
    Color portal_intra_random;
    Color portal_intra_portal;
    Color portal_label;
    Color ui_text;
    Color map_boundary;
    Color grid;
    Color floor;
    Color bot;
    Color ghost;
    Color coin;
    Color weapon;
    Color skill;
} GameColors;

// Per-entity-type visual defaults received in init_data payload.
typedef struct {
    char entity_type[32];
    char live_item_ids[MAX_DEFAULT_ITEM_IDS][128];
    int  live_item_id_count;
    char dead_item_ids[MAX_DEFAULT_ITEM_IDS][128];
    int  dead_item_id_count;
    char color_key[32];
} EntityTypeDefault;

// Skill map entry received from init_data: one trigger item → N logic event IDs
typedef struct {
    char trigger_item_id[MAX_ID_LENGTH];
    char logic_event_ids[MAX_LOGIC_EVENT_IDS][MAX_ID_LENGTH];
    int  logic_event_count;
} SkillEntry;

// Main game state structure
struct GameState {
    // Player identification
    char player_id[MAX_ID_LENGTH];

    // Game configuration (from init_data)
    int grid_w;
    int grid_h;
    float cell_size;
    int fps;
    int interpolation_ms;
    float aoi_radius;
    float default_obj_width;
    float default_obj_height;

    // Graphics configuration
    float camera_smoothing;
    float camera_zoom;
    float default_width_screen_factor;
    float default_height_screen_factor;

    // Game colors
    GameColors colors;

    // Per-entity-type visual defaults (from init_data)
    EntityTypeDefault entity_defaults[MAX_ENTITY_TYPES];
    int entity_defaults_count;

    // Main player state
    PlayerState player;

    // Other entities
    PlayerState other_players[MAX_ENTITIES];
    int other_player_count;

    BotState bots[MAX_ENTITIES];
    int bot_count;

    // World objects
    WorldObject obstacles[MAX_OBJECTS];
    int obstacle_count;

    WorldObject foregrounds[MAX_OBJECTS];
    int foreground_count;

    WorldObject portals[MAX_OBJECTS];
    int portal_count;

    WorldObject floors[MAX_OBJECTS];
    int floor_count;

    // UI state
    char associated_item_ids[MAX_ENTITIES][MAX_ITEM_ID_LENGTH];
    int associated_item_count;
    SkillEntry skill_map[MAX_SKILL_ENTRIES];
    int skill_map_count;
    char last_error_message[MAX_MESSAGE_SIZE];
    double error_display_time;
    size_t download_size_bytes;
    size_t upload_size_bytes;

    // Stats
    int sum_stats_limit;
    int active_stats_sum;

    // Economy
    int player_coins; /* coin balance pushed by server each AOI frame */

    // Full inventory (all OLs including inactive) \u2014 self-player only.
    // Decoded from WriteSelfPlayer's writeFullInventory section.
    // Powers the inventory bottom bar and modal UI.
    ObjectLayerState full_inventory[MAX_OBJECT_LAYERS];
    int full_inventory_count;

    // Runtime flags
    bool init_received;
    bool dev_ui;
    double last_update_time;

    // Camera
    Camera2D camera;
};

// Global game state instance
extern GameState g_game_state;

/**
 * @brief Update entity position interpolation
 * @param delta_time Time elapsed since last update
 */
void game_state_update_interpolation(float delta_time);

/**
 * @brief Find player by ID in other_players array
 * @param id Player ID to search for
 * @return Pointer to PlayerState or NULL if not found
 */
PlayerState* game_state_find_player(const char* id);

/**
 * @brief Find bot by ID in bots array
 * @param id Bot ID to search for
 * @return Pointer to BotState or NULL if not found
 */
BotState* game_state_find_bot(const char* id);

/**
 * @brief Add or update a player in other_players array
 * @param player Player data to add/update
 * @return 0 on success, -1 on failure
 */
int game_state_update_player(const PlayerState* player);

/**
 * @brief Add or update a bot in bots array
 * @param bot Bot data to add/update
 * @return 0 on success, -1 on failure
 */
int game_state_update_bot(const BotState* bot);

/**
 * @brief Remove player from other_players array
 * @param id Player ID to remove
 */
void game_state_remove_player(const char* id);

/**
 * @brief Remove bot from bots array
 * @param id Bot ID to remove
 */
void game_state_remove_bot(const char* id);

/**
 * @brief Initialize camera based on current game state
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 */
void game_state_init_camera(int screen_width, int screen_height);

/**
 * @brief Update camera position to follow player
 */
void game_state_update_camera(void);

/**
 * @brief Update camera offset when screen size changes
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 */
void game_state_update_camera_offset(int screen_width, int screen_height);

/**
 * @brief Look up entity type defaults by entity type string.
 * @return Pointer to matching EntityTypeDefault, or NULL if not found.
 */
static inline const EntityTypeDefault* game_state_get_entity_default(const char* entity_type) {
    for (int i = 0; i < g_game_state.entity_defaults_count; i++) {
        if (strcmp(g_game_state.entity_defaults[i].entity_type, entity_type) == 0)
            return &g_game_state.entity_defaults[i];
    }
    return NULL;
}

/**
 * @brief Resolve a color palette key string to the matching GameColors field.
 * Falls back to a neutral gray when the key is unrecognised or NULL.
 */
static inline Color game_state_get_color_by_key(const char* key) {
    if (!key || key[0] == '\0') return (Color){ 100, 100, 100, 200 };
    if (strcmp(key, "PLAYER")       == 0) return g_game_state.colors.player;
    if (strcmp(key, "OTHER_PLAYER") == 0) return g_game_state.colors.other_player;
    if (strcmp(key, "BOT")          == 0) return g_game_state.colors.bot;
    if (strcmp(key, "SKILL")        == 0) return g_game_state.colors.skill;
    if (strcmp(key, "COIN")         == 0) return g_game_state.colors.coin;
    if (strcmp(key, "GHOST")        == 0) return g_game_state.colors.ghost;
    if (strcmp(key, "FLOOR")        == 0) return g_game_state.colors.floor;
    if (strcmp(key, "OBSTACLE")     == 0) return g_game_state.colors.obstacle;
    if (strcmp(key, "PORTAL")              == 0) return g_game_state.colors.portal;
    if (strcmp(key, "PORTAL_INTER_PORTAL") == 0) return g_game_state.colors.portal_inter_portal;
    if (strcmp(key, "PORTAL_INTER_RANDOM") == 0) return g_game_state.colors.portal_inter_random;
    if (strcmp(key, "PORTAL_INTRA_RANDOM") == 0) return g_game_state.colors.portal_intra_random;
    if (strcmp(key, "PORTAL_INTRA_PORTAL") == 0) return g_game_state.colors.portal_intra_portal;
    if (strcmp(key, "FOREGROUND")   == 0) return g_game_state.colors.foreground;
    return (Color){ 100, 100, 100, 200 };
}

/**
 * @brief Return the player's current coin balance.
 *
 * The value is pushed by the server in every AOI frame as a dedicated u32
 * field at the end of the self-player section.  It is stored directly in
 * GameState.player_coins so no object-layer scanning is needed.
 */
static inline int game_state_get_player_coins(void) {
    return g_game_state.player_coins;
}

#endif // GAME_STATE_H