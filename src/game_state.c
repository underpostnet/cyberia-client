#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <raylib.h>
#include <assert.h>

// Global game state instance
GameState g_game_state = {0};

/**
 * Compute a Direction enum from a 2D movement vector.
 * Returns DIRECTION_NONE when the vector is near-zero.
 */
static Direction direction_from_delta(float dx, float dy) {
    if (dx * dx + dy * dy < 0.0001f) return DIRECTION_NONE;
    float angle = atan2f(dy, dx); // radians, right=0, down=+π/2
    // Map to 8 compass directions (each covers 45°)
    // Boundaries at ±22.5° from each axis
    if (angle < -2.748893f) return DIRECTION_LEFT;        // (-π, -157.5°)
    if (angle < -1.963495f) return DIRECTION_UP_LEFT;     // (-157.5°, -112.5°)
    if (angle < -1.178097f) return DIRECTION_UP;          // (-112.5°, -67.5°)
    if (angle < -0.392699f) return DIRECTION_UP_RIGHT;    // (-67.5°, -22.5°)
    if (angle <  0.392699f) return DIRECTION_RIGHT;       // (-22.5°, +22.5°)
    if (angle <  1.178097f) return DIRECTION_DOWN_RIGHT;  // (+22.5°, +67.5°)
    if (angle <  1.963495f) return DIRECTION_DOWN;        // (+67.5°, +112.5°)
    if (angle <  2.748893f) return DIRECTION_DOWN_LEFT;   // (+112.5°, +157.5°)
    return DIRECTION_LEFT;                                 // (+157.5°, +π)
}

/**
 * Update position interpolation for all entities.
 *
 * Main player: local prediction toward tap target + exponential blend
 * toward server position. This provides:
 *   - Immediate visual feedback on TAP (no latency gap)
 *   - Smooth server correction (no backward snaps)
 *   - Frame-rate independent behavior via exponential decay
 *
 * Other players / bots: linear interpolation between pos_prev and
 * pos_server over the interpolation_ms window (unchanged).
 */
void game_state_update_interpolation(float delta_time) {
    const double current_time = GetTime();

    // === Main player: prediction + exponential server correction ===
    {
        PlayerState* p = &g_game_state.player;
        float speed = p->estimated_speed > 0.1f ? p->estimated_speed : 3.0f;

        // Track frame movement for predicted direction
        float frame_dx = 0.0f;
        float frame_dy = 0.0f;

        // Local prediction: move interp_pos toward tap target
        if (p->has_tap_target) {
            float dx = p->tap_target.x - p->base.interp_pos.x;
            float dy = p->tap_target.y - p->base.interp_pos.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 0.05f) {
                float step = speed * delta_time;
                if (step > dist) step = dist;
                float mx = (dx / dist) * step;
                float my = (dy / dist) * step;
                p->base.interp_pos.x += mx;
                p->base.interp_pos.y += my;
                frame_dx += mx;
                frame_dy += my;
            } else {
                p->has_tap_target = false;
            }
        }

        // Exponential blend toward server position (smooth correction)
        float correction_rate = 8.0f;
        float blend = 1.0f - expf(-correction_rate * delta_time);
        float cx = (p->base.pos_server.x - p->base.interp_pos.x) * blend;
        float cy = (p->base.pos_server.y - p->base.interp_pos.y) * blend;
        p->base.interp_pos.x += cx;
        p->base.interp_pos.y += cy;
        frame_dx += cx;
        frame_dy += cy;

        // Predicted direction & mode from the frame's actual movement vector.
        // This keeps the sprite facing the direction it visually moves,
        // regardless of whether the server update has arrived yet.
        Direction predicted_dir = direction_from_delta(frame_dx, frame_dy);
        if (predicted_dir != DIRECTION_NONE) {
            p->base.direction = predicted_dir;
            p->base.mode = MODE_WALKING;
        } else if (!p->has_tap_target) {
            // Stopped predicting — let server direction/mode take over.
            // Don't force IDLE here; the server already sends the right state.
        }
    }

    // === Other players and bots: linear interpolation (unchanged) ===
    float interp_factor = 1.0f;
    if(g_game_state.interpolation_ms > 0)
    {
        interp_factor = (current_time - g_game_state.last_update_time) * 1000.0f / g_game_state.interpolation_ms;
        if (interp_factor > 1.0f)
        {
            interp_factor = 1.0f;
        }
    }

    // Interpolate other players
    for (int i = 0; i < g_game_state.other_player_count; i++)
    {
        PlayerState* player = &g_game_state.other_players[i];
        player->base.interp_pos.x = player->base.pos_prev.x +
            (player->base.pos_server.x - player->base.pos_prev.x) * interp_factor;
        player->base.interp_pos.y = player->base.pos_prev.y +
            (player->base.pos_server.y - player->base.pos_prev.y) * interp_factor;
    }

    // Interpolate bots
    for (int i = 0; i < g_game_state.bot_count; i++)
    {
        BotState* bot = &g_game_state.bots[i];
        bot->base.interp_pos.x = bot->base.pos_prev.x +
            (bot->base.pos_server.x - bot->base.pos_prev.x) * interp_factor;
        bot->base.interp_pos.y = bot->base.pos_prev.y +
            (bot->base.pos_server.y - bot->base.pos_prev.y) * interp_factor;
    }
}

PlayerState* game_state_find_player(const char* id) {
    assert(id);

    for (int i = 0; i < g_game_state.other_player_count; i++) {
        if (strcmp(g_game_state.other_players[i].base.id, id) == 0) {
            return &g_game_state.other_players[i];
        }
    }
    return NULL;
}

BotState* game_state_find_bot(const char* id) {
    assert(id);

    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (strcmp(g_game_state.bots[i].base.id, id) == 0) {
            return &g_game_state.bots[i];
        }
    }
    return NULL;
}

int game_state_update_player(const PlayerState* player) {
    assert(player);

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
    assert(bot);

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
    assert(id);

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
    assert(id);

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

void game_state_init_camera(int screen_width, int screen_height) {
    // Calculate initial target position (player center in world coordinates)
    float player_center_x = (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * g_game_state.cell_size;
    float player_center_y = (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * g_game_state.cell_size;

    Vector2 offset = {screen_width / 2.0f, screen_height / 2.0f};
    Vector2 target = {player_center_x, player_center_y};

    g_game_state.camera.offset = offset;
    g_game_state.camera.target = target;
    g_game_state.camera.rotation = 0.0f;
    g_game_state.camera.zoom = g_game_state.camera_zoom;

    printf("[GAME_STATE] Camera initialized - Target: (%.1f, %.1f), Zoom: %.2f\n",
           target.x, target.y, g_game_state.camera_zoom);
}

void game_state_update_camera(float delta_time) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Calculate desired target position (smooth follow player)
    // Use player's center so camera keeps player centered regardless of its dimensions
    float player_center_x = (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * cell_size;
    float player_center_y = (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * cell_size;

    Vector2 desired_target = {player_center_x, player_center_y};

    // Frame-rate independent exponential smoothing.
    // camera_smoothing 0.15 was tuned at 60 fps → lambda ≈ 10.
    float lambda = 10.0f;
    float blend = 1.0f - expf(-lambda * delta_time);

    g_game_state.camera.target.x += (desired_target.x - g_game_state.camera.target.x) * blend;
    g_game_state.camera.target.y += (desired_target.y - g_game_state.camera.target.y) * blend;
}

void game_state_update_camera_offset(int screen_width, int screen_height) {
    // Keep camera offset centered on screen (good practice for proper rendering)
    g_game_state.camera.offset.x = screen_width / 2.0f;
    g_game_state.camera.offset.y = screen_height / 2.0f;
}
