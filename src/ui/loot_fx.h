#ifndef CYBERIA_UI_LOOT_FX_H
#define CYBERIA_UI_LOOT_FX_H

#include <stdbool.h>
#include <stdint.h>

#include "object_layer.h"
#include "world_types.h"

/* Loot drop FX — the full client-side visual lifecycle of a scattered drop.
 *
 * Presentation-only: the simulation never reads this module. It interpolates
 * authoritative server events into a three-stage cinematic:
 *
 *   1. Launch  (MsgTypeDropSpawn) — the in-world drop token arcs from the
 *      corpse center to its rest cell with a swift parabola and a landing
 *      bounce. Collision is server-gated for the same window.
 *   2. Idle    — while it waits, the token floats on a continuous sine bounce
 *      over its cell (no background plate).
 *   3. Collect (MsgTypeDropCollect) — a golden/cyan particle burst fires at the
 *      ground location and the token vacuums along a parabolic arc into the
 *      collecting player's avatar boundary before it is erased.
 *
 * Stages 1–2 drive the render position of the drop bot that lives in the world
 * mirror (loot_fx_drop_render_pos). Stage 3 owns detached flight tokens and the
 * particle field, painted through the renderer bridge below. */

typedef struct {
    char  item_id[MAX_ITEM_ID_LENGTH];
    float x, y;   /* current token center, grid units          */
    float size;   /* current square side, grid units           */
    float alpha;  /* 0..1 — fades to 0 as it enters the UI      */
} LootFxRender;

typedef struct {
    float   x, y;    /* center, grid units                        */
    float   size;    /* square side, grid units                   */
    float   alpha;   /* 0..1                                       */
    uint8_t tint;    /* 0 = gold, 1 = cyan                         */
} LootFxParticle;

/* Reset all flights, drop animations, and particles (call on world reset). */
void loot_fx_reset(void);

/* MsgTypeDropSpawn: register a corpse→cell launch for an in-world drop token.
 * origin/landing are grid-unit centers; launch_ms is the settle duration. */
void loot_fx_note_spawn(const char* drop_id, float origin_x, float origin_y,
                        float landing_x, float landing_y, const char* item_id,
                        uint16_t launch_ms);

/* MsgTypeDropCollect: fire the burst + vacuum flight and stop the in-world idle
 * render for this token. world_x / world_y are the token's ground center. */
void loot_fx_push(const char* drop_id, const char* collector_id,
                  const char* item_id, float world_x, float world_y);

/* Advance every flight, drop animation, and particle by dt seconds. */
void loot_fx_update(float dt);

/* In-world drop render hook. Given the token's authoritative AOI rect (top-left
 * + dims, grid units), writes the animated top-left draw position (launch arc
 * or idle float). Returns false when the token has been collected and its
 * in-world render must yield to the detached vacuum flight. */
bool loot_fx_drop_render_pos(const char* drop_id,
                             float land_topx, float land_topy,
                             float dims_w, float dims_h,
                             float* out_topx, float* out_topy);

/* Renderer bridge: detached vacuum tokens. */
int  loot_fx_slot_count(void);
bool loot_fx_render_at(int i, LootFxRender* out);

/* Renderer bridge: collection-burst particles. */
int  loot_fx_particle_slot_count(void);
bool loot_fx_particle_at(int i, LootFxParticle* out);

#endif /* CYBERIA_UI_LOOT_FX_H */
