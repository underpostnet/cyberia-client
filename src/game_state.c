#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(PLATFORM_WEB)
    #include <math.h>
#endif

// Global game state instance
GameState g_game_state = {0};

int game_state_init(void) {
    printf("[GAME_STATE] Initializing game state...\n");
    
#if !defined(PLATFORM_WEB)
    // Initialize mutex for desktop builds
    if (pthread_mutex_init(&g_game_state.mutex, NULL) != 0) {
        printf("[GAME_STATE] Failed to initialize mutex\n");
        return -1;
    }
#endif
    
    // Initialize default values
    memset(&g_game_state, 0, sizeof(GameState));
    
    // Set default configuration values
    g_game_state.grid_w = 100;
    g_game_state.grid_h = 100;
    g_game_state.cell_size = 12.0f;
    g_game_state.fps = 60;
    g_game_state.interpolation_ms = 200;
    g_game_state.aoi_radius = 15.0f;
    g_game_state.default_obj_width = 1.0f;
    g_game_state.default_obj_height = 1.0f;
    
    // Graphics defaults
    g_game_state.camera_smoothing = 0.15f;
    g_game_state.camera_zoom = 1.0f;
    g_game_state.default_width_screen_factor = 0.5f;
    g_game_state.default_height_screen_factor = 0.5f;
    
    // Color defaults
    g_game_state.colors.background = (Color){30, 30, 30, 255};
    g_game_state.colors.grid_background = (Color){20, 20, 20, 255};
    g_game_state.colors.floor_background = (Color){25, 25, 25, 255};
    g_game_state.colors.foreground = (Color){60, 140, 60, 220};
    g_game_state.colors.target = (Color){255, 255, 0, 255};
    g_game_state.colors.path = (Color){0, 255, 0, 128};
    g_game_state.colors.aoi = (Color){255, 0, 255, 51};
    g_game_state.colors.grid = (Color){100, 100, 100, 255};
    g_game_state.colors.map_boundary = (Color){150, 150, 150, 255};
    g_game_state.colors.player = (Color){0, 162, 232, 255};
    g_game_state.colors.bot = (Color){255, 127, 39, 255};
    g_game_state.colors.obstacle = (Color){139, 69, 19, 255};
    g_game_state.colors.portal = (Color){138, 43, 226, 255};
    g_game_state.colors.floor = (Color){105, 105, 105, 255};
    
    // Initialize player state
    strncpy(g_game_state.player_id, "", MAX_ID_LENGTH - 1);
    g_game_state.player.base.pos_server = (Vector2){0, 0};
    g_game_state.player.base.pos_prev = (Vector2){0, 0};
    g_game_state.player.base.interp_pos = (Vector2){0, 0};
    g_game_state.player.base.dims = (Vector2){1.0f, 1.0f};
    g_game_state.player.base.direction = DIRECTION_NONE;
    g_game_state.player.base.mode = MODE_IDLE;
    g_game_state.player.base.life = 100.0f;
    g_game_state.player.base.max_life = 100.0f;
    g_game_state.player.target_pos = (Vector2){-1, -1};
    
    // Stats limit
    g_game_state.sum_stats_limit = 9999;
    
    // Runtime state
    g_game_state.init_received = false;
    g_game_state.dev_ui = false;
    g_game_state.camera_initialized = false;
    g_game_state.last_update_time = GetTime();
    
    strncpy(g_game_state.last_error_message, "", MAX_MESSAGE_SIZE - 1);
    
    printf("[GAME_STATE] Initialization complete\n");
    return 0;
}

void game_state_cleanup(void) {
    printf("[GAME_STATE] Cleaning up game state...\n");
    
#if !defined(PLATFORM_WEB)
    // Destroy mutex for desktop builds
    pthread_mutex_destroy(&g_game_state.mutex);
#endif
    
    // Clear all data
    memset(&g_game_state, 0, sizeof(GameState));
    
    printf("[GAME_STATE] Cleanup complete\n");
}

void game_state_lock(void) {
#if !defined(PLATFORM_WEB)
    pthread_mutex_lock(&g_game_state.mutex);
#endif
}

void game_state_unlock(void) {
#if !defined(PLATFORM_WEB)
    pthread_mutex_unlock(&g_game_state.mutex);
#endif
}

/**
 * Update position interpolation for all entities.
 * 
 * This function implements smooth position transitions to prevent flickering and jumps.
 * It interpolates between pos_prev (last interpolated position) and pos_server (new position
 * from server) over the interpolation_ms time window.
 * 
 * How it works:
 * 1. When a new server position arrives:
 *    - pos_server = new position from server
 *    - pos_prev = current interp_pos (preserved in update functions)
 *    - interp_pos = current interp_pos (no immediate jump)
 * 
 * 2. This function gradually moves interp_pos from pos_prev to pos_server:
 *    - interp_factor starts at 0 (just after update) and grows to 1.0
 *    - interp_pos = lerp(pos_prev, pos_server, interp_factor)
 * 
 * This creates smooth movement even when server updates are infrequent.
 */
void game_state_update_interpolation(float delta_time) {
    game_state_lock();
    
    double current_time = GetTime();
    // Calculate interpolation factor based on time since last server update
    float interp_factor = g_game_state.interpolation_ms > 0 ? 
        (float)(current_time - g_game_state.last_update_time) * 1000.0f / g_game_state.interpolation_ms : 1.0f;
    
    // Clamp to 1.0 to prevent overshooting
    if (interp_factor > 1.0f) interp_factor = 1.0f;
    
    // Interpolate main player position (linear interpolation from prev to server position)
    g_game_state.player.base.interp_pos.x = g_game_state.player.base.pos_prev.x + 
        (g_game_state.player.base.pos_server.x - g_game_state.player.base.pos_prev.x) * interp_factor;
    g_game_state.player.base.interp_pos.y = g_game_state.player.base.pos_prev.y + 
        (g_game_state.player.base.pos_server.y - g_game_state.player.base.pos_prev.y) * interp_factor;
    
    // Interpolate other players
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* player = &g_game_state.other_players[i];
        player->base.interp_pos.x = player->base.pos_prev.x + 
            (player->base.pos_server.x - player->base.pos_prev.x) * interp_factor;
        player->base.interp_pos.y = player->base.pos_prev.y + 
            (player->base.pos_server.y - player->base.pos_prev.y) * interp_factor;
    }
    
    // Interpolate bots
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* bot = &g_game_state.bots[i];
        bot->base.interp_pos.x = bot->base.pos_prev.x + 
            (bot->base.pos_server.x - bot->base.pos_prev.x) * interp_factor;
        bot->base.interp_pos.y = bot->base.pos_prev.y + 
            (bot->base.pos_server.y - bot->base.pos_prev.y) * interp_factor;
    }
    
    game_state_unlock();
}

PlayerState* game_state_find_player(const char* id) {
    if (!id) return NULL;
    
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        if (strcmp(g_game_state.other_players[i].base.id, id) == 0) {
            return &g_game_state.other_players[i];
        }
    }
    return NULL;
}

BotState* game_state_find_bot(const char* id) {
    if (!id) return NULL;
    
    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (strcmp(g_game_state.bots[i].base.id, id) == 0) {
            return &g_game_state.bots[i];
        }
    }
    return NULL;
}

int game_state_update_player(const PlayerState* player) {
    if (!player) return -1;
    
    PlayerState* existing = game_state_find_player(player->base.id);
    if (existing) {
        // Update existing player - preserve interpolation state for smooth transitions
        Vector2 prev_interp_pos = existing->base.interp_pos;
        *existing = *player;
        // Set pos_prev to the last interpolated position (or current server pos if it's the same)
        existing->base.pos_prev = prev_interp_pos;
        // Keep interp_pos at its current value - interpolation will smoothly update it
        existing->base.interp_pos = prev_interp_pos;
        return 0;
    }
    
    // Add new player - no previous position, so use server position
    if (g_game_state.other_player_count >= MAX_ENTITIES) {
        printf("[GAME_STATE] Cannot add player, maximum entities reached\n");
        return -1;
    }
    
    g_game_state.other_players[g_game_state.other_player_count] = *player;
    g_game_state.other_player_count++;
    return 0;
}

int game_state_update_bot(const BotState* bot) {
    if (!bot) return -1;
    
    BotState* existing = game_state_find_bot(bot->base.id);
    if (existing) {
        // Update existing bot - preserve interpolation state for smooth transitions
        Vector2 prev_interp_pos = existing->base.interp_pos;
        *existing = *bot;
        // Set pos_prev to the last interpolated position (or current server pos if it's the same)
        existing->base.pos_prev = prev_interp_pos;
        // Keep interp_pos at its current value - interpolation will smoothly update it
        existing->base.interp_pos = prev_interp_pos;
        return 0;
    }
    
    // Add new bot - no previous position, so use server position
    if (g_game_state.bot_count >= MAX_ENTITIES) {
        printf("[GAME_STATE] Cannot add bot, maximum entities reached\n");
        return -1;
    }
    
    g_game_state.bots[g_game_state.bot_count] = *bot;
    g_game_state.bot_count++;
    return 0;
}

void game_state_remove_player(const char* id) {
    if (!id) return;
    
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        if (strcmp(g_game_state.other_players[i].base.id, id) == 0) {
            // Shift remaining players
            for (int j = i; j < g_game_state.other_player_count - 1; j++) {
                g_game_state.other_players[j] = g_game_state.other_players[j + 1];
            }
            g_game_state.other_player_count--;
            return;
        }
    }
}

void game_state_remove_bot(const char* id) {
    if (!id) return;
    
    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (strcmp(g_game_state.bots[i].base.id, id) == 0) {
            // Shift remaining bots
            for (int j = i; j < g_game_state.bot_count - 1; j++) {
                g_game_state.bots[j] = g_game_state.bots[j + 1];
            }
            g_game_state.bot_count--;
            return;
        }
    }
}

const char* direction_to_string(Direction dir) {
    switch (dir) {
        case DIRECTION_UP: return "UP";
        case DIRECTION_UP_RIGHT: return "UP_RIGHT";
        case DIRECTION_RIGHT: return "RIGHT";
        case DIRECTION_DOWN_RIGHT: return "DOWN_RIGHT";
        case DIRECTION_DOWN: return "DOWN";
        case DIRECTION_DOWN_LEFT: return "DOWN_LEFT";
        case DIRECTION_LEFT: return "LEFT";
        case DIRECTION_UP_LEFT: return "UP_LEFT";
        case DIRECTION_NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

const char* mode_to_string(ObjectLayerMode mode) {
    switch (mode) {
        case MODE_IDLE: return "IDLE";
        case MODE_WALKING: return "WALKING";
        case MODE_TELEPORTING: return "TELEPORTING";
        default: return "UNKNOWN";
    }
}

void game_state_init_camera(int screen_width, int screen_height) {
    if (g_game_state.camera_initialized) return;
    
    game_state_lock();
    
    // Calculate initial target position (player center in world coordinates)
    float player_center_x = (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * g_game_state.cell_size;
    float player_center_y = (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * g_game_state.cell_size;
    
    Vector2 offset = {screen_width / 2.0f, screen_height / 2.0f};
    Vector2 target = {player_center_x, player_center_y};
    
    g_game_state.camera.offset = offset;
    g_game_state.camera.target = target;
    g_game_state.camera.rotation = 0.0f;
    g_game_state.camera.zoom = g_game_state.camera_zoom;
    
    g_game_state.camera_initialized = true;
    
    printf("[GAME_STATE] Camera initialized - Target: (%.1f, %.1f), Zoom: %.2f\n", 
           target.x, target.y, g_game_state.camera_zoom);
    
    game_state_unlock();
}

void game_state_update_camera(void) {
    if (!g_game_state.camera_initialized) return;
    
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    // Calculate desired target position (smooth follow player)
    // Use player's center so camera keeps player centered regardless of its dimensions
    float player_center_x = (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * cell_size;
    float player_center_y = (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * cell_size;
    
    Vector2 desired_target = {player_center_x, player_center_y};
    
    // Apply camera smoothing
    float smoothing = g_game_state.camera_smoothing > 0 ? g_game_state.camera_smoothing : 0.15f;
    
    g_game_state.camera.target.x += (desired_target.x - g_game_state.camera.target.x) * smoothing;
    g_game_state.camera.target.y += (desired_target.y - g_game_state.camera.target.y) * smoothing;
    
    game_state_unlock();
}

void game_state_update_camera_offset(int screen_width, int screen_height) {
    if (!g_game_state.camera_initialized) return;
    
    game_state_lock();
    
    // Keep camera offset centered on screen (good practice for proper rendering)
    g_game_state.camera.offset.x = screen_width / 2.0f;
    g_game_state.camera.offset.y = screen_height / 2.0f;
    
    game_state_unlock();
}