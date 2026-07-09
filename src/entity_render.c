#include "entity_render.h"
#include "ui/text.h"
#include "object_layers_management.h"
#include "layer_z_order.h"
#include "hash_table.h"
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define ANIM_TABLE_INITIAL_CAPACITY 4096
#define MAX_LAYERS_PER_ENTITY 20
#define DEFAULT_FRAME_DURATION_MS 100

/* Ground-shadow tuning — a small flat ellipse under the feet, not a puddle. */
#define ENTITY_SHADOW_RADIUS_RATIO 0.34f
#define ENTITY_SHADOW_SQUASH 0.45f

static const Color ENTITY_SHADOW_COLOR = { 10, 8, 6, 130 };

/* Animation states untouched for this long are assumed to belong to entities
 * that left the AOI and are evicted by entity_render_gc(). */
#define ANIM_IDLE_EVICT_SECONDS 4.0

// --- Data Structures ---

typedef struct {
    int   last_direction_enum;   /* Direction cast to int; -1 = unset */
    int   last_mode_enum;        /* ObjectLayerMode cast to int; -1 = unset */
    float time_acc;              /* delta accumulator in seconds */
    double last_access_time;     /* GC sentinel — set on every get/create */
    int   frame_index;
    Direction last_facing_direction;
    bool  textures_ready;
    int   failed_texture_attempts;
} AnimationState;

struct EntityRender {
    ObjectLayersManager* obj_layers_mgr;
    HashTable animations;  // "<entity_id>_<item_id>" → AnimationState*
};

typedef struct {
    ObjectLayerState* state;
    ObjectLayer* layer;
    AtlasSpriteSheetData* atlas;
    int priority;
} LayerRenderInfo;

// --- Helper Functions ---

/* Pool allocator for AnimationState. With 200+ entities × up to 20 layers,
 * per-entry malloc/free churns the WASM heap; a block free-list keeps the
 * fixed-size records contiguous and recycles them on eviction. Blocks live
 * for the process lifetime (single EntityRender instance). */
#define ANIM_POOL_BLOCK 512

typedef union AnimNode {
    AnimationState   anim;        /* live entry (union first member) */
    union AnimNode*  next_free;   /* free-list link when recycled */
} AnimNode;

typedef struct AnimBlock {
    struct AnimBlock* next;
    AnimNode          nodes[ANIM_POOL_BLOCK];
} AnimBlock;

static AnimBlock* s_anim_blocks = NULL;
static AnimNode*  s_anim_free   = NULL;

static AnimationState* anim_pool_alloc(void) {
    if (!s_anim_free) {
        AnimBlock* b = malloc(sizeof(AnimBlock));
        assert(b);
        b->next = s_anim_blocks;
        s_anim_blocks = b;
        for (int i = 0; i < ANIM_POOL_BLOCK; i++) {
            b->nodes[i].next_free = s_anim_free;
            s_anim_free = &b->nodes[i];
        }
    }
    AnimNode* n = s_anim_free;
    s_anim_free = n->next_free;
    return &n->anim;
}

static void anim_pool_free(AnimationState* a) {
    AnimNode* n = (AnimNode*)a;   /* anim is the first union member */
    n->next_free = s_anim_free;
    s_anim_free  = n;
}

static void free_anim_state(void* p) {
    anim_pool_free((AnimationState*)p);
}

static AnimationState* get_animation_state(EntityRender* render, const char* entity_id, const char* item_id) {
    assert(render && entity_id && item_id);

    char key[256];
    snprintf(key, sizeof(key), "%s_%s", entity_id, item_id);

    AnimationState* anim = (AnimationState*)hash_table_get(&render->animations, key);
    if (anim) {
        anim->last_access_time = GetTime();
        return anim;
    }

    anim = anim_pool_alloc();
    *anim = (AnimationState){
        .last_direction_enum  = -1,
        .last_mode_enum       = -1,
        .time_acc             = 0.0f,
        .last_facing_direction = DIRECTION_DOWN,
        .last_access_time     = GetTime(),
    };
    hash_table_put(&render->animations, key, anim);
    return anim;
}

static bool anim_is_stale(const char* key, void* value, void* user_data) {
    const AnimationState* anim = value;
    double now = *(const double*)user_data;
    return (now - anim->last_access_time) > ANIM_IDLE_EVICT_SECONDS;
}

static bool anim_key_has_prefix(const char* key, void* value, void* user_data) {
    const char* prefix = user_data;
    return 0 == strncmp(key, prefix, strlen(prefix));
}

static int compare_layer_priority(const void* a, const void* b) {
    LayerRenderInfo* info_a = (LayerRenderInfo*)a;
    LayerRenderInfo* info_b = (LayerRenderInfo*)b;
    return info_a->priority - info_b->priority;
}

/**
 * @brief Determine the direction string and frame count for the current animation state.
 *
 * Frame counts are resolved exclusively from AtlasSpriteSheetData metadata.
 * The render path picks the first available animation in a small fallback
 * chain.
 */
static int pick_available_direction(
    const AtlasSpriteSheetData* atlas,
    const char* requested_dir,
    const char* secondary_dir,
    const char** out_dir_string
) {
    const char* candidates[5] = {
        requested_dir,
        secondary_dir,
        "down_idle",
        "none_idle",
        "default_idle"
    };

    for (int i = 0; i < 5; i++) {
        const char* dir_str = candidates[i];
        if (!dir_str || dir_str[0] == '\0') {
            continue;
        }
        const DirectionFrameData* dfd = atlas_get_direction_frames(atlas, dir_str);
        if (dfd && dfd->count > 0) {
            *out_dir_string = dir_str;
            return dfd->count;
        }
    }

    *out_dir_string = requested_dir && requested_dir[0] != '\0' ? requested_dir : "down_idle";
    return 0;
}

static int get_frame_count_and_direction(
    const AtlasSpriteSheetData* atlas,
    Direction dir,
    ObjectLayerMode mode,
    const char** out_dir_string
) {
    // Build the direction string based on direction + mode
    const char* dir_str = "down_idle";
    const char* fallback_dir = NULL;

    if (mode == MODE_WALKING) {
        switch (dir) {
            case DIRECTION_UP:         dir_str = "up_walking"; fallback_dir = "up_idle"; break;
            case DIRECTION_DOWN:       dir_str = "down_walking"; fallback_dir = "down_idle"; break;
            case DIRECTION_LEFT:       dir_str = "left_walking"; fallback_dir = "left_idle"; break;
            case DIRECTION_RIGHT:      dir_str = "right_walking"; fallback_dir = "right_idle"; break;
            case DIRECTION_UP_RIGHT:   dir_str = "up_right_walking"; fallback_dir = "up_right_idle"; break;
            case DIRECTION_UP_LEFT:    dir_str = "up_left_walking"; fallback_dir = "up_left_idle"; break;
            case DIRECTION_DOWN_RIGHT: dir_str = "down_right_walking"; fallback_dir = "down_right_idle"; break;
            case DIRECTION_DOWN_LEFT:  dir_str = "down_left_walking"; fallback_dir = "down_left_idle"; break;
            default:                   dir_str = "down_walking"; fallback_dir = "down_idle"; break;
        }
    } else {
        switch (dir) {
            case DIRECTION_UP:         dir_str = "up_idle"; break;
            case DIRECTION_DOWN:       dir_str = "down_idle"; break;
            case DIRECTION_LEFT:       dir_str = "left_idle"; break;
            case DIRECTION_RIGHT:      dir_str = "right_idle"; break;
            case DIRECTION_UP_RIGHT:   dir_str = "up_right_idle"; break;
            case DIRECTION_UP_LEFT:    dir_str = "up_left_idle"; break;
            case DIRECTION_DOWN_RIGHT: dir_str = "down_right_idle"; break;
            case DIRECTION_DOWN_LEFT:  dir_str = "down_left_idle"; break;
            case DIRECTION_NONE:       dir_str = "down_idle"; break;
            default:                   dir_str = "down_idle"; break;
        }
    }

    if (!atlas) {
        *out_dir_string = dir_str;
        return 0;
    }

    return pick_available_direction(atlas, dir_str, fallback_dir, out_dir_string);
}

static void draw_dev_ui_box(Rectangle dest_rec, const char* entity_type) {
    Color color = RED;
    if (strcmp(entity_type, "self") == 0) color = BLUE;
    else if (strcmp(entity_type, "other") == 0) color = ORANGE;
    else if (strcmp(entity_type, "bot") == 0) color = GREEN;

    DrawRectangleLinesEx(dest_rec, 1.0f, color);
    DrawText(entity_type, (int)dest_rec.x, (int)dest_rec.y - 10, 10, color);
}

// ============================================================================
// Public API - Lifecycle Management
// ============================================================================

EntityRender* create_entity_render(ObjectLayersManager* object_layers_manager) {
    EntityRender* render = (EntityRender*)malloc(sizeof(EntityRender));
    if (!render) return NULL;

    render->obj_layers_mgr = object_layers_manager;
    hash_table_init(&render->animations, ANIM_TABLE_INITIAL_CAPACITY, free_anim_state, "entity_animations");

    return render;
}

void destroy_entity_render(EntityRender* render) {
    if (!render) return;
    hash_table_destroy(&render->animations);
    free(render);
}

void entity_render_gc(EntityRender* render) {
    assert(render);
    double now = GetTime();
    hash_table_remove_if(&render->animations, anim_is_stale, &now);
}

void entity_render_forget_entity(EntityRender* render, const char* entity_id) {
    assert(render && entity_id);
    char prefix[256];
    snprintf(prefix, sizeof(prefix), "%s_", entity_id);
    hash_table_remove_if(&render->animations, anim_key_has_prefix, prefix);
}

// ============================================================================
// Public API - Rendering
// ============================================================================

void draw_entity_layers(
    EntityRender* render,
    const char* entity_id,
    float pos_x,
    float pos_y,
    float width,
    float height,
    Direction direction,
    ObjectLayerMode mode,
    ObjectLayerState** layers_state,
    int layers_count,
    const char* entity_type,
    bool dev_ui,
    float cell_size,
    Color fallback_color
) {
    assert(render && entity_id);

    if (cell_size <= 0.0f) cell_size = 12.0f;

    float scaled_pos_x = pos_x * cell_size;
    float scaled_pos_y = pos_y * cell_size;
    float scaled_dims_w = width * cell_size;
    float scaled_dims_h = height * cell_size;

    Rectangle dest_rec = {
        scaled_pos_x,
        scaled_pos_y,
        scaled_dims_w,
        scaled_dims_h
    };

    // Draw dev UI debug box if enabled
    if (dev_ui && entity_type) {
        draw_dev_ui_box(dest_rec, entity_type);
    }

    if (!layers_state || layers_count <= 0) {
        return;
    }

    // ========================================================================
    // Layer Collection and Sorting
    // ========================================================================

    LayerRenderInfo layers_to_render[MAX_LAYERS_PER_ENTITY];
    int render_count = 0;
    bool has_associated_item_id = false;

    for (int i = 0; i < layers_count && render_count < MAX_LAYERS_PER_ENTITY; i++) {
        ObjectLayerState* state = layers_state[i];
        if (!state || !state->active || state->item_id[0] == '\0') {
            continue;
        }

        has_associated_item_id = true;

        // Fetch object layer metadata (for item type, ledger, render CIDs)
        ObjectLayer* layer = lookup_cached_layer(state->item_id);

        // Fetch atlas sprite sheet data (for frame metadata + atlas texture reference)
        AtlasSpriteSheetData* atlas = get_or_fetch_atlas_data(state->item_id);

        // If atlas metadata not yet cached, pump the async REST fetch state
        // machine each frame. On first call: schedules
        // GET /api/atlas-sprite-sheet/metadata/:itemKey. On subsequent calls:
        // polls the in-flight request and caches the JSON when it arrives,
        // then automatically kicks off the PNG blob fetch.
        if (!atlas) {
            get_atlas_texture(state->item_id);
        }

        if (!layer && !atlas) {
            // Neither data source available yet — still loading
            continue;
        }

        // Add to render list (atlas may be NULL if only ObjectLayer is available)
        layers_to_render[render_count].state = state;
        layers_to_render[render_count].layer = layer;
        layers_to_render[render_count].atlas = atlas;
        layers_to_render[render_count].priority = layer
            ? layer_z_priority(layer->data.item.type, direction == DIRECTION_UP)
            : 50; // Default priority if no ObjectLayer metadata yet
        render_count++;
    }

    if (!has_associated_item_id) {
        DrawRectangleRec(dest_rec, fallback_color);
        return;
    }

    if (render_count == 0) {
        // Entity has active item IDs but no atlas data loaded yet —
        // draw the fallback color so the entity is visible while textures load.
        DrawRectangleRec(dest_rec, fallback_color);
        return;
    }

    // Sort layers by priority (lower z-order first)
    qsort(layers_to_render, render_count, sizeof(LayerRenderInfo), compare_layer_priority);

    // ========================================================================
    // Texture Availability Check & Animation Update
    // ========================================================================

    float frame_dt = GetFrameTime();
    // Per-layer rendering data
    Texture2D layer_textures[MAX_LAYERS_PER_ENTITY] = { 0 };
    Rectangle layer_source_rects[MAX_LAYERS_PER_ENTITY] = { 0 };

    for (int i = 0; i < render_count; i++) {
        ObjectLayerState* state = layers_to_render[i].state;
        AtlasSpriteSheetData* atlas = layers_to_render[i].atlas;

        // Get or create animation state
        AnimationState* anim = get_animation_state(
            render,
            entity_id,
            state->item_id
        );
        if (!anim) {
            continue;
        }

        // Update last_facing_direction
        if (direction != DIRECTION_NONE) {
            anim->last_facing_direction = direction;
        }

        Direction render_direction = direction;
        ObjectLayerMode render_mode = mode;

        // When idle with no direction, use last known facing direction
        if (direction == DIRECTION_NONE && mode == MODE_IDLE) {
            if (anim->last_facing_direction != DIRECTION_DOWN) {
                render_direction = anim->last_facing_direction;
            }
        }

        // Determine frame duration from atlas metadata fetched from the engine.
        int frame_duration_ms = DEFAULT_FRAME_DURATION_MS;

        if (atlas) {
            frame_duration_ms = atlas->frame_duration;
            if (frame_duration_ms <= 0) frame_duration_ms = DEFAULT_FRAME_DURATION_MS;
        }

        // Frame Selection — resolved exclusively from atlas metadata
        const char* dir_string = NULL;
        int num_frames = get_frame_count_and_direction(
            atlas,
            render_direction,
            render_mode,
            &dir_string
        );

        if (num_frames <= 0) {
            // No frames for this state — skip this layer
            continue;
        }

        // State Change Detection — two int compares, zero allocations
        if ((int)render_direction != anim->last_direction_enum ||
            (int)render_mode     != anim->last_mode_enum) {
            anim->last_direction_enum = (int)render_direction;
            anim->last_mode_enum      = (int)render_mode;
            anim->frame_index         = 0;
            anim->time_acc            = 0.0f;
        }

        // Animation Advancement — delta accumulator; catches skipped frames
        // under load (e.g. 300 ms frame at 125 ms/frame advances by 2).
        float frame_sec = frame_duration_ms / 1000.0f;
        anim->time_acc += frame_dt;
        while (anim->time_acc >= frame_sec) {
            anim->time_acc -= frame_sec;
            anim->frame_index = (anim->frame_index + 1) % num_frames;
        }
        if (anim->frame_index >= num_frames) anim->frame_index = 0;

        // ====================================================================
        // Atlas-based texture loading (single texture per item)
        // ====================================================================

        if (atlas && atlas->item_key[0] != '\0') {
            // Get or poll the atlas texture (async loading)
            Texture2D atlas_texture = get_atlas_texture(atlas->item_key);

            if (atlas_texture.id > 0) {
                // Atlas texture is ready — look up the source rectangle
                // from the FrameMetadata for the current direction and frame
                const DirectionFrameData* dfd = atlas_get_direction_frames(atlas, dir_string);

                if (dfd && anim->frame_index < dfd->count) {
                    const FrameMetadata* fm = &dfd->frames[anim->frame_index];

                    layer_textures[i] = atlas_texture;
                    layer_source_rects[i] = (Rectangle){
                        (float)fm->x,
                        (float)fm->y,
                        (float)fm->width,
                        (float)fm->height
                    };

                    if (!anim->textures_ready) {
                        anim->textures_ready = true;
                        anim->failed_texture_attempts = 0;
                    }
                } else {
                    // Frame metadata missing for this direction/frame
                    anim->failed_texture_attempts++;
                }
            } else {
                // Atlas texture not yet loaded (async in progress)
                anim->failed_texture_attempts++;
            }
        } else {
            // No atlas available — cannot render this layer
            anim->failed_texture_attempts++;
        }
    }

    // ========================================================================
    // Final Rendering
    // ========================================================================

    for (int i = 0; i < render_count; i++) {
        if (layer_textures[i].id > 0) {
            DrawTexturePro(
                layer_textures[i],
                layer_source_rects[i],
                dest_rec,
                (Vector2){0.0f, 0.0f},
                0.0f,
                WHITE
            );
        }
    }
}

void draw_entity_shadow(float pos_x, float pos_y, float width, float height, float cell_size) {
    if (cell_size <= 0.0f) cell_size = 12.0f;

    float center_x = (pos_x + width * 0.5f) * cell_size;
    float feet_y = (pos_y + height) * cell_size;
    float rx = width * cell_size * ENTITY_SHADOW_RADIUS_RATIO;
    float ry = rx * ENTITY_SHADOW_SQUASH;

    DrawEllipse((int)center_x, (int)feet_y, rx, ry, ENTITY_SHADOW_COLOR);
}
