// Transport aura. A ring of sparks turns on a flat ellipse around the feet and
// more sparks climb a spiral from the feet, shrinking to nothing. Shapes stay
// opaque and leave by shrinking, in the particle style of the other auras, and
// everything is sized from the entity footprint and the cell.

#include "fx_portal_aura.h"

#include "fx_shapes.h"

#include "content/presentation_runtime.h"
#include "sim/game_state.h"
#include "world/world_types.h"

#include <math.h>
#include <raylib.h>
#include <stdint.h>

#define FX_PORTAL_AURA_TAU    6.28318530718f
#define FX_PORTAL_AURA_RING   10
#define FX_PORTAL_AURA_SPARKS 12
#define FX_PORTAL_AURA_TURN_S 1.4f   /* one turn of the ring */
#define FX_PORTAL_AURA_RISE_S 0.8f   /* one spark's climb */

static float s_clock;

/* Deterministic 0..1 per spark, so each keeps its lane and phase. */
static float lane(int i, int salt) {
    unsigned h = (unsigned)(i * 2654435761u) ^ (unsigned)(salt * 40503u);
    return (float)(h % 1000u) * 0.001f;
}

void fx_portal_aura_update(float dt) {
    s_clock += dt;
}

static void draw_entity(int slot, float cell) {
    Vector2 pos = g_game_state.render[slot].interp_pos;
    Vector2 size = g_game_state.transform[slot].size;
    float charge = fminf(fmaxf(g_game_state.player[slot].portal_hold, 0.0f), 1.0f);
    float width = size.x * cell;
    float height = size.y * cell;
    float feet = (pos.y + size.y) * cell;
    float cx = pos.x * cell + width * 0.5f;
    float rx = width * 0.5f + cell * 0.35f;
    float ry = rx * 0.35f;
    float turn = s_clock / FX_PORTAL_AURA_TURN_S * FX_PORTAL_AURA_TAU;

    /* Ring: the far side draws smaller, so the ring reads as lying on the ground. */
    for (int i = 0; FX_PORTAL_AURA_RING > i; i++) {
        float angle = turn + (float)i / FX_PORTAL_AURA_RING * FX_PORTAL_AURA_TAU;
        float size = cell * 0.24f * (0.75f + 0.25f * sinf(angle));
        if (3.0f > size) continue;
        fx_shape_spark_shaded(cx + cosf(angle) * rx, feet + sinf(angle) * ry, size, FX_SPARK_BLUE);
    }

    /* Spiral: the charge raises the column and adds sparks to it. */
    float reach = (height + cell * 0.6f) * (0.35f + 0.65f * charge);
    int sparks = (int)(FX_PORTAL_AURA_SPARKS * (0.4f + 0.6f * charge) + 0.5f);
    for (int i = 0; sparks > i; i++) {
        float t = fmodf(s_clock / FX_PORTAL_AURA_RISE_S + lane(i, 1), 1.0f);
        float angle = 2.0f * turn + (lane(i, 2) + t) * FX_PORTAL_AURA_TAU;
        float size = cell * (0.18f + 0.1f * lane(i, 3)) * (1.0f - t);
        if (3.0f > size) continue;
        fx_shape_spark_shaded(cx + cosf(angle) * rx * (1.0f - 0.5f * t), feet - t * reach, size, FX_SPARK_BLUE);
    }
}

void fx_portal_aura_draw(void) {
    float cell = world_cell_size();
    const uint64_t need = COMP_PLAYER | COMP_TRANSFORM;
    for (int s = 0; ENTITY_CAP > s; s++) {
        if (need == (g_game_state.mask[s] & need) && g_game_state.player[s].on_portal) draw_entity(s, cell);
    }
}
