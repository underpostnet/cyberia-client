#include "game_render.h"
#include "game_state.h"
#include "dev_ui.h"
#include "modal.h"
#include "modal_player.h"
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
        // Ensure camera offset is centered each frame (good practice)
        game_state_update_camera_offset(g_renderer.screen_width, g_renderer.screen_height);
        
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
    
    // Render click effects in world space
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
    float map_w = grid_w * cell_size;
    float map_h = grid_h * cell_size;
    
    // Always draw grid background
    Rectangle grid_bg = {0, 0, map_w, map_h};
    DrawRectangleRec(grid_bg, g_game_state.colors.grid_background);
    
    // Only draw grid lines and boundary when dev_ui is enabled
    if (!g_game_state.dev_ui) {
        game_state_unlock();
        return;
    }
    
    // Draw map boundary rectangle outline
    DrawRectangleLinesEx(
        (Rectangle){0, 0, map_w, map_h},
        1.0f,
        g_game_state.colors.map_boundary
    );
    
    // Draw grid lines - use red color if grid color is too similar to background
    Color grid_color = g_game_state.colors.grid;
    
    // If grid color alpha is too low or color is too dark, use visible red
    if (grid_color.a < 50 || (grid_color.r < 50 && grid_color.g < 50 && grid_color.b < 50)) {
        grid_color = (Color){255, 0, 0, 128};  // Bright red with semi-transparency
    }
    
    // Draw vertical grid lines with explicit thickness
    for (int x = 0; x <= grid_w; x++) {
        float x_pos = x * cell_size;
        DrawLineEx(
            (Vector2){x_pos, 0},
            (Vector2){x_pos, map_h},
            1.0f,
            grid_color
        );
    }
    
    // Draw horizontal grid lines with explicit thickness
    for (int y = 0; y <= grid_h; y++) {
        float y_pos = y * cell_size;
        DrawLineEx(
            (Vector2){0, y_pos},
            (Vector2){map_w, y_pos},
            1.0f,
            grid_color
        );
    }
    
    game_state_unlock();
}


void game_render_floors(void) {
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    for (int i = 0; i < g_game_state.floor_count; i++) {
        WorldObject* floor = &g_game_state.floors[i];
        
        Rectangle rect = {
            floor->pos.x * cell_size,
            floor->pos.y * cell_size,
            floor->dims.x * cell_size,
            floor->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.floor_background);
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
        WorldObject* portal = &g_game_state.portals[i];
        
        Rectangle rect = {
            portal->pos.x * cell_size,
            portal->pos.y * cell_size,
            portal->dims.x * cell_size,
            portal->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.portal);
    }
    
    // Render foregrounds
    for (int i = 0; i < g_game_state.foreground_count; i++) {
        WorldObject* fg = &g_game_state.foregrounds[i];
        
        Rectangle rect = {
            fg->pos.x * cell_size,
            fg->pos.y * cell_size,
            fg->dims.x * cell_size,
            fg->dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.foreground);
    }
    
    game_state_unlock();
}

void game_render_entities(void) {
    // Only render entities if dev_ui is enabled
    if (!g_game_state.dev_ui) {
        return;
    }
    
    game_state_lock();
    
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    // Render main player with PLAYER color
    Rectangle player_rect = {
        g_game_state.player.base.interp_pos.x * cell_size,
        g_game_state.player.base.interp_pos.y * cell_size,
        g_game_state.player.base.dims.x * cell_size,
        g_game_state.player.base.dims.y * cell_size
    };
    DrawRectangleRec(player_rect, g_game_state.colors.player);
    
    // Render other players with OTHER_PLAYER color
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* player = &g_game_state.other_players[i];
        
        Rectangle rect = {
            player->base.interp_pos.x * cell_size,
            player->base.interp_pos.y * cell_size,
            player->base.dims.x * cell_size,
            player->base.dims.y * cell_size
        };
        
        DrawRectangleRec(rect, g_game_state.colors.other_player);
    }
    
    // Render bots with color based on behavior
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* bot = &g_game_state.bots[i];
        
        Rectangle rect = {
            bot->base.interp_pos.x * cell_size,
            bot->base.interp_pos.y * cell_size,
            bot->base.dims.x * cell_size,
            bot->base.dims.y * cell_size
        };
        
        // Choose color based on behavior
        Color bot_color;
        if (strcmp(bot->behavior, "hostile") == 0) {
            bot_color = g_game_state.colors.error_text;
        } else {
            bot_color = g_game_state.colors.other_player;
        }
        
        DrawRectangleRec(rect, bot_color);
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
    float aoi_radius = g_game_state.aoi_radius * cell_size;
    
    Vector2 center = {
        (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * cell_size,
        (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * cell_size
    };
    
    DrawCircleLines(center.x, center.y, aoi_radius, g_game_state.colors.aoi);
    
    game_state_unlock();
}

void game_render_ui(void) {
    // Render error messages (always visible)
    game_render_error_messages();
    
    // Determine what to render based on dev_ui flag
    game_state_lock();
    bool dev_ui_enabled = g_game_state.dev_ui;
    bool init_received = g_game_state.init_received;
    game_state_unlock();
    
    if (dev_ui_enabled && init_received) {
        // Render dev UI only
        dev_ui_draw(g_renderer.screen_width, g_renderer.screen_height, 0);
    } else {
        // Render player modal only (map, position, FPS)
        modal_player_draw(g_renderer.screen_width, g_renderer.screen_height);
    }
}

void game_render_location_info(void) {
    // This function is no longer used - modal_player handles this
}

void game_render_connection_status(void) {
    // This function is no longer used - modal_player handles this
}

void game_render_performance_info(void) {
    // This function is no longer used - modal_player handles this
}

void game_render_error_messages(void) {
    game_state_lock();
    
    if (g_game_state.last_error_message[0] != '\0') {
        double current_time = GetTime();
        if (current_time - g_game_state.error_display_time < 5.0) {
            int font_size = 16;
            int text_width = MeasureText(g_game_state.last_error_message, font_size);
            int x = (g_renderer.screen_width - text_width) / 2;
            int y = 100;
            
            DrawText(g_game_state.last_error_message, x, y, font_size, g_game_state.colors.error_text);
        }
    }
    
    game_state_unlock();
}

void game_render_click_effects(void) {
    for (int i = 0; i < g_renderer.click_effect_count; i++) {
        ClickEffect* effect = &g_renderer.click_effects[i];
        if (!effect->active) continue;
        
        float alpha = effect->life_time / effect->max_life_time;
        Color c = effect->color;
        c.a = (unsigned char)(c.a * alpha);
        
        float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
        Vector2 world_pos = {
            effect->position.x * cell_size,
            effect->position.y * cell_size
        };
        
        DrawCircleLines(world_pos.x, world_pos.y, effect->radius, c);
    }
}

void game_render_floating_texts(void) {
    for (int i = 0; i < g_renderer.floating_text_count; i++) {
        FloatingText* text = &g_renderer.floating_texts[i];
        if (!text->active) continue;
        
        float alpha = text->life_time / text->max_life_time;
        Color c = text->color;
        c.a = (unsigned char)(c.a * alpha);
        
        float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
        Vector2 world_pos = {
            text->position.x * cell_size,
            text->position.y * cell_size
        };
        
        DrawText(text->text, world_pos.x, world_pos.y, text->font_size, c);
    }
}

void game_render_debug_overlay(void) {
    // Additional debug information can be rendered here
}

Vector2 game_render_world_to_screen(Vector2 world_pos) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    Vector2 scaled = {world_pos.x * cell_size, world_pos.y * cell_size};
    return GetWorldToScreen2D(scaled, g_game_state.camera);
}

Vector2 game_render_screen_to_world(Vector2 screen_pos) {
    Vector2 world = GetScreenToWorld2D(screen_pos, g_game_state.camera);
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    return (Vector2){world.x / cell_size, world.y / cell_size};
}

Rectangle game_render_get_camera_bounds(void) {
    Vector2 top_left = game_render_screen_to_world((Vector2){0, 0});
    Vector2 bottom_right = game_render_screen_to_world(
        (Vector2){g_renderer.screen_width, g_renderer.screen_height}
    );
    
    return (Rectangle){
        top_left.x,
        top_left.y,
        bottom_right.x - top_left.x,
        bottom_right.y - top_left.y
    };
}

void game_render_add_click_effect(Vector2 world_pos, Color color) {
    for (int i = 0; i < 20; i++) {
        if (!g_renderer.click_effects[i].active) {
            g_renderer.click_effects[i].position = world_pos;
            g_renderer.click_effects[i].radius = 10.0f;
            g_renderer.click_effects[i].max_radius = 30.0f;
            g_renderer.click_effects[i].life_time = 1.0f;
            g_renderer.click_effects[i].max_life_time = 1.0f;
            g_renderer.click_effects[i].color = color;
            g_renderer.click_effects[i].active = true;
            break;
        }
    }
}

void game_render_add_floating_text(Vector2 world_pos, const char* text, 
                                   Color color, float font_size, float life_time) {
    for (int i = 0; i < 100; i++) {
        if (!g_renderer.floating_texts[i].active) {
            g_renderer.floating_texts[i].position = world_pos;
            g_renderer.floating_texts[i].velocity = (Vector2){0, -1.0f};
            strncpy(g_renderer.floating_texts[i].text, text, 63);
            g_renderer.floating_texts[i].text[63] = '\0';
            g_renderer.floating_texts[i].color = color;
            g_renderer.floating_texts[i].life_time = life_time;
            g_renderer.floating_texts[i].max_life_time = life_time;
            g_renderer.floating_texts[i].font_size = font_size;
            g_renderer.floating_texts[i].active = true;
            break;
        }
    }
}

void game_render_update_effects(float delta_time) {
    // Update click effects
    for (int i = 0; i < g_renderer.click_effect_count; i++) {
        ClickEffect* effect = &g_renderer.click_effects[i];
        if (!effect->active) continue;
        
        effect->life_time -= delta_time;
        if (effect->life_time <= 0.0f) {
            effect->active = false;
            continue;
        }
        
        // Expand radius over time
        float progress = 1.0f - (effect->life_time / effect->max_life_time);
        effect->radius = effect->max_radius * progress;
    }
    
    // Update floating texts
    for (int i = 0; i < g_renderer.floating_text_count; i++) {
        FloatingText* text = &g_renderer.floating_texts[i];
        if (!text->active) continue;
        
        text->life_time -= delta_time;
        if (text->life_time <= 0.0f) {
            text->active = false;
            continue;
        }
        
        // Move text based on velocity
        text->position.x += text->velocity.x * delta_time;
        text->position.y += text->velocity.y * delta_time;
    }
}

void game_render_cleanup(void) {
    printf("[GAME_RENDER] Cleaning up game renderer...\n");
    
    // Unload font if loaded
    if (g_renderer.font_loaded) {
        UnloadFont(g_renderer.game_font);
        g_renderer.font_loaded = false;
    }
    
    printf("[GAME_RENDER] Game renderer cleaned up\n");
}

void game_render_clear_texture_cache(void) {
    // Texture cache cleanup (if implemented)
}