#include "entity_render.h"
#include "game_state.h"
#include "texture_manager.h"
#include "object_layers_management.h"
#include "direction_converter.h"
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
    // Basic priority list based on common RPG layers
    if (strcmp(type, "skin") == 0 || strcmp(type, "body") == 0) return 10;
    if (strcmp(type, "eyes") == 0) return 11;
    if (strcmp(type, "hair") == 0) return 12;
    if (strcmp(type, "clothes") == 0 || strcmp(type, "armor") == 0) return 20;
    if (strcmp(type, "hat") == 0 || strcmp(type, "helmet") == 0) return 30;
    if (strcmp(type, "weapon") == 0) return 40;
    if (strcmp(type, "shield") == 0) return 41;
    return 50; // Default
}

static int compare_layer_priority(const void* a, const void* b) {
    LayerRenderInfo* info_a = (LayerRenderInfo*)a;
    LayerRenderInfo* info_b = (LayerRenderInfo*)b;
    return info_a->priority - info_b->priority;
}



static int get_frame_count_and_direction(
    RenderFrames* frames,
    Direction dir,
    ObjectLayerMode mode,
    bool is_stateless,
    const char** out_dir_string
) {
    if (is_stateless) {
        *out_dir_string = "default_idle";
        return frames->default_idle_count;
    }

    // Determine state string and count based on dir and mode
    if (mode == MODE_WALKING) {
        switch (dir) {
            case DIRECTION_UP: *out_dir_string = "up_walking"; return frames->up_walking_count;
            case DIRECTION_DOWN: *out_dir_string = "down_walking"; return frames->down_walking_count;
            case DIRECTION_LEFT: *out_dir_string = "left_walking"; return frames->left_walking_count;
            case DIRECTION_RIGHT: *out_dir_string = "right_walking"; return frames->right_walking_count;
            case DIRECTION_UP_RIGHT: *out_dir_string = "up_right_walking"; return frames->up_right_walking_count;
            case DIRECTION_UP_LEFT: *out_dir_string = "up_left_walking"; return frames->up_left_walking_count;
            case DIRECTION_DOWN_RIGHT: *out_dir_string = "down_right_walking"; return frames->down_right_walking_count;
            case DIRECTION_DOWN_LEFT: *out_dir_string = "down_left_walking"; return frames->down_left_walking_count;
            default: *out_dir_string = "down_walking"; return frames->down_walking_count;
        }
    } else {
        // Idle
        switch (dir) {
            case DIRECTION_UP: *out_dir_string = "up_idle"; return frames->up_idle_count;
            case DIRECTION_DOWN: *out_dir_string = "down_idle"; return frames->down_idle_count;
            case DIRECTION_LEFT: *out_dir_string = "left_idle"; return frames->left_idle_count;
            case DIRECTION_RIGHT: *out_dir_string = "right_idle"; return frames->right_idle_count;
            case DIRECTION_UP_RIGHT: *out_dir_string = "up_right_idle"; return frames->up_right_idle_count;
            case DIRECTION_UP_LEFT: *out_dir_string = "up_left_idle"; return frames->up_left_idle_count;
            case DIRECTION_DOWN_RIGHT: *out_dir_string = "down_right_idle"; return frames->down_right_idle_count;
            case DIRECTION_DOWN_LEFT: *out_dir_string = "down_left_idle"; return frames->down_left_idle_count;
            case DIRECTION_NONE: *out_dir_string = "down_idle"; return frames->down_idle_count; // Fallback
            default: *out_dir_string = "down_idle"; return frames->down_idle_count;
        }
    }
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

    // Initialize hash table
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        render->anim_buckets[i] = NULL;
    }

    return render;
}

void destroy_entity_render(EntityRender* render) {
    if (!render) return;

    // Free all animation state entries
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

    // Calculate scaled positions and dimensions
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

    // If no layers, draw a fallback representation so entity is still visible
    if (!layers_state || layers_count <= 0) {
        return;
    }

    // ========================================================================
    // Layer Collection and Sorting
    // ========================================================================

    LayerRenderInfo layers_to_render[MAX_LAYERS_PER_ENTITY];
    int render_count = 0;
    bool any_layer_data_missing = false;

    for (int i = 0; i < layers_count && render_count < MAX_LAYERS_PER_ENTITY; i++) {
        ObjectLayerState* state = layers_state[i];
        if (!state || !state->active || state->item_id[0] == '\0') {
            continue;
        }

        // Fetch object layer data
        ObjectLayer* layer = get_or_fetch_object_layer(
            render->obj_layers_mgr,
            state->item_id
        );
        if (!layer) {
            // Layer data is being fetched
            any_layer_data_missing = true;
            // We continue to ensure all needed layers are requested
            continue;
        }

        // Add to render list
        layers_to_render[render_count].state = state;
        layers_to_render[render_count].layer = layer;
        layers_to_render[render_count].priority = get_priority_for_type(
            layer->data.item.type
        );
        render_count++;
    }

    // If any layer data is missing, we can't render the full entity properly.
    if (any_layer_data_missing) {
        // Draw fallback placeholder while loading layer data
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
    
    // We need to check if ALL textures for the current frame are available.
    // We also update animation state here to ensure it progresses.
    
    double now = GetTime();
    bool all_textures_ready = true;
    Texture2D layer_textures[MAX_LAYERS_PER_ENTITY];
    memset(layer_textures, 0, sizeof(layer_textures));

    for (int i = 0; i < render_count; i++) {
        ObjectLayer* layer = layers_to_render[i].layer;
        ObjectLayerState* state = layers_to_render[i].state;

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

        // Frame Selection
        const char* dir_string = NULL;
        int num_frames = get_frame_count_and_direction(
            &layer->data.render.frames,
            render_direction,
            render_mode,
            layer->data.render.is_stateless,
            &dir_string
        );

        if (num_frames <= 0) {
            // No frames for this state? Skip rendering this layer.
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
        int frame_duration_ms = layer->data.render.frame_duration;
        if (frame_duration_ms <= 0) frame_duration_ms = DEFAULT_FRAME_DURATION_MS;

        double elapsed_ms = (now - anim->last_update_time) * 1000.0;
        if (elapsed_ms >= frame_duration_ms) {
            anim->frame_index = (anim->frame_index + 1) % num_frames;
            anim->last_update_time = now;
        }
        if (anim->frame_index >= num_frames) anim->frame_index = 0;

        // Texture Loading Check
        const char* direction_code = get_code_from_direction(dir_string);
        if (!direction_code) continue;

        char uri[512];
        build_object_layer_uri(
            uri,
            sizeof(uri),
            layer->data.item.type,
            layer->data.item.id,
            direction_code,
            anim->frame_index
        );

        // Check/Load texture
        // This will trigger async fetch if not present
        Texture2D texture = load_texture_from_url(render->texture_manager, uri);

        if (texture.id > 0) {
            layer_textures[i] = texture;
            if (!anim->textures_ready) {
                anim->textures_ready = true;
                anim->failed_texture_attempts = 0;
            }
        } else {
            all_textures_ready = false;
            anim->failed_texture_attempts++;
        }
    }

    // ========================================================================
    // Final Rendering
    // ========================================================================

    if (all_textures_ready) {
        // All layers are ready, draw them all
        for (int i = 0; i < render_count; i++) {
            if (layer_textures[i].id > 0) {
                Rectangle source_rec = {
                    0.0f, 0.0f,
                    (float)layer_textures[i].width,
                    (float)layer_textures[i].height
                };
                DrawTexturePro(
                    layer_textures[i],
                    source_rec,
                    dest_rec,
                    (Vector2){0.0f, 0.0f},
                    0.0f,
                    WHITE
                );
            }
        }
    } else {
        // Something is missing, render fallback (wait for load)
        DrawRectangleRec(dest_rec, (Color){ 100, 100, 100, 200 });
    }
}