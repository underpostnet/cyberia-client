#include "game_render.h"
#include "ui/session_gate.h"
#include "ui/text.h"
#include "camera.h"
#include "sim/local_player.h"
#include "local_player_view.h"
#include "overhead_occlusion.h"

#include "data/dialogue_data.h"
#include "content/presentation_runtime.h"
#include "entity_render.h"
#include "sim/game_state.h"
#include "sim/replication.h"
#include "sim/self_motion.h"
#include "ui/toolbar.h"
#include "world/object_layers_management.h"
#include "ui/ol_as_animated_ico.h"
#include "ui/dev_ui.h"
#include "ui/entity_overhead_ui.h"
#include "ui/floating_combat_text.h"
#include "fx/fx_shapes.h"
#include "ui/interaction_bubble.h"
#include "ui/hud_minimap_overlay.h"
#include "fx/fx_death.h"
#include "fx/fx_heal_aura.h"
#include "fx/fx_hit_burst.h"
#include "fx/fx_level_up.h"
#include "fx/fx_portal_aura.h"
#include "fx/fx_obstacle_burst.h"
#include "ui/inventory_bar.h"
#include "fx/fx_loot.h"
#include "ui/inventory_modal.h"
#include "ui/modal_dialogue.h"
#include "data/action_cache.h"
#include "ui/modal_instance_map.h"
#include "ui/chat_panel.h"
#include "ui/social_panel.h"
#include "ui/container_panel.h"
#include "ui/modal_interact.h"
#include "ui/modal_map.h"
#include "ui/nameplate.h"
#include "ui/quest_journal.h"
#include "ui/modal_notification.h"
#include "fx/fx_dust.h"
#include "fx/fx_tap.h"
#include "input/input.h"
#include "ui/hud_joystick.h"
#include "ui/ui_icon.h"
#include "log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global renderer instance
GameRenderer g_renderer = {0};

// Global manager for entity rendering with object layers
// ObjectLayersManager is owned by its module — access via obj_layers_mgr_get().
static EntityRender* g_entity_render = NULL;

int game_render_init(int screen_width, int screen_height) {

    // Initialize renderer state
    memset(&g_renderer, 0, sizeof(GameRenderer));

    g_renderer.screen_width = screen_width;
    g_renderer.screen_height = screen_height;

    // Initialize object layer rendering system
    create_object_layers_manager();
    ObjectLayersManager* olm = obj_layers_mgr_get();

    g_entity_render = create_entity_render(olm);
    if (NULL == g_entity_render) {
        LOG_ERROR("Failed to create entity render system");
        game_render_cleanup();
        return -1;
    }
    inventory_bar_init(olm);
    inventory_modal_init(olm);
    modal_dialogue_init();
    modal_interact_init();
    dialogue_data_init();
    interaction_bubble_init();
    quest_journal_init();
    modal_notification_init();
    return 0;
}

void game_render_set_screen_size(int width, int height) {
    g_renderer.screen_width = width;
    g_renderer.screen_height = height;
}

void game_render_frame(void) {
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

    // Floating joystick (screen space), under every HUD surface and modal.
    hud_joystick_draw(input_pointer_gesture());

    // Render UI (screen space)
    game_render_ui();

    // Tap effects are rendered in screen space so input systems can spawn
    // them directly from screen coordinates without camera conversions.
    fx_tap_draw();

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
static void game_render_fx_loot(void) {
    if (!g_entity_render) return;
    const float cell_size = world_cell_size();

    /* Bordered-square particles (ambient float + collection burst). */
    int pslots = fx_loot_particle_slot_count();
    for (int i = 0; i < pslots; i++) {
        FxLootParticle pt;
        if (!fx_loot_particle_at(i, &pt)) continue;
        Color body = (FX_LOOT_TINT_GRAY == pt.tint) ? FX_SPARK_GRAY : FX_SPARK_GOLD;
        fx_shape_spark(pt.x * cell_size, pt.y * cell_size,
                       pt.size * cell_size, body, 1.0f);
    }

    /* Vacuum flight tokens. */
    int slots = fx_loot_slot_count();
    for (int i = 0; i < slots; i++) {
        FxLootRender fx;
        if (!fx_loot_render_at(i, &fx)) continue;

        float half = fx.size * 0.5f;

        ObjectLayerState layer = { .active = true, .quantity = 1 };
        strncpy(layer.item_id, fx.item_id, MAX_ITEM_ID_LENGTH - 1);
        ObjectLayerState* layers[1] = { &layer };

        draw_entity_layers(
            g_entity_render,
            0,                     /* no entity: the animation keys on the item */
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
            (Color){ 255, 225, 130, 255 },
            WHITE
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

    // 3. Footstep dust - on the ground, so under every entity that raised it
    fx_dust_draw();

    // 4. Entities (sorted by depth) - players and bots
    game_render_entities();
    if (g_entity_render) { entity_render_gc(g_entity_render); }

    // 5. Player path (if dev_ui enabled) - visual debug aid
    if (presentation_runtime_dev_ui()) {
        game_render_player_path();
    }

    // 6. AOI circle (if dev_ui enabled) - visual debug aid
    if (presentation_runtime_dev_ui()) {
        game_render_aoi_circle();
    }

    // 7. Foregrounds (always on top of entities) - creates depth
    game_render_foregrounds();

    // 8. Effects — FCT pop-ups, loot flights, level-up, heal and transport auras, hit bursts, death cues
    fct_draw();
    game_render_fx_loot();
    fx_level_up_draw();
    fx_heal_aura_draw();
    fx_portal_aura_draw();
    fx_hit_burst_draw();
    fx_death_draw();
    fx_obstacle_burst_draw();

    // 9. Grid overlay (if dev_ui enabled - renders on top of everything)
    if (presentation_runtime_dev_ui()) {
        game_render_grid();
    }
}

void game_render_grid(void) {
    // Only render grid overlay when dev_ui is enabled
    // Grid is transparent with red lines and white border on top of everything

    float cell_size = world_cell_size();
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
    const float cell_size = world_cell_size();
    bool any_floor = false;

    for (int i = 0; i < g_game_state.scenery_count; i++) {
        const EntityID id = g_game_state.scenery[i];
        if (DRAW_FLOOR != game_state_data(id)->draw_order) continue;
        any_floor = true;
        const EntityTransform* tf = game_state_transform(id);
        const Appearance* ap = game_state_appearance(id);
        Color floor_color = presentation_runtime_palette("FLOOR");

        if (ap->layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < ap->layer_count; j++) {
                layers[j] = &OBJ_LAYERS(ap)[j];
            }

            draw_entity_layers(
                g_entity_render,
                id,
                tf->pos.x,
                tf->pos.y,
                tf->size.x,
                tf->size.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                ap->layer_count,
                "floor",
                presentation_runtime_dev_ui(),
                cell_size,
                floor_color,
                WHITE
            );
        } else {
            Color fallback = presentation_runtime_palette("FLOOR_BACKGROUND");
            Rectangle rect = {
                tf->pos.x * cell_size,
                tf->pos.y * cell_size,
                tf->size.x * cell_size,
                tf->size.y * cell_size
            };
            DrawRectangleRec(rect, fallback);
        }
    }

    // In dev mode, show that floors are missing with a dark background
    if (!any_floor && presentation_runtime_dev_ui()) {
        DrawRectangle(0, 0, g_renderer.screen_width * 2, g_renderer.screen_height * 2,
                      (Color){20, 20, 20, 255});
    }
}

void game_render_world_objects(void) {
    const float cell_size = world_cell_size();

    // Render portals
    for (int i = 0; i < g_game_state.portal_count; i++) {
        const EntityID portal_id = g_game_state.portals[i];
        const Portal* portal = game_state_portal(portal_id);
        const EntityTransform* tf = game_state_transform(portal_id);
        const Appearance* ap = game_state_appearance(portal_id);
        Color portal_color = presentation_runtime_palette("PORTAL");

        if (ap->layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < ap->layer_count; j++) {
                layers[j] = &OBJ_LAYERS(ap)[j];
            }

            draw_entity_layers(
                g_entity_render,
                portal_id,
                tf->pos.x,
                tf->pos.y,
                tf->size.x,
                tf->size.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                ap->layer_count,
                "portal",
                presentation_runtime_dev_ui(),
                cell_size,
                portal_color,
                WHITE
            );
        } else {
            Rectangle rect = {
                tf->pos.x * cell_size,
                tf->pos.y * cell_size,
                tf->size.x * cell_size,
                tf->size.y * cell_size
            };
            DrawRectangleRec(rect, portal_color);
        }

        /* Overhead: portals carry only the 'portal' presence icon (transport)
         * plus a "<targetMapCode> <x>,<y>" destination nameplate. */
        char portal_name[80];
        nameplate_resolve_portal(portal->target_map, portal->target_cell.x,
                                 portal->target_cell.y, portal_name, (int)sizeof(portal_name));
        EntityOverheadParams ohp = {
            .name              = portal_name,
            .show_name         = portal_name[0] != '\0',
            .show_stats        = false,
            .show_hp           = false,
            .status_icon       = game_state_data(portal_id)->status_icon,
            .interaction_flags = 0,
        };
        entity_overhead_ui_draw(&ohp, tf->pos.x, tf->pos.y,
                                tf->size.x, cell_size);
    }
}

/* Equal depths sort by ID across snapshots. */
static int compare_depth(float depth_a, EntityID id_a, float depth_b, EntityID id_b) {
    if (depth_b > depth_a) return -1;
    if (depth_b < depth_a) return 1;
    return (id_a > id_b) - (id_a < id_b);
}

static int compare_foregrounds_by_depth(const void* a, const void* b) {
    const EntityID ia = *(const EntityID*)a;
    const EntityID ib = *(const EntityID*)b;
    const EntityTransform* ta = game_state_transform(ia);
    const EntityTransform* tb = game_state_transform(ib);
    return compare_depth(ta->pos.y + ta->size.y, ia, tb->pos.y + tb->size.y, ib);
}

void game_render_foregrounds(void) {
    const float cell_size = world_cell_size();

    static EntityID sorted[MAX_OBJECTS];
    int count = 0;
    for (int i = 0; i < g_game_state.scenery_count; i++) {
        const EntityID id = g_game_state.scenery[i];
        if (DRAW_FOREGROUND == game_state_data(id)->draw_order) sorted[count++] = id;
    }
    qsort(sorted, (size_t)count, sizeof(sorted[0]), compare_foregrounds_by_depth);

    // Render foregrounds (always on top of entities)
    for (int i = 0; i < count; i++) {
        const EntityID id = sorted[i];
        const EntityTransform* tf = game_state_transform(id);
        const Appearance* ap = game_state_appearance(id);
        const Color tint = Fade(WHITE, overhead_occlusion_opacity(id));
        const Color fg_color = ColorTint(presentation_runtime_palette("FOREGROUND"), tint);

        if (ap->layer_count > 0) {
            ObjectLayerState* layers[MAX_OBJECT_LAYERS];
            for (int j = 0; j < ap->layer_count; j++) {
                layers[j] = &OBJ_LAYERS(ap)[j];
            }

            draw_entity_layers(
                g_entity_render,
                id,
                tf->pos.x,
                tf->pos.y,
                tf->size.x,
                tf->size.y,
                DIRECTION_NONE,
                MODE_IDLE,
                layers,
                ap->layer_count,
                "foreground",
                presentation_runtime_dev_ui(),
                cell_size,
                fg_color,
                tint
            );
        } else {
            Rectangle rect = {
                tf->pos.x * cell_size,
                tf->pos.y * cell_size,
                tf->size.x * cell_size,
                tf->size.y * cell_size
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
    float depth_y;
    EntityID sort_id;
    int source_order;
    bool is_main_player;
} EntitySortEntry;

// Comparison function for qsort - entities with lower Y render first (appear behind)
static int compare_entities_by_depth(const void* a, const void* b) {
    const EntitySortEntry* ea = (const EntitySortEntry*)a;
    const EntitySortEntry* eb = (const EntitySortEntry*)b;
    const int depth_order = compare_depth(ea->depth_y, ea->sort_id, eb->depth_y, eb->sort_id);
    if (0 != depth_order) return depth_order;

    if (ea->type != eb->type) {
        return (int)ea->type - (int)eb->type;
    }

    return ea->source_order - eb->source_order;
}

/* The bottom edge of an entity as drawn, in grid units: the depth sort key. */
static float drawn_bottom(EntityID id) {
    return game_state_render(id)->interp_pos.y + game_state_transform(id)->size.y;
}

/* The drawn box of an entity at its interpolated position, in world pixels. */
static Rectangle entity_screen_rect(EntityID id, float cell_size) {
    const Vector2 at   = game_state_render(id)->interp_pos;
    const Vector2 size = game_state_transform(id)->size;
    return (Rectangle){ at.x * cell_size, at.y * cell_size, size.x * cell_size, size.y * cell_size };
}

void game_render_entities(void) {
    // Safety check - ensure entity render system is initialized
    if (!g_entity_render) {
        // Fallback to simple rendering if entity render system not initialized
        const float cell_size = world_cell_size();

        // Draw simple rectangles as fallback to ensure entities are visible
        DrawRectangleRec(entity_screen_rect(g_local_player.id, cell_size),
                         presentation_runtime_palette("PLAYER"));

        // Also draw other players as rectangles
        for (int i = 0; i < g_game_state.other_player_count; i++) {
            DrawRectangleRec(entity_screen_rect(g_game_state.other_players[i], cell_size),
                             presentation_runtime_palette("OTHER_PLAYER"));
        }

        // Draw bots as rectangles
        for (int i = 0; i < g_game_state.bot_count; i++) {
            DrawRectangleRec(entity_screen_rect(g_game_state.bots[i], cell_size),
                             (Color){100, 200, 100, 200});
        }
        return;
    }

    const float cell_size = world_cell_size();
    const bool dev_ui = presentation_runtime_dev_ui();

    // Create array to hold all depth-sorted actors
    static EntitySortEntry sort_entries[MAX_DEPTH_SORT_ENTRIES];
    int entry_count = 0;

    // Add depth-sorted scenery, blocking or passable, so the player can pass
    // behind or in front of it by the same depth rules as entities.
    for (int i = 0; i < g_game_state.scenery_count; i++) {
        const EntityID id = g_game_state.scenery[i];
        const EntityData* data = game_state_data(id);
        if (DRAW_SORTED != data->draw_order) continue;

        sort_entries[entry_count].type = (data->flags & ENTITY_FLAG_BLOCKS) ? ENTITY_TYPE_OBSTACLE
                                                                            : ENTITY_TYPE_STATIC;
        /* Depleted obstacles draw below entities on their cells. */
        sort_entries[entry_count].depth_y = game_state_obstacle_depleted(game_state_vitals(id))
                                            ? game_state_render(id)->interp_pos.y : drawn_bottom(id);
        sort_entries[entry_count].sort_id = id;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add main player to sort list. It has no slot between init_data and the
    // first snapshot, and the preload loop already draws then.
    if (game_state_has(g_local_player.id, COMP_TRANSFORM)) {
        sort_entries[entry_count].type = ENTITY_TYPE_PLAYER;
        sort_entries[entry_count].depth_y = drawn_bottom(g_local_player.id);
        sort_entries[entry_count].sort_id = g_local_player.id;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].is_main_player = true;
        entry_count++;
    }

    // Add other players to sort list
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        EntityID player = g_game_state.other_players[i];
        float bottom_y = drawn_bottom(player);

        sort_entries[entry_count].type = ENTITY_TYPE_OTHER_PLAYER;
        sort_entries[entry_count].depth_y = bottom_y;
        sort_entries[entry_count].sort_id = player;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add bots to sort list
    for (int i = 0; i < g_game_state.bot_count; i++) {
        EntityID bot = g_game_state.bots[i];
        float bottom_y = drawn_bottom(bot);

        sort_entries[entry_count].type = ENTITY_TYPE_BOT;
        sort_entries[entry_count].depth_y = bottom_y;
        sort_entries[entry_count].sort_id = bot;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Add resources to sort list
    for (int i = 0; i < g_game_state.resource_count; i++) {
        EntityID res = g_game_state.resources[i];
        float bottom_y = drawn_bottom(res);

        sort_entries[entry_count].type = ENTITY_TYPE_RESOURCE;
        sort_entries[entry_count].depth_y = bottom_y;
        sort_entries[entry_count].sort_id = res;
        sort_entries[entry_count].source_order = entry_count;
        sort_entries[entry_count].is_main_player = false;
        entry_count++;
    }

    // Sort entities by depth.
    qsort(sort_entries, entry_count, sizeof(EntitySortEntry), compare_entities_by_depth);

    // Allocate temporary layer pointer array for all entities
    ObjectLayerState* temp_layers[MAX_OBJECT_LAYERS];

    // Render entities in sorted order using EntityRender system
    // We don't need lock here as we're using copied data from sort_entries
    for (int i = 0; i < entry_count; i++) {
        EntitySortEntry* entry = &sort_entries[i];

        const char* entity_type_str = NULL;
        EntityID entity_id = 0;
        bool scenery = false;

        switch (entry->type) {
            case ENTITY_TYPE_OBSTACLE:
                scenery = true;
                entity_type_str = "obstacle";
                entity_id = entry->sort_id;
                break;

            case ENTITY_TYPE_STATIC:
                scenery = true;
                entity_type_str = "static";
                entity_id = entry->sort_id;
                break;

            case ENTITY_TYPE_PLAYER:
                entity_type_str = entry->is_main_player ? "self" : "other";
                entity_id = entry->sort_id;
                break;

            case ENTITY_TYPE_OTHER_PLAYER:
                entity_type_str = "other";
                entity_id = entry->sort_id;
                break;

            case ENTITY_TYPE_BOT:
                // Map bot behavior to the entity_type_str used for entity defaults lookup.
                // Skills, coins and loot tokens are spawned as bots server-side; their
                // behavior distinguishes them so the correct color / item defaults apply.
                // A loot token travels with the behavior "drop".
                if (strcmp(game_state_agent(entry->sort_id)->behavior, "skill") == 0)
                    entity_type_str = "skill";
                else if (strcmp(game_state_agent(entry->sort_id)->behavior, "coin") == 0)
                    entity_type_str = "coin";
                else if (strcmp(game_state_agent(entry->sort_id)->behavior, "drop") == 0)
                    entity_type_str = "loot";
                else
                    entity_type_str = "bot";
                entity_id = entry->sort_id;
                break;

            case ENTITY_TYPE_RESOURCE:
                entity_type_str = "resource";
                entity_id = entry->sort_id;
                break;
        }
        const Appearance* ap = game_state_appearance(entity_id);
        const int layers_count = ap->layer_count;

        if (scenery) {
            const Vitals* v = game_state_vitals(entity_id);
            /* A depleted obstacle with no dead items is gone. */
            if (game_state_obstacle_depleted(v) && 0 == layers_count) continue;

            // entity_type_str is "obstacle" or "static"; resolve the solid-colour
            // fallback from the entity-colour table so each renders its own palette
            // key when no object-layer texture is present.
            Color obstacle_color = presentation_runtime_entity_fallback_color(entity_type_str);
            const EntityTransform* tf = game_state_transform(entity_id);

            if (layers_count == 0) {
                Rectangle rect = {
                    tf->pos.x * cell_size,
                    tf->pos.y * cell_size,
                    tf->size.x * cell_size,
                    tf->size.y * cell_size
                };
                DrawRectangleRec(rect, obstacle_color);
            } else {
                for (int j = 0; j < layers_count && j < MAX_OBJECT_LAYERS; j++) {
                    temp_layers[j] = &OBJ_LAYERS(ap)[j];
                }

                draw_entity_layers(
                    g_entity_render,
                    entity_id,
                    tf->pos.x,
                    tf->pos.y,
                    tf->size.x,
                    tf->size.y,
                    DIRECTION_NONE,
                    MODE_IDLE,
                    temp_layers,
                    layers_count,
                    entity_type_str,
                    dev_ui,
                    cell_size,
                    obstacle_color,
                    WHITE
                );
            }
            entity_overhead_ui_draw_obstacle(v, tf, cell_size);

            continue;
        }

        if (!scenery && entity_id) {
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
                    || strcmp(entity_type_str, "loot")  == 0);

            /* Draw position — authoritative interpolation, except loot drops,
             * whose top-left is driven by the launch/idle-float FX timeline.
             * A collected drop yields entirely to its detached vacuum flight. */
            const Vector2 interp_pos = game_state_render(entity_id)->interp_pos;
            const Vector2 size       = game_state_transform(entity_id)->size;
            float draw_x = interp_pos.x;
            float draw_y = interp_pos.y;
            if (entry->type == ENTITY_TYPE_BOT && strcmp(entity_type_str, "loot") == 0) {
                /* Per-viewer loot eligibility (self interaction flag) picks the
                 * drop's particle tint: gold = collectable by us, gray =
                 * another player's loot. */
                bool loot_eligible = 0 != (local_player_interaction_flags(entity_id)
                                           & INTERACTION_FLAG_LOOT_ELIGIBLE);
                if (!fx_loot_drop_render_pos(entity_id,
                                             interp_pos.x,
                                             interp_pos.y,
                                             size.x,
                                             size.y,
                                             loot_eligible,
                                             &draw_x, &draw_y)) {
                    continue;
                }
            }

            float render_width = size.x;
            float render_height = size.y;

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
                    temp_layers[j] = &OBJ_LAYERS(ap)[j];
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
                    entry->is_main_player ? local_player_view_direction()
                                          : game_state_motion(entity_id)->direction,
                    entry->is_main_player ? local_player_view_mode()
                                          : game_state_motion(entity_id)->mode,
                    temp_layers,
                    layers_count,
                    entity_type_str,
                    dev_ui,
                    cell_size,
                    entity_fallback_color,
                    WHITE
                );
            }

            /* On-grid quantity counter above a stacked drop (coins, bundles). */
            if (entry->type == ENTITY_TYPE_BOT && strcmp(entity_type_str, "loot") == 0
                && ap->layer_count > 0
                && OBJ_LAYERS(ap)[0].quantity > 1) {
                draw_drop_count(draw_x, draw_y, size.x, cell_size,
                                OBJ_LAYERS(ap)[0].quantity);
            }

            /* ── Overhead UI — nameplate, capacity bar, HP bar ─────────── */
            /* Skip for skill/coin projectiles and inert loot drops — none of
             * them carry combat/identity overhead (is_non_combat_bot computed
             * above, shared with the ground-shadow gate). */
            if (!is_non_combat_bot) {
                /* Every entity carries a stats_sum (server sum of its
                 * active stats) used by the overhead capability bar.  */
                bool np_is_player = game_state_is_player(entity_id);
                char np_buf[80];
                /* For bots, prefer the alive-layer cache so the nameplate
                 * always reflects the living skin (even when dead/ghost). */
                const ObjectLayerState *np_layers = OBJ_LAYERS(ap);
                int np_lc = ap->layer_count;
                if (!np_is_player) {
                    int alive_lc = 0;
                    const ObjectLayerState *alive = interaction_bubble_get_alive_layers(
                        entity_id, &alive_lc);
                    if (alive && alive_lc > 0) {
                        np_layers = alive;
                        np_lc = alive_lc;
                    }
                }
                nameplate_resolve(entity_id, np_is_player,
                                  np_layers, np_lc,
                                  np_buf, (int)sizeof(np_buf));
                /* Action bots show the action's label (NPC name) once its
                 * metadata loads, fetched by the action code from AOI. The
                 * per-player capability bitmask drives the overhead overlays. */
                uint8_t np_flags = 0;
                /* Provider NPCs (mission/action givers) are immortal and inert:
                 * suppress their HP bar and the level and stats-sum lead — their action/quest
                 * capability icons still show. */
                bool np_is_provider = false;
                if (!np_is_player) {
                    const EntityID np_bot = entity_id;
                    if (game_state_has(np_bot, COMP_AGENT)) {
                        np_flags = local_player_interaction_flags(np_bot);
                        np_is_provider = (0 == strcmp(game_state_agent(np_bot)->behavior, "provider"))
                                         || (0 == strcmp(game_state_agent(np_bot)->behavior, "provider-static"));
                        if (game_state_agent(np_bot)->action_code[0] != '\0') {
                            action_cache_fetch(game_state_agent(np_bot)->action_code);
                            const ActionMetadataEntry* am = action_cache_get(game_state_agent(np_bot)->action_code);
                            if (am && META_CACHE_READY == am->head.state && am->label[0] != '\0') {
                                strncpy(np_buf, am->label, sizeof(np_buf) - 1);
                                np_buf[sizeof(np_buf) - 1] = '\0';
                            }
                        }
                    }
                }
                /* A running level-up or death cue takes the whole overhead
                 * space: every row and the presence icon yield to it. */
                bool cue = fx_level_up_active(entity_id) || fx_death_active(entity_id);
                const Vitals* v = game_state_vitals(entity_id);
                EntityOverheadParams ohp = {
                    .name              = np_buf,
                    .stats_sum         = game_state_progress(entity_id)->stats_sum,
                    .level             = game_state_progress(entity_id)->level,
                    .life              = (float)v->life,
                    .max_life          = (float)v->max_life,
                    .xp_ratio          = entry->is_main_player ? local_player_xp_ratio() : 0.0f,
                    .xp                = entry->is_main_player ? g_local_player.xp : 0.0,
                    .xp_next           = entry->is_main_player ? g_local_player.next_level_xp : 0.0,
                    .show_name         = !cue,
                    .show_stats        = !cue && 0 >= v->respawn_in
                                         && (!np_is_provider || np_flags != 0),
                    .show_stats_value  = !np_is_provider,
                    .show_hp           = !cue && !np_is_provider && 0 < v->max_life
                                         && 0 >= v->respawn_in,
                    .status_icon       = cue ? 0 : game_state_data(entity_id)->status_icon,
                    .interaction_flags = np_flags,
                    .relation          = np_is_player && !entry->is_main_player
                                         ? local_player_interaction_flags(entity_id) &
                                               (INTERACTION_FLAG_FRIEND | INTERACTION_FLAG_PARTY_MEMBER)
                                         : 0,
                    .on_portal         = !cue && np_is_player && game_state_player(entity_id)->on_portal,
                    .portal_hold       = np_is_player ? game_state_player(entity_id)->portal_hold : 0.0f,
                    .portal_fee        = entry->is_main_player ? g_local_player.portal_fee : 0,
                    .portal_short      = entry->is_main_player && local_player_coins() < g_local_player.portal_fee,
                    /* Respawn countdown is local-player-only: a remote client
                     * must never see another player's countdown. */
                    .respawn_seconds   = (!cue && entry->is_main_player) ? v->respawn_in : 0,
                };
                entity_overhead_ui_draw(
                    &ohp,
                    interp_pos.x,
                    interp_pos.y,
                    size.x,
                    cell_size
                );
            }


        }
    }
}

void game_render_player_path(void) {
    const float cell_size = world_cell_size();
    const self_motion_t* self = prediction_self();
    const int route_count = self->path_count;
    const Vector2* route = self->path;

    // Render target position
    if (0 < route_count) {
        const Vector2 target = route[route_count - 1];
        Rectangle target_rect = {
            target.x * cell_size,
            target.y * cell_size,
            cell_size,
            cell_size
        };
        /* TARGET is a debug-overlay colour; not part of the engine palette
         * contract. Use a stable constant so it can't be overridden. */
        DrawRectangleRec(target_rect, (Color){ 220, 220, 80, 180 });
    }

    // Render path
    for (int i = 0; i < route_count; i++) {
        Vector2 path_point = route[i];

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
    if (!game_state_has(g_local_player.id, COMP_TRANSFORM)) return;
    float cell_size = world_cell_size();
    float aoi_radius = g_game_state.aoi_radius * cell_size;

    const Rectangle box = entity_screen_rect(g_local_player.id, cell_size);
    Vector2 center = { box.x + box.width / 2.0f, box.y + box.height / 2.0f };

    /* AOI ring is a debug overlay; fixed constant. */
    DrawCircleLines(center.x, center.y, aoi_radius, (Color){ 180, 180, 255, 120 });
}

/* Screen-space loot FX (slot delivery sparks + the reduction "expend" spray, and
 * the item icon riding either stream). Drawn in a top pass above the HUD/modals,
 * and after the notification toast (see game_render_ui), so a reward's delivery
 * flight — which originates from inside the notification card — is never dimmed
 * or clipped by the toast's overlay while it travels to the inventory bar. */
static void draw_loot_screen_fx(void) {
    int loot_scr = fx_loot_screen_particle_slot_count();
    for (int i = 0; i < loot_scr; i++) {
        FxLootScreenParticle sp;
        if (!fx_loot_screen_particle_at(i, &sp)) continue;
        fx_shape_spark(sp.x, sp.y, sp.size, FX_SPARK_GOLD, sp.alpha);
    }

    int loot_tok = fx_loot_delivery_token_slot_count();
    for (int i = 0; i < loot_tok; i++) {
        FxLootDeliveryToken tk;
        if (!fx_loot_delivery_token_at(i, &tk)) continue;
        Color tint = { 255, 255, 255, (unsigned char)(255.0f * tk.alpha + 0.5f) };
        ol_as_ico_draw(obj_layers_mgr_get(), tk.item_id,
                       (int)(tk.x - tk.size * 0.5f), (int)(tk.y - tk.size * 0.5f),
                       (int)tk.size, OL_ICO_DEFAULT_DIR, 0, tint);
    }
}

void game_render_ui(void) {

    // Entity interaction bubbles (left side, collapsible column)
    interaction_bubble_draw();

    // Social, Quest Journal and chat (right side) — grid-level HUD, below every modal.
    social_panel_draw();
    quest_journal_draw();
    chat_panel_draw();

    // Read-only minimap HUD. It never participates in UI input coverage.
    hud_minimap_overlay_draw();

    // The inventory bar stays live and on top while any bottom modal is open
    // (interact, dialogue, or the inventory modal itself), so slots remain
    // clickable and never sit under a dim overlay.
    bool bar_on_top = modal_interact_is_open() || modal_dialogue_is_open() ||
                      inventory_modal_is_open();
    if (!bar_on_top) {
        inventory_bar_draw();
    }

    // Inventory modal (shown on top of everything when open)
    if (inventory_modal_is_open()) {
        inventory_modal_draw();
    }

    // Intermediate interaction modal (below dialogue in the draw order).
    // Hidden while the mobile fullscreen dialogue reader is up.
    if (modal_interact_is_open() && !modal_dialogue_is_fullscreen()) {
        modal_interact_draw();
    }

    // Dialogue modal
    if (modal_dialogue_is_open()) {
        modal_dialogue_draw();
    }

    if (bar_on_top) {
        inventory_bar_draw();
    }

    // A dragged container slot rides above every surface, the bar included.
    container_panel_draw_drag();

    // Instance Map content inside the modal_map container — independent
    // visualization system above the gameplay UI (world stays visible
    // behind the translucent container).
    modal_instance_map_draw(g_renderer.screen_width, g_renderer.screen_height);

    // Top toolbar + the compact map readout it hosts draw above the
    // expanded container so its toggles can retract it.
    toolbar_draw(g_renderer.screen_width);
    modal_map_draw();

    if (presentation_runtime_dev_ui()) {
        dev_ui_draw(g_renderer.screen_width, g_renderer.screen_height);
    }

    // Transient notification toast — draws first so the loot delivery flight
    // (a reward travelling out of the toast's own reward slot) lands on top of
    // it instead of being dimmed by the toast's overlay.
    modal_notification_draw();

    // Screen-space loot FX (pickup delivery + the reduction "expend" spray) —
    // above the HUD, every modal, and the notification toast.
    draw_loot_screen_fx();

    // The end of the session covers everything.
    session_gate_draw((Rectangle){ 0, 0, (float)g_renderer.screen_width, (float)g_renderer.screen_height });
}

Vector2 game_render_world_to_screen(Vector2 world_pos) {
    float cell_size = world_cell_size();
    Vector2 scaled = {world_pos.x * cell_size, world_pos.y * cell_size};
    return GetWorldToScreen2D(scaled, camera_get());
}

Vector2 game_render_screen_to_world(Vector2 screen_pos) {
    Vector2 world = GetScreenToWorld2D(screen_pos, camera_get());
    float cell_size = world_cell_size();
    return (Vector2){world.x / cell_size, world.y / cell_size};
}

void game_render_cleanup(void) {

    // Cleanup entity rendering system
    destroy_entity_render(g_entity_render);
    g_entity_render = NULL;

    // Cleanup object layers manager
    destroy_object_layers_manager();

    dialogue_data_cleanup();
}
