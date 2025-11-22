#include "game_render.h"
#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global game renderer instance
GameRenderer g_renderer = {0};

int game_render_init(int screen_width, int screen_height) {
    printf("[GAME_RENDER] Initializing game renderer (%dx%d)...\n", screen_width, screen_height);
    
    // Initialize renderer state
    memset(&g_renderer, 0, sizeof(GameRenderer));
    
    g_renderer.screen_width = screen_width;
    g_renderer.screen_height = screen_height;
    
    // Initialize texture cache
    g_renderer.texture_cache.capacity = 100;
    g_renderer.texture_cache.texture_count = 0;
    
    // Initialize effect arrays
    g_renderer.floating_text_count = 0;
    g_renderer.click_effect_count = 0;
    
    // Initialize render flags
    g_renderer.show_debug_info = false;
    g_renderer.show_grid = true;
    g_renderer.show_aoi = false;
    g_renderer.show_paths = true;
    g_renderer.hud_visible = true;
    
    // Performance tracking
    g_renderer.frames_rendered = 0;
    g_renderer.last_fps_update = GetTime();
    g_renderer.current_fps = 60.0f;
    
    // Font loading (using default for now)
    g_renderer.font_loaded = false; // Will use default font
    
    printf("[GAME_RENDER] Game renderer initialized successfully\n");
    return 0;
}

void game_render_set_screen_size(int width, int height) {
    g_renderer.screen_width = width;
    g_renderer.screen_height = height;
    
    // Update HUD area
    g_renderer.hud_area.x = 0;
    g_renderer.hud_area.y = height - 60; // Bottom 60 pixels for HUD
    g_renderer.hud_area.width = width;
    g_renderer.hud_area.height = 60;
}

void game_render_frame(void) {
    BeginDrawing();
    
    // Clear background
    ClearBackground(g_game_state.colors.background);
    
    // Begin camera mode for world rendering
    if (g_game_state.camera_initialized) {
        BeginMode2D(g_game_state.camera);
        game_render_world();
        EndMode2D();
    }
    
    // Render UI (screen space)
    game_render_ui();
    
    // Update performance tracking
    g_renderer.frames_rendered++;
    double current_time = GetTime();
    if (current_time - g_renderer.last_fps_update >= 1.0) {
        g_renderer.current_fps = g_renderer.frames_rendered / (current_time - g_renderer.last_fps_update);
        g_renderer.frames_rendered = 0;
        g_renderer.last_fps_update = current_time;
    }
    
    EndDrawing();
}

void game_render_world(void) {
    // Render world components in order
    if (g_renderer.show_grid) {
        game_render_grid();
    }
    
    game_render_floors();
    game_render_world_objects();
    game_render_entities();
    
    if (g_renderer.show_paths) {
        game_render_player_path();
    }
    
    if (g_renderer.show_aoi) {
        game_render_aoi_circle();
    }
    
    game_render_click_effects();
    game_render_floating_texts();
    
    if (g_renderer.show_debug_info) {
        game_render_debug_overlay();
    }
}

void game_render_grid(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    int grid_w = g_game_state.grid_w;
    int grid_h = g_game_state.grid_h;
    Color grid_color = g_game_state.colors.grid;
    
    game_state_unlock();
    
    // Get camera bounds to only draw visible grid lines
    Rectangle camera_bounds = game_render_get_camera_bounds();
    
    // Calculate visible grid range
    int start_x = (int)(camera_bounds.x / cell_size) - 1;
    int end_x = (int)((camera_bounds.x + camera_bounds.width) / cell_size) + 1;
    int start_y = (int)(camera_bounds.y / cell_size) - 1;
    int end_y = (int)((camera_bounds.y + camera_bounds.height) / cell_size) + 1;
    
    // Clamp to grid bounds
    if (start_x < 0) start_x = 0;
    if (end_x > grid_w) end_x = grid_w;
    if (start_y < 0) start_y = 0;
    if (end_y > grid_h) end_y = grid_h;
    
    // Draw vertical lines
    for (int x = start_x; x <= end_x; x++) {
        float line_x = x * cell_size;
        DrawLine(line_x, start_y * cell_size, line_x, end_y * cell_size, grid_color);
    }
    
    // Draw horizontal lines
    for (int y = start_y; y <= end_y; y++) {
        float line_y = y * cell_size;
        DrawLine(start_x * cell_size, line_y, end_x * cell_size, line_y, grid_color);
    }
}

void game_render_floors(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    Color floor_color = g_game_state.colors.floor;
    
    for (int i = 0; i < g_game_state.floor_count; i++) {
        WorldObject* floor = &g_game_state.floors[i];
        
        Rectangle rect = {
            floor->pos.x * cell_size,
            floor->pos.y * cell_size,
            floor->dims.x * cell_size,
            floor->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, floor_color);
    }
    
    game_state_unlock();
}

void game_render_world_objects(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    // Render obstacles
    for (int i = 0; i < g_game_state.obstacle_count; i++) {
        WorldObject* obj = &g_game_state.obstacles[i];
        
        Rectangle rect = {
            obj->pos.x * cell_size,
            obj->pos.y * cell_size,
            obj->dims.x * cell_size,
            obj->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.obstacle);
    }
    
    // Render portals
    for (int i = 0; i < g_game_state.portal_count; i++) {
        WorldObject* obj = &g_game_state.portals[i];
        
        Rectangle rect = {
            obj->pos.x * cell_size,
            obj->pos.y * cell_size,
            obj->dims.x * cell_size,
            obj->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.portal);
    }
    
    // Render foregrounds
    for (int i = 0; i < g_game_state.foreground_count; i++) {
        WorldObject* obj = &g_game_state.foregrounds[i];
        
        Rectangle rect = {
            obj->pos.x * cell_size,
            obj->pos.y * cell_size,
            obj->dims.x * cell_size,
            obj->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.foreground);
    }
    
    game_state_unlock();
}

void game_render_entities(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    // Render main player
    Rectangle player_rect = {
        g_game_state.player.base.interp_pos.x * cell_size,
        g_game_state.player.base.interp_pos.y * cell_size,
        g_game_state.player.base.dims.x * cell_size,
        g_game_state.player.base.dims.y * cell_size
    };
    DrawRectangleRec(player_rect, g_game_state.colors.player);
    
    // Render other players
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* player = &g_game_state.other_players[i];
        
        Rectangle rect = {
            player->base.interp_pos.x * cell_size,
            player->base.interp_pos.y * cell_size,
            player->base.dims.x * cell_size,
            player->base.dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.player);
    }
    
    // Render bots
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* bot = &g_game_state.bots[i];
        
        Rectangle rect = {
            bot->base.interp_pos.x * cell_size,
            bot->base.interp_pos.y * cell_size,
            bot->base.dims.x * cell_size,
            bot->base.dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.bot);
    }
    
    game_state_unlock();
}

void game_render_player_path(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    // Render target position
    if (g_game_state.player.target_pos.x >= 0 && g_game_state.player.target_pos.y >= 0) {
        Rectangle target_rect = {
            g_game_state.player.target_pos.x * cell_size,
            g_game_state.player.target_pos.y * cell_size,
            cell_size,
            cell_size
        };
        DrawRectangleRec(target_rect, g_game_state.colors.target);
    }
    
    // Render path
    for (int i = 0; i < g_game_state.player.path_count; i++) {
        Vector2 path_point = g_game_state.player.path[i];
        
        Rectangle path_rect = {
            path_point.x * cell_size,
            path_point.y * cell_size,
            cell_size,
            cell_size
        };
        
        DrawRectangleRec(path_rect, g_game_state.colors.path);
    }
    
    game_state_unlock();
}

void game_render_aoi_circle(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    float aoi_radius = g_game_state.aoi_radius > 0 ? g_game_state.aoi_radius : 15.0f;
    
    // Calculate player center
    float center_x = (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * cell_size;
    float center_y = (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * cell_size;
    
    DrawCircleV((Vector2){center_x, center_y}, aoi_radius * cell_size, g_game_state.colors.aoi);
    
    game_state_unlock();
}

void game_render_ui(void) {
    // Render connection status
    game_render_connection_status();
    
    // Render location info
    game_render_location_info();
    
    // Render performance info if debug mode
    if (g_renderer.show_debug_info) {
        game_render_performance_info();
    }
    
    // Render error messages
    game_render_error_messages();
}

void game_render_location_info(void) {
    game_state_lock();
    
    int map_id = g_game_state.player.map_id;
    Vector2 pos = g_game_state.player.base.interp_pos;
    
    game_state_unlock();
    
    char map_text[64];
    char pos_text[64];
    snprintf(map_text, sizeof(map_text), "Map: %d", map_id);
    snprintf(pos_text, sizeof(pos_text), "(%.1f, %.1f)", pos.x, pos.y);
    
    int font_size = 18;
    int padding = 10;
    
    // Draw with shadow effect
    DrawText(map_text, padding + 1, padding + 1, font_size, BLACK);
    DrawText(map_text, padding, padding, font_size, YELLOW);
    
    DrawText(pos_text, padding + 1, padding + font_size + 6, font_size, BLACK);
    DrawText(pos_text, padding, padding + font_size + 5, font_size, YELLOW);
}

void game_render_connection_status(void) {
    // This will be implemented when client connection status is available
    // For now, just show a simple indicator
    const char* status = "Connected"; // TODO: Get from client
    int text_width = MeasureText(status, 16);
    
    DrawText(status, g_renderer.screen_width - text_width - 10, 10, 16, GREEN);
}

void game_render_performance_info(void) {
    char fps_text[32];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", g_renderer.current_fps);
    
    int y_offset = g_renderer.screen_height - 80;
    DrawText(fps_text, 10, y_offset, 16, WHITE);
}

void game_render_error_messages(void) {
    game_state_lock();
    
    if (strlen(g_game_state.last_error_message) > 0 && 
        GetTime() - g_game_state.error_display_time < 5.0) {
        
        int text_width = MeasureText(g_game_state.last_error_message, 20);
        int x = (g_renderer.screen_width - text_width) / 2;
        int y = g_renderer.screen_height / 2 - 50;
        
        // Draw error with background
        Rectangle bg_rect = {x - 10, y - 5, text_width + 20, 30};
        DrawRectangleRec(bg_rect, (Color){255, 0, 0, 180});
        DrawText(g_game_state.last_error_message, x, y, 20, WHITE);
    }
    
    game_state_unlock();
}

void game_render_click_effects(void) {
    // Update and render click effects
    float delta_time = GetFrameTime();
    
    for (int i = 0; i < g_renderer.click_effect_count; i++) {
        ClickEffect* effect = &g_renderer.click_effects[i];
        if (!effect->active) continue;
        
        // Update effect
        effect->life_time += delta_time;
        float progress = effect->life_time / effect->max_life_time;
        
        if (progress >= 1.0f) {
            effect->active = false;
            continue;
        }
        
        // Animate radius
        effect->radius = effect->max_radius * progress;
        
        // Fade alpha
        Color render_color = effect->color;
        render_color.a = (unsigned char)(effect->color.a * (1.0f - progress));
        
        // Render
        DrawCircleV(effect->position, effect->radius, render_color);
    }
}

void game_render_floating_texts(void) {
    // Update and render floating texts
    float delta_time = GetFrameTime();
    
    for (int i = 0; i < g_renderer.floating_text_count; i++) {
        FloatingText* text = &g_renderer.floating_texts[i];
        if (!text->active) continue;
        
        // Update text
        text->life_time += delta_time;
        text->position.x += text->velocity.x * delta_time;
        text->position.y += text->velocity.y * delta_time;
        
        float progress = text->life_time / text->max_life_time;
        
        if (progress >= 1.0f) {
            text->active = false;
            continue;
        }
        
        // Fade alpha
        Color render_color = text->color;
        render_color.a = (unsigned char)(text->color.a * (1.0f - progress));
        
        // Convert world to screen coordinates
        Vector2 screen_pos = game_render_world_to_screen(text->position);
        
        // Render
        DrawText(text->text, (int)screen_pos.x, (int)screen_pos.y, (int)text->font_size, render_color);
    }
}

void game_render_debug_overlay(void) {
    // This will show debug information when dev mode is enabled
    // For now, just show basic info
    DrawText("DEBUG MODE", g_renderer.screen_width - 120, 30, 16, RED);
}

Vector2 game_render_world_to_screen(Vector2 world_pos) {
    if (!g_game_state.camera_initialized) {
        return world_pos;
    }
    
    return GetWorldToScreen2D(world_pos, g_game_state.camera);
}

Vector2 game_render_screen_to_world(Vector2 screen_pos) {
    if (!g_game_state.camera_initialized) {
        return screen_pos;
    }
    
    return GetScreenToWorld2D(screen_pos, g_game_state.camera);
}

Rectangle game_render_get_camera_bounds(void) {
    if (!g_game_state.camera_initialized) {
        return (Rectangle){0, 0, g_renderer.screen_width, g_renderer.screen_height};
    }
    
    Camera2D camera = g_game_state.camera;
    
    // Calculate camera bounds in world coordinates
    Vector2 top_left = GetScreenToWorld2D((Vector2){0, 0}, camera);
    Vector2 bottom_right = GetScreenToWorld2D((Vector2){g_renderer.screen_width, g_renderer.screen_height}, camera);
    
    return (Rectangle){
        top_left.x,
        top_left.y,
        bottom_right.x - top_left.x,
        bottom_right.y - top_left.y
    };
}

void game_render_add_click_effect(Vector2 world_pos, Color color) {
    // Find an inactive effect slot
    for (int i = 0; i < 20; i++) {
        ClickEffect* effect = &g_renderer.click_effects[i];
        if (!effect->active) {
            effect->position = world_pos;
            effect->color = color;
            effect->radius = 0.0f;
            effect->max_radius = 20.0f;
            effect->life_time = 0.0f;
            effect->max_life_time = 0.5f;
            effect->active = true;
            
            if (i >= g_renderer.click_effect_count) {
                g_renderer.click_effect_count = i + 1;
            }
            break;
        }
    }
}

void game_render_add_floating_text(Vector2 world_pos, const char* text, 
                                   Color color, float font_size, float life_time) {
    // Find an inactive text slot
    for (int i = 0; i < 100; i++) {
        FloatingText* ft = &g_renderer.floating_texts[i];
        if (!ft->active) {
            ft->position = world_pos;
            ft->velocity = (Vector2){0, -50}; // Float upward
            strncpy(ft->text, text, sizeof(ft->text) - 1);
            ft->text[sizeof(ft->text) - 1] = '\0';
            ft->color = color;
            ft->font_size = font_size;
            ft->life_time = 0.0f;
            ft->max_life_time = life_time;
            ft->active = true;
            
            if (i >= g_renderer.floating_text_count) {
                g_renderer.floating_text_count = i + 1;
            }
            break;
        }
    }
}

void game_render_cleanup(void) {
    printf("[GAME_RENDER] Cleaning up game renderer...\n");
    
    // Cleanup texture cache
    game_render_clear_texture_cache();
    
    // Unload font if loaded
    if (g_renderer.font_loaded) {
        UnloadFont(g_renderer.game_font);
        g_renderer.font_loaded = false;
    }
    
    // Clear renderer state
    memset(&g_renderer, 0, sizeof(GameRenderer));
    
    printf("[GAME_RENDER] Game renderer cleanup complete\n");
}

void game_render_clear_texture_cache(void) {
    // TODO: Implement when texture caching is added
    g_renderer.texture_cache.texture_count = 0;
}