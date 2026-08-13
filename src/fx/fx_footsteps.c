/* Footstep emitter: turns entity movement into dust bursts. Fixed tracker
 * table, no heap. See fx_footsteps.h for why the scan lives here instead of in
 * a character controller. */

#include "fx_footsteps.h"

#include "fx_dust.h"

#include "domain/camera.h"
#include "domain/local_player.h"
#include "game_state.h"
#include "object_layer.h"
#include "world_types.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

/* Grid units of travel between bursts — the stride. Shorter when running, so
 * the extra dust comes from the faster gait rather than only from speed. */
#define FX_STEP_STRIDE_CELLS      0.55f
#define FX_STEP_RUN_STRIDE_CELLS  0.42f

/* Ground travel over the base move speed that reads as a run. */
#define FX_STEP_RUN_FRACTION      1.25f

/* Below this the entity is drifting (interpolation settle, knockback residue),
 * not walking, and raises nothing. Grid units per second. */
#define FX_STEP_MIN_SPEED_CELLS_S 0.35f

/* A jump beyond this in one frame is a teleport, respawn, or snap: the stride
 * restarts from the new position with no dust between the two. Grid units. */
#define FX_STEP_TELEPORT_CELLS    1.50f

/* Spawn offset behind the feet, along the reverse of travel. Grid units. */
#define FX_STEP_BEHIND_CELLS      0.16f

/* Fallback when the server has not pushed a move speed yet. Grid units/s. */
#define FX_STEP_SPEED_FALLBACK    4.0f

/* Screen margin kept around the viewport when culling off-screen steps. */
#define FX_STEP_CULL_MARGIN_PX    64

/* Run burst, relative to the walking baseline in fx_dust_default_params. */
#define FX_STEP_RUN_SIZE_SCALE    1.30f
#define FX_STEP_RUN_DRIFT_SCALE   1.45f
#define FX_STEP_RUN_EXTRA_PUFFS   1

typedef struct {
    char    entity_id[MAX_ID_LENGTH];
    Vector2 last_pos;      /* grid units, entity top-left */
    float   stride_accum;  /* grid units travelled since the last burst */
    bool    primed;        /* last_pos holds a real sample to measure against */
    bool    seen;          /* touched by this frame's scan */
    bool    used;
} FxStepTracker;

static FxStepTracker s_trackers[FX_FOOTSTEPS_MAX_TRACKED];
static bool          s_ready = false;

void fx_footsteps_init(void) {
    memset(s_trackers, 0, sizeof(s_trackers));
    s_ready = true;
}

void fx_footsteps_reset(void) {
    memset(s_trackers, 0, sizeof(s_trackers));
}

/* Existing tracker for `id`, or a free slot claimed for it. NULL when the
 * table is full — that entity raises no dust until a slot frees up. */
static FxStepTracker* tracker_for(const char* id) {
    FxStepTracker* free_slot = NULL;
    for (int i = 0; i < FX_FOOTSTEPS_MAX_TRACKED; i++) {
        FxStepTracker* t = &s_trackers[i];
        if (t->used) {
            if (0 == strcmp(t->entity_id, id)) return t;
        } else if (!free_slot) {
            free_slot = t;
        }
    }
    if (!free_slot) return NULL;
    memset(free_slot, 0, sizeof(*free_slot));
    strncpy(free_slot->entity_id, id, sizeof(free_slot->entity_id) - 1);
    free_slot->used = true;
    return free_slot;
}

static float cell_size(void) {
    return g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
}

static float walk_speed_ref(void) {
    float speed = local_player_move_speed();
    return speed > 0.0f ? speed : FX_STEP_SPEED_FALLBACK;
}

/* Burst shape for the gait: a run throws more dust, bigger, further, higher. */
static FxDustParams step_params(bool running) {
    FxDustParams p = fx_dust_default_params();
    if (running) {
        p.size  *= FX_STEP_RUN_SIZE_SCALE;
        p.drift *= FX_STEP_RUN_DRIFT_SCALE;
        p.rise  *= FX_STEP_RUN_DRIFT_SCALE;
        p.count += FX_STEP_RUN_EXTRA_PUFFS;
    }
    return p;
}

/* Dust nobody can see still costs a pool slot, and the pool is what keeps a
 * crowd's puffs alive long enough to read. AOI reaches past the viewport, so
 * cull off-screen steps rather than let them recycle the visible ones. */
static bool step_is_visible(Vector2 world_px) {
    Vector2 s = GetWorldToScreen2D(world_px, camera_get());
    float margin = (float)FX_STEP_CULL_MARGIN_PX;
    return s.x >= -margin && s.x <= (float)GetScreenWidth() + margin &&
           s.y >= -margin && s.y <= (float)GetScreenHeight() + margin;
}

/* One burst at the entity's feet, set back along the reverse of travel so the
 * dust trails the character instead of sitting under the sprite. */
static void emit_step(const EntityState* e, Vector2 heading, bool running) {
    float cs = cell_size();
    float feet_x = (e->interp_pos.x + e->dims.x * 0.5f) * cs;
    float feet_y = (e->interp_pos.y + e->dims.y) * cs;
    Vector2 at = { feet_x - heading.x * FX_STEP_BEHIND_CELLS * cs,
                   feet_y - heading.y * FX_STEP_BEHIND_CELLS * cs };
    if (!step_is_visible(at)) return;

    FxDustParams params = step_params(running);
    fx_dust_spawn(at, heading, &params);
}

/* Advance one entity's stride and emit whatever steps it crossed this frame. */
static void track_entity(const EntityState* e, float dt) {
    if (!e || '\0' == e->id[0]) return;

    FxStepTracker* t = tracker_for(e->id);
    if (!t) return;
    t->seen = true;

    Vector2 pos = e->interp_pos;
    Vector2 delta = { pos.x - t->last_pos.x, pos.y - t->last_pos.y };
    float moved = sqrtf(delta.x * delta.x + delta.y * delta.y);
    bool  fresh = !t->primed;
    t->last_pos = pos;
    t->primed   = true;

    /* Dead entities, snapped ones, and the first frame of a tracker have no
     * gait to read — restart the stride from here. */
    if (fresh || moved > FX_STEP_TELEPORT_CELLS || e->respawn_in > 0.0f) {
        t->stride_accum = 0.0f;
        return;
    }
    /* The client's own walk/idle state, so the dust agrees with the animation
     * being drawn rather than with the raw snapshot. */
    if (MODE_WALKING != e->mode || moved <= 0.0f || dt <= 0.0f) return;

    float speed = moved / dt;
    if (speed < FX_STEP_MIN_SPEED_CELLS_S) return;

    bool  running = speed >= walk_speed_ref() * FX_STEP_RUN_FRACTION;
    float stride  = running ? FX_STEP_RUN_STRIDE_CELLS : FX_STEP_STRIDE_CELLS;
    Vector2 heading = { delta.x / moved, delta.y / moved };

    t->stride_accum += moved;
    if (t->stride_accum < stride) return;
    /* One burst per frame: a frame long enough to cover several strides is a
     * hitch, and stacking its bursts on one spot only makes a blob. */
    t->stride_accum = fmodf(t->stride_accum, stride);
    emit_step(e, heading, running);
}

/* Loot, coins, and skill projectiles travel without walking. Their bots are
 * excluded exactly as they are from the ground shadow. */
static bool bot_has_feet(const BotState* bot) {
    if ('\0' != bot->caster_id[0]) return false;
    return 0 != strcmp(bot->behavior, "skill") &&
           0 != strcmp(bot->behavior, "coin") &&
           0 != strcmp(bot->behavior, "drop");
}

void fx_footsteps_update(float dt) {
    if (!s_ready) fx_footsteps_init();
    if (!g_game_state.init_received) return;

    for (int i = 0; i < FX_FOOTSTEPS_MAX_TRACKED; i++) s_trackers[i].seen = false;

    if ('\0' != g_game_state.player_id[0]) track_entity(&g_game_state.player.base, dt);
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        track_entity(&g_game_state.other_players[i].base, dt);
    }
    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (bot_has_feet(&g_game_state.bots[i])) track_entity(&g_game_state.bots[i].base, dt);
    }

    /* Release entities that left the AOI, so their slots serve whoever walks
     * in next and a returning entity restarts its stride instead of measuring
     * against a position it left long ago. */
    for (int i = 0; i < FX_FOOTSTEPS_MAX_TRACKED; i++) {
        if (s_trackers[i].used && !s_trackers[i].seen) {
            memset(&s_trackers[i], 0, sizeof(s_trackers[i]));
        }
    }
}
