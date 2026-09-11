// Level-up celebration. The aura is a fixed set of sparks, each on its own
// phase of one rise: born on the bottom edge of the entity, lifted past the
// head, shrinking to nothing, then reborn. Shapes stay opaque and leave by
// shrinking, in the particle style of the tap and loot effects. Everything is
// sized from the entity footprint and the cell so it reads at any zoom.

#include "fx_level_up.h"

#include "fx_shapes.h"

#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "ui/text.h"
#include "ui/ui_icon.h"
#include "world_types.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#define FX_LEVEL_UP_CAPACITY    8
#define FX_LEVEL_UP_TAU         6.28318530718f

#define FX_LEVEL_UP_DURATION    3.0f
#define FX_LEVEL_UP_EXIT_S      0.4f
#define FX_LEVEL_UP_SPARKS      18
#define FX_LEVEL_UP_RISE_S      1.3f   /* one spark's climb, feet to above the head */

typedef struct {
    char    entity_id[MAX_ID_LENGTH];
    Vector2 pos;      /* last known top-left, grid units */
    Vector2 dims;     /* last known footprint, grid units */
    float   age;
    bool    active;
} FxLevelUpEntry;

static FxLevelUpEntry s_entries[FX_LEVEL_UP_CAPACITY];

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
    float left = FX_LEVEL_UP_DURATION - age;
    return left < FX_LEVEL_UP_EXIT_S ? left / FX_LEVEL_UP_EXIT_S : 1.0f;
}

bool fx_level_up_observe(int previous, int current, const EntityState* entity) {
    if (0 >= previous || current <= previous || NULL == entity || '\0' == entity->id[0]) return false;
    FxLevelUpEntry* slot = &s_entries[0];
    for (int i = 0; FX_LEVEL_UP_CAPACITY > i; i++) {
        if (0 == strcmp(s_entries[i].entity_id, entity->id)) { slot = &s_entries[i]; break; }
        if (!s_entries[i].active) { slot = &s_entries[i]; break; }
        if (s_entries[i].age > slot->age) slot = &s_entries[i];
    }
    *slot = (FxLevelUpEntry){ .pos = entity->interp_pos, .dims = entity->dims, .active = true };
    strncpy(slot->entity_id, entity->id, MAX_ID_LENGTH - 1);
    return true;
}

void fx_level_up_update(float dt) {
    for (int i = 0; FX_LEVEL_UP_CAPACITY > i; i++) {
        FxLevelUpEntry* entry = &s_entries[i];
        if (!entry->active) continue;
        entry->age += dt;
        if (entry->age >= FX_LEVEL_UP_DURATION) { entry->active = false; continue; }
        /* Follow the entity while it is in view; keep the last footprint
         * when it leaves so the aura finishes where it was last seen. */
        const EntityState* entity = game_state_find_entity(entry->entity_id);
        if (NULL != entity) { entry->pos = entity->interp_pos; entry->dims = entity->dims; }
    }
}

static void draw_entry(const FxLevelUpEntry* entry, float cell, int salt) {
    float scale = exit_scale(entry->age);
    float left = entry->pos.x * cell;
    float width = entry->dims.x * cell;
    float feet = (entry->pos.y + entry->dims.y) * cell;
    float height = entry->dims.y * cell;
    float cx = left + width * 0.5f;
    float climb = height + cell * 0.8f;

    /* Aura: each spark rises from a fixed lane on the bottom edge, swaying a
     * little, and shrinks as it climbs. Its phase offsets it in the loop. */
    for (int i = 0; FX_LEVEL_UP_SPARKS > i; i++) {
        float t = fmodf(entry->age / FX_LEVEL_UP_RISE_S + lane(i, salt), 1.0f);
        float rise = ease_out_cubic(t);
        float x = left + lane(i, salt + 1) * width + sinf((t + lane(i, salt + 2)) * FX_LEVEL_UP_TAU) * cell * 0.08f;
        float y = feet - rise * climb;
        float size = cell * (0.32f + 0.16f * lane(i, salt + 3)) * (1.0f - t) * scale;
        if (3.0f > size) continue;
        fx_shape_spark_shaded(x, y, size, FX_SPARK_GOLD);
    }

    /* Star: swells in over the head, spins slowly, pulses, then shrinks away. */
    float star_t = ease_out_cubic(fminf(1.0f, entry->age / 0.4f));
    float star_size = cell * 1.1f * star_t * scale * (1.0f + 0.1f * sinf(entry->age * FX_LEVEL_UP_TAU * 1.2f));
    float star_y = feet - climb - cell * 0.5f;
    if (1.0f <= star_size) {
        ui_icon_draw_ex("star", cx, star_y, star_size, entry->age * 60.0f, (Color){ 255, 226, 20, 255 });
    }

    /* Label: above the star with a black outline so it reads anywhere. */
    int font = (int)(cell * 0.5f * scale);
    if (4 > font) return;
    const char* label = "LEVEL UP";
    int x = (int)(cx - MeasureText(label, font) * 0.5f);
    int y = (int)(star_y - star_size * 0.5f - font - cell * 0.15f);
    for (int dy = -1; 1 >= dy; dy++)
        for (int dx = -1; 1 >= dx; dx++)
            if (dx || dy) DrawText(label, x + dx, y + dy, font, BLACK);
    DrawText(label, x, y, font, (Color){ 255, 226, 20, 255 });
}

void fx_level_up_draw(void) {
    float cell = world_cell_size();
    for (int i = 0; FX_LEVEL_UP_CAPACITY > i; i++) {
        if (s_entries[i].active) draw_entry(&s_entries[i], cell, i);
    }
}

void fx_level_up_reset(void) {
    memset(s_entries, 0, sizeof(s_entries));
}

bool fx_level_up_active(const char* entity_id) {
    for (int i = 0; FX_LEVEL_UP_CAPACITY > i; i++) {
        if (s_entries[i].active && 0 == strcmp(s_entries[i].entity_id, entity_id)) return true;
    }
    return false;
}
