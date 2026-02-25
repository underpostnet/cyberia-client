#include "entity_render.h"
#include "game_state.h"
#include "texture_manager.h"
#include "object_layers_management.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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

static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

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
    if (!render || !entity_id || !item_id) return NULL;
    
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

static int get_priority_for_type(const char* type) {
    if (!type) return 0;
    if (strcmp(type, "skin") == 0 || strcmp(type, "body") == 0) return 10;
    if (strcmp(type, "eyes") == 0) return 11;
    if (strcmp(type, "hair") == 0) return 12;
    if (strcmp(type, "clothes") == 0 || strcmp(type, "armor") == 0) return 20;
    if (strcmp(type, "hat") == 0 || strcmp(type, "helmet") == 0) return 30;
    if (strcmp(type, "weapon") == 0) return 40;
    if (strcmp(type, "shield") == 0) return 41;
    return 50;
}

static int compare_layer_priority(const void* a, const void* b) {
    LayerRenderInfo* info_a = (LayerRenderInfo*)a;
    LayerRenderInfo* info_b = (LayerRenderInfo*)b;
    return info_a->priority - info_b->priority;
}

/**
 * @brief Determine the direction string and frame count for the current animation state.
 *
 * When atlas data is available, uses the DirectionFrameData counts from the atlas
 * metadata. Otherwise falls back to the ObjectLayer RenderFrames counts.
 */
static int get_frame_count_and_direction(
    RenderFrames* frames,
    const AtlasSpriteSheetData* atlas,
    Direction dir,
    ObjectLayerMode mode,
    bool is_stateless,
    const char** out_dir_string
) {
    if (is_stateless) {
        *out_dir_string = "default_idle";
        if (atlas) {
            const DirectionFrameData* dfd = atlas_get_direction_frames(atlas, "default_idle");
            return dfd ? dfd->count : 0;
        }
        return frames ? frames->default_idle_count : 0;
    }

    // Build the direction string based on direction + mode
    const char* dir_str = "down_idle";
    int count = 0;

    if (mode == MODE_WALKING) {
        switch (dir) {
            case DIRECTION_UP:         dir_str = "up_walking"; break;
            case DIRECTION_DOWN:       dir_str = "down_walking"; break;
            case DIRECTION_LEFT:       dir_str = "left_walking"; break;
            case DIRECTION_RIGHT:      dir_str = "right_walking"; break;
            case DIRECTION_UP_RIGHT:   dir_str = "up_right_walking"; break;
            case DIRECTION_UP_LEFT:    dir_str = "up_left_walking"; break;
            case DIRECTION_DOWN_RIGHT: dir_str = "down_right_walking"; break;
            case DIRECTION_DOWN_LEFT:  dir_str = "down_left_walking"; break;
            default:                   dir_str = "down_walking"; break;
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

    *out_dir_string = dir_str;

    // Get frame count from atlas if available, else from RenderFrames
    if (atlas) {
        const DirectionFrameData* dfd = atlas_get_direction_frames(atlas, dir_str);
        count = dfd ? dfd->count : 0;
    } else if (frames) {
        // Legacy fallback using RenderFrames counts
        if (mode == MODE_WALKING) {
            switch (dir) {
                case DIRECTION_UP:         count = frames->up_walking_count; break;
                case DIRECTION_DOWN:       count = frames->down_walking_count; break;
                case DIRECTION_LEFT:       count = frames->left_walking_count; break;
                case DIRECTION_RIGHT:      count = frames->right_walking_count; break;
                case DIRECTION_UP_RIGHT:   count = frames->up_right_walking_count; break;
                case DIRECTION_UP_LEFT:    count = frames->up_left_walking_count; break;
                case DIRECTION_DOWN_RIGHT: count = frames->down_right_walking_count; break;
                case DIRECTION_DOWN_LEFT:  count = frames->down_left_walking_count; break;
                default:                   count = frames->down_walking_count; break;
            }
        } else {
            switch (dir) {
                case DIRECTION_UP:         count = frames->up_idle_count; break;
                case DIRECTION_DOWN:       count = frames->down_idle_count; break;
                case DIRECTION_LEFT:       count = frames->left_idle_count; break;
                case DIRECTION_RIGHT:      count = frames->right_idle_count; break;
                case DIRECTION_UP_RIGHT:   count = frames->up_right_idle_count; break;
                case DIRECTION_UP_LEFT:    count = frames->up_left_idle_count; break;
                case DIRECTION_DOWN_RIGHT: count = frames->down_right_idle_count; break;
                case DIRECTION_DOWN_LEFT:  count = frames->down_left_idle_count; break;
                case DIRECTION_NONE:       count = frames->down_idle_count; break;
                default:                   count = frames->down_idle_count; break;
            }
        }
    }

    return count;
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
    float cell_size
) {
    if (!render || !entity_id) return;

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

        // Fetch object layer metadata (for item type, frame_duration, is_stateless)
        ObjectLayer* layer = get_or_fetch_object_layer(
            render->obj_layers_mgr,
            state->item_id
        );

        // Fetch atlas sprite sheet data (for frame metadata + atlas texture reference)
        AtlasSpriteSheetData* atlas = get_or_fetch_atlas_data(
            render->obj_layers_mgr,
            state->item_id
        );

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
            ? get_priority_for_type(layer->data.item.type)
            : 50; // Default priority if no ObjectLayer metadata yet
        render_count++;
    }

    if (any_data_missing && render_count == 0) {
        DrawRectangleRec(dest_rec, (Color){ 100, 100, 100, 200 });
        return;
    }

    // Sort layers by priority (lower z-order first)
    qsort(
        layers_to_render,
        render_count,
        sizeof(LayerRenderInfo),
        compare_layer_priority
    );

    // ========================================================================
    // Texture Availability Check & Animation Update
    // ========================================================================

    double now = GetTime();
    bool all_textures_ready = true;

    // Per-layer rendering data
    Texture2D layer_textures[MAX_LAYERS_PER_ENTITY];
    Rectangle layer_source_rects[MAX_LAYERS_PER_ENTITY];
    memset(layer_textures, 0, sizeof(layer_textures));
    memset(layer_source_rects, 0, sizeof(layer_source_rects));

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

        // Determine is_stateless and frame_duration from ObjectLayer if available
        bool is_stateless = false;
        int frame_duration_ms = DEFAULT_FRAME_DURATION_MS;

        if (layer) {
            is_stateless = layer->data.render.is_stateless;
            frame_duration_ms = layer->data.render.frame_duration;
            if (frame_duration_ms <= 0) frame_duration_ms = DEFAULT_FRAME_DURATION_MS;
        } else if (atlas) {
            // Infer is_stateless: if default_idle has frames, treat as stateless
            is_stateless = (atlas->default_idle.count > 0);
        }

        // Frame Selection
        const char* dir_string = NULL;
        RenderFrames* render_frames = layer ? &layer->data.render.frames : NULL;
        int num_frames = get_frame_count_and_direction(
            render_frames,
            atlas,
            render_direction,
            render_mode,
            is_stateless,
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

        if (atlas && atlas->file_id[0] != '\0') {
            // Get or poll the atlas texture (async loading)
            Texture2D atlas_texture = get_atlas_texture(
                render->obj_layers_mgr,
                atlas->file_id
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
        DrawRectangleRec(dest_rec, (Color){ 100, 100, 100, 200 });
    }
}