// Heal aura. A fixed set of sparks turns on an ellipse around the feet, more
// sparks rise from the feet and shrink to nothing, and plus marks float up
// over the head one after another. Shapes stay opaque and leave by shrinking,
// in the particle style of the level-up aura. Everything is sized from the
// entity footprint and the cell so it reads at any zoom.

#include "fx_heal_aura.h"

#include "fx_shapes.h"

#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "world_types.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#define FX_HEAL_AURA_CAPACITY   8
#define FX_HEAL_AURA_TAU        6.28318530718f
#define FX_HEAL_AURA_BEHAVIOR   "consume-on-pickup-heal"

#define FX_HEAL_AURA_DURATION   1.8f
#define FX_HEAL_AURA_EXIT_S     0.4f
#define FX_HEAL_AURA_GROW_S     0.3f   /* the ring opens out from the feet */
#define FX_HEAL_AURA_RING       12
#define FX_HEAL_AURA_TURN_S     1.2f   /* one turn of the ring */
#define FX_HEAL_AURA_SPARKS     10
#define FX_HEAL_AURA_RISE_S     0.9f   /* one spark's climb, feet to head */
#define FX_HEAL_AURA_PLUSES     3
#define FX_HEAL_AURA_PLUS_S     0.9f   /* one plus mark's float */
#define FX_HEAL_AURA_PLUS_GAP_S 0.35f  /* between two plus marks */

typedef struct {
    EntityID entity_id;
    Vector2 pos;      /* last known top-left, grid units */
    Vector2 dims;     /* last known footprint, grid units */
    float   age;
    bool    active;
} FxHealAuraEntry;

static FxHealAuraEntry s_entries[FX_HEAL_AURA_CAPACITY];

static float ease_out_cubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

/* Deterministic 0..1 per spark, so each keeps its lane and phase. */
static float lane(int i, int salt) {
    unsigned h = (unsigned)(i * 2654435761u) ^ (unsigned)(salt * 40503u);
    return (float)(h % 1000u) * 0.001f;
}

/* Full while the aura runs, then shrinks to 0 over the exit window. */
static float exit_scale(float age) {
    float left = FX_HEAL_AURA_DURATION - age;
    return left < FX_HEAL_AURA_EXIT_S ? left / FX_HEAL_AURA_EXIT_S : 1.0f;
}

bool fx_heal_aura_observe(const char* behavior, const EntityState* entity) {
    if (NULL == behavior || 0 != strcmp(behavior, FX_HEAL_AURA_BEHAVIOR)) return false;
    if (NULL == entity || 0 == entity->id) return false;
    FxHealAuraEntry* slot = &s_entries[0];
    for (int i = 0; FX_HEAL_AURA_CAPACITY > i; i++) {
        if (s_entries[i].entity_id == entity->id) { slot = &s_entries[i]; break; }
        if (!s_entries[i].active) { slot = &s_entries[i]; break; }
        if (s_entries[i].age > slot->age) slot = &s_entries[i];
    }
    *slot = (FxHealAuraEntry){ .entity_id = entity->id, .pos = entity->interp_pos,
                               .dims = entity->dims, .active = true };
    return true;
}

void fx_heal_aura_update(float dt) {
    for (int i = 0; FX_HEAL_AURA_CAPACITY > i; i++) {
        FxHealAuraEntry* entry = &s_entries[i];
        if (!entry->active) continue;
        entry->age += dt;
        if (entry->age >= FX_HEAL_AURA_DURATION) { entry->active = false; continue; }
        /* Follow the entity while it is in view; keep the last footprint
         * when it leaves so the aura finishes where it was last seen. */
        const EntityState* entity = game_state_find_entity(entry->entity_id);
        if (NULL != entity) { entry->pos = entity->interp_pos; entry->dims = entity->dims; }
    }
}

static void draw_entry(const FxHealAuraEntry* entry, float cell, int salt) {
    float scale = exit_scale(entry->age);
    float left = entry->pos.x * cell;
    float width = entry->dims.x * cell;
    float height = entry->dims.y * cell;
    float feet = (entry->pos.y + entry->dims.y) * cell;
    float cx = left + width * 0.5f;

    /* Ring: sparks turn on a flat ellipse around the feet. The far side draws
     * smaller, so the ring reads as lying on the ground. */
    float rx = (width * 0.5f + cell * 0.3f) * ease_out_cubic(fminf(1.0f, entry->age / FX_HEAL_AURA_GROW_S));
    float ry = rx * 0.35f;
    float turn = entry->age / FX_HEAL_AURA_TURN_S * FX_HEAL_AURA_TAU;
    for (int i = 0; FX_HEAL_AURA_RING > i; i++) {
        float angle = turn + (float)i / FX_HEAL_AURA_RING * FX_HEAL_AURA_TAU;
        float size = cell * 0.26f * (0.75f + 0.25f * sinf(angle)) * scale;
        if (3.0f > size) continue;
        fx_shape_spark_shaded(cx + cosf(angle) * rx, feet + sinf(angle) * ry, size, FX_SPARK_GREEN);
    }

    /* Sparks: each rises from a fixed lane on the feet through the body and
     * shrinks as it climbs. Its phase offsets it in the loop. */
    for (int i = 0; FX_HEAL_AURA_SPARKS > i; i++) {
        float t = fmodf(entry->age / FX_HEAL_AURA_RISE_S + lane(i, salt), 1.0f);
        float x = left + lane(i, salt + 1) * width;
        float y = feet - ease_out_cubic(t) * height;
        float size = cell * (0.22f + 0.12f * lane(i, salt + 2)) * (1.0f - t) * scale;
        if (3.0f > size) continue;
        fx_shape_spark_shaded(x, y, size, FX_SPARK_GREEN);
    }

    /* Plus marks: one after another, each floats from the chest to above the
     * head, swelling in and shrinking away. */
    for (int i = 0; FX_HEAL_AURA_PLUSES > i; i++) {
        float t = (entry->age - i * FX_HEAL_AURA_PLUS_GAP_S) / FX_HEAL_AURA_PLUS_S;
        if (0.0f > t || 1.0f < t) continue;
        float x = left + width * (0.2f + 0.6f * lane(i, salt + 3));
        float y = feet - height * 0.5f - ease_out_cubic(t) * (height * 0.5f + cell * 0.6f);
        float size = cell * 0.5f * sinf(t * FX_HEAL_AURA_TAU * 0.5f);
        if (3.0f > size) continue;
        fx_shape_plus(x, y, size, FX_SPARK_GREEN);
    }
}

void fx_heal_aura_draw(void) {
    float cell = world_cell_size();
    for (int i = 0; FX_HEAL_AURA_CAPACITY > i; i++) {
        if (s_entries[i].active) draw_entry(&s_entries[i], cell, i);
    }
}

void fx_heal_aura_reset(void) {
    memset(s_entries, 0, sizeof(s_entries));
}
