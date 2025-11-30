#include "entity_render.h"
#include "direction_converter.h"
#include "config.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define HASH_TABLE_SIZE 256
#define MAX_LAYERS_PER_ENTITY 64

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @struct AnimationState
 * Tracks the animation state for a specific entity-item combination.
 * Manages frame progression, timing, and direction memory.
 */
typedef struct {
    int frame_index;                    // Current animation frame
    double last_update_time;            // Time of last frame update (seconds)
    Direction last_facing_direction;    // Last non-NONE direction faced
    Direction last_render_direction;    // Direction used in last render
    ObjectLayerMode last_render_mode;   // Mode used in last render
    char* last_state_string;            // State string for change detection
} AnimationState;

/**
 * @struct AnimationEntry
 * Hash table entry for animation state lookup.
 */
typedef struct AnimationEntry {
    char* key;                      // "entity_id:item_id"
    AnimationState state;
    struct AnimationEntry* next;    // For chaining in hash table
} AnimationEntry;

/**
 * @struct LayerRenderInfo
 * Temporary structure for sorting layers by render priority.
 */
typedef struct {
    ObjectLayerState* state;
    int priority;
    ObjectLayer* layer;
} LayerRenderInfo;

/**
 * @struct EntityRender
 * Main rendering system for entities and their object layers.
 */
struct EntityRender {
    ObjectLayersManager* obj_layers_mgr;
    TextureManager* texture_manager;
    AnimationEntry* anim_buckets[HASH_TABLE_SIZE];  // Hash table for animation states
};

// ============================================================================
// Helper Functions - Hash Table Operations
// ============================================================================

/**
 * @brief Simple hash function for string keys.
 * Uses DJB2 algorithm for consistent hashing.
 */
static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief Creates a new animation entry with initialized state.
 */
static AnimationEntry* create_animation_entry(const char* key) {
    AnimationEntry* entry = (AnimationEntry*)malloc(sizeof(AnimationEntry));
    if (!entry) return NULL;

    entry->key = (char*)malloc(strlen(key) + 1);
    if (!entry->key) {
        free(entry);
        return NULL;
    }
    strcpy(entry->key, key);

    // Initialize animation state
    memset(&entry->state, 0, sizeof(AnimationState));
    entry->state.last_facing_direction = DIRECTION_DOWN;
    entry->state.last_render_direction = DIRECTION_DOWN;
    entry->state.last_render_mode = MODE_IDLE;
    entry->state.last_update_time = GetTime();
    entry->state.last_state_string = NULL;
    entry->state.frame_index = 0;

    entry->next = NULL;
    return entry;
}

/**
 * @brief Retrieves or creates animation state for an entity-item pair.
 * Uses hash table for O(1) average lookup.
 */
static AnimationState* get_animation_state(
    EntityRender* render,
    const char* entity_id,
    const char* item_id
) {
    if (!render || !entity_id || !item_id) return NULL;

    char key[512];
    snprintf(key, sizeof(key), "%s:%s", entity_id, item_id);

    unsigned long index = hash_string(key) % HASH_TABLE_SIZE;
    AnimationEntry* entry = render->anim_buckets[index];

    // Search for existing entry
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return &entry->state;
        }
        entry = entry->next;
    }

    // Create new entry
    AnimationEntry* new_entry = create_animation_entry(key);
    if (!new_entry) return NULL;

    new_entry->next = render->anim_buckets[index];
    render->anim_buckets[index] = new_entry;

    return &new_entry->state;
}

// ============================================================================
// Helper Functions - Rendering Logic
// ============================================================================

/**
 * @brief Determines render priority based on item type.
 * Lower values render first (background), higher values render last (foreground).
 */
static int get_priority_for_type(const char* type) {
    if (!type) return 99;

    if (strcmp(type, "floor") == 0) return 0;
    if (strcmp(type, "skin") == 0) return 1;
    if (strcmp(type, "weapon") == 0) return 2;
    if (strcmp(type, "skill") == 0) return 3;
    if (strcmp(type, "coin") == 0) return 4;

    return 99;  // Unknown types render on top
}

/**
 * @brief Comparison function for qsort.
 * Sorts layers by priority for correct z-order rendering.
 */
static int compare_layer_priority(const void* a, const void* b) {
    const LayerRenderInfo* info_a = (const LayerRenderInfo*)a;
    const LayerRenderInfo* info_b = (const LayerRenderInfo*)b;
    return info_a->priority - info_b->priority;
}

/**
 * @brief Gets the frame count for a specific animation state.
 * Returns the frame list and corresponding direction string with proper fallbacks.
 *
 * This implements the frame selection logic with the following priority:
 * 1. Specific direction + mode (e.g., "up_walking")
 * 2. Same direction idle (e.g., "up_idle")
 * 3. Default idle
 * 4. None idle
 */
static int get_frame_count_and_direction(
    RenderFrames* frames,
    Direction dir,
    ObjectLayerMode mode,
    bool is_stateless,
    const char** out_dir_string
) {
    int count = 0;
    const char* dir_str = "default_idle";

    // Handle stateless objects - they use fixed animation
    if (is_stateless) {
        if (frames->none_idle_count > 0) {
            count = frames->none_idle_count;
            dir_str = "none_idle";
        } else if (frames->default_idle_count > 0) {
            count = frames->default_idle_count;
            dir_str = "default_idle";
        }
        if (out_dir_string) *out_dir_string = dir_str;
        return count;
    }

    // Priority 1: Specific direction + mode
    if (mode == MODE_WALKING) {
        switch (dir) {
            case DIRECTION_UP:
                count = frames->up_walking_count;
                dir_str = "up_walking";
                break;
            case DIRECTION_DOWN:
                count = frames->down_walking_count;
                dir_str = "down_walking";
                break;
            case DIRECTION_LEFT:
                count = frames->left_walking_count;
                dir_str = "left_walking";
                break;
            case DIRECTION_RIGHT:
                count = frames->right_walking_count;
                dir_str = "right_walking";
                break;
            case DIRECTION_UP_LEFT:
                count = frames->up_left_walking_count;
                dir_str = "up_left_walking";
                break;
            case DIRECTION_UP_RIGHT:
                count = frames->up_right_walking_count;
                dir_str = "up_right_walking";
                break;
            case DIRECTION_DOWN_LEFT:
                count = frames->down_left_walking_count;
                dir_str = "down_left_walking";
                break;
            case DIRECTION_DOWN_RIGHT:
                count = frames->down_right_walking_count;
                dir_str = "down_right_walking";
                break;
            default:
                break;
        }
    } else {
        // IDLE mode
        switch (dir) {
            case DIRECTION_UP:
                count = frames->up_idle_count;
                dir_str = "up_idle";
                break;
            case DIRECTION_DOWN:
                count = frames->down_idle_count;
                dir_str = "down_idle";
                break;
            case DIRECTION_LEFT:
                count = frames->left_idle_count;
                dir_str = "left_idle";
                break;
            case DIRECTION_RIGHT:
                count = frames->right_idle_count;
                dir_str = "right_idle";
                break;
            case DIRECTION_UP_LEFT:
                count = frames->up_left_idle_count;
                dir_str = "up_left_idle";
                break;
            case DIRECTION_UP_RIGHT:
                count = frames->up_right_idle_count;
                dir_str = "up_right_idle";
                break;
            case DIRECTION_DOWN_LEFT:
                count = frames->down_left_idle_count;
                dir_str = "down_left_idle";
                break;
            case DIRECTION_DOWN_RIGHT:
                count = frames->down_right_idle_count;
                dir_str = "down_right_idle";
                break;
            case DIRECTION_NONE:
                count = frames->none_idle_count;
                dir_str = "none_idle";
                break;
            default:
                break;
        }
    }

    // Priority 2: Fallback to idle for same direction if no frames
    if (count == 0 && mode != MODE_IDLE) {
        // Try direction + idle
        switch (dir) {
            case DIRECTION_UP:
                count = frames->up_idle_count;
                dir_str = "up_idle";
                break;
            case DIRECTION_DOWN:
                count = frames->down_idle_count;
                dir_str = "down_idle";
                break;
            case DIRECTION_LEFT:
                count = frames->left_idle_count;
                dir_str = "left_idle";
                break;
            case DIRECTION_RIGHT:
                count = frames->right_idle_count;
                dir_str = "right_idle";
                break;
            case DIRECTION_UP_LEFT:
                count = frames->up_left_idle_count;
                dir_str = "up_left_idle";
                break;
            case DIRECTION_UP_RIGHT:
                count = frames->up_right_idle_count;
                dir_str = "up_right_idle";
                break;
            case DIRECTION_DOWN_LEFT:
                count = frames->down_left_idle_count;
                dir_str = "down_left_idle";
                break;
            case DIRECTION_DOWN_RIGHT:
                count = frames->down_right_idle_count;
                dir_str = "down_right_idle";
                break;
            default:
                break;
        }
    }

    // Priority 3: Fallback to default idle
    if (count == 0) {
        if (frames->default_idle_count > 0) {
            count = frames->default_idle_count;
            dir_str = "default_idle";
        }
    }

    // Priority 4: Final fallback to none idle
    if (count == 0) {
        if (frames->none_idle_count > 0) {
            count = frames->none_idle_count;
            dir_str = "none_idle";
        }
    }

    if (out_dir_string) *out_dir_string = dir_str;
    return count;
}

/**
 * @brief Draws a debug box when dev_ui is enabled.
 * Visual debugging aid to see entity boundaries and types.
 */
static void draw_dev_ui_box(
    Rectangle dest_rec,
    const char* entity_type
) {
    Color color = BLACK;

    if (strcmp(entity_type, "self") == 0) {
        color = (Color){0, 200, 255, 255};      // Cyan for player
    } else if (strcmp(entity_type, "other") == 0) {
        color = (Color){255, 100, 0, 255};      // Orange for other players
    } else if (strcmp(entity_type, "bot") == 0) {
        color = (Color){100, 200, 100, 255};    // Green for bots
    } else if (strcmp(entity_type, "floor") == 0) {
        color = (Color){50, 55, 50, 100};       // Dark gray for floor
    }

    DrawRectanglePro(dest_rec, (Vector2){0, 0}, 0.0f, color);
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

/**
 * @brief Draws all object layers for an entity with proper animation and sorting.
 *
 * When dev_ui is true, also draws debug boxes showing entity boundaries.
 * Only draws when dev_ui is false to render the actual game graphics.
 *
 * @param render The EntityRender system
 * @param entity_id Unique identifier for this entity
 * @param pos_x World X position in grid coordinates
 * @param pos_y World Y position in grid coordinates
 * @param width Entity width in grid units
 * @param height Entity height in grid units
 * @param direction Current facing direction
 * @param mode Current animation mode (Idle, Walking, Teleporting)
 * @param layers_state Array of active object layer states
 * @param layers_count Number of layers in the array
 * @param entity_type Type identifier ("self", "other", "bot", "floor")
 * @param dev_ui Whether developer UI debugging is enabled
 * @param cell_size Size of a grid cell in pixels
 */
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

    // Return early if no layers
    // Object layers should render regardless of dev_ui setting
    if (!layers_state || layers_count <= 0) {
        return;
    }

    // ========================================================================
    // Layer Collection and Sorting
    // ========================================================================

    LayerRenderInfo layers_to_render[MAX_LAYERS_PER_ENTITY];
    int render_count = 0;

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

    // Sort layers by priority (lower z-order first)
    qsort(
        layers_to_render,
        render_count,
        sizeof(LayerRenderInfo),
        compare_layer_priority
    );

    // ========================================================================
    // Animation Frame Rendering Loop
    // ========================================================================

    double now = GetTime();

    for (int i = 0; i < render_count; i++) {
        ObjectLayer* layer = layers_to_render[i].layer;
        ObjectLayerState* state = layers_to_render[i].state;

        // Get or create animation state
        AnimationState* anim = get_animation_state(
            render,
            entity_id,
            state->item_id
        );
        if (!anim) continue;

        // ====================================================================
        // Direction and Mode Selection Logic
        // ====================================================================

        // Update last_facing_direction if we have a new non-NONE direction
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

        // ====================================================================
        // Frame Selection with Fallbacks
        // ====================================================================

        const char* dir_string = NULL;
        int num_frames = get_frame_count_and_direction(
            &layer->data.render.frames,
            render_direction,
            render_mode,
            layer->data.render.is_stateless,
            &dir_string
        );

        if (num_frames <= 0) {
            continue;  // No frames available, skip this layer
        }

        // ====================================================================
        // State Change Detection and Reset
        // ====================================================================

        // Reset animation if state has changed
        if (!anim->last_state_string ||
            strcmp(anim->last_state_string, dir_string) != 0) {
            
            // Free old string
            if (anim->last_state_string) {
                free(anim->last_state_string);
            }

            // Set new state
            anim->last_state_string = (char*)malloc(strlen(dir_string) + 1);
            if (anim->last_state_string) {
                strcpy(anim->last_state_string, dir_string);
            }

            anim->frame_index = 0;
            anim->last_update_time = now;
        }

        // ====================================================================
        // Frame Timing and Animation Advancement
        // ====================================================================

        int frame_duration_ms = layer->data.render.frame_duration;
        if (frame_duration_ms <= 0) {
            frame_duration_ms = DEFAULT_FRAME_DURATION_MS;
        }

        // Check if enough time has passed to advance to next frame
        double elapsed_ms = (now - anim->last_update_time) * 1000.0;
        if (elapsed_ms >= frame_duration_ms) {
            anim->frame_index = (anim->frame_index + 1) % num_frames;
            anim->last_update_time = now;
        }

        // Safety clamp
        if (anim->frame_index >= num_frames) {
            anim->frame_index = 0;
        }

        // ====================================================================
        // Texture Loading and Rendering
        // ====================================================================

        // Get direction code for texture URI construction
        const char* direction_code = get_code_from_direction(dir_string);
        if (!direction_code) {
            continue;
        }

        // Build texture URI
        char uri[512];
        build_object_layer_uri(
            uri,
            sizeof(uri),
            layer->data.item.type,
            layer->data.item.id,
            direction_code,
            anim->frame_index
        );

        // Load texture (cached)
        Texture2D texture = load_texture_from_url(render->texture_manager, uri);

        // Draw texture if loaded successfully
        if (texture.id > 0) {
            Rectangle source_rec = {
                0.0f,
                0.0f,
                (float)texture.width,
                (float)texture.height
            };

            DrawTexturePro(
                texture,
                source_rec,
                dest_rec,
                (Vector2){0.0f, 0.0f},
                0.0f,
                WHITE
            );
        }
    }
}