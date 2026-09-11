// Defeat cue. The sparks are a fixed set thrown out of the body's centre on
// their own headings: each flies out, arcs down under gravity, and shrinks to
// nothing. The skull swells in over the head, drifts up, and leaves by
// shrinking. Shapes stay opaque, in the particle style of the tap and loot
// effects, and everything is sized from the footprint and the cell.

#include "fx_death.h"

#include "fx_shapes.h"

#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "ui/ui_icon.h"
#include "world_types.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#define FX_DEATH_CAPACITY   8
#define FX_DEATH_TAU        6.28318530718f

#define FX_DEATH_DURATION   2.6f
#define FX_DEATH_EXIT_S     0.45f
#define FX_DEATH_SPARKS     16
#define FX_DEATH_BURST_S    1.8f   /* a spark's flight, burst to gone */

typedef struct {
    char    entity_id[MAX_ID_LENGTH];
    Vector2 pos;      /* last known top-left, grid units */
    Vector2 dims;     /* last known footprint, grid units */
    float   age;
    bool    active;
} FxDeathEntry;

static FxDeathEntry s_entries[FX_DEATH_CAPACITY];

/* Deterministic 0..1 per spark, so each keeps its heading. */
static float lane(int i, int salt) {
    unsigned h = (unsigned)(i * 2654435761u) ^ (unsigned)(salt * 40503u);
    return (float)(h % 1000u) * 0.001f;
}

/* Full while the cue runs, then shrinks to 0 over the exit window. */
static float exit_scale(float age) {
    float left = FX_DEATH_DURATION - age;
    return left < FX_DEATH_EXIT_S ? left / FX_DEATH_EXIT_S : 1.0f;
}

bool fx_death_observe(bool prior_alive, bool dead, const EntityState* entity) {
    if (!prior_alive || !dead || NULL == entity || '\0' == entity->id[0]) return false;
    FxDeathEntry* slot = &s_entries[0];
    for (int i = 0; FX_DEATH_CAPACITY > i; i++) {
        if (0 == strcmp(s_entries[i].entity_id, entity->id)) { slot = &s_entries[i]; break; }
        if (!s_entries[i].active) { slot = &s_entries[i]; break; }
        if (s_entries[i].age > slot->age) slot = &s_entries[i];
    }
    *slot = (FxDeathEntry){ .pos = entity->interp_pos, .dims = entity->dims, .active = true };
    strncpy(slot->entity_id, entity->id, MAX_ID_LENGTH - 1);
    return true;
}

void fx_death_update(float dt) {
    for (int i = 0; FX_DEATH_CAPACITY > i; i++) {
        FxDeathEntry* entry = &s_entries[i];
        if (!entry->active) continue;
        entry->age += dt;
        if (entry->age >= FX_DEATH_DURATION) { entry->active = false; continue; }
        const EntityState* entity = game_state_find_entity(entry->entity_id);
        if (NULL != entity) { entry->pos = entity->interp_pos; entry->dims = entity->dims; }
    }
}

static void draw_entry(const FxDeathEntry* entry, float cell, int salt) {
    float scale = exit_scale(entry->age);
    float width = entry->dims.x * cell;
    float height = entry->dims.y * cell;
    float cx = (entry->pos.x + entry->dims.x * 0.5f) * cell;
    float cy = (entry->pos.y + entry->dims.y * 0.5f) * cell;
    float top = entry->pos.y * cell;

    /* Sparks: thrown from the body on fixed headings, mostly upward, then
     * pulled down past the feet while they shrink. */
    float t = entry->age / FX_DEATH_BURST_S;
    if (1.0f > t) {
        for (int i = 0; FX_DEATH_SPARKS > i; i++) {
            float angle = -FX_DEATH_TAU * 0.5f * (0.1f + 0.8f * lane(i, salt));
            float speed = cell * (1.2f + 1.0f * lane(i, salt + 1));
            float x = cx + cosf(angle) * speed * t + (lane(i, salt + 2) - 0.5f) * width * 0.6f;
            float y = cy + sinf(angle) * speed * t + cell * 2.2f * t * t;
            float size = cell * (0.3f + 0.2f * lane(i, salt + 3)) * (1.0f - t);
            if (3.0f > size) continue;
            fx_shape_spark_shaded(x, y, size, FX_SPARK_RED);
        }
    }

    /* Skull: swells in over the head, drifts up, wobbles, then shrinks away. */
    float rise = fminf(1.0f, entry->age / 0.35f);
    rise = 1.0f - (1.0f - rise) * (1.0f - rise);
    float size = cell * 1.2f * rise * scale;
    float y = top - cell * (0.4f + 0.5f * entry->age / FX_DEATH_DURATION) - height * 0.1f;
    if (1.0f <= size) {
        ui_icon_draw_ex("skull", cx, y, size, sinf(entry->age * FX_DEATH_TAU * 0.8f) * 12.0f,
                        (Color){ 255, 255, 255, 255 });
    }
}

void fx_death_draw(void) {
    float cell = world_cell_size();
    for (int i = 0; FX_DEATH_CAPACITY > i; i++) {
        if (s_entries[i].active) draw_entry(&s_entries[i], cell, i);
    }
}

void fx_death_reset(void) {
    memset(s_entries, 0, sizeof(s_entries));
}

bool fx_death_active(const char* entity_id) {
    for (int i = 0; FX_DEATH_CAPACITY > i; i++) {
        if (s_entries[i].active && 0 == strcmp(s_entries[i].entity_id, entity_id)) return true;
    }
    return false;
}
