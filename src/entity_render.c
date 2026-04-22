#include "entity_render.h"
#include "texture_manager.h"
#include "object_layers_management.h"
#include "layer_z_order.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "helper.h"

#define HASH_TABLE_SIZE 1024
#define MAX_LAYERS_PER_ENTITY 20
#define DEFAULT_FRAME_DURATION_MS 100

// --- Data Structures ---

typedef struct {
    char* last_state_string;
    double last_update_time;
    int frame_index;
    Direction last_facing_direction;
    bool textures_ready;
    int failed_texture_attempts;
} AnimationState;

typedef struct AnimationEntry {
    char* key;
    AnimationState state;
    struct AnimationEntry* next;
} AnimationEntry;

struct EntityRender {
    ObjectLayersManager* obj_layers_mgr;
    TextureManager* texture_manager;
    AnimationEntry* anim_buckets[HASH_TABLE_SIZE];
};

typedef struct {
    ObjectLayerState* state;
    ObjectLayer* layer;
    AtlasSpriteSheetData* atlas;
    int priority;
} LayerRenderInfo;

// --- Helper Functions ---

static AnimationEntry* create_animation_entry(const char* key) {
    AnimationEntry* entry = (AnimationEntry*)malloc(sizeof(AnimationEntry));
    if (entry) {
        entry->key = strdup(key);
        entry->state.last_state_string = NULL;
        entry->state.last_update_time = 0;
        entry->state.frame_index = 0;
        entry->state.last_facing_direction = DIRECTION_DOWN;
        entry->state.textures_ready = false;
        entry->state.failed_texture_attempts = 0;
        entry->next = NULL;
    }
    return entry;
}

static AnimationState* get_animation_state(EntityRender* render, const char* entity_id, const char* item_id) {
    assert(render && entity_id && item_id);

    char key[256];
    snprintf(key, sizeof(key), "%s_%s", entity_id, item_id);

    unsigned long index = hash_string(key) % HASH_TABLE_SIZE;
    AnimationEntry* entry = render->anim_buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return &entry->state;
        }
        entry = entry->next;
    }

    // Create new
    entry = create_animation_entry(key);
    if (entry) {
        entry->next = render->anim_buckets[index];
        render->anim_buckets[index] = entry;
        return &entry->state;
    }

    return NULL;
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

EntityRender* create_entity_render(
    ObjectLayersManager* object_layers_manager,
    TextureManager* texture_manager
) {
    EntityRender* render = (EntityRender*)malloc(sizeof(EntityRender));
    if (!render) return NULL;

    render->obj_layers_mgr = object_layers_manager;
    render->texture_manager = texture_manager;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        render->anim_buckets[i] = NULL;
    }

    return render;
}

void destroy_entity_render(EntityRender* render) {
    if (!render) return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        AnimationEntry* entry = render->anim_buckets[i];
        while (entry) {
            AnimationEntry* next = entry->next;
            if (entry->key) free(entry->key);
            if (entry->state.last_state_string) free(entry->state.last_state_string);
            free(entry);
            entry = next;
        }
        render->anim_buckets[i] = NULL;
    }

    free(render);
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
    bool any_data_missing = false;

    for (int i = 0; i < layers_count && render_count < MAX_LAYERS_PER_ENTITY; i++) {
        ObjectLayerState* state = layers_state[i];
        if (!state || !state->active || state->item_id[0] == '\0') {
            continue;
        }

        // Fetch object layer metadata (for item type, ledger, render CIDs)
        ObjectLayer* layer = get_or_fetch_object_layer(
            render->obj_layers_mgr,
            state->item_id
        );

        // Fetch atlas sprite sheet data (for frame metadata + atlas texture reference)
        AtlasSpriteSheetData* atlas = get_or_fetch_atlas_data(
            render->obj_layers_mgr,
            state->item_id
        );

        // If atlas metadata not yet cached, pump the async REST fetch state
        // machine each frame. On first call: schedules
        // GET /api/atlas-sprite-sheet/metadata/:itemKey. On subsequent calls:
        // polls the in-flight request and caches the JSON when it arrives,
        // then automatically kicks off the PNG blob fetch.
        if (!atlas) {
            get_atlas_texture(render->obj_layers_mgr, state->item_id);
        }

        if (!layer && !atlas) {
            // Neither data source available yet — still loading
            any_data_missing = true;
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

    if (any_data_missing && render_count == 0) {
        DrawRectangleRec(dest_rec, fallback_color);
        return;
    }

    // Sort layers by priority (lower z-order first)
    qsort(layers_to_render, render_count, sizeof(LayerRenderInfo), compare_layer_priority);

    // ========================================================================
    // Texture Availability Check & Animation Update
    // ========================================================================

    double now = GetTime();
    bool all_textures_ready = true;

    // Per-layer rendering data
    Texture2D layer_textures[MAX_LAYERS_PER_ENTITY] = { 0 };
    Rectangle layer_source_rects[MAX_LAYERS_PER_ENTITY] = { 0 };

    for (int i = 0; i < render_count; i++) {
        ObjectLayer* layer = layers_to_render[i].layer;
        ObjectLayerState* state = layers_to_render[i].state;
        AtlasSpriteSheetData* atlas = layers_to_render[i].atlas;

        // Get or create animation state
        AnimationState* anim = get_animation_state(
            render,
            entity_id,
            state->item_id
        );
        if (!anim) {
            all_textures_ready = false;
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

        // Determine frame duration.
        // This no longer lives on ObjectLayer.data.render (which now holds
        // IPFS CIDs). It is parsed from the populated
        // objectLayerRenderFramesId reference and stored directly on the
        // ObjectLayer struct.
        int frame_duration_ms = DEFAULT_FRAME_DURATION_MS;

        if (layer) {
            frame_duration_ms = layer->frame_duration;
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

        // State Change Detection
        if (!anim->last_state_string ||
            strcmp(anim->last_state_string, dir_string) != 0) {

            if (anim->last_state_string) free(anim->last_state_string);
            anim->last_state_string = strdup(dir_string);
            anim->frame_index = 0;
            anim->last_update_time = now;
        }

        // Animation Advancement
        double elapsed_ms = (now - anim->last_update_time) * 1000.0;
        if (elapsed_ms >= frame_duration_ms) {
            anim->frame_index = (anim->frame_index + 1) % num_frames;
            anim->last_update_time = now;
        }
        if (anim->frame_index >= num_frames) anim->frame_index = 0;

        // ====================================================================
        // Atlas-based texture loading (single texture per item)
        // ====================================================================

        if (atlas && atlas->item_key[0] != '\0') {
            // Get or poll the atlas texture (async loading)
            Texture2D atlas_texture = get_atlas_texture(
                render->obj_layers_mgr,
                atlas->item_key
            );

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
                    all_textures_ready = false;
                    anim->failed_texture_attempts++;
                }
            } else {
                // Atlas texture not yet loaded (async in progress)
                all_textures_ready = false;
                anim->failed_texture_attempts++;
            }
        } else {
            // No atlas available — cannot render this layer
            all_textures_ready = false;
            anim->failed_texture_attempts++;
        }
    }

    // ========================================================================
    // Final Rendering
    // ========================================================================

    if (all_textures_ready) {
        // All layers are ready — draw them all with atlas source rects
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
    } else {
        // Something is still loading — render fallback placeholder
        DrawRectangleRec(dest_rec, fallback_color);
    }
}