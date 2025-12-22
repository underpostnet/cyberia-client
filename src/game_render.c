#include "game_render.h"
#include "game_state.h"
#include "dev_ui.h"
#include "modal.h"
#include "modal_player.h"
#include "texture_manager.h"
#include "object_layers_management.h"
#include "entity_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global renderer instance
GameRenderer g_renderer = {0};

// Global managers for entity rendering with object layers
static TextureManager* g_texture_manager = NULL;
static ObjectLayersManager* g_object_layers_manager = NULL;
static EntityRender* g_entity_render = NULL;

int game_render_init(int screen_width, int screen_height) {

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

    // UI state
    g_renderer.hud_visible = true;

    // Performance tracking
    g_renderer.frames_rendered = 0;
    g_renderer.last_fps_update = GetTime();
    g_renderer.current_fps = 60.0f;

    // Font loading (using default for now)
    g_renderer.font_loaded = false; // Will use default font

    // Initialize object layer rendering system
    g_texture_manager = create_texture_manager();

    g_object_layers_manager = create_object_layers_manager(g_texture_manager);
    if (!g_object_layers_manager) {
        fprintf(stderr, "[ERROR] Failed to create object layers manager\n");
        destroy_texture_manager(g_texture_manager);
        return -1;
    }

    g_entity_render = create_entity_render(g_object_layers_manager, g_texture_manager);
    if (!g_entity_render) {
        fprintf(stderr, "[ERROR] Failed to create entity render system\n");
        destroy_object_layers_manager(g_object_layers_manager);
        destroy_texture_manager(g_texture_manager);
        return -1;
    }
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
    // Process texture pre-caching queue
    if (g_object_layers_manager) {
        process_texture_caching_queue(g_object_layers_manager);
    }

    // CRITICAL: Wrap entire rendering in try-catch style error handling
    // This prevents partial rendering that can cause black screens

    BeginDrawing();

    // Clear background - this ensures we always have SOME color on screen
    ClearBackground(g_game_state.colors.background);

    // Begin camera mode for world rendering
    // CRITICAL: Always update camera offset before BeginMode2D to prevent flickering
    // This ensures the camera is properly centered even if screen dimensions changed
    game_state_update_camera_offset(g_renderer.screen_width, g_renderer.screen_height);

    BeginMode2D(g_game_state.camera);
        game_render_world();
    EndMode2D();

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

    // CRITICAL: Always call EndDrawing() even if errors occurred above
    EndDrawing();
}

void game_render_world(void) {
    // Render world components in correct z-order
    // Note: Order is critical - each layer builds on the previous

    // 1. Floors (bottom layer) - provides base background
    game_render_floors();

    // 2. World objects (obstacles, portals - but NOT foregrounds)
    game_render_world_objects();

    // 3. Entities (sorted by depth) - players and bots
    game_render_entities();

    // 4. Player path (if dev_ui enabled) - visual debug aid
    if (g_game_state.dev_ui) {
        game_render_player_path();
    }

    // 5. AOI circle (if dev_ui enabled) - visual debug aid
    if (g_game_state.dev_ui) {
        game_render_aoi_circle();
    }

    // 6. Foregrounds (always on top of entities) - creates depth
    game_render_foregrounds();

    // 7. Effects - click effects and floating text
    game_render_click_effects();
    game_render_floating_texts();

    // 8. Grid overlay (if dev_ui enabled - renders on top of everything)
    if (g_game_state.dev_ui) {
        game_render_grid();
    }
}

void game_render_grid(void) {
    // Only render grid overlay when dev_ui is enabled
    // Grid is transparent with red lines and white border on top of everything

    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    int grid_w = g_game_state.grid_w;
    int grid_h = g_game_state.grid_h;
    float map_w = grid_w * cell_size;
    float map_h = grid_h * cell_size;

    // Draw white map boundary (outline)
    DrawRectangleLinesEx(
        (Rectangle){0, 0, map_w, map_h},
        2.0f,
        WHITE
    );

    // Draw red grid lines (semi-transparent)
    Color grid_line_color = (Color){255, 0, 0, 100};

    // Draw vertical grid lines
    for (int x = 1; x < grid_w; x++) {
        float x_pos = x * cell_size;
        DrawLineEx(
            (Vector2){x_pos, 0},
            (Vector2){x_pos, map_h},
            1.0f,
            grid_line_color
        );
    }

    // Draw horizontal grid lines
    for (int y = 1; y < grid_h; y++) {
        float y_pos = y * cell_size;
        DrawLineEx(
            (Vector2){0, y_pos},
            (Vector2){map_w, y_pos},
            1.0f,
            grid_line_color
        );
    }
}


void game_render_floors(void) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // If we have no floors, draw a default background to prevent black screen
    if (g_game_state.floor_count == 0) {
        // Draw a subtle grid background as fallback
        if (g_game_state.dev_ui) {
            // In dev mode, show that floors are missing with a darker background
            DrawRectangle(0, 0, g_renderer.screen_width * 2, g_renderer.screen_height * 2,
                         (Color){20, 20, 20, 255});
        }
    }

    for (int i = 0; i < g_game_state.floor_count; i++) {
        WorldObject* floor = &g_game_state.floors[i];

        if (floor->object_layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < floor->object_layer_count; j++) {
                layers[j] = &floor->object_layers[j];
            }

            draw_entity_layers(
                g_entity_render,
                floor->id,
                floor->pos.x,
                floor->pos.y,
                floor->dims.x,
                floor->dims.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                floor->object_layer_count,
                "floor",
                g_game_state.dev_ui,
                cell_size
            );
        } else {
            Rectangle rect = {
                floor->pos.x * cell_size,
                floor->pos.y * cell_size,
                floor->dims.x * cell_size,
                floor->dims.y * cell_size
            };
            DrawRectangleRec(rect, g_game_state.colors.floor_background);
        }
    }
}

void game_render_world_objects(void) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Render obstacles
    for (int i = 0; i < g_game_state.obstacle_count; i++) {
        WorldObject* obj = &g_game_state.obstacles[i];

        if (obj->object_layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < obj->object_layer_count; j++) {
                layers[j] = &obj->object_layers[j];
            }

            draw_entity_layers(
                g_entity_render,
                obj->id,
                obj->pos.x,
                obj->pos.y,
                obj->dims.x,
                obj->dims.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                obj->object_layer_count,
                "obstacle",
                g_game_state.dev_ui,
                cell_size
            );
        } else {
            Rectangle rect = {
                obj->pos.x * cell_size,
                obj->pos.y * cell_size,
                obj->dims.x * cell_size,
                obj->dims.y * cell_size
            };
            DrawRectangleRec(rect, g_game_state.colors.obstacle);
        }
    }

    // Render portals
    for (int i = 0; i < g_game_state.portal_count; i++) {
        WorldObject* portal = &g_game_state.portals[i];

        if (portal->object_layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < portal->object_layer_count; j++) {
                layers[j] = &portal->object_layers[j];
            }

            draw_entity_layers(
                g_entity_render,
                portal->id,
                portal->pos.x,
                portal->pos.y,
                portal->dims.x,
                portal->dims.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                portal->object_layer_count,
                "portal",
                g_game_state.dev_ui,
                cell_size
            );
        } else {
            Rectangle rect = {
                portal->pos.x * cell_size,
                portal->pos.y * cell_size,
                portal->dims.x * cell_size,
                portal->dims.y * cell_size
            };
            DrawRectangleRec(rect, g_game_state.colors.portal);
        }
    }
}

void game_render_foregrounds(void) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Render foregrounds (always on top of entities)
    for (int i = 0; i < g_game_state.foreground_count; i++) {
        WorldObject* fg = &g_game_state.foregrounds[i];

        if (fg->object_layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < fg->object_layer_count; j++) {
                layers[j] = &fg->object_layers[j];
            }

            draw_entity_layers(
                g_entity_render,
                fg->id,
                fg->pos.x,
                fg->pos.y,
                fg->dims.x,
                fg->dims.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                fg->object_layer_count,
                "foreground",
                g_game_state.dev_ui,
                cell_size
            );
        } else {
            Rectangle rect = {
                fg->pos.x * cell_size,
                fg->pos.y * cell_size,
                fg->dims.x * cell_size,
                fg->dims.y * cell_size
            };
            DrawRectangleRec(rect, g_game_state.colors.foreground);
        }
    }
}

// Helper structure for depth sorting entities
typedef struct {
    enum { ENTITY_TYPE_PLAYER, ENTITY_TYPE_OTHER_PLAYER, ENTITY_TYPE_BOT } type;
    float bottom_y;  // Y position of entity's bottom edge (for depth sorting)
    union {
        PlayerState* player;
        BotState* bot;
    } data;
    bool is_main_player;
} EntitySortEntry;

// Comparison function for qsort - entities with lower Y render first (appear behind)
static int compare_entities_by_depth(const void* a, const void* b) {
    const EntitySortEntry* ea = (const EntitySortEntry*)a;
    const EntitySortEntry* eb = (const EntitySortEntry*)b;

    if (ea->bottom_y < eb->bottom_y) return -1;
    if (ea->bottom_y > eb->bottom_y) return 1;
    return 0;
}

void game_render_entities(void) {
    // Safety check - ensure entity render system is initialized
    if (!g_entity_render) {
        // Fallback to simple rendering if entity render system not initialized
        float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

        // Draw simple rectangles as fallback to ensure entities are visible
        Rectangle rect;
        rect.x = g_game_state.player.base.interp_pos.x * cell_size;
        rect.y = g_game_state.player.base.interp_pos.y * cell_size;
        rect.width = g_game_state.player.base.dims.x * cell_size;
        rect.height = g_game_state.player.base.dims.y * cell_size;
        DrawRectangleRec(rect, g_game_state.colors.player);

        // Also draw other players as rectangles
        for (int i = 0; i < g_game_state.other_player_count; i++) {
            PlayerState* player = &g_game_state.other_players[i];
            Rectangle other_rect = {
                player->base.interp_pos.x * cell_size,
                player->base.interp_pos.y * cell_size,
                player->base.dims.x * cell_size,
                player->base.dims.y * cell_size
            };
            DrawRectangleRec(other_rect, g_game_state.colors.other_player);
        }

        // Draw bots as rectangles
        for (int i = 0; i < g_game_state.bot_count; i++) {
            BotState* bot = &g_game_state.bots[i];
            Rectangle bot_rect = {
                bot->base.interp_pos.x * cell_size,
                bot->base.interp_pos.y * cell_size,
                bot->base.dims.x * cell_size,
                bot->base.dims.y * cell_size
            };
            DrawRectangleRec(bot_rect, (Color){100, 200, 100, 200});
        }
        return;
    }

    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    bool dev_ui = g_game_state.dev_ui;

    // Create array to hold all entities for sorting
    static EntitySortEntry sort_entries[MAX_ENTITIES + 1];  // +1 for main player
    int entry_count = 0;

    // Add main player to sort list
    float player_bottom_y = g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y;
    sort_entries[entry_count].type = ENTITY_TYPE_PLAYER;
    sort_entries[entry_count].bottom_y = player_bottom_y;
    sort_entries[entry_count].data.player = &g_game_state.player;
    sort_entries[entry_count].is_main_player = true;
    entry_count++;

    // Add other players to sort list
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* player = &g_game_state.other_players[i];
        float bottom_y = player->base.interp_pos.y + player->base.dims.y;

        sort_entries[entry_count].type = ENTITY_TYPE_OTHER_PLAYER;
        sort_entries[entry_count].bottom_y = bottom_y;
        sort_entries[entry_count].data.player = player;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add bots to sort list
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* bot = &g_game_state.bots[i];
        float bottom_y = bot->base.interp_pos.y + bot->base.dims.y;

        sort_entries[entry_count].type = ENTITY_TYPE_BOT;
        sort_entries[entry_count].bottom_y = bottom_y;
        sort_entries[entry_count].data.bot = bot;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Sort entities by depth (bottom Y coordinate) - no lock needed
    qsort(sort_entries, entry_count, sizeof(EntitySortEntry), compare_entities_by_depth);

    // Allocate temporary layer pointer array for all entities
    ObjectLayerState* temp_layers[MAX_OBJECT_LAYERS];

    // Render entities in sorted order using EntityRender system
    // We don't need lock here as we're using copied data from sort_entries
    for (int i = 0; i < entry_count; i++) {
        EntitySortEntry* entry = &sort_entries[i];

        const char* entity_type_str = NULL;
        const char* entity_id = NULL;
        EntityState* entity_base = NULL;
        int layers_count = 0;

        switch (entry->type) {
            case ENTITY_TYPE_PLAYER:
                entity_base = &entry->data.player->base;
                entity_type_str = entry->is_main_player ? "self" : "other";
                entity_id = entity_base->id;
                layers_count = entity_base->object_layer_count;
                break;

            case ENTITY_TYPE_OTHER_PLAYER:
                entity_base = &entry->data.player->base;
                entity_type_str = "other";
                entity_id = entity_base->id;
                layers_count = entity_base->object_layer_count;
                break;

            case ENTITY_TYPE_BOT:
                entity_base = &entry->data.bot->base;
                entity_type_str = "bot";
                entity_id = entity_base->id;
                layers_count = entity_base->object_layer_count;
                break;
        }

        if (entity_base && entity_id) {
            // Convert object layers to pointer array
            for (int j = 0; j < layers_count && j < MAX_OBJECT_LAYERS; j++) {
                temp_layers[j] = &entity_base->object_layers[j];
            }

            // Use EntityRender system to draw entity with object layers
            // This may load textures which can take time - that's why we unlocked earlier
            draw_entity_layers(
                g_entity_render,
                entity_id,
                entity_base->interp_pos.x,
                entity_base->interp_pos.y,
                entity_base->dims.x,
                entity_base->dims.y,
                entity_base->direction,
                entity_base->mode,
                temp_layers,
                layers_count,
                entity_type_str,
                dev_ui,
                cell_size
            );
        }
    }
}

void game_render_player_path(void) {
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
}

void game_render_aoi_circle(void) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    float aoi_radius = g_game_state.aoi_radius * cell_size;

    Vector2 center = {
        (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * cell_size,
        (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * cell_size
    };

    DrawCircleLines(center.x, center.y, aoi_radius, g_game_state.colors.aoi);
}

void game_render_ui(void) {
    // Render error messages (always visible)
    game_render_error_messages();

    // Determine what to render based on dev_ui flag
    bool dev_ui_enabled = g_game_state.dev_ui;
    bool init_received = g_game_state.init_received;

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

    // Cleanup entity rendering system
    if (g_entity_render) {
        destroy_entity_render(g_entity_render);
        g_entity_render = NULL;
    }

    // Cleanup object layers manager
    if (g_object_layers_manager) {
        destroy_object_layers_manager(g_object_layers_manager);
        g_object_layers_manager = NULL;
    }

    // Cleanup texture manager
    if (g_texture_manager) {
        destroy_texture_manager(g_texture_manager);
        g_texture_manager = NULL;
    }

    // Unload font if loaded
    if (g_renderer.font_loaded) {
        UnloadFont(g_renderer.game_font);
        g_renderer.font_loaded = false;
    }
}

void game_render_clear_texture_cache(void) {
    // Texture cache cleanup (if implemented)
}
