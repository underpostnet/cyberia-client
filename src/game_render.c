#include "game_render.h"
#include "ui/text.h"
#include "domain/camera.h"
#include "domain/local_player.h"
#include "domain/local_player_view.h"

#include "dialogue_data.h"
#include "domain/presentation_runtime.h"
#include "entity_render.h"
#include "game_state.h"
#include "js/fullscreen_bridge.h"
#include "object_layers_management.h"
#include "ui/dev_ui.h"
#include "ui/entity_overhead_ui.h"
#include "ui/floating_combat_text.h"
#include "ui/fx_shapes.h"
#include "ui/interaction_bubble.h"
#include "ui/inventory_bar.h"
#include "ui/loot_fx.h"
#include "ui/inventory_modal.h"
#include "ui/modal_dialogue.h"
#include "ui/action_cache.h"
#include "ui/modal_interact.h"
#include "ui/modal_map.h"
#include "ui/nameplate.h"
#include "ui/quest_journal.h"
#include "ui/modal_notification.h"
#include "ui/fx_tap.h"
#include "ui/ui_button.h"
#include "ui/ui_icon.h"
#include "util/log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Zoom buttons ─────────────────────────────────────────────────────── */
#define ZOOM_BTN_SIZE   44
#define ZOOM_BTN_GAP     6
#define ZOOM_BTN_MARGIN 10

static Rectangle zoom_btn_rect(int idx, int sw, int sh) {
    /* Two square buttons stacked vertically, bottom-right, above the bar. */
    float x = sw - ZOOM_BTN_SIZE - ZOOM_BTN_MARGIN;
    float y = sh - INV_BAR_HEIGHT - ZOOM_BTN_MARGIN
              - (2 - idx) * (ZOOM_BTN_SIZE + ZOOM_BTN_GAP) + ZOOM_BTN_GAP;
    return (Rectangle){ x, y, ZOOM_BTN_SIZE, ZOOM_BTN_SIZE };
}

static void draw_zoom_buttons(int sw, int sh) {
    const char* labels[2] = { "+", "-" };   /* 0 = zoom-in, 1 = zoom-out */
    int mx = GetMouseX(), my = GetMouseY();
    for (int i = 0; i < 2; i++) {
        Rectangle r = zoom_btn_rect(i, sw, sh);
        UIButtonStyle style = {
            .text       = labels[i],
            .font_size  = 24,
            .bg         = { 20, 20, 35, 200 },
            .bg_hover   = { 50, 50, 70, 220 },
            .border     = { 80, 80, 120, 180 },
        };
        UIButtonState st = ui_button_resolve_state(true, false, ui_button_hit(r, mx, my));
        ui_button_draw(r, &style, st);
    }
}

/* ── Fullscreen button ────────────────────────────────────────────────── */
/* Size/margin are public (game_render.h) so modal_map.c can offset its own
 * HUD box to sit beside this button in the top-right corner. */
static Rectangle fullscreen_btn_rect(int sw) {
    float x = sw - FULLSCREEN_BTN_SIZE - FULLSCREEN_BTN_MARGIN;
    float y = FULLSCREEN_BTN_MARGIN;
    return (Rectangle){ x, y, FULLSCREEN_BTN_SIZE, FULLSCREEN_BTN_SIZE };
}

static void draw_fullscreen_button(int sw) {
    Rectangle r = fullscreen_btn_rect(sw);
    int mx = GetMouseX(), my = GetMouseY();
    bool active = fullscreen_bridge_is_active();
    UIButtonStyle style = {
        .icon_id    = active ? "close-yellow" : "fullscreen",
        .icon_size  = FULLSCREEN_BTN_SIZE - 12,
        .bg         = { 20, 20, 35, 200 },
        .bg_hover   = { 50, 50, 70, 220 },
        .border     = { 80, 80, 120, 180 },
    };
    UIButtonState st = ui_button_resolve_state(true, false, ui_button_hit(r, mx, my));
    ui_button_draw(r, &style, st);
}

/* ── Portal hold progress bar ─────────────────────────────────────────── */
#define PORTAL_BAR_W       220
#define PORTAL_BAR_H        14
#define PORTAL_BAR_MARGIN   10

/* Centered just above the inventory hotbar; visible only while the local
 * player is standing inside an active portal. Renders the authoritative
 * replicated hold progress — the client derives nothing. */
static void draw_portal_progress_bar(int sw, int sh) {
    if (!local_player_on_portal()) return;

    float progress = local_player_portal_hold_progress();
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    float x = (sw - PORTAL_BAR_W) * 0.5f;
    float y = (float)(sh - INV_BAR_HEIGHT - PORTAL_BAR_MARGIN - PORTAL_BAR_H);
    Rectangle track = { x, y, (float)PORTAL_BAR_W, (float)PORTAL_BAR_H };

    DrawRectangleRounded(track, 0.5f, 6, (Color){ 10, 12, 24, 220 });
    if (progress > 0.0f) {
        Rectangle fill = { x, y, PORTAL_BAR_W * progress, (float)PORTAL_BAR_H };
        DrawRectangleRounded(fill, 0.5f, 6, (Color){ 90, 170, 255, 240 });
    }
    DrawRectangleRoundedLinesEx(track, 0.5f, 6, 1.0f, (Color){ 120, 160, 220, 200 });

    const char* label = "Teleporting...";
    int fs = 12;
    int tw = MeasureText(label, fs);
    int tx = (int)(x + (PORTAL_BAR_W - tw) * 0.5f);
    int ty = (int)(y - fs - 2);
    DrawText(label, tx + 1, ty + 1, fs, (Color){ 0, 0, 0, 200 });
    DrawText(label, tx, ty, fs, (Color){ 220, 230, 245, 245 });
}

int game_render_zoom_btn_hit(int mx, int my) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    for (int i = 0; i < 2; i++) {
        Rectangle r = zoom_btn_rect(i, sw, sh);
        if ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height) {
            return (i == 0) ? 1 : -1;
        }
    }
    return 0;
}

bool game_render_fullscreen_btn_hit(int mx, int my) {
    return ui_button_hit(fullscreen_btn_rect(GetScreenWidth()), mx, my);
}

// Global renderer instance
GameRenderer g_renderer = {0};

// Global manager for entity rendering with object layers
// ObjectLayersManager is owned by its module — access via obj_layers_mgr_get().
static EntityRender* g_entity_render = NULL;

static void on_entity_removed(const char* id) {
    if (g_entity_render) { entity_render_forget_entity(g_entity_render, id); }
}

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

    // Initialize object layer rendering system
    create_object_layers_manager();
    ObjectLayersManager* olm = obj_layers_mgr_get();

    g_entity_render = create_entity_render(olm);
    if (NULL == g_entity_render) {
        LOG_ERROR("Failed to create entity render system");
        game_render_cleanup();
        return -1;
    }
    game_state_set_entity_removed_cb(on_entity_removed);

    inventory_bar_init(olm);
    inventory_modal_init(olm);
    modal_dialogue_init();
    modal_interact_init();
    dialogue_data_init();
    interaction_bubble_init();
    quest_journal_init();
    modal_notification_init();
    ui_icon_init(UI_ICON_CACHE_CAPACITY);
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
    /* Consume any error posted by message_parser via game_state.pending_error. */
    if (g_game_state.pending_error[0] != '\0') {
        game_render_set_error_message(g_game_state.pending_error);
        g_game_state.pending_error[0] = '\0';
    }

    // Atlas textures are loaded on-demand via get_atlas_texture() during rendering.
    // No pre-caching queue processing is needed.

    // CRITICAL: Wrap entire rendering in try-catch style error handling
    // This prevents partial rendering that can cause black screens

    BeginDrawing();

    // Clear background - this ensures we always have SOME color on screen
    ClearBackground(presentation_runtime_palette("BACKGROUND"));

    // Begin camera mode for world rendering
    // CRITICAL: Always update camera offset before BeginMode2D to prevent flickering
    // This ensures the camera is properly centered even if screen dimensions changed
    camera_resize(g_renderer.screen_width, g_renderer.screen_height);

    BeginMode2D(camera_get());
        game_render_world();
    EndMode2D();

    // FCT screen-space overlay: damage red flash / regen green pulse
    fct_draw_overlay();

    // Render UI (screen space)
    game_render_ui();

    // Tap effects are rendered in screen space so input systems can spawn
    // them directly from screen coordinates without camera conversions.
    fx_tap_draw();

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

/* Draw the on-grid quantity counter centered above a drop token. Discreet
 * fixed-size glyph with a gapless wide black border (two concentric 8-direction
 * rings), matching the capacity-bar sum-of-stats value style. */
static void draw_drop_count(float top_x, float top_y, float dims_w, float cell_size, int qty) {
    char buf[16];
    snprintf(buf, sizeof(buf), "x%d", qty);

    const int fs = 13;
    int tw = MeasureText(buf, fs);
    int nx = (int)((top_x + dims_w * 0.5f) * cell_size) - tw / 2;
    int ny = (int)(top_y * cell_size) - fs - 3;

    for (int o = 1; o <= 2; o++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (dx || dy)
                    DrawText(buf, nx + dx * o, ny + dy * o, fs, (Color){ 0, 0, 0, 255 });
    DrawText(buf, nx, ny, fs, (Color){ 255, 255, 255, 245 });
}

/* Draw the loot collection stage: the treasure-burst / ambient sparks at each
 * pickup (gold when the local player may collect, gray for another player's
 * loot), then the Object Layer tokens vacuuming toward their collectors.
 * Tokens reuse the shared entity-layer path (async atlas pipeline). No
 * background plate — the token and bordered-square sparks carry the read. */
static void game_render_loot_fx(void) {
    if (!g_entity_render) return;
    const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    /* Bordered-square particles (ambient float + collection burst). */
    int pslots = loot_fx_particle_slot_count();
    for (int i = 0; i < pslots; i++) {
        LootFxParticle pt;
        if (!loot_fx_particle_at(i, &pt)) continue;
        Color body = (LOOT_FX_TINT_GRAY == pt.tint) ? FX_SPARK_GRAY : FX_SPARK_GOLD;
        fx_shape_spark(pt.x * cell_size, pt.y * cell_size,
                       pt.size * cell_size, body, 1.0f);
    }

    /* Vacuum flight tokens. */
    int slots = loot_fx_slot_count();
    for (int i = 0; i < slots; i++) {
        LootFxRender fx;
        if (!loot_fx_render_at(i, &fx)) continue;

        float half = fx.size * 0.5f;

        ObjectLayerState layer = { .active = true, .quantity = 1 };
        strncpy(layer.item_id, fx.item_id, MAX_ITEM_ID_LENGTH - 1);
        ObjectLayerState* layers[1] = { &layer };

        draw_entity_layers(
            g_entity_render,
            fx.item_id,            /* per-item animation key */
            fx.x - half,
            fx.y - half,
            fx.size,
            fx.size,
            DIRECTION_DOWN,
            MODE_IDLE,
            layers,
            1,
            "loot",
            false,
            cell_size,
            (Color){ 255, 225, 130, 255 }
        );
    }
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
    if (g_entity_render) { entity_render_gc(g_entity_render); }

    // 4. Player path (if dev_ui enabled) - visual debug aid
    if (presentation_runtime_dev_ui()) {
        game_render_player_path();
    }

    // 5. AOI circle (if dev_ui enabled) - visual debug aid
    if (presentation_runtime_dev_ui()) {
        game_render_aoi_circle();
    }

    // 6. Foregrounds (always on top of entities) - creates depth
    game_render_foregrounds();

    // 7. Effects — click effects, floating text, FCT pop-ups, loot flights
    game_render_click_effects();
    game_render_floating_texts();
    fct_draw();
    game_render_loot_fx();

    // 8. Grid overlay (if dev_ui enabled - renders on top of everything)
    if (presentation_runtime_dev_ui()) {
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
    const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // If we have no floors, draw a default background to prevent black screen
    if (g_game_state.floor_count == 0) {
        // Draw a subtle grid background as fallback
        if (presentation_runtime_dev_ui()) {
            // In dev mode, show that floors are missing with a darker background
            DrawRectangle(0, 0, g_renderer.screen_width * 2, g_renderer.screen_height * 2,
                         (Color){20, 20, 20, 255});
        }
    }

    for (int i = 0; i < g_game_state.floor_count; i++) {
        WorldObject* floor = &g_game_state.floors[i];
        Color floor_color = presentation_runtime_palette("FLOOR");

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
                presentation_runtime_dev_ui(),
                cell_size,
                floor_color
            );
        } else {
            Color fallback = presentation_runtime_palette("FLOOR_BACKGROUND");
            Rectangle rect = {
                floor->pos.x * cell_size,
                floor->pos.y * cell_size,
                floor->dims.x * cell_size,
                floor->dims.y * cell_size
            };
            DrawRectangleRec(rect, fallback);
        }
    }
}

void game_render_world_objects(void) {
    const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Render portals
    for (int i = 0; i < g_game_state.portal_count; i++) {
        WorldObject* portal = &g_game_state.portals[i];
        Color portal_color = presentation_runtime_palette("PORTAL");

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
                presentation_runtime_dev_ui(),
                cell_size,
                portal_color
            );
        } else {
            Rectangle rect = {
                portal->pos.x * cell_size,
                portal->pos.y * cell_size,
                portal->dims.x * cell_size,
                portal->dims.y * cell_size
            };
            DrawRectangleRec(rect, portal_color);
        }

        /* Overhead: portals carry only the 'portal' presence icon (transport)
         * plus a "<targetMapCode> <x>,<y>" destination nameplate. */
        char portal_name[80];
        nameplate_resolve_portal(portal->target_map_code, portal->target_cell_x,
                                 portal->target_cell_y, portal_name, (int)sizeof(portal_name));
        EntityOverheadParams ohp = {
            .name              = portal_name,
            .show_name         = portal_name[0] != '\0',
            .show_stats        = false,
            .show_hp           = false,
            .status_icon       = portal->status_icon,
            .interaction_flags = 0,
        };
        entity_overhead_ui_draw(&ohp, portal->pos.x, portal->pos.y,
                                portal->dims.x, portal->dims.y, cell_size);
    }
}

void game_render_foregrounds(void) {
    const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Render foregrounds (always on top of entities)
    for (int i = 0; i < g_game_state.foreground_count; i++) {
        WorldObject* fg = &g_game_state.foregrounds[i];
        Color fg_color = presentation_runtime_palette("FOREGROUND");

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
                presentation_runtime_dev_ui(),
                cell_size,
                fg_color
            );
        } else {
            Rectangle rect = {
                fg->pos.x * cell_size,
                fg->pos.y * cell_size,
                fg->dims.x * cell_size,
                fg->dims.y * cell_size
            };
            DrawRectangleRec(rect, fg_color);
        }
    }
}

// Helper structure for depth sorting entities.
// Capacity covers obstacles (≤ MAX_OBJECTS) plus the main player, other players,
// bots, resources, and statics (each ≤ MAX_ENTITIES).
#define MAX_DEPTH_SORT_ENTRIES (MAX_OBJECTS + (MAX_ENTITIES * 4) + 1)

typedef struct {
    enum { ENTITY_TYPE_OBSTACLE, ENTITY_TYPE_STATIC, ENTITY_TYPE_PLAYER, ENTITY_TYPE_OTHER_PLAYER, ENTITY_TYPE_BOT, ENTITY_TYPE_RESOURCE } type;
    float bottom_y;  // Y position of entity's bottom edge (for depth sorting)
    const char* sort_id;
    int source_order;
    union {
        WorldObject* object;
        PlayerState* player;
        BotState* bot;
    } data;
    bool is_main_player;
} EntitySortEntry;

// Comparison function for qsort - entities with lower Y render first (appear behind)
#define ENTITY_DEPTH_EPSILON 0.001f

static int compare_entities_by_depth(const void* a, const void* b) {
    const EntitySortEntry* ea = (const EntitySortEntry*)a;
    const EntitySortEntry* eb = (const EntitySortEntry*)b;
    float depth_delta = ea->bottom_y - eb->bottom_y;

    if (depth_delta < -ENTITY_DEPTH_EPSILON) return -1;
    if (depth_delta > ENTITY_DEPTH_EPSILON) return 1;

    if (ea->sort_id && eb->sort_id) {
        int id_cmp = strcmp(ea->sort_id, eb->sort_id);
        if (id_cmp != 0) return id_cmp;
    }

    if (ea->type != eb->type) {
        return (int)ea->type - (int)eb->type;
    }

    return ea->source_order - eb->source_order;
}

void game_render_entities(void) {
    // Safety check - ensure entity render system is initialized
    if (!g_entity_render) {
        // Fallback to simple rendering if entity render system not initialized
        const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

        // Draw simple rectangles as fallback to ensure entities are visible
        Rectangle rect ={
            .x = g_game_state.player.base.interp_pos.x * cell_size,
            .y = g_game_state.player.base.interp_pos.y * cell_size,
            .width = g_game_state.player.base.dims.x * cell_size,
            .height = g_game_state.player.base.dims.y * cell_size
        };
        DrawRectangleRec(rect, presentation_runtime_palette("PLAYER"));

        // Also draw other players as rectangles
        for (int i = 0; i < g_game_state.other_player_count; i++) {
            PlayerState* player = &g_game_state.other_players[i];
            Rectangle other_rect = {
                player->base.interp_pos.x * cell_size,
                player->base.interp_pos.y * cell_size,
                player->base.dims.x * cell_size,
                player->base.dims.y * cell_size
            };
            DrawRectangleRec(other_rect, presentation_runtime_palette("OTHER_PLAYER"));
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

    const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    const bool dev_ui = presentation_runtime_dev_ui();

    // Create array to hold all depth-sorted actors
    static EntitySortEntry sort_entries[MAX_DEPTH_SORT_ENTRIES];
    int entry_count = 0;

    // Add obstacles to sort list so they share the same depth rules as entities.
    for (int i = 0; i < g_game_state.obstacle_count; i++) {
        WorldObject* obj = &g_game_state.obstacles[i];
        float bottom_y = obj->pos.y + obj->dims.y;

        sort_entries[entry_count].type = ENTITY_TYPE_OBSTACLE;
        sort_entries[entry_count].bottom_y = bottom_y;
        sort_entries[entry_count].sort_id = obj->id;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].data.object = obj;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add static decorators to sort list — passable, but share the same depth
    // rules as entities so the player can pass behind / in front of them.
    for (int i = 0; i < g_game_state.static_count; i++) {
        WorldObject* st = &g_game_state.statics[i];
        float bottom_y = st->pos.y + st->dims.y;

        sort_entries[entry_count].type = ENTITY_TYPE_STATIC;
        sort_entries[entry_count].bottom_y = bottom_y;
        sort_entries[entry_count].sort_id = st->id;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].data.object = st;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add main player to sort list
    float player_bottom_y = g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y;
    sort_entries[entry_count].type = ENTITY_TYPE_PLAYER;
    sort_entries[entry_count].bottom_y = player_bottom_y;
    sort_entries[entry_count].sort_id = g_game_state.player.base.id;
    sort_entries[entry_count].source_order = entry_count;
    sort_entries[entry_count].data.player = &g_game_state.player;
    sort_entries[entry_count].is_main_player = true;
    entry_count++;

    // Add other players to sort list
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* player = &g_game_state.other_players[i];
        float bottom_y = player->base.interp_pos.y + player->base.dims.y;

        sort_entries[entry_count].type = ENTITY_TYPE_OTHER_PLAYER;
        sort_entries[entry_count].bottom_y = bottom_y;
        sort_entries[entry_count].sort_id = player->base.id;
        sort_entries[entry_count].source_order = entry_count;
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
        sort_entries[entry_count].sort_id = bot->base.id;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].data.bot = bot;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add resources to sort list
    for (int i = 0; i < g_game_state.resource_count; i++) {
        BotState* res = &g_game_state.resources[i];
        float bottom_y = res->base.interp_pos.y + res->base.dims.y;

        sort_entries[entry_count].type = ENTITY_TYPE_RESOURCE;
        sort_entries[entry_count].bottom_y = bottom_y;
        sort_entries[entry_count].sort_id = res->base.id;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].data.bot = res;
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
        WorldObject* obstacle = NULL;
        int layers_count = 0;

        switch (entry->type) {
            case ENTITY_TYPE_OBSTACLE:
                obstacle = entry->data.object;
                entity_type_str = "obstacle";
                entity_id = obstacle->id;
                layers_count = obstacle->object_layer_count;
                break;

            case ENTITY_TYPE_STATIC:
                obstacle = entry->data.object;
                entity_type_str = "static";
                entity_id = obstacle->id;
                layers_count = obstacle->object_layer_count;
                break;

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
                // Map bot behavior to the entity_type_str used for entity defaults lookup.
                // Skills and coins are spawned as bots server-side; their behavior
                // string distinguishes them so the correct color / item defaults apply.
                if (strcmp(entry->data.bot->behavior, "skill") == 0)
                    entity_type_str = "skill";
                else if (strcmp(entry->data.bot->behavior, "coin") == 0)
                    entity_type_str = "coin";
                else if (strcmp(entry->data.bot->behavior, "drop") == 0)
                    entity_type_str = "drop";
                else
                    entity_type_str = "bot";
                entity_id = entity_base->id;
                layers_count = entity_base->object_layer_count;
                break;

            case ENTITY_TYPE_RESOURCE:
                entity_base = &entry->data.bot->base;
                entity_type_str = "resource";
                entity_id = entity_base->id;
                layers_count = entity_base->object_layer_count;
                break;
        }

        if (obstacle && entity_id) {
            // entity_type_str is "obstacle" or "static"; resolve the solid-colour
            // fallback from the entity-colour table so each renders its own palette
            // key when no object-layer texture is present.
            Color obstacle_color = presentation_runtime_entity_fallback_color(entity_type_str);

            if (layers_count == 0) {
                Rectangle rect = {
                    obstacle->pos.x * cell_size,
                    obstacle->pos.y * cell_size,
                    obstacle->dims.x * cell_size,
                    obstacle->dims.y * cell_size
                };
                DrawRectangleRec(rect, obstacle_color);
            } else {
                for (int j = 0; j < layers_count && j < MAX_OBJECT_LAYERS; j++) {
                    temp_layers[j] = &obstacle->object_layers[j];
                }

                draw_entity_layers(
                    g_entity_render,
                    entity_id,
                    obstacle->pos.x,
                    obstacle->pos.y,
                    obstacle->dims.x,
                    obstacle->dims.y,
                    DIRECTION_NONE,
                    MODE_IDLE,
                    temp_layers,
                    layers_count,
                    entity_type_str,
                    dev_ui,
                    cell_size,
                    obstacle_color
                );
            }

            continue;
        }

        if (entity_base && entity_id) {
            // Compute the solid-colour fallback once — used both when layers_count==0
            // and passed into draw_entity_layers so it can use the same colour when
            // a texture fails to load instead of a generic gray rectangle.
            Color entity_fallback_color = presentation_runtime_entity_fallback_color(entity_type_str);

            /* Skill/coin projectiles and inert loot drops carry no ground
             * shadow or combat/identity overhead — only players, other
             * players, combat bots (passive/hostile), and resources do. */
            bool is_non_combat_bot = (entry->type == ENTITY_TYPE_BOT)
                && (strcmp(entity_type_str, "skill") == 0
                    || strcmp(entity_type_str, "coin")  == 0
                    || strcmp(entity_type_str, "drop")  == 0);

            /* Draw position — authoritative interpolation, except loot drops,
             * whose top-left is driven by the launch/idle-float FX timeline.
             * A collected drop yields entirely to its detached vacuum flight. */
            float draw_x = entity_base->interp_pos.x;
            float draw_y = entity_base->interp_pos.y;
            if (entry->type == ENTITY_TYPE_BOT && strcmp(entity_type_str, "drop") == 0) {
                /* Per-viewer loot eligibility (AOI bot flag) picks the drop's
                 * particle tint: gold = collectable by us, gray = another
                 * player's loot. */
                const BotState* drop_bot = game_state_find_bot(entity_id);
                bool loot_eligible = (NULL != drop_bot)
                    && 0 != (drop_bot->interaction_flags & INTERACTION_FLAG_LOOT_ELIGIBLE);
                if (!loot_fx_drop_render_pos(entity_id,
                                             entity_base->interp_pos.x,
                                             entity_base->interp_pos.y,
                                             entity_base->dims.x,
                                             entity_base->dims.y,
                                             loot_eligible,
                                             &draw_x, &draw_y)) {
                    continue;
                }
            }

            float render_width = entity_base->dims.x;
            float render_height = entity_base->dims.y;

            /* Ground shadow, shared by every living entity (drawn before the
             * sprite so it sits beneath it). Overhead UI below keeps using
             * the unscaled interp_pos/dims so the nameplate/HP bar never
             * shifts. */
            if (!is_non_combat_bot) {
                draw_entity_shadow(draw_x, draw_y, render_width, render_height, cell_size);
            }

            /* Local-player-exclusive render-scale bump, so the character
             * reads slightly larger than everyone else. */
            if (entry->is_main_player) {
                Rectangle scaled = local_player_view_scaled_footprint(draw_x, draw_y, render_width, render_height);
                draw_x = scaled.x;
                draw_y = scaled.y;
                render_width = scaled.width;
                render_height = scaled.height;
            }

            if (layers_count == 0) {
                /* No object layers — draw a solid colored rectangle as fallback. */
                Rectangle rect = {
                    draw_x * cell_size,
                    draw_y * cell_size,
                    render_width * cell_size,
                    render_height * cell_size
                };
                DrawRectangleRec(rect, entity_fallback_color);
            } else {
                // Convert object layers to pointer array
                for (int j = 0; j < layers_count && j < MAX_OBJECT_LAYERS; j++) {
                    temp_layers[j] = &entity_base->object_layers[j];
                }

                // Use EntityRender system to draw entity with object layers
                // This may load textures which can take time - that's why we unlocked earlier
                draw_entity_layers(
                    g_entity_render,
                    entity_id,
                    draw_x,
                    draw_y,
                    render_width,
                    render_height,
                    entity_base->direction,
                    entity_base->mode,
                    temp_layers,
                    layers_count,
                    entity_type_str,
                    dev_ui,
                    cell_size,
                    entity_fallback_color
                );
            }

            /* On-grid quantity counter above a stacked drop (coins, bundles). */
            if (entry->type == ENTITY_TYPE_BOT && strcmp(entity_type_str, "drop") == 0
                && entity_base->object_layer_count > 0
                && entity_base->object_layers[0].quantity > 1) {
                draw_drop_count(draw_x, draw_y, entity_base->dims.x, cell_size,
                                entity_base->object_layers[0].quantity);
            }

            /* ── Overhead UI — nameplate, capacity bar, HP bar ─────────── */
            /* Skip for skill/coin projectiles and inert loot drops — none of
             * them carry combat/identity overhead (is_non_combat_bot computed
             * above, shared with the ground-shadow gate). */
            if (!is_non_combat_bot) {
                /* Every entity carries a stats_sum (server-clamped sum of its
                 * active stats) used by the overhead capability bar.  */
                bool np_is_player = (entry->type == ENTITY_TYPE_PLAYER
                                  || entry->type == ENTITY_TYPE_OTHER_PLAYER);
                char np_buf[80];
                /* For bots, prefer the alive-layer cache so the nameplate
                 * always reflects the living skin (even when dead/ghost). */
                const ObjectLayerState *np_layers = entity_base->object_layers;
                int np_lc = entity_base->object_layer_count;
                if (!np_is_player) {
                    int alive_lc = 0;
                    const ObjectLayerState *alive = interaction_bubble_get_alive_layers(
                        entity_base->id, &alive_lc);
                    if (alive && alive_lc > 0) {
                        np_layers = alive;
                        np_lc = alive_lc;
                    }
                }
                nameplate_resolve(entity_base->id, np_is_player,
                                  np_layers, np_lc,
                                  obj_layers_mgr_get(),
                                  np_buf, (int)sizeof(np_buf));
                /* Action bots show the action's label (NPC name) once its
                 * metadata loads, fetched by the action code from AOI. The
                 * per-player capability bitmask drives the overhead overlays. */
                uint8_t np_flags = 0;
                /* Provider NPCs (mission/action givers) are immortal and inert:
                 * suppress their HP bar and the Σ-stats value — their action/quest
                 * capability icons still show. */
                bool np_is_provider = false;
                if (!np_is_player) {
                    const BotState* np_bot = game_state_find_bot(entity_base->id);
                    if (np_bot) {
                        np_flags = np_bot->interaction_flags;
                        np_is_provider = (0 == strcmp(np_bot->behavior, "provider"))
                                         || (0 == strcmp(np_bot->behavior, "provider-static"));
                        if (np_bot->action_code[0] != '\0') {
                            action_cache_fetch(np_bot->action_code);
                            const ActionMetadataEntry* am = action_cache_get(np_bot->action_code);
                            if (am && ACTION_CACHE_READY == am->state && am->label[0] != '\0') {
                                strncpy(np_buf, am->label, sizeof(np_buf) - 1);
                                np_buf[sizeof(np_buf) - 1] = '\0';
                            }
                        }
                    }
                }
                EntityOverheadParams ohp = {
                    .name              = np_buf,
                    .stats_sum         = entity_base->stats_sum,
                    .life              = entity_base->life,
                    .max_life          = entity_base->max_life,
                    .show_name         = true,
                    .show_stats        = entity_base->respawn_in <= 0.0f
                                         && (!np_is_provider || np_flags != 0),
                    .show_stats_value  = !np_is_provider,
                    .show_hp           = !np_is_provider && entity_base->max_life > 0.0f
                                         && entity_base->respawn_in <= 0.0f,
                    .status_icon       = entity_base->status_icon,
                    .interaction_flags = np_flags,
                    /* Respawn countdown is local-player-only: a remote client
                     * must never see another player's countdown. */
                    .respawn_seconds   = (entry->is_main_player && entity_base->respawn_in > 0.0f)
                                         ? (int)(entity_base->respawn_in + 0.5f) : 0,
                };
                entity_overhead_ui_draw(
                    &ohp,
                    entity_base->interp_pos.x,
                    entity_base->interp_pos.y,
                    entity_base->dims.x,
                    entity_base->dims.y,
                    cell_size
                );
            }


        }
    }
}

void game_render_player_path(void) {
    const float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Render target position
    if (g_game_state.player.target_pos.x >= 0 && g_game_state.player.target_pos.y >= 0) {
        Rectangle target_rect = {
            g_game_state.player.target_pos.x * cell_size,
            g_game_state.player.target_pos.y * cell_size,
            cell_size,
            cell_size
        };
        /* TARGET is a debug-overlay colour; not part of the engine palette
         * contract. Use a stable constant so it can't be overridden. */
        DrawRectangleRec(target_rect, (Color){ 220, 220, 80, 180 });
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

        /* PATH is a debug-overlay colour, see note above on TARGET. */
        DrawRectangleRec(path_rect, (Color){ 100, 200, 100, 120 });
    }
}

void game_render_aoi_circle(void) {
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    float aoi_radius = g_game_state.aoi_radius * cell_size;

    Vector2 center = {
        (g_game_state.player.base.interp_pos.x + g_game_state.player.base.dims.x / 2.0f) * cell_size,
        (g_game_state.player.base.interp_pos.y + g_game_state.player.base.dims.y / 2.0f) * cell_size
    };

    /* AOI ring is a debug overlay; fixed constant. */
    DrawCircleLines(center.x, center.y, aoi_radius, (Color){ 180, 180, 255, 120 });
}

void game_render_ui(void) {

    // Render error messages (always visible)
    game_render_error_messages();

    if (presentation_runtime_dev_ui()) {
        dev_ui_draw(g_renderer.screen_width, g_renderer.screen_height, 0);
    } else {
        modal_map_draw(g_renderer.screen_width, g_renderer.screen_height);
    }

    // Fullscreen toggle (top-right corner, beside the map/fps HUD box)
    draw_fullscreen_button(g_renderer.screen_width);

    // Entity interaction bubbles (left side, collapsible column)
    interaction_bubble_draw();

    // Quest Journal (right side, below map info, collapsible)
    quest_journal_draw();

    // Inventory bar (always visible in screen space)
    inventory_bar_draw();

    // Loot delivery + slot-arrival sparks land on top of the bar slots.
    int loot_scr = loot_fx_screen_particle_slot_count();
    for (int i = 0; i < loot_scr; i++) {
        LootFxScreenParticle sp;
        if (!loot_fx_screen_particle_at(i, &sp)) continue;
        fx_shape_spark(sp.x, sp.y, sp.size, FX_SPARK_GOLD, 1.0f);
    }

    // Portal hold progress bar (centered above the inventory bar; only while
    // the local player stands inside an active portal)
    draw_portal_progress_bar(g_renderer.screen_width, g_renderer.screen_height);

    // Zoom buttons (above inventory bar, right side)
    draw_zoom_buttons(g_renderer.screen_width, g_renderer.screen_height);

    // Inventory modal (shown on top of everything when open)
    if (inventory_modal_is_open()) {
        inventory_modal_draw();
    }

    // Intermediate interaction modal (below dialogue in the draw order)
    if (modal_interact_is_open()) {
        modal_interact_draw();
    }

    // Dialogue modal
    if (modal_dialogue_is_open()) {
        modal_dialogue_draw();
    }

    // Transient notification toast — top-most, above every modal.
    modal_notification_draw();
}

typedef struct {
    char   text[MAX_MESSAGE_SIZE];
    double display_time;
} ErrorBanner;

static ErrorBanner g_error_banner = {0};

void game_render_set_error_message(const char* msg) {
    assert(msg);
    if (msg[0] == '\0') {
        g_error_banner.text[0] = '\0';
        g_error_banner.display_time = 0.0;
        return;
    }
    strncpy(g_error_banner.text, msg, sizeof(g_error_banner.text) - 1);
    g_error_banner.text[sizeof(g_error_banner.text) - 1] = '\0';
    g_error_banner.display_time = GetTime();
}

const char* game_render_get_error_message(void) {
    return g_error_banner.text;
}

void game_render_error_messages(void) {
    if (g_error_banner.text[0] != '\0') {
        double current_time = GetTime();
        if (current_time - g_error_banner.display_time < 5.0) {
            int font_size = 16;
            int text_width = MeasureText(g_error_banner.text, font_size);
            int x = (g_renderer.screen_width - text_width) / 2;
            int y = 100;

            DrawText(g_error_banner.text, x, y, font_size, (Color){ 220, 80, 80, 255 });
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
    return GetWorldToScreen2D(scaled, camera_get());
}

Vector2 game_render_screen_to_world(Vector2 screen_pos) {
    Vector2 world = GetScreenToWorld2D(screen_pos, camera_get());
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
    destroy_entity_render(g_entity_render);
    g_entity_render = NULL;

    // Cleanup object layers manager
    destroy_object_layers_manager();

    // Unload font if loaded
    if (IsFontValid(g_renderer.game_font)) {
        UnloadFont(g_renderer.game_font);
    }

    ui_icon_cleanup();
    dialogue_data_cleanup();
}

